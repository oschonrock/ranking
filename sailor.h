#ifndef __SAILOR_H__
#define __SAILOR_H__

#include <sys/types.h>

typedef struct Sailor {
    int id;
    char *name;
    unsigned int sailno;
    char *gender;
} Sailor;

void sailorPoolInit();
Sailor *sailorPoolAdd(Sailor *sailor);
Sailor *sailorPoolFindByExampleOrNew(Sailor *ex_sailor);
Sailor *SailorPoolFindByIndex(int i);
size_t sailorPoolGetUsed();
void sailorPoolFree();

Sailor *sailorNew();
void sailorFree(Sailor *sailor);
Sailor *sailorSetName(Sailor *sailor, char *name);
Sailor *sailorSetSailno(Sailor *sailor, int sailno);
Sailor *sailorSetSailnoString(Sailor *sailor, char *sailno_str);
Sailor *sailorSetGender(Sailor *sailor, char *gender);

#endif /* __SAILOR_H__ */
