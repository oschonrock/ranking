#ifndef __REGATTA_H__
#define __REGATTA_H__

#include <libxml/HTMLparser.h>
#include <libxml/xpath.h>

typedef struct Regatta {
  int       id;
  char*     url;
  xmlDocPtr doc;
} Regatta;

void     regattaPoolInit(void);
Regatta* regattaPoolAdd(Regatta* regatta);
Regatta* regattaNew(int id, char* url);
Regatta* regattaPoolFindByIndex(int i);
void     regattaPoolFree(void);

void regattaAdd(int id, char* url);
void regattaLoad(Regatta* regatta);
void regattaLoadDoc(Regatta* regatta);

#endif /* __REGATTA_H__ */
