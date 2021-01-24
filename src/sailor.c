#include "sailor.h"
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>  // fprintf
#include <stdlib.h> // realloc
#include <string.h> // strdup
#include <strings.h>

typedef struct SailorPool {
  Sailor** sailors;
  size_t   count;
  size_t   size;
} SailorPool;

// just a single private instance of the pool
static SailorPool          pool;
static pthread_mutex_t     mut;
static pthread_mutexattr_t mut_attr;

void sailorPoolInit(void) {
  // setup the __sp struct
  pool = (SailorPool){0};
  // recursive mutex for  nested or recursive calls 
  pthread_mutexattr_init(&mut_attr);
  pthread_mutexattr_settype(&mut_attr, PTHREAD_MUTEX_RECURSIVE);
  pthread_mutex_init(&mut, &mut_attr);
}

void sailorPoolFree(void) {
  pthread_mutex_lock(&mut);
  for (size_t i = 0; i < pool.count; i++) {
    sailorFree(pool.sailors[i]); // each sailor Object
  }
  free(pool.sailors);     // and the array of pointers to those objects
  pool = (SailorPool){0}; // and reset the whole pool
  pthread_mutex_unlock(&mut);
  pthread_mutex_destroy(&mut);
  pthread_mutexattr_destroy(&mut_attr);
}

size_t sailorPoolGetUsed(void) { return pool.count; }

Sailor* sailorNewNoPool(void) {
  Sailor* sailor = calloc(1, sizeof *sailor);
  return sailor;
}

Sailor* sailorNew(void) {
  Sailor* sailor = sailorNewNoPool();
  sailorPoolAdd(sailor);
  return sailor;
}

void sailorFree(Sailor* sailor) {
  free(sailor->name);
  free(sailor->gender);
  free(sailor->club);
  free(sailor);
}

Sailor* sailorPoolFindByExampleOrNew(Sailor* ex_sailor) {
  bool   found = false;
  size_t i;
  pthread_mutex_lock(&mut);
  for (i = 0; !found && i < pool.count; i++) {
    if (ex_sailor->name &&
        strcasecmp(pool.sailors[i]->name, ex_sailor->name) == 0 &&
        ex_sailor->sailno && pool.sailors[i]->sailno == ex_sailor->sailno) {
      found = true;
    }
  }
  // we must still hold the lock here, bacause the state of "found" depends on
  // the state of the __sp which might get changed
  Sailor* sailor;
  if (found) {
    sailor = pool.sailors[i];
    // TODO update details
    sailorFree(ex_sailor);
  } else {
    sailor = ex_sailor;
    sailorPoolAdd(sailor);
  }
  pthread_mutex_unlock(&mut);
  return sailor;
}

// need all the below setters, because used as function pointers
// in the field map
Sailor* sailorSetName(Sailor* sailor, char* name) {
  sailor->name = strdup(name);
  return sailor;
}

Sailor* sailorSetSailno(Sailor* sailor, unsigned int sailno) {
  sailor->sailno = sailno;
  return sailor;
}

Sailor* sailorSetSailnoString(Sailor* sailor, char* sailno_str) {
  return sailorSetSailno(sailor, atoi(sailno_str));
}

Sailor* sailorSetRank(Sailor* sailor, unsigned int rank) {
  sailor->rank = rank;
  return sailor;
}

Sailor* sailorSetRankString(Sailor* sailor, char* rank_str) {
  return sailorSetRank(sailor, atoi(rank_str));
}

Sailor* sailorSetGender(Sailor* sailor, char* gender) {
  sailor->gender = strndup(gender, 1); // just first letter
  return sailor;
}

Sailor* sailorSetAge(Sailor* sailor, unsigned short int age) {
  sailor->age = age;
  return sailor;
}

Sailor* sailorSetAgeString(Sailor* sailor, char* age_str) {
  return sailorSetAge(sailor, atoi(age_str));
}

Sailor* sailorSetClub(Sailor* sailor, char* club) {
  sailor->club = strdup(club);
  return sailor;
}

Sailor* sailorPoolAdd(Sailor* sailor) {
  pthread_mutex_lock(&mut);
  if (pool.count == pool.size) {
    // grow the array allocation
    pool.size = 3 * pool.size / 2 + 8;

    Sailor** t_sailors = realloc(pool.sailors, pool.size * sizeof *sailor);
    if (!t_sailors) {
      fprintf(stderr, "realloc failed to allocate bytes = %zu\n", pool.size);
      free(pool.sailors);
      exit(EXIT_FAILURE);
    }
    pool.sailors = t_sailors;
  }
  sailor->id               = pool.count + 1; // not zero based for this
  pool.sailors[pool.count] = sailor;
  pool.count++;
  pthread_mutex_unlock(&mut);
  return sailor;
}

Sailor* SailorPoolFindByIndex(int i) { return pool.sailors[i]; }
