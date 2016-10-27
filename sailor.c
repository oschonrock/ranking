#include <stdlib.h> // realloc
#include <stdio.h>  // fprintf
#include <pthread.h>
#include <stdbool.h>
#include <strings.h>
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
    //fprintf(stderr, "freeing %s\n", __sp.sailors[i]->name);
    free(__sp.sailors[i]->name);   // free the name
    free(__sp.sailors[i]);         // each sailor Object
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

Sailor *sailorPoolFindOrNew(char *name, int sailno)
{
  bool found = false;
  int i;
  pthread_mutex_lock(&__sp_mutex);
  for (i = 0; !found && i < __sp.used; i++)
  {
    if (strcasecmp(__sp.sailors[i]->name, name) == 0 && __sp.sailors[i]->sailno == sailno) {
      found = true;
    }
  }
  // we must still hold the lock here, bacause the state of "found" depends on the state of the __sp which might get changed
  Sailor *sailor;
  if (found) {
    sailor = __sp.sailors[i];
    // if we are not going to use the name string passed in, we must free it (memleak!)
    free(name);
  } else {
    sailor = sailorNew();
    sailor->name = name;
    sailor->sailno = sailno;
  }
  pthread_mutex_unlock(&__sp_mutex);
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
