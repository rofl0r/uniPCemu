#ifndef FOPEN64_H
#define FOPEN64_H

#include "headers/types.h" //Basic types!

FILE *fopen64(char *filename, char *mode);
int fseek64(FILE *stream, int64_t pos, int direction);
int64_t ftell64(FILE *stream);
int feof64(FILE *stream);
int64_t fread64(void *data,int64_t multiplication,int64_t size,FILE *stream);
int64_t fwrite64(void *data,int64_t multiplication,int64_t size,FILE *stream);
int fclose64(FILE *stream);

#endif