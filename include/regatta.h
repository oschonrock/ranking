#ifndef __REGATTA_H__
#define __REGATTA_H__

#include <libxml/HTMLparser.h>
#include <libxml/xpath.h>

typedef struct Regatta {
  int   id;
  char* url;
} Regatta;

typedef struct RegattaPool {
  Regatta** regattas;
  size_t    used;
  size_t    size;
} RegattaPool;

void     regattaPoolInit();
Regatta* regattaPoolAdd(Regatta* regatta);
Regatta* regattaNew();
void     regattaPoolFree();

void regattaAdd(int id, char* url);
void regattaLoad(Regatta* regatta);

#endif /* __REGATTA_H__ */
