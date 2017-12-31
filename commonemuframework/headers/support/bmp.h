#ifndef BMP_H
#define BMP_H

byte writeBMP(char *thefilename, uint_32 *image, int w, int h, byte doublexres, byte doubleyres, int virtualwidth); //1 on success, 0 on failure!

#endif