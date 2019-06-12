#include "../include/buffer_control.h"
#include <stdlib.h>

/* Converte um os 4 primeiros bytes de um buffer para um DWORD (4 bytes) 
Assume que a primeira posicao do vetor de bytes do buffer seja a parte menos significativa*/
DWORD bufferToDWORD(BYTE *buffer) {
    int i;
    DWORD value = 0;
    for (i = 0; i < 4; i++) {
        value += buffer[i] << (8*i); 
    }
    return value;
}