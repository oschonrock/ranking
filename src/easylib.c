#include <ctype.h>   // isspace()
#include <stdbool.h> // bool
#include <stdio.h>   // fprintf()
#include <string.h>  // strlen()

#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h> // pcre

/* very simple matching wrapper for pcre, just say if it matched at all */
bool preg_match(const PCRE2_SPTR pattern, const PCRE2_SPTR subject,
                bool ignore_case) {

  pcre2_code* re;
  int         errornumber;
  PCRE2_SIZE  erroffset;
  re = pcre2_compile(pattern, /* the pattern */
                     PCRE2_ZERO_TERMINATED,
                     ignore_case ? PCRE2_CASELESS : 0, /* default options */
                     &errornumber,                     /* for error message */
                     &erroffset,                       /* for error offset */
                     NULL); /* use default character tables */

  if (re == NULL) {
    PCRE2_UCHAR buffer[256];
    pcre2_get_error_message(errornumber, buffer, sizeof(buffer));
    fprintf(stderr, "PCRE compilation failed at offset %zu: %d: %s\n",
            erroffset, errornumber, buffer);
    return false;
  }

  int subject_length = (int)strlen((const char*)subject);

  pcre2_match_data* match_data = pcre2_match_data_create_from_pattern(re, NULL);

  int         rc;
  rc = pcre2_match(re,             /* the compiled pattern */
                   subject,        /* the subject string */
                   subject_length, /* the length of the subject */
                   0,              /* start at offset 0 in the subject */
                   0,              /* default options */
                   match_data,     /* block for storing the result */
                   NULL);          /* use default match context */

  if (rc < 0) {
    switch (rc) {
    case PCRE2_ERROR_NOMATCH:
      break;
    default:
      fprintf(stderr, "Matching error %d\n", rc);
      break;
    }
    pcre2_match_data_free(match_data);   /* Release memory used for the match */
    pcre2_code_free(re); /* Release memory used for the compiled pattern */
    return false;
  }
  pcre2_match_data_free(match_data);   /* Release memory used for the match */
  pcre2_code_free(re); /* Release memory used for the compiled pattern */
  return true;
}

// http://stackoverflow.com/questions/1726302/removing-spaces-from-a-string-in-c
void remove_spaces(char* restrict res, const char* restrict str) {
  while (*str != '\0') {
    if (!isspace(*str)) {
      *res = *str;
      res++;
    }
    str++;
  }
  *res = '\0';
}
