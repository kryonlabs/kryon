#ifndef LIB9_STRING_H
#define LIB9_STRING_H

#include "lib9_types.h"
#include <string.h>

/*
 * lib9 String Functions
 *
 * String utility functions, mostly wrappers around standard libc.
 */

/* Standard string functions (use libc directly) */
#define strlen  strlen
#define strcmp  strcmp
#define strncmp strncmp
#define strcpy  strcpy
#define strncpy strncpy
#define strcat  strcat
#define strncat strncat
#define strchr  strchr
#define strrchr strrchr
#define strstr  strstr

/* lib9-specific string functions */
char*   strecpy(char *to, char *e, char *from);
char*   strdup(const char *s);
int     tokenize(char *s, char **args, int maxargs);
int     getfields(char *str, char **args, int maxargs, int multiflag, char *delims);

/* Case-insensitive variants */
int     cistrcmp(char *s1, char *s2);
int     cistrncmp(char *s1, char *s2, int n);
char*   cistrstr(char *s1, char *s2);

#endif /* LIB9_STRING_H */
