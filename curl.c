// directly from: https://curl.haxx.se/libcurl/c/threaded-ssl.html

#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <curl/curl.h>
#include <openssl/crypto.h>
#include "curl.h"

/* we have this global to let the callback get easy access to it */
static pthread_mutex_t *lockarray;

static void lock_callback(int mode, int type, char *file, int line)
{
  (void)file;
  (void)line;
  if(mode & CRYPTO_LOCK) {
    pthread_mutex_lock(&(lockarray[type]));
  }
  else {
    pthread_mutex_unlock(&(lockarray[type]));
  }
}

static unsigned long thread_id(void)
{
  unsigned long ret;

  ret=(unsigned long)pthread_self();
  return ret;
}

void curl_init_locks(void)
{
  int i;

  lockarray=(pthread_mutex_t *)OPENSSL_malloc(CRYPTO_num_locks() *
                                            sizeof(pthread_mutex_t));
  for(i=0; i<CRYPTO_num_locks(); i++) {
    pthread_mutex_init(&(lockarray[i]), NULL);
  }

  CRYPTO_set_id_callback((unsigned long (*)())thread_id);
  CRYPTO_set_locking_callback((void (*)())lock_callback);
}

void curl_kill_locks(void)
{
  int i;

  CRYPTO_set_locking_callback(NULL);
  for(i=0; i<CRYPTO_num_locks(); i++)
    pthread_mutex_destroy(&(lockarray[i]));

  OPENSSL_free(lockarray);
}

static size_t curl_write_buffer_cb(void *contents, size_t size, size_t nmemb, void *userp)
{
  size_t realsize = size * nmemb;
  Buffer *buffer = (Buffer *)userp;

  buffer->mem = realloc(buffer->mem, buffer->size + realsize + 1);
  if(buffer->mem == NULL) {
    fprintf(stderr, "not enough memory (realloc returned NULL)\n");
    return 0;
  }

  memcpy(&(buffer->mem[buffer->size]), contents, realsize);
  buffer->size += realsize;
  buffer->mem[buffer->size] = 0;

  return realsize;
}

void curl_load_url(char *url, Buffer *buffer)
{
  CURL *curl;
  CURLcode res;

  curl = curl_easy_init();
  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_buffer_cb);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)buffer);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");
  res = curl_easy_perform(curl);
  if(res != CURLE_OK) {
    fprintf(stderr, "curl_easy_perform() failed: %s\n",
            curl_easy_strerror(res));
  }
  else {
    fprintf(stderr, "%lu bytes retrieved\n", (long)buffer->size);
  }

  curl_easy_cleanup(curl);
}

