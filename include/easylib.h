#include <stdbool.h>
#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

bool preg_match(const char* pattern, const char* subject, bool ignore_case);
void remove_spaces(char* restrict res, const char* restrict str);
