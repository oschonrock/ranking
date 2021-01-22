#include <stdio.h>     // fprintf()
#include <ctype.h>     // isspace()
#include <string.h>    // strlen()
#include <stdbool.h>   // bool
#include <pcre.h>      // pcre

#define OVECCOUNT 30    /* should be a multiple of 3 */

/* very simple matching wrapper for pcre, just say if it matched at all */
bool preg_match(const char *pattern, const char *subject, bool ignore_case)
{
    pcre *re;
    const char *error;
    int erroffset;
    int ovector[OVECCOUNT];
    int rc;

    re = pcre_compile(
        pattern,                                      /* the pattern */
        ignore_case ? PCRE_CASELESS : 0,              /* default options */
        &error,                                       /* for error message */
        &erroffset,                                   /* for error offset */
        NULL);                                        /* use default character tables */

    if (re == NULL) {
        fprintf(stderr, "PCRE compilation failed at offset %d: %s\n", erroffset, error);
        return false;
    }

    int subject_length = (int)strlen(subject);
    
    rc = pcre_exec(
        re,                   /* the compiled pattern */
        NULL,                 /* no extra data - we didn't study the pattern */
        subject,              /* the subject string */
        subject_length,       /* the length of the subject */
        0,                    /* start at offset 0 in the subject */
        0,                    /* default options */
        ovector,              /* output vector for substring information */
        OVECCOUNT);           /* number of elements in the output vector */

    if (rc < 0) {
        switch(rc) {
        case PCRE_ERROR_NOMATCH:
            break;
        default:
            fprintf(stderr, "Matching error %d\n", rc);
            break;
        }
        pcre_free(re);     /* Release memory used for the compiled pattern */
        return false;
    }

    pcre_free(re);     /* Release memory used for the compiled pattern */
    return true;
    
}

// http://stackoverflow.com/questions/1726302/removing-spaces-from-a-string-in-c
void remove_spaces(char* restrict res, const char* restrict str)
{
    while (*str != '\0') {
        if(!isspace(*str)) {
            *res = *str;
            res++;
        }
        str++;
    }
    *res = '\0';
}
