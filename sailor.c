#include <stdlib.h> // realloc
#include <stdio.h>  // fprintf
#include <pthread.h>
#include <stdbool.h>
#include <strings.h>
#include <string.h>  // strdup
#include "sailor.h"

typedef struct SailorPool {
    Sailor **sailors;
    size_t used;
    size_t size;
} SailorPool;

// just a single private instance of the pool, not going to pass it around
// no external code should acces this, because it involves a bunch of locking to make it thread safe
SailorPool __sp;

// make it thread-safe
pthread_mutex_t __sp_mutex;
pthread_mutexattr_t __sp_mutex_attr;

void sailorPoolInit() {
    // setup the __sp struct
    __sp = (SailorPool) {0};
    // in case we make nested or recursive calls which both lock, we use RECURSIVE type, which counts locks and unlocks
    pthread_mutexattr_init(&__sp_mutex_attr);
    pthread_mutexattr_settype(&__sp_mutex_attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&__sp_mutex, &__sp_mutex_attr);
}

void sailorPoolFree()
{
    pthread_mutex_lock(&__sp_mutex);
    for (int i=0; i<__sp.used; i++)
    {
        sailorFree(__sp.sailors[i]);   // each sailor Object
    }
    free(__sp.sailors);          // and the array of pointers to those objects
    __sp = (SailorPool) {0};     // and reset the whole pool
    pthread_mutex_unlock(&__sp_mutex);
    pthread_mutex_destroy(&__sp_mutex);
    pthread_mutexattr_destroy(&__sp_mutex_attr);
}

size_t sailorPoolGetUsed()
{
    return __sp.used;
}

Sailor *sailorNew()
{
    Sailor *sailor= malloc(sizeof(Sailor));
    *sailor= (Sailor) {0};
    sailorPoolAdd(sailor);
    return sailor;
}

void sailorFree(Sailor *sailor)
{
    if (sailor->name) {
        free(sailor->name);      // free the name
    }
    if (sailor->gender) {
        free(sailor->gender);   // free the gender
    }
    if (sailor->club) {
        free(sailor->club);   // free the club
    }
    free(sailor);
}

Sailor *sailorPoolFindByExampleOrNew(Sailor *ex_sailor)
{
    bool found = false;
    int i;
    pthread_mutex_lock(&__sp_mutex);
    for (i = 0; !found && i < __sp.used; i++)
    {
        if (ex_sailor->name && strcasecmp(__sp.sailors[i]->name, ex_sailor->name) == 0 &&
            ex_sailor->sailno && __sp.sailors[i]->sailno == ex_sailor->sailno) {
            found = true;
        }
    }
    // we must still hold the lock here, bacause the state of "found" depends on the state of the __sp which might get changed
    Sailor *sailor;
    if (found) {
        sailor = __sp.sailors[i];
        // TODO update details
        sailorFree(ex_sailor);
    } else {
        sailor = ex_sailor;
        sailorPoolAdd(sailor);
    }
    pthread_mutex_unlock(&__sp_mutex);
    return sailor;
}

Sailor *sailorSetName(Sailor *sailor, char *name)
{
    sailor->name = strdup(name);
    return sailor;
}

Sailor *sailorSetSailno(Sailor *sailor, unsigned int sailno)
{
    sailor->sailno = sailno;
    return sailor;
}

Sailor *sailorSetSailnoString(Sailor *sailor, char *sailno_str)
{
    return sailorSetSailno(sailor, atoi(sailno_str));
}

Sailor *sailorSetGender(Sailor *sailor, char *gender)
{
    sailor->gender = strndup(gender, 1); // just first letter
    return sailor;
}

Sailor *sailorSetAge(Sailor *sailor, unsigned short int age)
{
    sailor->age = age;
    return sailor;
}

Sailor *sailorSetAgeString(Sailor *sailor, char *age_str)
{
    return sailorSetAge(sailor, atoi(age_str));
}

Sailor *sailorSetClub(Sailor *sailor, char *club)
{
    sailor->club = strdup(club);
    return sailor;
}

Sailor *sailorPoolAdd(Sailor *sailor)
{
    pthread_mutex_lock(&__sp_mutex);
    if (__sp.used == __sp.size) {
        // grow the array allocation
        __sp.size = 3 * __sp.size / 2 + 8;
                
        Sailor **t_sailors = realloc(__sp.sailors, __sp.size * sizeof(Sailor));
        if (!t_sailors) {
            fprintf(stderr, "realloc failed to allocate bytes = %zu\n", __sp.size);
            free(__sp.sailors);
            exit(-1);
        }
        __sp.sailors = t_sailors;
    }
    __sp.sailors[__sp.used++] = sailor;
    pthread_mutex_unlock(&__sp_mutex);
    return sailor;
}

Sailor *SailorPoolFindByIndex(int i)
{
    return __sp.sailors[i];
}
