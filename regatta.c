#include <libxml/HTMLparser.h>
#include <libxml/xpath.h>
#include <pthread.h>
#include <curl/curl.h>
#include <string.h>
#include <libgen.h>          // basename
#include <sys/param.h>       // MAXPATHLEN
#include "regatta.h"
#include "sailor.h"
#include "curl.h"
  
// just a single private instance of the pool, not going to pass it around
RegattaPool __rp;

// make it thread-safe
pthread_mutex_t __rp_mutex;
pthread_mutexattr_t __rp_mutex_attr;

xmlDocPtr getDoc(char *docname);
xmlNodeSetPtr getXpathNodeSet(char *xpath, xmlXPathContextPtr ctx);
xmlNodeSetPtr getXpathNodeSetRel(char *xpath, xmlNodePtr relnode, xmlXPathContextPtr ctx);
void regattaFree(Regatta *regatta);

void regattaPoolInit()
{
    xmlInitParser();
    LIBXML_TEST_VERSION; // don't need this ';' but without it the indentation gets messed up

    /* Must initialize libcurl before any threads are started */
    curl_global_init(CURL_GLOBAL_ALL);
    curl_ssl_init_locks();

    // NULL pointer for __rp.regattas means that first call to regattaPoolAdd will realloc from NULL (equiv to malloc)
    __rp = (RegattaPool) {0};
    // in case we make nested or recursive calls which both lock, we use RECURSIVE type, which counts locks and unlocks
    pthread_mutexattr_init(&__rp_mutex_attr);
    pthread_mutexattr_settype(&__rp_mutex_attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&__rp_mutex, &__rp_mutex_attr);
}

void regattaPoolFree()
{
    pthread_mutex_lock(&__rp_mutex);
    for (int i=0; i<__rp.used; i++)
    {
        regattaFree(__rp.regattas[i]);  // each regatta Object
    }
    free(__rp.regattas); // and the array of pointers to those objects
    __rp = (RegattaPool) {0};
    pthread_mutex_unlock(&__rp_mutex);
    pthread_mutex_destroy(&__rp_mutex);
    pthread_mutexattr_destroy(&__rp_mutex_attr);

    curl_ssl_kill_locks();
    curl_global_cleanup();
    xmlCleanupParser();
}

// http://stackoverflow.com/questions/3536153/c-dynamically-growing-array
Regatta *regattaPoolAdd(Regatta *regatta)
{
    pthread_mutex_lock(&__rp_mutex);
    if (__rp.used == __rp.size) {
        // grow the array allocation
        __rp.size = 3 * __rp.size / 2 + 8;
                
        Regatta **t_regattas = realloc(__rp.regattas, __rp.size * sizeof(Regatta));
        if (!t_regattas) {
            fprintf(stdout, "realloc failed allocate bytes = %zu\n", __rp.size);
            free(__rp.regattas);
            exit(-1);
        }
        __rp.regattas = t_regattas;
    }
    __rp.regattas[__rp.used++] = regatta;
    pthread_mutex_unlock(&__rp_mutex);
    return regatta;
}

Regatta *regattaNew()
{
    Regatta *regatta= malloc(sizeof(Regatta));
    *regatta= (Regatta) {0};   // not {} because that is a GNU extension: https://stackoverflow.com/questions/25578115/cryptic-struct-definition-in-c
    regattaPoolAdd(regatta);   // all the regattas must be in the pool        
    return regatta;
}


void regattaFree(Regatta *regatta)
{
    // this causes probs because the urls are often in the source code of the caller and hence not malloc'd and read only
    // free(regatta->url);     // char * for the url
    free(regatta);             // and the struct which contains the 2D array of pointer to the result xmlChar's
}

#define RESULT_ROW_MAX_FIELDS 25

typedef enum {
    NAF = -1,        // not a field, used in std and cust
    HELM = 0,        // must start at zero
    SAILNO,          // and then 1,2,3
    GENDER,
} StdField;

typedef struct FieldMapItem {
    StdField std;
    int cust;
    char* pattern;
    Sailor * (*setter)(Sailor *, char *);
} FieldMapItem;


FieldMapItem pattern_fmis[] = {
    { HELM,          NAF,   "Helm",             &sailorSetName },  // the '&' is not strictly required by the compiler but more correct
    { SAILNO,        NAF,   "Sail no",          &sailorSetSailnoString },
    { GENDER,        NAF,   "M/F",              &sailorSetGender },
    { NAF,           NAF,   "",                 NULL },  // End of list terminator
};

typedef struct FieldMap {
    FieldMapItem items[RESULT_ROW_MAX_FIELDS];
} FieldMap;

typedef char *ResultRow[RESULT_ROW_MAX_FIELDS];

FieldMap *regattaNewFieldMap()
{
    FieldMap *fm = malloc(sizeof(FieldMap));
    for (int std = 0; std < RESULT_ROW_MAX_FIELDS; std++) fm->items[std] = (FieldMapItem) { std, NAF, NULL, NULL };
    return fm;
}

FieldMap *regattaMakeFieldMap(xmlNodeSetPtr header_cells)
{
    FieldMap *fm = regattaNewFieldMap();
    char *val;
    int c, p;
    for(c = 0; c < header_cells->nodeNr; c++) {
        val = (char *)xmlNodeGetContent(header_cells->nodeTab[c]);
        for(p = 0; pattern_fmis[p].std != NAF; p++) {
            if (strcasecmp(val, pattern_fmis[p].pattern) == 0) {
                fm->items[pattern_fmis[p].std] = pattern_fmis[p];   // copy pattern
                fm->items[pattern_fmis[p].std].cust = c;            // record mathching pattern
                break;
            }
        }
        free(val);
    }
    return fm;
}

Sailor *regattaBuildSailorFromMappedRow(ResultRow row, FieldMap *fm) {
    // new but not into pool
    Sailor *sailor= malloc(sizeof(Sailor));
    *sailor= (Sailor) {0};
    
    for(int std = 0; std < RESULT_ROW_MAX_FIELDS && fm->items[std].cust != NAF; std++) {
        // use the function pointer "setter" to update the sailor with the mapped row value
        (fm->items[std].setter)(sailor, row[fm->items[std].cust]);
    }
    return sailor;
}

void regattaLoad(Regatta *regatta)
{
    xmlDocPtr doc = getDoc(regatta->url);
    xmlXPathContextPtr ctx = xmlXPathNewContext(doc); 

    int r, c;
    ResultRow row_vals = {0};
    xmlNodeSetPtr tables = getXpathNodeSet("//table[@border=1]", ctx);
    xmlNodeSetPtr rows, cells;
    if (tables->nodeNr == 1)
    {
        rows = getXpathNodeSetRel(".//tr", tables->nodeTab[0], ctx);     // rows of the current table
        xmlNodeSetPtr header_cells = getXpathNodeSetRel(".//td", rows->nodeTab[0], ctx);
        FieldMap *fm = regattaMakeFieldMap(header_cells);
        xmlXPathFreeNodeSet(header_cells);
        for(r = 1; r < rows->nodeNr; r++) {                              // r = 1, because we want to skip first row, as headers
            cells = getXpathNodeSetRel(".//td", rows->nodeTab[r], ctx);  // cells of the current row
            for(c = 0; c < cells->nodeNr; c++) {
                row_vals[c] = (char *)xmlNodeGetContent(cells->nodeTab[c]);
            }


            Sailor *sailor_ex =regattaBuildSailorFromMappedRow(row_vals, fm);
            sailorPoolFindByExampleOrNew(sailor_ex); // sailor_ex gets free'd inside, or added to pool. ignore returned Sailor * for now
            
            for(c = 0; c < cells->nodeNr; c++) free(row_vals[c]); // cleanup strings created
            xmlXPathFreeNodeSet(cells);
        }
        xmlXPathFreeNodeSet(rows);
        free(fm);
    }

    xmlXPathFreeNodeSet(tables);
    xmlXPathFreeContext(ctx); 
    xmlFreeDoc(doc);
}

xmlDocPtr getDoc(char *url) {

    Buffer buffer = (Buffer) {0}; // .mem NULL prt to memory buffer will be set by realloc
  
    // can't pass url to libxml->htmlParseFile, appears it's not thread safe, need to use curl
    // TODO: there might be an option to parse the stream, rather than wait for the full html: htmlReadIO?
    if (curl_load_url(url, &buffer)) {
        fprintf(stderr,"Document not loaded successfully. \n");
        return NULL;
    }
    char *base = strdup(dirname(url));         // take a copy as it is not guaranted to persist or might be part of url
    xmlDocPtr doc = htmlReadMemory(buffer.mem, buffer.size, base, NULL,
                                   HTML_PARSE_NONET | HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING | HTML_PARSE_RECOVER); // for dirty html(5)
    free(base);
    free(buffer.mem); // don't need this anymore

    if (doc == NULL ) {
        fprintf(stderr,"Document not parsed successfully. \n");
        return NULL;
    }
    return doc;
}

xmlNodeSetPtr getXpathNodeSetRel(char *xpath, xmlNodePtr relnode, xmlXPathContextPtr ctx)
{
    xmlXPathSetContextNode(relnode, ctx);
    return getXpathNodeSet(xpath, ctx);
}
  
xmlNodeSetPtr getXpathNodeSet(char *xpath, xmlXPathContextPtr ctx){

    xmlXPathObjectPtr result;

    result = xmlXPathEvalExpression(BAD_CAST xpath, ctx);
    if (result == NULL) {
        fprintf(stderr, "Error in xmlXPathEvalExpression\n");
        return NULL;
    }
    if(xmlXPathNodeSetIsEmpty(result->nodesetval)){
        xmlXPathFreeObject(result);
        fprintf(stderr, "No result\n");
        return NULL;
    }
    xmlNodeSetPtr nodeSet = result->nodesetval;
    xmlXPathFreeNodeSetList(result); // frees the xmlXPathObjectPtr but not the list of nodes in nodesetval (nor nodes themselves) 
    return nodeSet;
}
