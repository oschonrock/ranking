#include <stdbool.h>
#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

bool preg_match(const char* pattern, const char* subject, bool ignore_case);
char* remove_spaces(const char* restrict str);
