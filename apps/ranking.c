#include "regatta.h"
#include "sailor.h"
#include <pthread.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

// purely a wrapper for pointer casting
void* loadDoc(void* regatta) {
  regattaLoadDoc((Regatta*)regatta);
  pthread_exit(NULL);
}

#define REGATTA_COUNT 3 // equals thread count

int main() {
  regattaPoolInit();

  char* urls[] = {
    // clang-format off
    "https://www.kbsuk.com/data/OptimistIOCAEvents/data/results/2018lscmainnh.html",
    "https://www.kbsuk.com/data/OptimistIOCAEvents/data/results/2018eosmainnh.html",
    "https://www.kbsuk.com/data/OptimistIOCAEvents/data/results/2018inlmainnh.html",
    // clang-format on
  };
  const int urlcnt = sizeof(urls) / sizeof(urls[0]);

  // spawn theads to retrieve docs over network and use libxml2 to parse them
  pthread_t threads[REGATTA_COUNT];
  for (int i = 0; i < REGATTA_COUNT; i++) {
    Regatta* regatta = regattaNew(i, urls[i % urlcnt]);
    int      rc = pthread_create(&threads[i], NULL, loadDoc, (void*)regatta);
    if (rc) {
      fprintf(stderr, "ERROR: pthread_create() returned %d\n", rc);
      exit(EXIT_FAILURE);
    }
  }

  // wait for threads to finish
  for (int i = 0; i < REGATTA_COUNT; i++) {
    void* status;
    int   rc = pthread_join(threads[i], &status);
    if (rc) {
      fprintf(stderr, "ERROR: pthread_join() returned %d\n", rc);
      exit(EXIT_FAILURE);
    }
  }

  // process data: single threaded because the business logic is order dependent
  for (int i = 0; i < REGATTA_COUNT; i++)
    regattaLoad(regattaPoolFindByIndex(i));

  // and display it
  fprintf(stderr, "%zu Sailors\n", sailorPoolGetUsed());
  for (size_t i = 0; i < sailorPoolGetUsed(); i++) {
    Sailor* sailor = sailorPoolGet(i);
    fprintf(stdout, "#%-3d %5i %-30s %-1s %3d %-30.30s\n", sailor->id,
            sailor->sailno, sailor->name, sailor->gender, sailor->age,
            sailor->club);
  }

  sailorPoolFree();
  regattaPoolFree();
  return EXIT_SUCCESS;
}
