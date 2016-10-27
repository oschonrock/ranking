#ifndef __SAILOR_H__
#define __SAILOR_H__

#include <sys/types.h>

typedef struct Sailor {
  int id;
  char *name;
  unsigned int sailno;
} Sailor;

void sailorPoolInit();
Sailor *sailorPoolAdd(Sailor *sailor);
Sailor *sailorPoolFindOrNew(char *name, int sailno);
Sailor *SailorPoolFindByIndex(int i);
size_t sailorPoolGetUsed();
void sailorPoolFree();

Sailor *sailorNew();

#endif /* __SAILOR_H__ */
