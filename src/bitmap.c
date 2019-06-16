#include "../include/bitmap.h"
#include "../include/t2fs.h"
#include "../include/buffer_control.h"
#include <stdbool.h>

typedef struct bitmapInfo{
    DWORD indexBitmapSize;  //Tamanho em blocos
    DWORD dataBitmapSize;   //Tamanho em blocos
    DWORD bitmapStart;      //Setor de inicio
    DWORD bitmapSize;       //Tamanho em setores
} BITMAP_INFO;

static PART_INFO partInfo;

static BITMAP_INFO bitmapInfo;
static bool initialized = false;
static BYTE *bufferBitmaps;

int readBitmapFromDisk(BYTE *bitmap, DWORD sectorStart, DWORD size) {
  // size eh dado em setores
  int i;
  bitmap = malloc(sizeof(SECTOR_SIZE * size));
  BYTE *buffer = malloc(sizeof(SECTOR_SIZE));
  for (i = 0; i < size; i++) {
			if (read_sector(sectorStart + i, buffer) != 0)
				return ERROR;

      memcpy(bitmap + (i * SECTOR_SIZE), buffer, SECTOR_SIZE);
  }
  free(buffer);
  return SUCCESS;
}

int writeBitmapFromDisk(BYTE *bitmap, DWORD sectorStart, DWORD size) {
  // size eh dado em setores
  int i;
  BYTE *buffer = malloc(sizeof(SECTOR_SIZE));
  BYTE *pointer;
  for (i = 0; i < size; i++) {
			pointer = bitmap + i * SECTOR_SIZE;
      memcpy(buffer, pointer, SECTOR_SIZE);
      if (write_sector(sectorStart + i, buffer) != 0)
				return ERROR;
  }
  free(buffer);
  return SUCCESS;
}

//função para inicializar os dados do bitmap
static void initializeBitmap(){
    //Inicializa PartInfo a partir do disco
    readPartInfoBlocks();
    readPartInfoSectors();
    
    //Calcula tamanho do bitmap em bits (indice,blocos)
    bitmapInfo.indexBitmapSize = partInfo.dataBlocksStart - partInfo.indexBlocksStart / partInfo.blockSize;
    bitmapInfo.dataBitmapSize = ((partInfo.lastSectorAddress - (partInfo.firstSectorAddress + 1))/partInfo.blockSize) - bitmapInfo.indexBitmapSize;
    
    //Calcula o tamanho do bitmap em setores.
    bitmapInfo.bitmapSize = ceil((float)(bitmapInfo.indexBitmapSize + bitmapInfo.dataBitmapSize) / (float)SECTOR_SIZE * 8);
    bitmapInfo.bitmapStart = partInfo.firstSectorAddress + 1;

    bufferBitmaps = malloc(bitmapInfo.bitmapSize *SECTOR_SIZE);
    initialized = true;

    return readBitmapFromDisk(bufferBitmaps,bitmapInfo.bitmapStart,bitmapInfo.bitmapSize);
}

static	bool getBit (int bitNumber, BYTE *cache) {
	int	byteAddress;
	int	mask;
	
	byteAddress = bitNumber / 8;
	mask = (0x01 << (bitNumber % 8));
	
	return (cache[byteAddress] & mask?true:false);
}

static	int setBit (int bitNumber, BYTE bitValue, BYTE *cache) {
	
	int	byteAddress;
	int	mask;
	int	deltaSetor;
	
	// Altera a cache
	byteAddress = bitNumber / 8;
	mask = 0x01 << (bitNumber % 8);
	if (bitValue)
		cache[byteAddress] |= mask;
	else
		cache[byteAddress] &= ~mask;
	
  // Grava no disco
  return writeBitmapFromDisk(cache,bitmapInfo.bitmapStart, bitmapInfo.bitmapStart);
	
}

int	searchBitmapIndex(BYTE* buffer, int value){
  int i;
  bool booleanValue;

  booleanValue = value == 0? false:true;

  for(i = 0; i < bitmapInfo.indexBitmapSize; i++){
    if (getBit(i,buffer) == booleanValue){
      return i;
    }
  }

  return ERROR;
}

int searchBitmapData(BYTE* buffer, int value){
  int i;
  bool booleanValue;

  booleanValue = value == 0? false:true;

  for(i = bitmapInfo.indexBitmapSize; i < bitmapInfo.indexBitmapSize + bitmapInfo.dataBitmapSize; i++){
    if (getBit(i,buffer) == booleanValue){
      return i;
    }
  }

  return ERROR;
}

int	searchBitmap(int bitmapType, int bitValue){
  if(!initialized)
    initializeBitmap();

  if(bitmapType == BITMAP_INDEX)
    return searchBitmapIndex(bufferBitmaps,bitValue);
  
  else //bitmapType == BITMAP_DATA
    return searchBitmapData(bufferBitmaps,bitValue); 
}


int setBitmap(int bitmapType, int bitNumber, int bitValue){
  if(!initialized)
    initializeBitmap();

  if (bitmapType == BITMAP_INDEX)
    return setBit(bitNumber,bitValue,bufferBitmaps);
  else //bitmapType == BITMAP_DATA
    return setBit(bitNumber + bitmapInfo.indexBitmapSize,bitValue,bufferBitmaps); 
}

int	getBitmap (int bitmapType, int bitNumber){
  if(!initialized)
    initializeBitmap();

  if(bitmapType == BITMAP_INDEX)
    return getBit(bitNumber,bufferBitmaps);
  else
    return getBit(bitNumber + bitmapInfo.indexBitmapSize, bufferBitmaps);
  
}
