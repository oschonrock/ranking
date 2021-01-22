/** 
 */

#include <string.h>
#include <pthread.h>
#include "regatta.h"
#include "sailor.h"

void *loadResults(void *threadarg)
{
    Regatta *regatta = threadarg;
    regattaLoad(regatta);
    pthread_exit(NULL);
}

#define REGATTA_COUNT 3
#define URL_COUNT 3

int main(int argc, char **argv) {
     
    regattaPoolInit();
    sailorPoolInit();

    char *urls[URL_COUNT] = {
        "http://www.kbsuk.com/data/OptimistIOCAEvents/data/results/2016inlmainnh.html",
        "http://www.kbsuk.com/data/OptimistIOCAEvents/data/results/2016lscnh.html",
        "http://www.kbsuk.com/data/OptimistIOCAEvents/data/results/2016eosmainnh.html",
    };
  
    pthread_t threads[REGATTA_COUNT];
    int rc, t;
    for (t = 0; t < REGATTA_COUNT; t++)
    {
        Regatta *regatta = regattaNew();
        *regatta = (Regatta) { .id = t, .url = urls[t % URL_COUNT] };
        rc = pthread_create(&threads[t], NULL, loadResults, (void *) regatta);
        if (rc) {
            fprintf(stderr, "ERROR; return code from pthread_create() is %d\n", rc);
            exit(-1);
        }
    }
  
    // wait for threads
    void *status;
    for(t = 0; t < REGATTA_COUNT; t++) {
        rc = pthread_join(threads[t], &status);
        if (rc) {
            fprintf(stderr, "ERROR; return code from pthread_join() is %d\n", rc);
            exit(-1);
        }
    }
        
    fprintf(stderr, "%zu Sailors\n", sailorPoolGetUsed());
    for (int s = 0; s < sailorPoolGetUsed(); s++) {
        Sailor *sailor = SailorPoolFindByIndex(s);
        fprintf(stdout, "#%-3d %5i %-30.30s %-1s %3d %-30.30s\n", sailor->id, sailor->sailno, sailor->name, sailor->gender, sailor->age, sailor->club);
    }

    sailorPoolFree();
    regattaPoolFree();
    pthread_exit(NULL);
    return 0;
}
