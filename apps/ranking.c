/**
 */

#include "regatta.h"
#include "sailor.h"
#include <pthread.h>
#include <stddef.h>
#include <string.h>

void* loadResults(void* threadarg) {
  Regatta* regatta = threadarg;
  regattaLoad(regatta);
  pthread_exit(NULL);
}

#define REGATTA_COUNT 3

int main(/*int argc, char **argv*/) {

  regattaPoolInit();
  sailorPoolInit();

  char* urls[] = {
      // clang-format off
    "https://www.kbsuk.com/data/OptimistIOCAEvents/data/results/2018lscmainnh.html",
    "https://www.kbsuk.com/data/OptimistIOCAEvents/data/results/2018eosmainnh.html",
    "https://www.kbsuk.com/data/OptimistIOCAEvents/data/results/2018inlmainnh.html",
      // clang-format on
  };
  const int urlcnt = sizeof(urls) / sizeof(urls[0]);

  pthread_t threads[REGATTA_COUNT];
  int       rc, t;
  for (t = 0; t < REGATTA_COUNT; t++) {
    Regatta* regatta = regattaNew();
    *regatta         = (Regatta){.id = t, .url = urls[t % urlcnt]};
    rc = pthread_create(&threads[t], NULL, loadResults, (void*)regatta);
    if (rc) {
      fprintf(stderr, "ERROR; return code from pthread_create() is %d\n", rc);
      exit(-1);
    }
  }

  // wait for threads
  void* status;
  for (t = 0; t < REGATTA_COUNT; t++) {
    rc = pthread_join(threads[t], &status);
    if (rc) {
      fprintf(stderr, "ERROR; return code from pthread_join() is %d\n", rc);
      exit(-1);
    }
  }

  fprintf(stderr, "%zu Sailors\n", sailorPoolGetUsed());
  for (size_t s = 0; s < sailorPoolGetUsed(); s++) {
    Sailor* sailor = SailorPoolFindByIndex(s);
    fprintf(stdout, "#%-3d %5i %-30.30s %-1s %3d %-30.30s\n", sailor->id,
            sailor->sailno, sailor->name, sailor->gender, sailor->age,
            sailor->club);
  }

  sailorPoolFree();
  regattaPoolFree();
  pthread_exit(NULL);
  return 0;
}
