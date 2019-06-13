#include "../include/buffer_control.h"
#include <stdlib.h>

/* Quebra um buffer e retorna apenas a parte espeicificada*/
BYTE *getValueFromBuffer(BYTE *buffer, int initialByte, int size) {
    int i;
    BYTE *value = (BYTE *) malloc(size);

    for (i = 0; i < size; i++) {
        value[i] = buffer[initialByte + i]; 
    }

    return value;
}

DWORD bufferToDWORD(BYTE *buffer, int initialByte) {
    BYTE *value = getValueFromBuffer(buffer, initialByte, sizeof(DWORD));
    DWORD result = (value[3] << 24) | (value[2] << 16) | (value[1] << 8) | value[0];
    free(value);
    return result;
}