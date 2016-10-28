
typedef struct Buffer {
  char *mem;
  size_t size;
} Buffer;


void curl_init_locks(void);
void curl_kill_locks(void);
int curl_load_url(char *url, Buffer *buffer);
