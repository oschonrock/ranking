#include <stdbool.h>
#include <pcre.h>

bool preg_match(const char *pattern, const char *subject, bool ignore_case);
void remove_spaces(char* restrict res, const char* restrict str);
