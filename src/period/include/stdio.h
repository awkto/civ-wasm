/* stdio.h: MSC 5.1 declarations, implemented by src/crt (crt_* names). */
#ifndef RECIV_STDIO_H
#define RECIV_STDIO_H
#include "crtdefs.h"

typedef struct _iobuf FILE;
#define EOF (-1)
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2


FILE *fopen(const char *, const char *);
int fclose(FILE *);
size_t fread(void *, size_t, size_t, FILE *);
size_t fwrite(const void *, size_t, size_t, FILE *);
int fseek(FILE *, long, int);
long ftell(FILE *);
char *fgets(char *, int, FILE *);
int fgetc(FILE *);
int fscanf(FILE *, const char *, ...);
int fprintf(FILE *, const char *, ...);
int sprintf(char *, const char *, ...);
int printf(const char *, ...);
void perror(const char *);
#endif
