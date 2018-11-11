#ifndef __CRC_H
#define __CRC_H

#include "init.h"

extern CRC_HandleTypeDef hcrc;

void CRC_Init();
void CRC_DeInit();
uint32_t calc_crc32(uint32_t* address, uint32_t size_in_byte);

#endif
