#ifndef __CURL_H__
#define __CURL_H__

#include <stddef.h>

typedef struct Buffer {
  char*  mem;
  size_t size;
} Buffer;

int  curl_load_url(char* url, Buffer* buffer);

#endif /* __CURL_H__ */
