/* stdlib.h: MSC 5.1 declarations, implemented by src/crt (crt_* names). */
#ifndef RECIV_STDLIB_H
#define RECIV_STDLIB_H
#include "crtdefs.h"

#define RAND_MAX 0x7FFF

int abs(int);
long labs(long);
int rand(void);
void srand(unsigned int);
void exit(int);
char *itoa(int, char *, int);
char *ltoa(long, char *, int);
int atoi(const char *);
long atol(const char *);
void *malloc(size_t);
void free(void *);
#endif
