#define INODE_TYPE 0
#define DATA_TYPE 1


typedef *WORD TBitmapVector;

typedef struct{
  int nElements_bytes;  //given the bits needed to record the info, more bytes may be used.
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
}

int createDBitmapVector(TBitmapDescriptor dBitmapVector, WORD size_bits){
  WORD nElements_bytes = size_bits/8 + (size_bits%8 > 1 ? 1:0);

  dBitmapVector.nElements_bytes = nElements_bytes;

  if((dBitmapVector.bitmapVector=(TBitmapVector) calloc(nElements_bytes, DWORD))==NULL)
    return -1;

    return 0;
}

//create global bitmap vectors descriptors
startDBitmapVectors(PART_INFO pinfo){  //change name

  WORD nBits_index = superblock.indexBlockAreaSize * pinfo.blockSize;
  WORD nBits_data = superblock.dataBlockAreaSize * pinfo.blockSize;

  createDBitmapVector(dBitmapIndexVector, nBits_index);
  createDBitmapVector(dBitmapDataVector, nBits_data);

  entryDisk(...)
  entryDisk(...)

}

int deleteDBitmapVector(TBitmapDescriptor dBitmapVector){
  free(dBitmapVector.bitmapVector);
}

/*
xxx(PART_INFO pinfo){





}
bitmap_start=partInfo.firstSectorAddress+1
tamanho_index = dataBlocksStart-indexBlocksStart (setores)
tamanho_bitmap = indexBlocksStart-bitmap_start (setores)
tamanho_data = [2^(tamanho_bitmap*256*8)]/8*256-tamanho_index (setores)
os n bits de bitmap representam 2^n blocos
*/
bool readPartInfoSectors() {
	BYTE buffer[tamanho_bitmap*SECTOR_SIZE];
	BYTE bufferSector[SECTOR_SIZE];

	if (read_sector(0, bufferSector) == 0) {
		partInfo.firstSectorAddress = bufferToDWORD(bufferSector, INIT_BYTE_PART_TABLE);
		partInfo.lastSectorAddress = bufferToDWORD(bufferSector, INIT_BYTE_PART_TABLE + sizeof(DWORD));
		return true;
	}

	return false;
}



/*
START
*/




int	getBitmap (int bitmapType, int bitNumber){
  BYTE  whole_byte;
  if(bitmapType==INODE_TYPE){
    if(bitNumber < dBitmapIndexVector.nElements_bytes*8)
      whole_byte = dBitmapIndexVector.bitmapVector + (bitNumber/8);
      return (whole_byte & 1<<bitNumber%8)>0 ? 1:0;
  }else{
    if(bitNumber < dBitmapDataVector.nElements_bytes*8)
      whole_byte = dBitmapDataVector.bitmapVector + (bitNumber/8);
      return (whole_byte & 1<<bitNumber%8)>0 ? 1:0;
  }
}

int	setBitmap (int bitmapType, int bitNumber, int bitValue){

//

}

int	searchBitmap (int bitmapType, int bitValue){


}
