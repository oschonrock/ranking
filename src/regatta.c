#include "regatta.h"
#include "curl.h"
#include "easylib.h"
#include "sailor.h"
#include <curl/curl.h>
#include <libgen.h> // basename
#include <libxml/HTMLparser.h>
#include <libxml/xpath.h>
#include <pthread.h>
#include <string.h>
#include <sys/param.h> // MAXPATHLEN

typedef struct RegattaPool {
  Regatta** regattas;
  size_t    used;
  size_t    size;
} RegattaPool;

// just a single private instance of the pool, not going to pass it around
// NULL pointer for __rp.regattas means that first call to regattaPoolAdd will
// realloc from NULL (equiv to malloc)
RegattaPool __rp = {0};

// make it thread-safe
pthread_mutex_t     __rp_mutex;
pthread_mutexattr_t __rp_mutex_attr;

xmlDocPtr     getDoc(char* docname);
xmlNodeSetPtr getXpathNodeSet(char* xpath, xmlXPathContextPtr ctx);
xmlNodeSetPtr getXpathNodeSetRel(char* xpath, xmlNodePtr relnode,
                                 xmlXPathContextPtr ctx);
void          regattaFree(Regatta* regatta);

void regattaPoolInit() {
  xmlInitParser();
  LIBXML_TEST_VERSION; // don't need this ';' but without it the indentation
                       // gets messed up

  /* Must initialize libcurl before any threads are started */
  curl_global_init(CURL_GLOBAL_ALL);
  curl_ssl_init_locks();

  // in case we make nested or recursive calls which both lock, we use RECURSIVE
  // type, which counts locks and unlocks
  pthread_mutexattr_init(&__rp_mutex_attr);
  pthread_mutexattr_settype(&__rp_mutex_attr, PTHREAD_MUTEX_RECURSIVE);
  pthread_mutex_init(&__rp_mutex, &__rp_mutex_attr);
}

void regattaPoolFree() {
  pthread_mutex_lock(&__rp_mutex);
  for (size_t i = 0; i < __rp.used; i++) {
    regattaFree(__rp.regattas[i]); // each regatta Object
  }
  free(__rp.regattas); // and the array of pointers to those objects
  __rp = (RegattaPool){0};
  pthread_mutex_unlock(&__rp_mutex);
  pthread_mutex_destroy(&__rp_mutex);
  pthread_mutexattr_destroy(&__rp_mutex_attr);

  curl_ssl_kill_locks();
  curl_global_cleanup();
  xmlCleanupParser();
}

// http://stackoverflow.com/questions/3536153/c-dynamically-growing-array
Regatta* regattaPoolAdd(Regatta* regatta) {
  pthread_mutex_lock(&__rp_mutex);
  if (__rp.used == __rp.size) {
    // grow the array allocation
    __rp.size = 3 * __rp.size / 2 + 8;

    size_t    req_bytes  = __rp.size * sizeof *regatta;
    Regatta** t_regattas = realloc(__rp.regattas, req_bytes);
    if (!t_regattas) {
      fprintf(stdout, "realloc failed allocate to bytes = %zu\n", req_bytes);
      free(__rp.regattas);
      exit(-1);
    }
    __rp.regattas = t_regattas;
  }
  __rp.regattas[__rp.used++] = regatta;
  pthread_mutex_unlock(&__rp_mutex);
  return regatta;
}

Regatta* regattaNew() {
  Regatta* regatta = calloc(1, sizeof *regatta);
  regattaPoolAdd(regatta); // all the regattas must be in the pool
  return regatta;
}

void regattaFree(Regatta* regatta) {
  // this causes probs because the urls are often in the source code of the
  // caller and hence not malloc'd and read only free(regatta->url);     // char
  // * for the url
  free(regatta); // and the struct which contains the 2D array of pointer to the
                 // result xmlChar's
}

#define RESULT_ROW_MAX_FIELDS 25

typedef enum {
  NAF  = -1, // not a field, used in std and cust
  HELM = 0,  // must start at zero
  SAILNO,    // and then 1,2,3
  RANK,
  GENDER,
  AGE,
  CLUB,
} StdField;

typedef struct FieldMapItem {
  StdField std;
  int      cust;
  char*    pattern;
  Sailor* (*setter)(Sailor*, char*);
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
  FieldMapItem items[RESULT_ROW_MAX_FIELDS];
} FieldMap;

typedef char* ResultRow[RESULT_ROW_MAX_FIELDS];

FieldMap* regattaNewFieldMap() {
  FieldMap* fm = malloc(sizeof *fm);
  for (int std = 0; std < RESULT_ROW_MAX_FIELDS; std++)
    fm->items[std] = (FieldMapItem){std, NAF, NULL, NULL};
  return fm;
}

FieldMap* regattaMakeFieldMap(xmlNodeSetPtr header_cells) {
  FieldMap* fm = regattaNewFieldMap();
  char*     val;
  int       c, p;
  for (c = 0; c < header_cells->nodeNr; c++) {
    val = (char*)xmlNodeGetContent(header_cells->nodeTab[c]);
    for (p = 0; pattern_fmis[p].std != NAF; p++) {
      char* trimmed_val = malloc(strlen(val) + 1);
      remove_spaces(trimmed_val, val);
      bool matched = preg_match(pattern_fmis[p].pattern, trimmed_val, true);
      free(trimmed_val);
      if (matched) {
        fm->items[pattern_fmis[p].std] =
            pattern_fmis[p]; // copy FieldMapItem (shallow copy, pattern char
                             // prt only)
        fm->items[pattern_fmis[p].std].cust = c; // record matching mapping
        break;
      }
    }
    free(val);
  }
  return fm;
}

Sailor* regattaBuildSailorFromMappedRow(ResultRow row, FieldMap* fm) {
  // new but not into pool  (should be separate function?)
  Sailor* sailor = sailorNewNoPool();

  for (int std = 0; std < RESULT_ROW_MAX_FIELDS && fm->items[std].cust != NAF;
       std++) {
    // use the function pointer "setter" to update the sailor with the mapped
    // row value
    (fm->items[std].setter)(sailor, row[fm->items[std].cust]);
  }
  return sailor;
}

void regattaLoad(Regatta* regatta) {
  xmlDocPtr          doc = getDoc(regatta->url);
  xmlXPathContextPtr ctx = xmlXPathNewContext(doc);

  int           row, col;
  ResultRow     row_vals = {0};
  xmlNodeSetPtr tables   = getXpathNodeSet("//table[@border=1]", ctx);
  xmlNodeSetPtr rows, cells;
  if (tables->nodeNr == 1) {
    rows = getXpathNodeSetRel(".//tr", tables->nodeTab[0],
                              ctx); // rows of the current table
    xmlNodeSetPtr header_cells =
        getXpathNodeSetRel(".//td", rows->nodeTab[0], ctx);
    FieldMap* fm = regattaMakeFieldMap(header_cells);

    xmlXPathFreeNodeSet(header_cells);

    // iterate over main table, with our field_map
    // r = 1, because we want to skip first row, as headers
    for (row = 1; row < rows->nodeNr; row++) {
      // cells of the current row
      cells = getXpathNodeSetRel(".//td", rows->nodeTab[row], ctx);
      for (col = 0; col < cells->nodeNr && col < RESULT_ROW_MAX_FIELDS; col++) {
        // we opt to build a whole row, as there may be some cases where cross
        // cell validation / fixing occurs
        row_vals[col] = (char*)xmlNodeGetContent(cells->nodeTab[col]);
      }

      Sailor* sailor_ex = regattaBuildSailorFromMappedRow(row_vals, fm);
      // sailor_ex free'd inside call, or added to pool.
      sailorPoolFindByExampleOrNew(sailor_ex);

      // cleanup strings created
      for (col = 0; col < cells->nodeNr && col < RESULT_ROW_MAX_FIELDS; col++)
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

  Buffer buffer =
      (Buffer){0}; // .mem NULL prt to memory buffer will be set by realloc

  // can't pass url to libxml->htmlParseFile, appears it's not thread safe, need
  // to use curl
  // TODO: there might be an option to parse the stream, rather than wait for
  // the full html: htmlReadIO?
  if (curl_load_url(url, &buffer)) {
    fprintf(stderr, "Document not loaded successfully. \n");
    return NULL;
  }
  char* mutable_url = strdup(url);
  char* base =
      strdup(dirname(mutable_url)); // take a copy as it is not guaranted to
                                    // persist or might be part of url
  free(mutable_url);
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

  xmlXPathObjectPtr result;

  result = xmlXPathEvalExpression(BAD_CAST xpath, ctx);
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
  xmlXPathFreeNodeSetList(
      result); // frees the xmlXPathObjectPtr but not the list of nodes in
               // nodesetval (nor nodes themselves)
  return nodeSet;
}
