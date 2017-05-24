#ifndef FLAGS_H
#define FLAGS_H

#include "headers/types.h" //CPU!

void flag_p8(uint8_t value);
void flag_p16(uint16_t value);
void flag_p32(uint32_t value);
void flag_s8(uint8_t value);
void flag_s16(uint16_t value);
void flag_s32(uint32_t value);
void flag_szp8(uint8_t value);
void flag_szp16(uint16_t value);
void flag_szp32(uint32_t value);
void flag_log8(uint8_t value);
void flag_log16(uint16_t value);
void flag_log32(uint32_t value);
void flag_adc8(uint8_t v1, uint8_t v2, uint8_t v3);
void flag_adc16(uint16_t v1, uint16_t v2, uint16_t v3);
void flag_adc32(uint32_t v1, uint32_t v2, uint32_t v3);
void flag_add8(uint8_t v1, uint8_t v2);
void flag_add16(uint16_t v1, uint16_t v2);
void flag_add32(uint32_t v1, uint32_t v2);
void flag_sbb8(uint8_t v1, uint8_t v2, uint8_t v3);
void flag_sbb16(uint16_t v1, uint16_t v2, uint16_t v3);
void flag_sbb32(uint32_t v1, uint32_t v2, uint32_t v3);
void flag_sub8(uint8_t v1, uint8_t v2);
void flag_sub16(uint16_t v1, uint16_t v2);
void flag_sub32(uint32_t v1, uint32_t v2);

#endif