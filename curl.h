
struct MemoryStruct {
  char *memory;
  size_t size;
};


void curl_init_locks(void);
void curl_kill_locks(void);
void curl_load_url(char *url, struct MemoryStruct *buffer);
