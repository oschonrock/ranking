#include "regatta.h"
#include "sailor.h"
#include <pthread.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

void* loadResults(void* regatta) {
  regattaLoad((Regatta*)regatta);
  pthread_exit(NULL);
}

#define REGATTA_COUNT 3 // equals thread count

int main() {

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
  for (int t = 0; t < REGATTA_COUNT; t++) {
    Regatta* regatta = regattaNew(t, urls[t % urlcnt]);
    int rc = pthread_create(&threads[t], NULL, loadResults, (void*)regatta);
    if (rc) {
      fprintf(stderr, "ERROR; return code from pthread_create() is %d\n", rc);
      exit(EXIT_FAILURE);
    }
  }

  // wait for threads
  for (int t = 0; t < REGATTA_COUNT; t++) {
    void* status;
    int rc = pthread_join(threads[t], &status);
    if (rc) {
      fprintf(stderr, "ERROR; return code from pthread_join() is %d\n", rc);
      exit(EXIT_FAILURE);
    }
  }

  fprintf(stderr, "%zu Sailors\n", sailorPoolGetUsed());
  for (size_t s = 0; s < sailorPoolGetUsed(); s++) {
    Sailor* sailor = SailorPoolFindByIndex(s);
    fprintf(stdout, "#%-3d %5i %-30s %-1s %3d %-30.30s\n", sailor->id,
            sailor->sailno, sailor->name, sailor->gender, sailor->age,
            sailor->club);
  }

  sailorPoolFree();
  regattaPoolFree();
  // don't call pthread_exit(NULL) here because we have
  // specifically pthread_join'd all threads already
  return EXIT_SUCCESS;
}
