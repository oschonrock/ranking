#include "regatta.h"
#include "curl.h"
#include "easylib.h"
#include "sailor.h"
#include <curl/curl.h>
#include <libgen.h> // basename
#include <libxml/HTMLparser.h>
#include <libxml/xpath.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

typedef struct RegattaPool {
  Regatta** regattas;
  size_t    count;
  size_t    size;
} RegattaPool;

// just a single private instance of the pool
static RegattaPool         pool = {0};
static pthread_mutex_t     mut;
static pthread_mutexattr_t mut_attr;

xmlDocPtr     getDoc(char* url);
xmlNodeSetPtr getXpathNodeSet(char* xpath, xmlXPathContextPtr ctx);
xmlNodeSetPtr getXpathNodeSetRel(char* xpath, xmlNodePtr relnode,
                                 xmlXPathContextPtr ctx);
void          regattaFree(Regatta* regatta);

void regattaPoolInit() {
  xmlInitParser();
  LIBXML_TEST_VERSION; // `;` for indentation only

  // Must initialize libcurl before any threads are started
  curl_global_init(CURL_GLOBAL_ALL);

  // recursive mutex for nested or recursive calls
  pthread_mutexattr_init(&mut_attr);
  pthread_mutexattr_settype(&mut_attr, PTHREAD_MUTEX_RECURSIVE);
  pthread_mutex_init(&mut, &mut_attr);
}

void regattaPoolFree() {
  pthread_mutex_lock(&mut);
  for (size_t i = 0; i < pool.count; i++) {
    regattaFree(pool.regattas[i]); // each regatta Object
  }
  free(pool.regattas); // and the array of pointers to those objects
  pool = (RegattaPool){0};
  pthread_mutex_unlock(&mut);
  pthread_mutex_destroy(&mut);
  pthread_mutexattr_destroy(&mut_attr);

  curl_global_cleanup();
  xmlCleanupParser();
}

// http://stackoverflow.com/questions/3536153/c-dynamically-growing-array
Regatta* regattaPoolAdd(Regatta* regatta) {
  pthread_mutex_lock(&mut);
  if (pool.count == pool.size) {
    // grow the array allocation
    pool.size = 3 * pool.size / 2 + 8;

    size_t    req_bytes  = pool.size * sizeof *regatta;
    Regatta** t_regattas = realloc(pool.regattas, req_bytes);
    if (!t_regattas) {
      fprintf(stdout, "realloc failed allocate to bytes = %zu\n", req_bytes);
      free(pool.regattas);
      exit(EXIT_FAILURE);
    }
    pool.regattas = t_regattas;
  }
  pool.regattas[pool.count++] = regatta;
  pthread_mutex_unlock(&mut);
  return regatta;
}

Regatta* regattaNew(int id, char* url) {
  Regatta* regatta = calloc(1, sizeof *regatta);
  regatta->id = id;
  regatta->url = strdup(url); // take copy, needed later
  regattaPoolAdd(regatta); // all the regattas must be in the pool
  return regatta;
}

void regattaFree(Regatta* regatta) {
  if (regatta) free(regatta->url);
  free(regatta); // struct contains the 2D array of pointers to the xmlChar's
}

#define MAX_FIELDS 25

typedef enum {
  NAF  = -1, // not a field, used in std and cust
  HELM = 0,  // must start at zero
  SAILNO,
  RANK,
  GENDER,
  AGE,
  CLUB,
} StdField;

typedef Sailor* (*setter_t)(Sailor*, char*);

typedef struct FieldMapItem {
  StdField std;
  int      cust;
  char*    pattern;
  setter_t setter;
} FieldMapItem;

const FieldMapItem pattern_fmis[] = {
    // clang-format off
    // std enum      cust   pattern             &setter_func_ptr
    {  HELM,         NAF,   "Helm",             &sailorSetName},
    {  SAILNO,       NAF,   "Sailno",           &sailorSetSailnoString},
    {  RANK,         NAF,   "rank|seriesplace", &sailorSetRankString},
    {  GENDER,       NAF,   "M/F",              &sailorSetGender},
    {  AGE,          NAF,   "Age",              &sailorSetAgeString},
    {  CLUB,         NAF,   "Club",             &sailorSetClub},
    {  NAF,          NAF,   "",                 NULL}, // End of list terminator
    // clang-format on
};

typedef struct FieldMap {
  FieldMapItem items[MAX_FIELDS];
} FieldMap;

typedef char* ResultRow[MAX_FIELDS];

FieldMap* regattaNewFieldMap() {
  FieldMap* fm = malloc(sizeof *fm);
  for (int std = 0; std < MAX_FIELDS; std++)
    fm->items[std] = (FieldMapItem){std, NAF, NULL, NULL};
  return fm;
}

FieldMap* regattaMakeFieldMap(xmlNodeSetPtr header_cells) {
  FieldMap* fm = regattaNewFieldMap();
  char*     val;
  for (int cell = 0; cell < header_cells->nodeNr; cell++) {
    val = (char*)xmlNodeGetContent(header_cells->nodeTab[cell]);
    for (int p = 0; pattern_fmis[p].std != NAF; p++) {
      char* trimmed_val = malloc(strlen(val) + 1);
      remove_spaces(trimmed_val, val);
      bool matched = preg_match(pattern_fmis[p].pattern, trimmed_val, true);
      free(trimmed_val);
      if (matched) {
        fm->items[pattern_fmis[p].std] =
            pattern_fmis[p]; // copy FieldMapItem (shallow copy)
        fm->items[pattern_fmis[p].std].cust = cell; // record matching mapping
        break;
      }
    }
    free(val);
  }
  return fm;
}

Sailor* regattaBuildSailorFromMappedRow(ResultRow row, FieldMap* fm) {
  Sailor* sailor = sailorNewNoPool();

  for (int std = 0; std < MAX_FIELDS && fm->items[std].cust != NAF; std++) {
    // use func_ptr "setter" to update the sailor with the mapped values
    (fm->items[std].setter)(sailor, row[fm->items[std].cust]);
  }
  return sailor;
}

void regattaLoad(Regatta* regatta) {
  xmlDocPtr          doc = getDoc(regatta->url);
  xmlXPathContextPtr ctx = xmlXPathNewContext(doc);

  xmlNodeSetPtr tables   = getXpathNodeSet("//table[@border=1]", ctx);
  if (tables->nodeNr == 1) {
    xmlNodeSetPtr rows = getXpathNodeSetRel(".//tr", tables->nodeTab[0],
                              ctx); // rows of the current table
    xmlNodeSetPtr header_cells =
        getXpathNodeSetRel(".//td", rows->nodeTab[0], ctx);
    FieldMap* fm = regattaMakeFieldMap(header_cells);

    xmlXPathFreeNodeSet(header_cells);

    // row = 1, to skip first row, as headers
    for (int row = 1; row < rows->nodeNr; row++) {
      // cells of the current row
      xmlNodeSetPtr cells = getXpathNodeSetRel(".//td", rows->nodeTab[row], ctx);
      ResultRow     row_vals = {0};
      for (int col = 0; col < cells->nodeNr && col < MAX_FIELDS; col++) {
        // build a whole row. Sometimes cross cell validation / fixing occurs
        row_vals[col] = (char*)xmlNodeGetContent(cells->nodeTab[col]);
      }

      Sailor* sailor_ex = regattaBuildSailorFromMappedRow(row_vals, fm);
      // sailor_ex free'd inside call, or added to pool.
      sailorPoolFindByExampleOrNew(sailor_ex);

      // cleanup strings created
      for (int col = 0; col < cells->nodeNr && col < MAX_FIELDS; col++)
        free(row_vals[col]);

      xmlXPathFreeNodeSet(cells);
    }
    xmlXPathFreeNodeSet(rows);
    free(fm);
  } // if not 1 table, then TODO: skipped for now

  xmlXPathFreeNodeSet(tables);
  xmlXPathFreeContext(ctx);
  xmlFreeDoc(doc);
}

xmlDocPtr getDoc(char* url) {

  Buffer buffer = (Buffer){0}; // ptr set by realloc

  // Can't just call libxml->htmlParseFile, not thread safe. Use curl
  if (curl_load_url(url, &buffer)) {
    fprintf(stderr, "Document not loaded successfully. \n");
    return NULL;
  }
  // htmlReadMemory needs this for relative urls within the html document
  // take copy, not guaranted to persist
  char* base = strdup(dirname(url));
  xmlDocPtr doc = htmlReadMemory(buffer.mem, buffer.size, base, NULL,
                                 HTML_PARSE_NONET | HTML_PARSE_NOERROR |
                                     HTML_PARSE_NOWARNING |
                                     HTML_PARSE_RECOVER); // for dirty html(5)
  free(base);
  free(buffer.mem);

  if (doc == NULL) {
    fprintf(stderr, "Document not parsed successfully. \n");
    return NULL;
  }
  return doc;
}

xmlNodeSetPtr getXpathNodeSetRel(char* xpath, xmlNodePtr relnode,
                                 xmlXPathContextPtr ctx) {
  xmlXPathSetContextNode(relnode, ctx);
  return getXpathNodeSet(xpath, ctx);
}

xmlNodeSetPtr getXpathNodeSet(char* xpath, xmlXPathContextPtr ctx) {
  xmlXPathObjectPtr result = xmlXPathEvalExpression(BAD_CAST xpath, ctx);
  if (result == NULL) {
    fprintf(stderr, "Error in xmlXPathEvalExpression\n");
    return NULL;
  }
  if (xmlXPathNodeSetIsEmpty(result->nodesetval)) {
    xmlXPathFreeObject(result);
    fprintf(stderr, "No result\n");
    return NULL;
  }
  xmlNodeSetPtr nodeSet = result->nodesetval;
  // frees the xmlXPathObjectPtr but not the list of nodes in
  // nodesetval (nor nodes themselves)
  xmlXPathFreeNodeSetList(result);
  return nodeSet;
}
