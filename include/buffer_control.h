#ifndef __buffer_control_h__
#define __buffer_control_h__

#include "../include/t2fs.h"

/* Quebra um buffer e retorna apenas a parte espeicificada*/
BYTE *getValueFromBuffer(BYTE *buffer, int initialByte, int size);

/* Converte um os 4 primeiros bytes de um buffer para um DWORD (4 bytes) 
Assume que a primeira posicao do vetor de bytes do buffer seja a parte menos significativa*/
DWORD bufferToDWORD(BYTE *buffer, int initialByte);

#endif