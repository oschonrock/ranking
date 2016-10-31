#ifndef __SAILOR_H__
#define __SAILOR_H__

#include <sys/types.h>

typedef struct Sailor {
    int id;
    char *name;
    unsigned int sailno;
    char *gender;
    char *club;
    unsigned short int age;
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
Sailor *sailorSetSailno(Sailor *sailor, unsigned int sailno);
Sailor *sailorSetSailnoString(Sailor *sailor, char *sailno_str);
Sailor *sailorSetGender(Sailor *sailor, char *gender);
Sailor *sailorSetAge(Sailor *sailor, unsigned short int age);
Sailor *sailorSetAgeString(Sailor *sailor, char *age_str);
Sailor *sailorSetClub(Sailor *sailor, char *club);

#endif /* __SAILOR_H__ */
