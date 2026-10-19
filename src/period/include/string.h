/* string.h: MSC 5.1 declarations, implemented by src/crt (crt_* names). */
#ifndef RECIV_STRING_H
#define RECIV_STRING_H
#include "crtdefs.h"


char *strcpy(char *, const char *);
char *strncpy(char *, const char *, size_t);
char *strcat(char *, const char *);
size_t strlen(const char *);
int strcmp(const char *, const char *);
int strncmp(const char *, const char *, size_t);
int stricmp(const char *, const char *);
int strnicmp(const char *, const char *, size_t);
char *strupr(char *);
char *strlwr(char *);
char *strstr(const char *, const char *);
char *strchr(const char *, int);
void *memcpy(void *, const void *, size_t);
void *memset(void *, int, size_t);
int memcmp(const void *, const void *, size_t);
void movedata(unsigned int, unsigned int, unsigned int, unsigned int, unsigned int);
#endif
