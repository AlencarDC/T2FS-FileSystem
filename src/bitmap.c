#include "../include/apidisk.h"

#define INODE_TYPE 0
#define DATA_TYPE 1

//vou mover as estruturas pra .h depois
typedef *BYTE TBitmapVector;

typedef struct{
  int nElements_bits;  //given the bits needed to record the info, more bytes may be used.
  //nElements_bytes = size_bits/8 + (size_bits mod 8 > 1 ? 1:0);
  TBitmapVector bitmapVector;
} TBitmapDescriptor;

TBitmapDescriptor dBitmapIndexVector;
TBitmapDescriptor dBitmapDataVector;

int readBitmapFromDisk(BYTE *bitmap, DWORD sectorStart, DWORD, size) {
  // size eh dado em setores
  int i;
  bitmap = malloc(sizeof(SECTOR_SIZE * size));
  BYTE *buffer = malloc(sizeof(SECTOR_SIZE));
  for (i = 0; i < size; i++) {
			if (read_sector(sectorStart + i, buffer) != 0)
				return ERROR;

      memcpy(bitmap + (i * SECTOR_SIZE), buffer);
  }
  free(buffer);
  return SUCCESS;
}

int writeBitmapFromDisk(BYTE *bitmap, DWORD sectorStart, DWORD, size) {
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

typedef struct{
  int bitmap_start;
  int tamanho_index;
  int tamanho_bitmap;
  int tamanho_data;
} TBitmap;

TBitmap my_bitmap_info;


//start struct tbitmap data
void callMyBitmap(PART_INFO pinfo){
  my_bitmap_info.bitmap_start = pinfo.firstSectorAddress+1;
  my_bitmap_info.tamanho_index = pinfo.dataBlocksStart-pinfo.indexBlocksStart;
  my_bitmap_info.tamanho_bitmap = pinfo.indexBlocksStart-bitmap_start;
  my_bitmap_info.tamanho_data = tamanho_bitmap-tamanho_index;
}


//create bitmap descriptor
int createDBitmapVector(TBitmapDescriptor dBitmapVector, int size_bits){
  int nElements_bytes = size_bits/8 + (size_bits%8 > 1 ? 1:0);

  dBitmapVector.nElements_bits = size_bits;
  if((dBitmapVector.bitmapVector=(TBitmapVector) calloc(nElements_bytes, sizeof(BYTE)))==NULL)
    return -1;

    return 0;
}

//create global bitmap vectors descriptors
startDBitmapVectors(PART_INFO pinfo){  //change name

  int nBits_index;
  int nBits_data;

  int bitmapIndex_sectorSize;
  int bitmap_dataStart;

  callMyBitmap(pinfo);  //create bitmap struct info.
  nBits_index = tamanho_index/pinfo.blockSize;
  nBits_data = tamanho_data/pinfo.blockSize;

  // create space for a copy in memory of bitmap
  createDBitmapVector(dBitmapIndexVector, nBits_index);
  createDBitmapVector(dBitmapDataVector, nBits_data);

  // problema em determinar o tamanho do setor, pra nao multiplos de SECTOR_SIZE*8 --->

  // fill space with actual data
  bitmapIndex_sectorSize = (dBitmaIndexVector.nElements_bits/(SECTOR_SIZE*8) + dBitmaIndexVector.nElements_bits%(SECTOR_SIZE*8)>0?1:0);
  sendBitmap2Memory(dBitmapIndexVector, my_bitmap_info.bitmap_start, bitmapIndex_sectorSize);

  //bitmapIndex_sectorSize
  bitmap_dataStart = my_bitmap_info.bitmap_start + bitmapIndex_sectorSize;
  sendBitmap2Memory(dBitmapDataVector, bitmap_dataStart, );

}

int deleteDBitmapVector(TBitmapDescriptor dBitmapVector){
  free(dBitmapVector.bitmapVector);
}

sendBitmap2Memory(TBitmapDescriptor dBitmapVector, unsigned int startSector, unsigned int tamanho_setor){

  unigned int actual_sector;

  for(actual_sector=0; actual_sector<tamanho_setor; actual_sector++)
    if(read_sector(startSector+actual_sector, dBitmapVector.bitmapVector + actual_sector*SECTOR_SIZE)!=0)
      return -1;

  return 0;
}

int	getBitmap (int bitmapType, int bitNumber){
  BYTE  whole_byte;
  if(bitmapType==INODE_TYPE){
    if(bitNumber < dBitmapIndexVector.nElements_bits)
      whole_byte = dBitmapIndexVector.bitmapVector + (bitNumber/8);
      return (whole_byte & 1<<bitNumber%8)>0 ? 1:0;
  }else{
    if(bitNumber < dBitmapDataVector.nElements_bits)
      whole_byte = dBitmapDataVector.bitmapVector + (bitNumber/8);
      return (whole_byte & 1<<bitNumber%8)>0 ? 1:0;
  }
}

int	setBitmap (int bitmapType, int bitNumber, int bitValue){

//

}

int	searchBitmap (int bitmapType, int bitValue){


}
