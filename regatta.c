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
  
// just a single public instance of the pool, not going to pass it around
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

  // just a single public instance of the pool, not going to pass it around
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

void regattaLoad(Regatta *regatta)
{
  // fprintf(stdout, "Starting: %s\n", regatta->url);

  // can't pass url to libxml->getDoc, not thread safe, need to use curl
  xmlDocPtr doc = getDoc(regatta->url);
  xmlXPathContextPtr ctx = xmlXPathNewContext(doc); 

  int r, c;
  char *row_vals[25] = {0};
  xmlNodeSetPtr tables = getXpathNodeSet("//table[@border=1]", ctx);
  xmlNodeSetPtr rows, cells;
  if (tables->nodeNr == 1)
  {
    rows = getXpathNodeSetRel(".//tr", tables->nodeTab[0], ctx);   // rows of the current table
    for(r = 1; r < rows->nodeNr; r++) {                            // skip first row, as headers
      cells = getXpathNodeSetRel(".//td", rows->nodeTab[r], ctx);  // cells of the current row
      for(c = 0; c < cells->nodeNr; c++) {
        row_vals[c] = (char *)xmlNodeGetContent(cells->nodeTab[c]);
      }

      // future mapping goes here
      sailorPoolFindOrNew(strdup((char *)row_vals[3]), atoi(row_vals[2]));
      
      // cleanup strings created
      for(c = 0; c < cells->nodeNr; c++) free(row_vals[c]);
      xmlXPathFreeNodeSet(cells);
    }
    xmlXPathFreeNodeSet(rows);
  }

  xmlXPathFreeNodeSet(tables);
  xmlXPathFreeContext(ctx); 
  xmlFreeDoc(doc);
  // fprintf(stdout, "Done: %s\n", regatta->url);
}

xmlDocPtr getDoc(char *url) {

  Buffer buffer = (Buffer) {0}; // .mem NULL prt to memory buffer will be set by realloc
  
  if (curl_load_url(url, &buffer)) {
    fprintf(stderr,"Document not loaded successfully. \n");
    return NULL;
  }
  char *base = malloc(MAXPATHLEN);
  base = strdup(dirname(url));  // take a copy as it is not guaranted to persist
  xmlDocPtr doc = htmlReadMemory(buffer.mem, buffer.size, base, NULL, 0);
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
