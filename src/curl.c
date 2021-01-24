#include "curl.h"
#include <curl/curl.h>
#include <openssl/crypto.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static size_t curl_write_cb(void* contents, size_t size, size_t nmemb,
                            void* user_p) {
  size_t  realsize = size * nmemb;
  Buffer* buffer   = (Buffer*)user_p;

  // fprintf(stderr, "curl read nmemb = %zu * %zu bytes = %zu bytes realsize\n",
  //         nmemb, size, realsize);
  buffer->mem = realloc(buffer->mem, buffer->size + realsize + 1);
  if (!buffer->mem) {
    perror("realloc curl buffer");
    exit(EXIT_FAILURE);
  }

  memcpy(&(buffer->mem[buffer->size]), contents, realsize);
  buffer->size += realsize;
  buffer->mem[buffer->size] = '\0'; // null terminator

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
  }
  if ((res = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE,
                                      &http_response_code)) != CURLE_OK) {
    fprintf(stderr, "curl_easy_getinfo() failed: %s\n",
            curl_easy_strerror(res));
    return -1;
  }
  if (http_response_code != 200) {
    fprintf(stderr, "Received HTTP Code: %lu for url = '%s'\n",
            http_response_code, url);
    return -1;
  }
  curl_easy_cleanup(curl);
  return 0;
}
