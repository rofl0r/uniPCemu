#ifndef FOPEN64_H
#define FOPEN64_H

#include "headers/types.h" //Basic types!

FILE *emufopen64(char *filename, char *mode);
int emufseek64(FILE *stream, int64_t pos, int direction);
int emufflush64(FILE *stream);
int64_t emuftell64(FILE *stream);
int emufeof64(FILE *stream);
int64_t emufread64(void *data,int64_t multiplication,int64_t size,FILE *stream);
int64_t emufwrite64(void *data,int64_t multiplication,int64_t size,FILE *stream);
int emufclose64(FILE *stream);
#endif
