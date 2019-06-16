#ifndef __buffer_control_h__
#define __buffer_control_h__

#include "../include/t2fs.h"

/* Quebra um buffer e retorna apenas a parte espeicificada*/
BYTE *getValueFromBuffer(BYTE *buffer, int initialByte, int size);

/* Converte um os 4 primeiros bytes de um buffer para um DWORD (4 bytes) 
Assume que a primeira posicao do vetor de bytes do buffer seja a parte menos significativa*/
DWORD bufferToDWORD(BYTE *buffer, int initialByte);

//Retira um char de um buffer de bytes apontado por initialByte
char bufferToCHAR(BYTE *buffer, int initialByte);

//inicializa dirEntryName com seu conteudo(string) retirado de buffer a partir de initialByte.
void bufferToDirEntryName(BYTE *buffer, char *dirEntryName,int initialByte);

//Retira um DIR_RECORD de buffer a partir de initialByte
DIR_RECORD bufferToDIR_RECORD(BYTE *buffer, int initialByte);

//Retira um BLOCK_POINTER de buffer a partir de initialByte.
BLOCK_POINTER bufferToBLOCK_POINTER(BYTE *buffer, int initialByte);

// Insere um dir_record no bloco de dados de diretorio passado como parametro na posicao dada pelo index
void insertDirEntryAt(BYTE *buffer, DIR_RECORD toInsert, int index);

#endif