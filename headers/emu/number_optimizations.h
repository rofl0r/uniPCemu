#ifndef NUMBER_OPTIMIZATIONS_H
#define NUMBER_OPTIMIZATIONS_H

uint_32 DIVMULPOW2_32(uint_32 val, uint_32 todiv, byte divide); //Find the lowest bit that's on!
unsigned int DIVMULPOW2_16(unsigned int val, unsigned int todiv, byte divide); //Find the lowest bit that's on!
unsigned int OPTMUL(unsigned int val, unsigned int multiplication);
unsigned int OPTDIV(unsigned int val, unsigned int division);
unsigned int OPTMOD(unsigned int val, unsigned int division);
uint_32 OPTMUL32(uint_32 val, uint_32 multiplication);
uint_32 OPTDIV32(uint_32 val, uint_32 division);
uint_32 OPTMOD32(uint_32 val, uint_32 division);

#endif