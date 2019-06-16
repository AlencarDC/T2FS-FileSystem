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

//função para inicializar os dados do bitmap
static void initializeBitmapInfo(){
    //Inicializa PartInfo a partir do disco
    readPartInfoBlocks();
    readPartInfoSectors();
    
    //Calcula tamanho do bitmap em bits (indice,blocos)
    bitmapInfo.indexBitmapSize = partInfo.dataBlocksStart - partInfo.indexBlocksStart / partInfo.blockSize;
    bitmapInfo.dataBitmapSize = ((partInfo.lastSectorAddress - (partInfo.firstSectorAddress + 1))/partInfo.blockSize) - bitmapInfo.indexBitmapSize;
    
    //Calcula o tamanho do bitmap em setores.
    bitmapInfo.bitmapSize = ceil((float)(bitmapInfo.indexBitmapSize + bitmapInfo.dataBitmapSize) / (float)SECTOR_SIZE * 8);
    bitmapInfo.bitmapStart = partInfo.firstSectorAddress + 1;
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

int	searchBitmap(int handle, int bitValue){
  if(handle == BITMAP_INDEX)
    return searchBitmapIndex(bufferBitmaps,bitValue);
  
  else //handle == BITMAP_DATA
    return searchBitmapData(bufferBitmaps,bitValue); 
}


int setBitmap(int handle, int bitNumber, int bitValue){
  if (handle == BITMAP_INDEX)
    setBit(bitNumber,bitValue,bufferBitmaps);
  else //handle == BITMAP_DATA
    setBit(bitNumber + bitmapInfo.indexBitmapSize,bitValue,bufferBitmaps); 
}
