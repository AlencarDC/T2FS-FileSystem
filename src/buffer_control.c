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

char bufferToCHAR(BYTE *buffer, int initialByte){
    return (char) buffer[initialByte];
}

void bufferToDirEntryName(BYTE *buffer, char *dirEntryName,int initialByte){
    int i;
    for(i = 0; i < FILE_NAME_SIZE + 1; i++){
        dirEntryName[i] = bufferToCHAR(buffer,initialByte+i);
    }
}

DIR_RECORD bufferToDIR_RECORD(BYTE *buffer, int initialByte){
    DIR_RECORD dirExtracted;
    
    dirExtracted.type = buffer[initialByte];
    bufferToDirEntryName(buffer,dirExtracted.name,initialByte + DIR_ENTRY_OFFSET);
    dirExtracted.byteFileSize = bufferToDWORD(buffer,initialByte + BLOCK_FILE_SIZE_OFFSET);
    dirExtracted.indexAddress = bufferToDWORD(buffer, initialByte + INDEX_ADDRESS_OFFSET);

    return dirExtracted;
}

BLOCK_POINTER bufferToBLOCK_POINTER(BYTE *buffer, int initialByte){
    BLOCK_POINTER blockPointerExtracted;
    blockPointerExtracted.valid = buffer[initialByte];
    blockPointerExtracted.blockPointer = bufferToDWORD(buffer,initialByte + 1);

    return blockPointerExtracted;
}

void insertDirEntryAt(BYTE *buffer, DIR_RECORD toInsert, int index) {
    BYTE *pointer;
    // Pega ponteiro para o local onde ve ser inserido
    pointer = buffer + index * sizeof(toInsert);
    // Copia para o buffer o registro pedido na posicao de pointer
    memcpy(pointer, &toInsert, sizeof(toInsert));
}

void insertBlockPointerAt(BYTE *buffer, BLOCK_POINTER toInsert, int index){
     BYTE *pointer;
    // Pega ponteiro para o local onde ve ser inserido
    pointer = buffer + index * sizeof(toInsert);
    // Copia para o buffer o registro pedido na posicao de pointer
    memcpy(pointer, &toInsert, sizeof(toInsert));
}