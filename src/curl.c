// directly from: https://curl.haxx.se/libcurl/c/threaded-ssl.html

#include "curl.h"
#include <curl/curl.h>
#include <openssl/crypto.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>

/* we have this global to let the callback get easy access to it */
static pthread_mutex_t* lockarray;

__attribute__((unused)) static void curl_ssl_lock_cb( int mode, int type, char* file, int line) {
  (void)file;
  (void)line;
  if (mode & CRYPTO_LOCK) {
    pthread_mutex_lock(&(lockarray[type]));
  } else {
    pthread_mutex_unlock(&(lockarray[type]));
  }
}

__attribute__((unused)) static unsigned long curl_ssl_thread_id(void) {
  unsigned long ret;
  ret = (unsigned long)pthread_self();
  return ret;
}

void curl_ssl_init_locks(void) {
  int i;

  lockarray = (pthread_mutex_t*)OPENSSL_malloc(CRYPTO_num_locks() *
                                               sizeof(pthread_mutex_t));
  for (i = 0; i < CRYPTO_num_locks(); i++) {
    pthread_mutex_init(&(lockarray[i]), NULL);
  }
  CRYPTO_set_id_callback((unsigned long (*)())curl_ssl_thread_id);
  CRYPTO_set_locking_callback((void (*)())curl_ssl_lock_cb);
}

void curl_ssl_kill_locks(void) {
  int i;

  CRYPTO_set_locking_callback(NULL);
  for (i = 0; i < CRYPTO_num_locks(); i++)
    pthread_mutex_destroy(&(lockarray[i]));

  OPENSSL_free(lockarray);
}

static size_t curl_write_cb(void* contents, size_t size, size_t nmemb,
                            void* user_p) {
  size_t  realsize = size * nmemb;
  Buffer* buffer   = (Buffer*)user_p;

  // fprintf(stderr, "curl read nmemb = %zu * %zu bytes = %zu bytes realsize\n",
  // nmemb, size, realsize);
  buffer->mem = realloc(buffer->mem, buffer->size + realsize + 1);
  if (buffer->mem == NULL) {
    fprintf(stderr, "not enough memory (realloc returned NULL)\n");
    return 0;
  }

  memcpy(&(buffer->mem[buffer->size]), contents, realsize);
  buffer->size += realsize;
  buffer->mem[buffer->size] = 0;

  return realsize;
}

int curl_load_url(char* url, Buffer* buffer) {
  CURL*    curl;
  CURLcode res;
  long     http_response_code;

  curl = curl_easy_init();
  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)buffer);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");
  res = curl_easy_perform(curl);
  if (res != CURLE_OK) {
    fprintf(stderr, "curl_easy_perform() failed: %s\n",
            curl_easy_strerror(res));
    return -1;
  } else if ((res = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE,
                                      &http_response_code)) != CURLE_OK) {
    fprintf(stderr, "curl_easy_getinfo() failed: %s\n",
            curl_easy_strerror(res));
    return -1;
  } else if (http_response_code != 200) {
    fprintf(stderr, "Received HTTP Code: %lu for url = '%s'\n",
            http_response_code, url);
    return -1;
  }
  curl_easy_cleanup(curl);
  return 0;
}
