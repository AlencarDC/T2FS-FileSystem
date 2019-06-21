
/**
*/
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../include/t2fs.h"
#include "../include/bitmap.h"
#include "../include/apidisk.h"
#include "../include/buffer_control.h"


/********************************************************************************/
/************************************ PRIVATE ***********************************/
/********************************************************************************/
bool initialized = false;	// Estado do sistema de arquivos: inicializado ou nao para correto funcionamento

char currentPath[MAX_FILE_NAME_SIZE];	// Caminho corrente do sistema de arquivos
DWORD currentDirIndexPointer = -1; // Ponteiro de bloco de indice de diretorio associado ao caminho corrente
DWORD rootDirIndex = -1; // Numero/endereco do Bloco de indice da raiz

static PART_INFO partInfo;	// Estrutura que armazenara enderecos e limites da particao
HANDLER openedFiles[MAX_FILE_OPEN];	// Estrutura armazenadora das informacoes dos arquivos abertos
HANDLER openedDirs[MAX_FILE_OPEN];	// Estrutura armazenadora das informacoes dos diretorios abertos


void _printSuperblock(SUPERBLOCK superblock) {
	printf("\n\nSUPERBLOCK:\n");
	printf("ID: %.4s\n", superblock.id);
	printf("BlockSize: %d | 0x%x\n", superblock.blockSize, superblock.blockSize);
	printf("PartitionSize: %d | 0x%x\n", superblock.partitionSize, superblock.partitionSize);
	printf("NumberOfPointers: %d | 0x%x\n", superblock.numberOfPointers, superblock.numberOfPointers);
	printf("bitmapSectorsSize: %d | 0x%x\n", superblock.bitmapSectorsSize, superblock.bitmapSectorsSize);
	printf("dataBlockAreaSize: %d | 0x%x\n", superblock.dataBlockAreaSize, superblock.dataBlockAreaSize);
	printf("indexBlockAreaSize: %d | 0x%x\n", superblock.indexBlockAreaSize, superblock.indexBlockAreaSize);
}

void _printPartInfo() {
	printf("\n\nPARTINFO:\n");
	printf("BlockSize: %d | 0x%x\n", partInfo.blockSize, partInfo.blockSize);
	printf("dataBlocksStart: %d | 0x%x\n", partInfo.dataBlocksStart, partInfo.dataBlocksStart);
	printf("indexBlocksStart: %d | 0x%x\n", partInfo.indexBlocksStart, partInfo.indexBlocksStart);
	printf("firstSectorAddress: %d | 0x%x\n", partInfo.firstSectorAddress, partInfo.firstSectorAddress);
	printf("lastSectorAddress: %d | 0x%x\n", partInfo.lastSectorAddress, partInfo.lastSectorAddress);
	printf("numberOfPointers: %d | 0x%x\n", partInfo.numberOfPointers, partInfo.numberOfPointers);
}

DWORD getFreeIndexBlock() {
	DWORD offset = searchBitmap(BITMAP_INDEX, 1);
	printf("ofsset indexBlock %d\n", offset);
	 if (offset >= 0 && setBitmap(BITMAP_INDEX, offset, 0) == 0)
		return (offset);
	
	return ERROR;
}

DWORD getFreeDataBlock() {
	DWORD offset = searchBitmap(BITMAP_DATA, 1);
	printf("ofsset dataBlock %d\n", offset);
	if (offset >= 0 && setBitmap(BITMAP_DATA, offset, 0) == 0)
		return (offset); 
	
	return ERROR;
}

DWORD freeDataBlock(DWORD dataBlockPointer){
	return setBitmap(BITMAP_DATA,dataBlockPointer,1);
}

DWORD freeIndexBlock(DWORD indexBlockPointer){
	return setBitmap(BITMAP_INDEX,indexBlockPointer,1);
}

bool createRootDir() {
	BLOCK_POINTER rootPointer;
	BYTE* indexBlockBuffer = malloc(partInfo.blockSize * SECTOR_SIZE);
	// Espera-se que essa funcao seja chamada logo apos uma formatacao
	// Dessa forma o bloco de indice alocado para raiz sera o primeiro
	if (rootDirIndex == -1) {
		DWORD freeIndexBlock = getFreeIndexBlock(); // Espera-se que seja o primeiro bloco de indice
		printf("freeIndexBlcok %d\n", freeIndexBlock);
		if (freeIndexBlock == 0) {
			DWORD freeDataBlock = getFreeDataBlock();
			printf("freeDataBlock %d\n", freeDataBlock);
			if (freeDataBlock >= 0) {
				getIndexBlockByPointer(indexBlockBuffer, freeIndexBlock);
				rootPointer.valid = RECORD_REGULAR;
				rootPointer.blockPointer = freeDataBlock;
				insertBlockPointerAt(indexBlockBuffer, rootPointer,0);
				writeIndexBlockAt(freeIndexBlock, indexBlockBuffer);
				initRootDir();
				//Cria . e .. (no caso do root dir é o mesmo.)
				createNavigationReferences(freeIndexBlock,freeIndexBlock);
				free(indexBlockBuffer);

				return true;
			}
		}
		printf("ERROR: O primeiro bloco de indice nao esta livre. Necesario formatar\n");
	}
	free(indexBlockBuffer);
	return false;
}

void cleanDisk() {
	int i;
	int size = partInfo.lastSectorAddress - partInfo.firstSectorAddress + 1;
	BYTE cleanBuffer[256] = {0}; 
	for (i = 0; i < size; i++)
		write_sector(partInfo.firstSectorAddress + i, cleanBuffer);
	
}

void cleanIndexBlock(DWORD pointer) {
	// Pointer eh o numero do bloco em relacao ao primeiro bloco de indice
	BYTE *zeroBlock = calloc(partInfo.blockSize * SECTOR_SIZE, sizeof(BYTE)); // inicializa em 0
	writeIndexBlockAt(pointer, zeroBlock);
	free(zeroBlock);
}

void cleanDataBlock(DWORD pointer) {
	// Pointer eh o numero do bloco em relacao ao primeiro bloco de indice
	BYTE *zeroBlock = calloc(partInfo.blockSize * SECTOR_SIZE, sizeof(BYTE)); // inicializa em 0
	writeDataBlockAt(pointer, zeroBlock);
	free(zeroBlock);
}

SUPERBLOCK createSuperblock(int sectorsPerBlock) {
	SUPERBLOCK superblock;

	int size = partInfo.lastSectorAddress - partInfo.firstSectorAddress + 1; // Tamanho da particao em setores
	int totalBlocks =  (size - 1) / sectorsPerBlock; // Superbloco ocupa 1 setor
	int utilBlocks = (totalBlocks * sectorsPerBlock * SECTOR_SIZE * 8) / (sectorsPerBlock * SECTOR_SIZE * 8 + 1);
	int totalSectorsBitmap = ceil((float)utilBlocks / (float)(SECTOR_SIZE * 8));

	int indexBlocksSize = utilBlocks / 3; //Numero de blocos de indice
	int remainingBlocks = utilBlocks % 3; // Blocos que sobraram
	int dataBlocksSize = (utilBlocks - indexBlocksSize) + remainingBlocks; // Numero de blocos de dados

	char id[4] = {'T', '2', 'F', 'S'};
	memcpy(&superblock.id, &id, 4);
	superblock.bitmapSectorsSize = totalSectorsBitmap;
	superblock.dataBlockAreaSize = dataBlocksSize;
	superblock.indexBlockAreaSize = indexBlocksSize;
	superblock.blockSize = sectorsPerBlock;
	superblock.partitionSize = utilBlocks; // Em blocos
	superblock.numberOfPointers = sectorsPerBlock * SECTOR_SIZE /(int) sizeof(BLOCK_POINTER);

	return superblock;
}

bool writeBlockAt(DWORD pointer, int type, BYTE *buffer) {
	/* type: 
		0 -> Bloco de dados
		1 ou outro -> Bloco de indice */
	DWORD startingSector = (type == 0) ? partInfo.dataBlocksStart : partInfo.indexBlocksStart;
	int i;
	BYTE *sectorBuffer = (BYTE *) malloc(SECTOR_SIZE);

	startingSector += pointer * partInfo.blockSize;
	for (i = 0; i < partInfo.blockSize; i++) {
		memcpy(sectorBuffer, buffer + (i * SECTOR_SIZE), SECTOR_SIZE);
		if (write_sector(startingSector + i, sectorBuffer) != 0) {
			free(sectorBuffer);
			return false;
		}
	}
	free(sectorBuffer);
	return true;
}

bool writeIndexBlockAt(DWORD pointer, BYTE *buffer) {
	return (writeBlockAt(pointer, 1, buffer));
}

bool writeDataBlockAt(DWORD pointer, BYTE *buffer) {
	return (writeBlockAt(pointer, 0, buffer));
}

int getBlockByPointer(BYTE *block,DWORD pointer, DWORD offset, int blockSize){
	int i,j;
	BYTE* sectorBuffer = malloc(SECTOR_SIZE); 
	DWORD initialSector = offset + (pointer * (DWORD)blockSize);

	for(i = 0; i < blockSize;i++){
		//Faz leitura e verifica se houve erro ao ler, se sim, aborta e retorna cod erro.
		if (read_sector(initialSector + i,sectorBuffer) != 0){
			printf("Erro ao ler setor ao preencher bloco\n");
			return SECTOR_ERROR;
		}
		//Copia setor para block
		for(j = 0; j < SECTOR_SIZE; j++){
			block[SECTOR_SIZE*i + j] = sectorBuffer[j];
		}
	}
	return SUCCESS;

}

FILE2 getFreeFileHandle() {
	int i;
	for (i = 0; i < MAX_FILE_OPEN; i++) {
		if (openedFiles[i].free == true)
			return i;
	}
	return ERROR;
}

DIR2 getFreeDirHandle() {
	int i;
	for (i = 0; i < MAX_FILE_OPEN; i++) {
		if (openedDirs[i].free == true)
			return i;
	}
	return ERROR;
}

int getIndexBlockByPointer(BYTE *indexBlock,DWORD pointer){
	return getBlockByPointer(indexBlock,pointer,partInfo.indexBlocksStart,partInfo.blockSize);
}

int getDataBlockByPointer(BYTE *dataBlock,DWORD pointer){
	return getBlockByPointer(dataBlock,pointer,partInfo.dataBlocksStart,partInfo.blockSize);
}

// WARNING: SE DER CORE DUMPED OLHE ESSA FUNCAO
char **splitPath(char *name, int *size) {
	int i = 0, n = 0;
	char *nameCopy = malloc(strlen(name) + 1);
  	strcpy(nameCopy, name);
	// Conta ocorrencias
	if (strlen(nameCopy) > 0 && nameCopy[0] != '/')
		n++;
	while (nameCopy[i] != '\0') {
		if (nameCopy[i] == '/')
			n++;
		i++;
	}
	if (strlen(nameCopy) > 0 && nameCopy[strlen(nameCopy)-1] == '/') {
		nameCopy[strlen(nameCopy)-1] = '\0';
		n--;
	}
	
	*size = n;

	i = 0;
	char **strings;
	strings = (char**)malloc(sizeof(char)*n);
	char *substring;
	char *nameBuff = strdup(nameCopy); // Necessaroi para strsep
	while( (substring = strsep(&nameBuff,"/")) != NULL ) {
		if (strlen(substring) > 0) {
			strings[i] = (char*) malloc(sizeof(char)*FILE_NAME_SIZE);
			strcpy(strings[i], substring);
			i++;
		}
	}
	*size = n;
	if (*size == 1 && strings[0] == NULL) {
		*size = 0;
		free(strings);
		return NULL;
	}
	return strings;
}

int getRecordByName(DIR_RECORD *record, DWORD indexPointer, char *name) {
	BYTE* indexBlock = malloc(SECTOR_SIZE * partInfo.blockSize);
	BYTE* dataBlock = malloc(SECTOR_SIZE * partInfo.blockSize);
	BLOCK_POINTER bufferPointer;
	DIR_RECORD bufferRecord;

	bool fileFound = false;
	while (!fileFound) {
		getIndexBlockByPointer(indexBlock, indexPointer);
		int indexIterator;
		// Busca entradas do diretorio
		for (indexIterator = 0; indexIterator < partInfo.numberOfPointers-1; indexIterator++) {
			
			bufferPointer = bufferToBLOCK_POINTER(indexBlock, indexIterator * sizeof(bufferPointer));

			if (bufferPointer.valid != INVALID_BLOCK_PTR) {
				getDataBlockByPointer(dataBlock, bufferPointer.blockPointer);
				
				int i; // Busca nos registros dos blocos de dados
				for (i = 0; i < (SECTOR_SIZE * partInfo.blockSize / sizeof(DIR_RECORD)); i++) {
					bufferRecord = bufferToDIR_RECORD(dataBlock, i * sizeof(bufferRecord));
					if (strcmp(bufferRecord.name, name) == 0 ) {
						*record = bufferRecord;

						return SUCCESS;
					}
				}
			}
		}
		// Checa se o encadeamento existe
		bufferPointer = bufferToBLOCK_POINTER(indexBlock, indexIterator * sizeof(bufferPointer));
		if (bufferPointer.valid == INVALID_BLOCK_PTR)
			return ERROR; 
	}

	return ERROR;
}

int getRecordByPath(DIR_RECORD *record, char *path) {
	DWORD indexPointer;
	if (path[0] == '/')
		indexPointer = rootDirIndex;
	else
		indexPointer = currentDirIndexPointer;

	int size;
	char **dirNames = splitPath(path, &size);
	int i;
	DIR_RECORD buffRecord;
	if (path[0] == '/' && size == 0) {
		//Root record
		buffRecord.type = RECORD_DIR;
		buffRecord.byteFileSize = 0;
		buffRecord.indexAddress = 0;
		strcpy(buffRecord.name, "/");
	} else {
		for (i = 0; i < size; i++) {
			if (getRecordByName(&buffRecord, indexPointer, dirNames[i]) == SUCCESS)
				indexPointer = buffRecord.indexAddress;
			else
				return ((i == size-1) ? FILE_NOT_EXIST : PATH_INCORRECT);
		}
	}
	
	// Encontrou o record que precisava
	*record = buffRecord;
	return SUCCESS;
}

int getDataBlockOfByte(DWORD offset, DWORD indexAddress, BYTE *buffer) {
	BYTE *bufferIndexBlock = malloc(SECTOR_SIZE * partInfo.blockSize);
	DWORD logicBlockToRead, offsetInCurrentIndex;
	DWORD offsetInBlock;

	//Fetch do bloco de indice (começa em 0)
	DWORD currentIndexLevel = 0; 
	getIndexBlockByPointer(bufferIndexBlock, indexAddress);

	//Posiciona num bloco logico o ponteiro
	logicBlockToRead = offset / (partInfo.blockSize * SECTOR_SIZE);
	//Determina o offset dentro do bloco logico acima
	offsetInBlock = offset % (partInfo.blockSize * SECTOR_SIZE);
	//Calcula o nível de indice necessário
	DWORD indexLevelNeeded = logicBlockToRead / (partInfo.numberOfPointers - 1);
	//Caso o nível de indice necessario para acessar seja maior que o nível acessado
	if(indexLevelNeeded > currentIndexLevel){
		//Itera sobre os níveis de indice até achar o correto
		while (currentIndexLevel < indexLevelNeeded){
			currentIndexLevel++;
			DWORD nextIndex = bufferToBLOCK_POINTER(bufferIndexBlock, (partInfo.numberOfPointers - 1) * sizeof(BLOCK_POINTER)).blockPointer;
			getIndexBlockByPointer(bufferIndexBlock,nextIndex);
		}
	}
	//Tradução do bloco lógico para o offset no nivel corrente
	offsetInCurrentIndex = logicBlockToRead - (currentIndexLevel * (partInfo.numberOfPointers - 1));
	//Tradução do bloco lógico para físico
	DWORD realBlockToRead = bufferToBLOCK_POINTER(bufferIndexBlock,sizeof(BLOCK_POINTER) *offsetInCurrentIndex).blockPointer;
	//Leitura do bloco físico
	getDataBlockByPointer(buffer,realBlockToRead);
}

// WARNING TODO: desalacar (free) buffers
int getNextDirRecordValid(DIR_RECORD *record, DWORD indexPointer, DWORD recordPointer) {
	BYTE indexBlock[SECTOR_SIZE * partInfo.blockSize];
	BYTE dataBlock[SECTOR_SIZE * partInfo.blockSize];
	BLOCK_POINTER bufferPointer;
	DIR_RECORD bufferRecord;

	DWORD numberOfRecordsPerDataBlock = (SECTOR_SIZE * partInfo.blockSize) / sizeof(DIR_RECORD);  
	DWORD dataBlockNumber = recordPointer / numberOfRecordsPerDataBlock;
	// numero do bloco de indice que armazena o bloco de dados que contem o registro apontado por recordPointer
	DWORD indexBlockNumber = (dataBlockNumber / (partInfo.numberOfPointers - 1)); 

	// Pegar o ponteiro do bloco de indice que o record esta
	int i;
	for (i = 0; i < indexBlockNumber; i++) {
		getIndexBlockByPointer(indexBlock, indexPointer);
		bufferPointer = bufferToBLOCK_POINTER(indexBlock, partInfo.numberOfPointers * sizeof(bufferPointer));
		if (bufferPointer.valid != INVALID_BLOCK_PTR)
			indexPointer = bufferPointer.blockPointer;
		else
			return ERROR;
	}

	//Pegar bloco de indice e endereco pro bloco de dados que esta o registro
	getIndexBlockByPointer(indexBlock, indexPointer);
	DWORD dataBlockPointer = dataBlockNumber % partInfo.numberOfPointers; 	// Nessa conta tem o -1 na numberOfPointer Ou nao??
	recordPointer -= dataBlockNumber * numberOfRecordsPerDataBlock; //Deslocamento relativo ao bloco de dados visto
	bufferPointer = bufferToBLOCK_POINTER(indexBlock, dataBlockPointer * sizeof(bufferPointer));
	if (bufferPointer.valid == INVALID_BLOCK_PTR)
		return ERROR;

	bool dirRecordNotFound = true;
	int recordIterator;

	// Buscar pelos blocos de indice encadeados atras do registro valido
	do {
		// Pega o bloco de dados da busca atual e procura pelo registro valido dentro
		getDataBlockByPointer(dataBlock, bufferPointer.blockPointer);
		for (recordIterator = recordPointer; recordIterator < numberOfRecordsPerDataBlock; recordIterator++) {
			bufferRecord = bufferToDIR_RECORD(dataBlock, recordIterator * sizeof(bufferRecord));
			// Achou o registro valido, calcula o seu numero equivalente
			
				*record = bufferRecord;				
				return (recordIterator + numberOfRecordsPerDataBlock * dataBlockNumber);
		}
		// Nao esta nesse bloco, pegar o proximo
		DWORD insideBlockPointerNumber = (dataBlockNumber % partInfo.numberOfPointers); // Vai -1?
		if ( insideBlockPointerNumber == (partInfo.numberOfPointers-1)) {
			// Eh o ultimo bloco de dados do bloco de indice precisamos pegar o proximo bloco de indices
			bufferPointer = bufferToBLOCK_POINTER(indexBlock, partInfo.numberOfPointers * sizeof(bufferPointer));
			if (bufferPointer.valid == INVALID_BLOCK_PTR) // O encadeamento terminou e nao ha proximo registro valido 
				return ERROR;
			getIndexBlockByPointer(indexBlock, bufferPointer.blockPointer);
			bufferPointer = bufferToBLOCK_POINTER(indexBlock, 0); // Primeiro bloco de dados
			if (bufferPointer.valid == INVALID_BLOCK_PTR)	// O bloco de dados nao seguinte nao eh valido, ou seja a busca terminou
				return ERROR;
		} else {
			// Pegar apenas o proximo bloco de dados dentro do bloco de indice atuaul
			bufferPointer = bufferToBLOCK_POINTER(indexBlock, insideBlockPointerNumber + 1);
		}

		dataBlockNumber++; // aumentar o numero do bloco de dados desse diretorio. Numero total, por exemplo diretorio utiliza 34 blocos de dados
	} while (dirRecordNotFound);

	return ERROR;
}

bool readPartInfoSectors(PART_INFO *partition) {
	BYTE bufferSector[SECTOR_SIZE];

	if (read_sector(0, bufferSector) == 0) {
		partition->firstSectorAddress = bufferToDWORD(bufferSector, INIT_BYTE_PART_TABLE);
		partition->lastSectorAddress = bufferToDWORD(bufferSector, INIT_BYTE_PART_TABLE + sizeof(DWORD));
		return true;
	}

	return false;
}

bool readPartInfoBlocks(PART_INFO *partition) {
	BYTE sectorBuffer[SECTOR_SIZE];
	SUPERBLOCK superblock;

	if (read_sector(partition->firstSectorAddress, sectorBuffer) == 0) {
		memcpy(&superblock, sectorBuffer, sizeof(SUPERBLOCK));
		// Se nao ter um particao T2FS formatada no local temos que formatar antes
		if (memcmp(superblock.id, "T2FS", 4) != 0) {
			format2(DEFAULT_BLOCK_SIZE);
			if (read_sector(partition->firstSectorAddress, sectorBuffer) == 0)
				memcpy(&superblock, sectorBuffer, sizeof(SUPERBLOCK));
			else 
				return false;
		}

		partition->indexBlocksStart = partition->firstSectorAddress + superblock.bitmapSectorsSize + 1; // Inicio + bitmap + superbloco
		partition->dataBlocksStart = partition->indexBlocksStart + (superblock.indexBlockAreaSize * superblock.blockSize);
		partition->numberOfPointers = superblock.numberOfPointers;
		partition->blockSize = superblock.blockSize;
		return true;
	}
	
	return false;
}

bool initPartInfo(PART_INFO *partition) {
	return (readPartInfoSectors(partition) == true && readPartInfoBlocks(partition) == true);
}

void initDirHandles(){
	int i;
	for(i = 0; i <MAX_DIR_OPEN; i++){
		openedDirs[i].free = true;
	}
}

void initFileHandles(){
	int i;
	for(i = 0; i <MAX_DIR_OPEN; i++){
		openedFiles[i].free = true;
	}
}

bool initRootDir() {
	if (getBitmap(BITMAP_INDEX, 0) == 0) {
		strcpy(currentPath, "/");
		currentDirIndexPointer = 0;
		rootDirIndex = 0;
		return true;
	}

	return false;
}

bool init() {
	if (initPartInfo(&partInfo) == true && initRootDir() == true) {
		initDirHandles();
		initFileHandles();
		initialized = true;
		return true;
	}

	return false;
}


DIR_RECORD createRecord(char *filename, BYTE type, DWORD dirIndexPointer) {
	DWORD indexBlockPointer = dirIndexPointer;
	int blockSizeInBytes = SECTOR_SIZE * partInfo.blockSize *sizeof(BYTE);
	int indexIterator, dirIterator;
	BLOCK_POINTER extractedPtr;
	BLOCK_POINTER ptrToIndexBlock;
	DWORD newDataBlockPointer;
	BLOCK_POINTER newBlockPointer;
	bool findEmptyEntry = false;

	//Buffers para fetch de bloco de indice e bloco de dados
	BYTE *indexBlockBuffer = malloc(blockSizeInBytes);
	BYTE *dataBlockBuffer = malloc(blockSizeInBytes);

	//Record a ser registrado 
	DIR_RECORD newRecord;
	newRecord.byteFileSize = 0;
	strcpy(newRecord.name,filename);
	newRecord.type = type;
	
	
	//Fetch do bloco de indices do diretorio corrente
	getIndexBlockByPointer(indexBlockBuffer,indexBlockPointer);
	int numberOfDirRecords = (partInfo.blockSize * SECTOR_SIZE) / sizeof(DIR_RECORD);
	
	while(!findEmptyEntry){
		for(indexIterator = 0; indexIterator < partInfo.numberOfPointers - 1; indexIterator++){
			extractedPtr = bufferToBLOCK_POINTER(indexBlockBuffer,indexIterator * sizeof(BLOCK_POINTER));

			if((char)extractedPtr.valid == INVALID_BLOCK_PTR){
				if((newDataBlockPointer = getFreeDataBlock()) >= 0){ //Achou um bloco válido
					newBlockPointer.blockPointer = newDataBlockPointer;
					newBlockPointer.valid = RECORD_REGULAR;

					insertBlockPointerAt(indexBlockBuffer,newBlockPointer,indexIterator);
					writeIndexBlockAt(indexBlockPointer,indexBlockBuffer);//Escreve no disco o bloco de indice
					newRecord.indexAddress = getFreeIndexBlock();
					getDataBlockByPointer(dataBlockBuffer,newDataBlockPointer);
					insertDirEntryAt(dataBlockBuffer,newRecord,0);
					writeDataBlockAt(newDataBlockPointer,dataBlockBuffer);//Escreve no disco o bloco de dados
					
					free(dataBlockBuffer);
					free(indexBlockBuffer);
					
					return newRecord;
				}

				else{//Não achou bloco válido
					free(indexBlockBuffer);
					free(dataBlockBuffer);
					
					newRecord.type = RECORD_INVALID;
					return newRecord;
				}
			}
			else{
				getDataBlockByPointer(dataBlockBuffer,extractedPtr.blockPointer);
				DIR_RECORD fetchedEntry;
				//Percorre as entradas de diretório no bloco
				for (dirIterator = 0; dirIterator < numberOfDirRecords; dirIterator++){
					fetchedEntry = bufferToDIR_RECORD(dataBlockBuffer, dirIterator * sizeof(DIR_RECORD));
					if(fetchedEntry.type == RECORD_INVALID){
						newRecord.indexAddress = getFreeIndexBlock();
						insertDirEntryAt(dataBlockBuffer,newRecord,dirIterator);
						writeDataBlockAt(extractedPtr.blockPointer, dataBlockBuffer);
						
						free(dataBlockBuffer);
						free(indexBlockBuffer);
						
						return newRecord;
					}
				}
			}
		}
		ptrToIndexBlock = bufferToBLOCK_POINTER(indexBlockBuffer,(partInfo.numberOfPointers - 1)* sizeof(BLOCK_POINTER));
		if(ptrToIndexBlock.valid == INVALID_BLOCK_PTR){//Necessário alocar outro bloco de indices
			DWORD newPtrIndexBlock = getFreeIndexBlock(); //Aloca bloco de indices
			newBlockPointer.valid = RECORD_REGULAR;//Cria estrutura block pointer
			newBlockPointer.blockPointer = newPtrIndexBlock;
			insertBlockPointerAt(indexBlockBuffer,newBlockPointer,indexIterator);//Insere no ultimo indice
			writeIndexBlockAt(indexBlockPointer,indexBlockBuffer);//Escreve Bloco de indice no disco.
			getIndexBlockByPointer(indexBlockBuffer,newPtrIndexBlock);
			indexBlockPointer = newPtrIndexBlock;
		}
		else{
			getIndexBlockByPointer(indexBlockBuffer,ptrToIndexBlock.blockPointer);
			indexBlockPointer = ptrToIndexBlock.blockPointer;
		}
		
	}
	return newRecord;
}

bool isFileOpened(FILE2 handle) {
	return (handle >= 0 && openedFiles[handle].free == false);
}

bool readIsWithinBoundary(HANDLER toCheck, int size){
	return (toCheck.pointer + size - 1 < toCheck.record.byteFileSize);
}

bool isDirOpened(DIR2 handle) {
	return (handle >= 0 && openedDirs[handle].free == false);
}

int writeFile(FILE2 handle, BYTE *buffer, int size){
	HANDLER archiveToWrite;
	BYTE *bufferDataBlock = malloc(SECTOR_SIZE * partInfo.blockSize);
	BYTE *bufferIndexBlock = malloc(SECTOR_SIZE * partInfo.blockSize);
	DWORD logicBlockToWrite, offsetInBlock, currentIndexLevel, currentIndexLevelBlkPointer, offsetInCurrentIndex;
	DWORD sizeWritten;
	DWORD bufferIndex;
	DWORD initialPointer;
	bool writeCompleted = false;
	int i;
	
	bufferIndex = 0;
	currentIndexLevel = 0;
	archiveToWrite = openedFiles[handle];
	initialPointer = archiveToWrite.pointer;
	currentIndexLevelBlkPointer = archiveToWrite.record.indexAddress;
	while(!writeCompleted){
		//Posiciona num bloco logico o ponteiro
		logicBlockToWrite = archiveToWrite.pointer / (partInfo.blockSize * SECTOR_SIZE);
		//Determina o offset dentro do bloco logico acima
		offsetInBlock = archiveToWrite.pointer % (partInfo.blockSize * SECTOR_SIZE);
		//Calcula o nível de indice necessário
		DWORD indexLevelNeeded = logicBlockToWrite / (partInfo.numberOfPointers - 1);	
		getIndexBlockByPointer(bufferIndexBlock,currentIndexLevelBlkPointer);
		while(currentIndexLevel < indexLevelNeeded){
			currentIndexLevel++;
			BLOCK_POINTER nextIndex = bufferToBLOCK_POINTER(bufferIndexBlock, (partInfo.numberOfPointers - 1) * sizeof(BLOCK_POINTER));
			if(nextIndex.valid == RECORD_REGULAR){
				getIndexBlockByPointer(bufferIndexBlock,nextIndex.blockPointer);
				currentIndexLevelBlkPointer= nextIndex.blockPointer;
			}else{//O bloco de indice n é válido
				BLOCK_POINTER dataBlockPointer;
				//Percorre os ponteiros verificando se um bloco de dados foi alocado, se n, aloca.
				for(i = 0; i < partInfo.numberOfPointers - 1; i++){
					dataBlockPointer = bufferToBLOCK_POINTER(bufferIndexBlock,i * sizeof(BLOCK_POINTER));
					if(dataBlockPointer.valid == RECORD_INVALID){
						DWORD newDataBlock = getFreeDataBlock();
						BLOCK_POINTER newBlockPointer;
						newBlockPointer.blockPointer = newDataBlock;
						newBlockPointer.valid = RECORD_REGULAR;
						insertBlockPointerAt(bufferIndexBlock,newBlockPointer,i);
						}
					}
				BLOCK_POINTER newBlockPointer;
				DWORD newIndexBlk = getFreeIndexBlock();
				newBlockPointer.valid = RECORD_REGULAR;
				newBlockPointer.blockPointer = newIndexBlk;
				insertBlockPointerAt(bufferIndexBlock,newBlockPointer,partInfo.numberOfPointers -1);
				writeIndexBlockAt(currentIndexLevelBlkPointer,bufferIndexBlock);
				currentIndexLevelBlkPointer = newBlockPointer.blockPointer;
				getIndexBlockByPointer(bufferIndexBlock,currentIndexLevelBlkPointer);
				}
			}

			//Tradução do bloco lógico para o offset no nivel corrente
			offsetInCurrentIndex = logicBlockToWrite - (currentIndexLevel * (partInfo.numberOfPointers - 1));
			//Fetch do ponteiro pra bloco do bloco de dados onde os dados serão escritos
			BLOCK_POINTER blockToWritePtr = bufferToBLOCK_POINTER(bufferIndexBlock,offsetInCurrentIndex);
			//Caso seja um ponteiro invalido, o bloco n foi alocado. Devemos alocar antes de fazer a escrita
			if(blockToWritePtr.valid == INVALID_BLOCK_PTR){
				BLOCK_POINTER newDataBlock;
				newDataBlock.blockPointer = getFreeDataBlock();
				newDataBlock.valid = RECORD_REGULAR;
				insertBlockPointerAt(bufferIndexBlock,newDataBlock,offsetInCurrentIndex);
				writeIndexBlockAt(currentIndexLevelBlkPointer, bufferIndexBlock);
				blockToWritePtr = newDataBlock;
			}
			getDataBlockByPointer(bufferDataBlock,blockToWritePtr.blockPointer);
			//Calculo do número de bytes que conseguimos escrever desse bloco
			DWORD remainingBytes = partInfo.blockSize * SECTOR_SIZE - offsetInBlock + 1;
			DWORD sizeToWrite;
			//Caso remainingBytes seja maior que o tamanho da escrita, conseguimos escrever td
			if(remainingBytes >= size){
				writeCompleted = true;
				sizeToWrite = size;
			}else//remaining_bytes < size (Escrevemos o que da)
				sizeToWrite = remainingBytes;
				
			//Copia o buffer de entradas para o buffer do bloco de dados
			for(i = 0; i < sizeToWrite; i++){
				bufferDataBlock[offsetInBlock + i] = buffer[bufferIndex + i];
			}
			//Escreve o bloco de dados no disco
			writeDataBlockAt(blockToWritePtr.blockPointer, bufferDataBlock);
			bufferIndex += sizeToWrite;
			archiveToWrite.pointer += sizeToWrite;
			sizeWritten += sizeToWrite;
			size -= sizeWritten;
		}

		free(bufferDataBlock);
		free(bufferIndexBlock);
		//Caso tenha aumentado arquivo, initalPointer + sizeWritten da o novo tamanho do arquivo
		if(initialPointer + sizeWritten > archiveToWrite.record.byteFileSize){
			archiveToWrite.record.byteFileSize = initialPointer + sizeWritten;
		}
		
		openedFiles[handle] = archiveToWrite;
		
		return sizeWritten;

	

	return ERROR;
}


int readFile(FILE2 handle, BYTE *buffer, int size){
	bool readDone = false;
	BYTE *bufferDataBlock = malloc(SECTOR_SIZE * partInfo.blockSize);
	BYTE *bufferIndexBlock = malloc(SECTOR_SIZE * partInfo.blockSize);
	HANDLER archiveToRead;
	DWORD logicBlockToRead, offsetInCurrentIndex;
	DWORD bufferPointer = 0;
	DWORD offsetInBlock;
	DWORD nBytesRead = 0;
	int i;

	archiveToRead = openedFiles[handle];
	//Caso o tamanho da leitura vá passar do tamanho do arquivo
	if(archiveToRead.pointer + size > archiveToRead.record.byteFileSize){
		size = archiveToRead.record.byteFileSize - archiveToRead.pointer;
	}
	//Fetch do bloco de indice (começa em 0)
	DWORD currentIndexLevel = 0; 
	getIndexBlockByPointer(bufferIndexBlock,archiveToRead.record.indexAddress);
	while(!readDone){
		//Posiciona num bloco logico o ponteiro
		logicBlockToRead = archiveToRead.pointer / (partInfo.blockSize * SECTOR_SIZE);
		//Determina o offset dentro do bloco logico acima
		offsetInBlock = archiveToRead.pointer % (partInfo.blockSize * SECTOR_SIZE);
		//Calcula o nível de indice necessário
		DWORD indexLevelNeeded = logicBlockToRead / (partInfo.numberOfPointers - 1);
		//Caso o nível de indice necessario para acessar seja maior que o nível acessado
		if(indexLevelNeeded > currentIndexLevel){
			//Itera sobre os níveis de indice até achar o correto
			while (currentIndexLevel < indexLevelNeeded){
				currentIndexLevel++;
				DWORD nextIndex = bufferToBLOCK_POINTER(bufferIndexBlock, (partInfo.numberOfPointers - 1) * sizeof(BLOCK_POINTER)).blockPointer;
				getIndexBlockByPointer(bufferIndexBlock,nextIndex);
			}
		}
		//Tradução do bloco lógico para o offset no nivel corrente
		offsetInCurrentIndex = logicBlockToRead - (currentIndexLevel * (partInfo.numberOfPointers - 1));
		//Tradução do bloco lógico para físico
		DWORD realBlockToRead = bufferToBLOCK_POINTER(bufferIndexBlock,sizeof(BLOCK_POINTER) *offsetInCurrentIndex).blockPointer;
		//Leitura do bloco físico
		getDataBlockByPointer(bufferDataBlock,realBlockToRead);
		//Calculo do número de bytes que conseguimos ler desse bloco
		DWORD remainingBytes = partInfo.blockSize * SECTOR_SIZE - offsetInBlock + 1;
		//Caso seja >= size então a leitura acabou
		if(remainingBytes >= size){
			nBytesRead+= size;//Conseguiu ler size bytes
			readDone = true;
			//Copia os dados pro buffer
			for(i = 0; i < size; i++){
				buffer[bufferPointer + i] = bufferDataBlock[offsetInBlock + i];
			}
			archiveToRead.pointer += size;//Posiciona ponteiro para proximo byte apos leitura
		}
		else{//remaining_bytes < size
			nBytesRead += remainingBytes;
			readDone = false;
			//Copia dados pro buffer
			for(i = 0; i < remainingBytes; i++){
				buffer[bufferPointer + i] = bufferDataBlock[offsetInBlock + i];
			}
			bufferPointer += remainingBytes; //Reposiciona bufferPointer
			archiveToRead.pointer += remainingBytes; //Reposiciona pointer do arquivo para o proximo dado a ser lido.
			size -= remainingBytes; //Tamanho a ser lido diminuido do que foi lido
		}

	}
	free(bufferIndexBlock);
	free(bufferDataBlock);
	openedFiles[handle] = archiveToRead;	//Atualiza o handle
	return nBytesRead;
		
}

int updateDirRecord(HANDLER toBeUpdated){
	bool recordFound = false;
	BYTE *bufferDataBlock = malloc(SECTOR_SIZE * partInfo.blockSize);
	BYTE *bufferIndexBlock = malloc(SECTOR_SIZE * partInfo.blockSize);
	DWORD acessedDirIndexPtr = toBeUpdated.dirIndexPtr;
	BLOCK_POINTER dataBlockPointer, indexBlockPointer;
	int indexIterator, blockIterator;

	DIR_RECORD fetchedRecord;

	int numberOfDirRecords = (partInfo.blockSize * SECTOR_SIZE) / sizeof(DIR_RECORD);

	while(!recordFound){
		//Fetch bloco de indice
		getIndexBlockByPointer(bufferIndexBlock,acessedDirIndexPtr);
		//Percorre os ponteiros simples de bloco
		for(indexIterator = 0; indexIterator < partInfo.numberOfPointers - 1; indexIterator++){
			dataBlockPointer = bufferToBLOCK_POINTER(bufferIndexBlock,indexIterator * sizeof(BLOCK_POINTER));
			if(dataBlockPointer.valid == RECORD_REGULAR){
				//Fetch bloco de dados
				getDataBlockByPointer(bufferDataBlock,dataBlockPointer.blockPointer);
				for(blockIterator = 0; blockIterator < numberOfDirRecords; blockIterator++){
					 fetchedRecord = bufferToDIR_RECORD(bufferDataBlock,blockIterator * sizeof(DIR_RECORD));
					 //Caso as strings sejam iguais, achou o record, logo atualiza ele na posição achada
					 if(strcmp(fetchedRecord.name,toBeUpdated.record.name) == 0){
						 insertDirEntryAt(bufferDataBlock,toBeUpdated.record,blockIterator);
						 writeDataBlockAt(dataBlockPointer.blockPointer,bufferDataBlock);
						 free(bufferDataBlock);
						 free(bufferIndexBlock);
						 return SUCCESS;
					 }
				}
			}
		}
		//Pega ponteiro para proximo bloco de indice
		indexBlockPointer = bufferToBLOCK_POINTER(bufferIndexBlock,sizeof(BLOCK_POINTER) * indexIterator);
		if(indexBlockPointer.valid == INVALID_BLOCK_PTR){
			free(bufferDataBlock);
			free(bufferIndexBlock);
			return ERROR;
		}
		acessedDirIndexPtr = indexBlockPointer.blockPointer;
	}
}

int createNavigationReferences(DWORD createdDirIndexPtr, DWORD parentDirIndexPtr){
	BYTE *bufferDataBlock = malloc(SECTOR_SIZE * partInfo.blockSize);
	BYTE *bufferIndexBlock = malloc(SECTOR_SIZE * partInfo.blockSize);
	BLOCK_POINTER newDataBlockPtr, fetchedPtr;
	DIR_RECORD parentRecord;
	DIR_RECORD currentRecord;

	//Criação do Record Pai
	strcpy(parentRecord.name, "..\0");
	parentRecord.type = RECORD_DIR;
	parentRecord.indexAddress = parentDirIndexPtr;
	parentRecord.byteFileSize = 0;

	//Criação do Record Corrente
	strcpy(currentRecord.name, ".\0");
	currentRecord.type = RECORD_DIR;
	currentRecord.indexAddress = createdDirIndexPtr;
	currentRecord.byteFileSize = 0;
	
	//Fetch do bloco de indice do diretorio criado
	getIndexBlockByPointer(bufferIndexBlock,createdDirIndexPtr);
	fetchedPtr = bufferToBLOCK_POINTER(bufferIndexBlock, 0);
	if(fetchedPtr.valid == RECORD_REGULAR){ //Caso já haja um bloco alocado
		//Fetch bloco e inserção dos dirRecords + escrita no disco
		getDataBlockByPointer(bufferDataBlock, fetchedPtr.blockPointer);
		insertDirEntryAt(bufferDataBlock,parentRecord,0);
		insertDirEntryAt(bufferDataBlock,currentRecord,1);
		writeDataBlockAt(fetchedPtr.blockPointer,bufferDataBlock);
		
		free(bufferDataBlock);
		free(bufferIndexBlock);
		return SUCCESS;

	}else{
		newDataBlockPtr.blockPointer = getFreeDataBlock();
		if(newDataBlockPtr.blockPointer < 0){ //Não há bloco de dados disponível
			free(bufferDataBlock);
			free(bufferIndexBlock);
			return ERROR;
		}
		else{//newDataBlockPtr.blockpointer >= 0 (válido)
			//Torna o ponteiro válido
			newDataBlockPtr.valid = RECORD_REGULAR;
			
			//Fetch bloco de dados e escrita dos dir records + escrita em disco
			getDataBlockByPointer(bufferDataBlock,newDataBlockPtr.blockPointer);
			insertDirEntryAt(bufferDataBlock,parentRecord,0);
			insertDirEntryAt(bufferDataBlock,currentRecord,1);
			writeDataBlockAt(newDataBlockPtr.blockPointer,bufferDataBlock);
			
			//Faz encadeamento com novo bloco alocado e escreve alterações no disco
			insertBlockPointerAt(bufferIndexBlock,newDataBlockPtr, 0);
			writeIndexBlockAt(createdDirIndexPtr,bufferIndexBlock);

			free(bufferDataBlock);
			free(bufferIndexBlock);
			return SUCCESS;
		}
	}
}

int deleteRecordInParentDir(HANDLER toDelete){
	bool recordFound = false;
	BYTE *bufferDataBlock = malloc(SECTOR_SIZE * partInfo.blockSize);
	BYTE *bufferIndexBlock = malloc(SECTOR_SIZE * partInfo.blockSize);
	DWORD acessedDirIndexPtr = toDelete.dirIndexPtr;
	BLOCK_POINTER dataBlockPointer, indexBlockPointer;
	int indexIterator, blockIterator;

	DIR_RECORD fetchedRecord;

	int numberOfDirRecords = (partInfo.blockSize * SECTOR_SIZE) / sizeof(DIR_RECORD);

	while(!recordFound){
		//Fetch bloco de indice
		getIndexBlockByPointer(bufferIndexBlock,acessedDirIndexPtr);
		//Percorre os ponteiros simples de bloco
		for(indexIterator = 0; indexIterator < partInfo.numberOfPointers - 1; indexIterator++){
			dataBlockPointer = bufferToBLOCK_POINTER(bufferIndexBlock,indexIterator * sizeof(BLOCK_POINTER));
			if(dataBlockPointer.valid == RECORD_REGULAR){
				//Fetch bloco de dados
				getDataBlockByPointer(bufferDataBlock,dataBlockPointer.blockPointer);
				for(blockIterator = 0; blockIterator < numberOfDirRecords; blockIterator++){
					 fetchedRecord = bufferToDIR_RECORD(bufferDataBlock,blockIterator * sizeof(DIR_RECORD));
					 //Caso as strings sejam iguais, achou o record, logo atualiza ele na posição achada
					 if(strcmp(fetchedRecord.name,toDelete.record.name) == 0){
						 toDelete.record.type = RECORD_INVALID;
						 toDelete.record.byteFileSize = 0;
						 strcpy(toDelete.record.name, "\0");
						 toDelete.record.indexAddress = 0;
						 insertDirEntryAt(bufferDataBlock,toDelete.record,blockIterator);
						 writeDataBlockAt(dataBlockPointer.blockPointer,bufferDataBlock);
						 free(bufferDataBlock);
						 free(bufferIndexBlock);
						 return SUCCESS;
					 }
				}
			}
		}
		//Pega ponteiro para proximo bloco de indice
		indexBlockPointer = bufferToBLOCK_POINTER(bufferIndexBlock,sizeof(BLOCK_POINTER) * indexIterator);
		if(indexBlockPointer.valid == INVALID_BLOCK_PTR){
			free(bufferDataBlock);
			free(bufferIndexBlock);
			return ERROR;
		}
		acessedDirIndexPtr = indexBlockPointer.blockPointer;
	}
}

int deleteFile(HANDLER toDelete){
	int indexIterator;
	BYTE *bufferIndexBlock = malloc(SECTOR_SIZE * partInfo.blockSize);
	BLOCK_POINTER fetchedDataBlockPtr, nextIndexBlockPtr;
	DWORD currentIndexBlockPtr;
	bool finishedToDelete = false;
	
	currentIndexBlockPtr = toDelete.record.indexAddress;
	while (!finishedToDelete){	
		getIndexBlockByPointer(bufferIndexBlock,currentIndexBlockPtr);

		//Percorre os ponteiros para blocos de dados desalocando-os
		for(indexIterator = 0; indexIterator < partInfo.numberOfPointers - 1; indexIterator++){
			fetchedDataBlockPtr = bufferToBLOCK_POINTER(bufferIndexBlock,indexIterator * sizeof(BLOCK_POINTER));
			if(fetchedDataBlockPtr.valid == RECORD_REGULAR){
				freeDataBlock(fetchedDataBlockPtr.blockPointer);
				cleanDataBlock(fetchedDataBlockPtr.blockPointer);
			}
		}

		nextIndexBlockPtr = bufferToBLOCK_POINTER(bufferIndexBlock, indexIterator * sizeof(BLOCK_POINTER));
		if(nextIndexBlockPtr.valid == INVALID_BLOCK_PTR){//Se ptr pra proximo indice é invalido, acaba
			freeIndexBlock(currentIndexBlockPtr);
			cleanIndexBlock(currentIndexBlockPtr);
			finishedToDelete = true;
		}
		else{
			freeIndexBlock(currentIndexBlockPtr);
			cleanIndexBlock(currentIndexBlockPtr);
			currentIndexBlockPtr = nextIndexBlockPtr.blockPointer;
		}	
	}
	free(bufferIndexBlock);
	return deleteRecordInParentDir(toDelete);
}



/********************************************************************************/
/************************************ PUBLIC ************************************/
/********************************************************************************/

/*-----------------------------------------------------------------------------
Função:	Informa a identificação dos desenvolvedores do T2FS.
-----------------------------------------------------------------------------*/
int identify2 (char *name, int size) {
	char *brosNames = (char*) malloc(sizeof(char) * size);
	strcpy(brosNames, "Alencar da Costa\t\t00288544\nMatheus Woeffel Camargo\t\t00288543\nRaphael Scherpinski Brandao\t00112233\n");	
	if (strlen(brosNames) > size)
		return ERROR;
	name = brosNames;
	return SUCCESS;
}

/*-----------------------------------------------------------------------------
Função:	Formata logicamente o disco virtual t2fs_disk.dat para o sistema de
		arquivos T2FS definido usando blocos de dados de tamanho 
		corresponde a um múltiplo de setores dados por sectors_per_block.
-----------------------------------------------------------------------------*/
int format2 (int sectors_per_block) {
	BYTE buffer[SECTOR_SIZE] = {0};

	if (readPartInfoSectors(&partInfo) == true) {
		cleanDisk();
		
		// Escreve o superbloco 
		SUPERBLOCK superblock = createSuperblock(sectors_per_block);
		memcpy(buffer, &superblock, sizeof(SUPERBLOCK)); // so terá sentido se for usado mesmo endian na estrutura e no array
		_printSuperblock(superblock);

		if (write_sector(partInfo.firstSectorAddress, buffer) != 0)
			return ERROR;
		
		// Escreve o bitmap de indices e dados
		int i;
		for(i = 0; i < SECTOR_SIZE ; i++) { // Bitmap com todos bits em 1;
			buffer[i] = 0xFF;
		} 
		for (i = 0; i < superblock.bitmapSectorsSize; i++)
			if (write_sector(partInfo.firstSectorAddress + 1 + i, buffer) != 0)
				return ERROR;
		


		// Definindo ultimas informacoes sobre a particao
		partInfo.indexBlocksStart = partInfo.firstSectorAddress + superblock.bitmapSectorsSize + 1; // Inicio + bitmap + superbloco
		partInfo.dataBlocksStart = partInfo.indexBlocksStart + (superblock.indexBlockAreaSize * superblock.blockSize);
		partInfo.numberOfPointers = superblock.numberOfPointers;
		partInfo.blockSize = superblock.blockSize;
		_printPartInfo();
		
		createRootDir();
		//WARNING: Shouldn't we use init instead?
		initDirHandles();
		initFileHandles();	
		initialized = true;
		//END OF WARNING
		return SUCCESS;
		
	}
	return ERROR;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para criar um novo arquivo no disco e abrí-lo,
		sendo, nesse último aspecto, equivalente a função open2.
		No entanto, diferentemente da open2, se filename referenciar um 
		arquivo já existente, o mesmo terá seu conteúdo removido e 
		assumirá um tamanho de zero bytes.
-----------------------------------------------------------------------------*/
FILE2 create2 (char *filename) {
	if (initialized == false && init() == false)
		return ERROR;

	DIR_RECORD record;
	int recordByPath = getRecordByPath(&record, filename);
	if (recordByPath == PATH_INCORRECT) {
		printf("ERRO: O caminho passado nao existe.\n");
		return ERROR;
	}

	if (recordByPath == SUCCESS) {
		//printf("%s %d %d\n", record.name, record.type, record.indexAddress);
		if (record.type == RECORD_REGULAR) // Encontrou um arquivo com o mesmo nome, precisa ser deletado
			delete2(filename);
		else {
			printf("ERRO: Ha um diretorio/link com esse mesmo nome, por favor escolha outro nome.\n");
			return ERROR;
		}
	}

	// Criar o arquivo propriamente dito
	int size;
	char **pathNames = splitPath(filename, &size);
	DWORD dirIndexBlock;
	char *lastSlash = strrchr(filename, '/');
	if (lastSlash == filename) // A ultima / esta na primeira posicao, ou seja eh a unica. Caminho absoluto para raiz
		dirIndexBlock = rootDirIndex;
	else if (lastSlash == NULL) // Nao ha / no nome entao o arquivo sera criado no diretorio corrente
		dirIndexBlock = currentDirIndexPointer;
	else { // O arquivo esta posicionado em algum local por ai
		// Remover nome do arquivo do path e buscar o index block do local
		int slashIndex = lastSlash - filename;
		char *filePath = malloc((slashIndex + 1) * sizeof(char));
		memcpy(filePath, filename, slashIndex);
		filePath[slashIndex] = '\0';
		getRecordByPath(&record, filePath);
		dirIndexBlock = record.indexAddress;
		free(filePath);
	}
	
	record = createRecord(pathNames[size-1], RECORD_REGULAR, dirIndexBlock);
	FILE2 handle = getFreeFileHandle();
	if (handle == ERROR) {
		printf("ATENCAO: o arquivo foi criado, mas o limite de arquivos abertos foi atingido.\nFeche um arquivo para poder abrir.\n");
		return ERROR;
	}

	openedFiles[handle].free = false;
	openedFiles[handle].path = filename;
	openedFiles[handle].record = record;
	openedFiles[handle].pointer = 0;
	openedFiles[handle].dirIndexPtr = dirIndexBlock;
	return handle;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para remover (apagar) um arquivo do disco. 
-----------------------------------------------------------------------------*/
int delete2 (char *filename) {
	if (initialized == false && init() == false)
		return ERROR;

	DIR_RECORD dirRecord;
	HANDLER handler;
	// Busca o registro do arquivos seguindo pelo seu path
	if (getRecordByPath(&dirRecord, filename) == SUCCESS) {
		if (dirRecord.type == RECORD_LINK || dirRecord.type == RECORD_REGULAR) {
			handler.free = false;
			handler.path = filename;
			handler.record = dirRecord;
			handler.pointer = 0;

			// Remover nome do arquivo do path e buscar o index block do local
			char *lastSlash = strrchr(filename, '/');
			if (lastSlash == NULL) {
				// arquivo no current dir
				handler.dirIndexPtr = currentDirIndexPointer;
			} else {
				int slashIndex = lastSlash - filename;
				char *filePath = malloc((slashIndex + 1) * sizeof(char));
				memcpy(filePath, filename, slashIndex);
				filePath[slashIndex] = '\0';
				getRecordByPath(&dirRecord, filePath);
				free(filePath);

				handler.dirIndexPtr = dirRecord.indexAddress;
				

				
			}
			//Tudo que precisamos, bora deletar
			return (deleteFile(handler));
		}
	}
	printf("ERRO: Nao foi possivel deletar o arquivo: \"%s\"\n", filename);
	return ERROR;
}

/*-----------------------------------------------------------------------------
Função:	Função que abre um arquivo existente no disco.
-----------------------------------------------------------------------------*/
//To do: correção para incluir no handle do arquivo aberto o parentDir
FILE2 open2 (char *filename) {
	if (initialized == false && init() == false)
		return ERROR;

	FILE2 handle = getFreeFileHandle();
	if (handle != -1 && strlen(filename) > 0) {
		DIR_RECORD dirRecord;
		// Busca o registro do arquivos seguindo pelo seu path
		if (getRecordByPath(&dirRecord, filename) == SUCCESS) {
			// checar se o arquivo é FILE ou LINK
			if (dirRecord.type == RECORD_LINK || dirRecord.type == RECORD_REGULAR) {
				openedFiles[handle].free = false;
				openedFiles[handle].path = filename;
				openedFiles[handle].record = dirRecord;
				openedFiles[handle].pointer = 0;
				// Se link, repetir abertura pro arquivo real
				if (dirRecord.type == RECORD_LINK) { //Buscar arquivo real
					char realFilePath[MAX_FILE_NAME_SIZE+ 1];
					if (readFile(handle, (BYTE*)realFilePath, MAX_FILE_NAME_SIZE + 1) > 0) {
						if (getRecordByPath(&dirRecord, realFilePath) == SUCCESS && dirRecord.type == RECORD_REGULAR) {
							openedFiles[handle].path = realFilePath;
							openedFiles[handle].record = dirRecord;
						} else {
							printf("ERRO: Nao foi possivel encontrar o arquivo referenciado.");
							openedFiles[handle].free = true;
							return ERROR;
						}
					} else {
						printf("ERRO: Nao possivel ler o endereco real\n");
						openedFiles[handle].free = true;
						return ERROR;
					}
				}
				// Remover nome do arquivo do path e buscar o index block do local
				char *lastSlash = strrchr(openedFiles[handle].path, '/');
				if (lastSlash == NULL) {
					// arquivo no current dir
					openedFiles[handle].dirIndexPtr = currentDirIndexPointer;
				} else {
					int slashIndex = lastSlash - openedFiles[handle].path;
					char *filePath = malloc((slashIndex + 1) * sizeof(char));
					memcpy(filePath, openedFiles[handle].path, slashIndex);
					filePath[slashIndex] = '\0';
					getRecordByPath(&dirRecord, filePath);
					free(filePath);

					openedFiles[handle].dirIndexPtr = dirRecord.indexAddress;
				}
				
				return handle;
			}
		}

	}

	printf("ERRO: O arquivo nao foi encontrado!\n");
	return ERROR;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para fechar um arquivo.
-----------------------------------------------------------------------------*/
int close2 (FILE2 handle) {
	if (initialized == false && init() == false)
		return ERROR;
	
	if (handle >= MAX_FILE_OPEN || handle < 0)
		return ERROR;

	openedFiles[handle].free = true;
	return SUCCESS;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para realizar a leitura de uma certa quantidade
		de bytes (size) de um arquivo.
-----------------------------------------------------------------------------*/
int read2 (FILE2 handle, char *buffer, int size) {
	if (initialized == false && init() == false)
		return ERROR;

	if (isFileOpened(handle)) 
		return readFile(handle, (BYTE *)buffer, size);
	return ERROR;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para realizar a escrita de uma certa quantidade
		de bytes (size) de  um arquivo.
-----------------------------------------------------------------------------*/
int write2 (FILE2 handle, char *buffer, int size) {
	DWORD sizeWritten;
	if (initialized == false && init() == false)
		return ERROR;

	if(isFileOpened(handle)){
		sizeWritten = writeFile(handle,(BYTE*)buffer,size);
		//Confirma alteração do tamanho do record no diretorio
		updateDirRecord(openedFiles[handle]);
		return sizeWritten;
	}
	
	return ERROR;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para truncar um arquivo. Remove do arquivo 
		todos os bytes a partir da posição atual do contador de posição
		(current pointer), inclusive, até o seu final.
-----------------------------------------------------------------------------*/
int truncate2 (FILE2 handle) {
	if (initialized == false && init() == false)
		return ERROR;

	// Remove o conteudo dentro do bloco de dados atual
	BYTE *bufferData = malloc(SECTOR_SIZE * partInfo.blockSize);
	getDataBlockOfByte(openedFiles[handle].pointer, openedFiles[handle].record.indexAddress, bufferData);
	

	// Remove os blocos de dados na sequencia, desfazendo o encadeamento

	return ERROR;
}

/*-----------------------------------------------------------------------------
Função:	Altera o contador de posição (current pointer) do arquivo.
-----------------------------------------------------------------------------*/
int seek2 (FILE2 handle, DWORD offset) {
	if (initialized == false && init() == false)
		return ERROR;

	if(handle >= MAX_FILE_OPEN || handle < 0)
		return ERROR;

	if(openedFiles[handle].free == true)
		return ERROR;
	
	if (offset == -1) 
		openedFiles[handle].pointer = openedFiles[handle].record.byteFileSize; // Posiciona ponteiro um byte apos o ultimo byte do arquivo
	else if (openedFiles[handle].record.byteFileSize > offset)
		openedFiles[handle].pointer = offset;
	else
		return ERROR;

	return SUCCESS;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para criar um novo diretório.
-----------------------------------------------------------------------------*/
int mkdir2 (char *pathname) {
	if (initialized == false && init() == false)
		return ERROR;

	DIR_RECORD record;
	int recordByPath = getRecordByPath(&record, pathname);
	if (recordByPath != FILE_NOT_EXIST) {
		if (recordByPath == PATH_INCORRECT)
			printf("ERRO: O caminho passado nao existe.\n");
		else if (recordByPath == SUCCESS)// Ha um arquivo com o mesmo nome nesse path
			printf("ERRRO: Ja existe um arquivo/diretorio/link com esse mesmo nome no caminho fornecido.\n");

		return ERROR;
	
	} else { // Criar o arquivo propriamente dito
		int size;
		char **pathNames = splitPath(pathname, &size);
		DWORD dirIndexBlock;
		char *lastSlash = strrchr(pathname, '/');

		if (lastSlash == pathname) // A ultima / esta na primeira posicao, ou seja eh a unica. Caminho absoluto para raiz
			dirIndexBlock = rootDirIndex;
		else if (lastSlash == NULL) // Nao ha / no nome entao o arquivo sera criado no diretorio corrente
			dirIndexBlock = currentDirIndexPointer;
		else { // O arquivo esta posicionado em algum local por ai
			// Remover nome do arquivo do path e buscar o index block do local
			int slashIndex = lastSlash - pathname;
			char *filePath = malloc((slashIndex + 1) * sizeof(char));
			memcpy(filePath, pathname, slashIndex);
			filePath[slashIndex] = '\0';

			getRecordByPath(&record, filePath);
			dirIndexBlock = record.indexAddress;
			free(filePath);
		}
		
		record = createRecord(pathNames[size-1], RECORD_DIR, dirIndexBlock);
		//Cria as referências . e ..
		createNavigationReferences(record.indexAddress,dirIndexBlock);

		return SUCCESS;
	}
}

/*-----------------------------------------------------------------------------
Função:	Função usada para remover (apagar) um diretório do disco.
-----------------------------------------------------------------------------*/
int rmdir2 (char *pathname) {
	if (initialized == false && init() == false)
		return ERROR;

	DIR_RECORD dirRecord;
	HANDLER handler;
	// Busca o registro do arquivos seguindo pelo seu path
	if (getRecordByPath(&dirRecord, pathname) == SUCCESS) {
		if (dirRecord.type == RECORD_DIR) {
			handler.free = false;
			handler.path = pathname;
			handler.record = dirRecord;
			handler.pointer = 0;

			// Remover nome do arquivo do path e buscar o index block do local
			char *lastSlash = strrchr(pathname, '/');
			if (lastSlash == NULL) {
				// arquivo no current dir
				handler.dirIndexPtr = currentDirIndexPointer;
			} else {
				int slashIndex = lastSlash - pathname;
				char *filePath = malloc((slashIndex + 1) * sizeof(char));
				memcpy(filePath, pathname, slashIndex);
				filePath[slashIndex] = '\0';
				getRecordByPath(&dirRecord, filePath);
				free(filePath);

				handler.dirIndexPtr = dirRecord.indexAddress;
			}
			//Verificar se ha regisitros validos
			DWORD recordPointer = getNextDirRecordValid(&dirRecord, handler.record.indexAddress, 2);
			printf("%d %d\n", recordPointer, dirRecord.type);
			if (dirRecord.type == RECORD_INVALID) // A partir do 2 por causa do . e ..
				return (deleteFile(handler));
		}
	}
	printf("ERRO: Nao foi possivel deletar o arquivo: \"%s\"\n", pathname);
	return ERROR;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para alterar o CP (current path)
-----------------------------------------------------------------------------*/
int chdir2 (char *pathname) {
	if (initialized == false && init() == false)
		return ERROR;
	
	DIR_RECORD record;
	int recordByPath = getRecordByPath(&record, pathname);
	if (recordByPath == SUCCESS) {
		if (record.type == RECORD_DIR) {
			strcpy(currentPath, pathname);
			currentDirIndexPointer = record.indexAddress;
			return SUCCESS;
		}
	}
	printf("ERRO: O caminho especificado nao foi encontrado.\n");
	return ERROR;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para obter o caminho do diretório corrente.
-----------------------------------------------------------------------------*/
int getcwd2 (char *pathname, int size) {
	if (initialized == false && init() == false)
		return ERROR;

	if (strlen(currentPath) <= size) {
		strcpy(pathname, currentPath);
		return SUCCESS;
	}

	return ERROR;
}

/*-----------------------------------------------------------------------------
Função:	Função que abre um diretório existente no disco.
-----------------------------------------------------------------------------*/
DIR2 opendir2 (char *pathname) {
	if (initialized == false && init() == false)
		return ERROR;

	DIR2 handle = getFreeDirHandle();
	if (handle != -1 && strlen(pathname) > 0) {
		DIR_RECORD dirRecord;
		DIR_RECORD buffer;
		// Busca o registro do diretorio seguindo pelo seu path
		if (getRecordByPath(&dirRecord, pathname) == SUCCESS) {
			// checar se o arquivo é DIR
			if (dirRecord.type == RECORD_DIR) {
				openedDirs[handle].free = false;
				openedDirs[handle].path = pathname;
				openedDirs[handle].record = dirRecord;
				openedDirs[handle].pointer = getNextDirRecordValid(&buffer, dirRecord.indexAddress, 0);
				return handle;
			}
		}

	}

	printf("ERRO: O diretorio nao foi encontrado!\n");
	return ERROR;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para ler as entradas de um diretório.
-----------------------------------------------------------------------------*/
int readdir2 (DIR2 handle, DIRENT2 *dentry) {
	if (initialized == false && init() == false)
		return ERROR;

	if (isDirOpened(handle) == true) {
		DIR_RECORD record;
		DWORD newPointer = getNextDirRecordValid(&record, openedDirs[handle].record.indexAddress, openedDirs[handle].pointer);
		openedDirs[handle].pointer = newPointer + 1;
		if (newPointer == ERROR) {
			dentry = NULL;
			return ERROR;
		}
		strcpy((*dentry).name, record.name);
		(*dentry).fileSize = record.byteFileSize;
		if (record.type == RECORD_DIR)
			(*dentry).fileType = RECORD_DIR;
		else if (record.type == RECORD_LINK || record.type == RECORD_REGULAR)
			(*dentry).fileType = RECORD_REGULAR;
		else 
			return ERROR;
		
		return SUCCESS;
	}

	return ERROR;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para fechar um diretório.
-----------------------------------------------------------------------------*/
int closedir2 (DIR2 handle) {
	if (initialized == false && init() == false)
		return ERROR;

	if (handle >= MAX_FILE_OPEN || handle < 0)
		return ERROR;

	openedDirs[handle].free = true;
	return SUCCESS;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para criar um caminho alternativo (softlink) com
		o nome dado por linkname (relativo ou absoluto) para um 
		arquivo ou diretório fornecido por filename.
-----------------------------------------------------------------------------*/
int ln2 (char *linkname, char *filename) {
	if (initialized == false && init() == false)
		return ERROR;

	DIR_RECORD recordFile, recordLink;
	int recordByPathLink = getRecordByPath(&recordLink, linkname);
	int recordByPathFile = getRecordByPath(&recordFile, filename);
	// Verifica se o arquivo para o qual queremos o link existe
	if (recordByPathFile == PATH_INCORRECT || recordByPathFile == FILE_NOT_EXIST) {
		printf("ERRO: O caminho \"%s\" nao existe.\n", filename);
		return ERROR;
	} else if (recordByPathFile == SUCCESS && recordFile.type == RECORD_DIR) {
		printf("ERRRO: Nao eh possivel criar links para diretorios.\n");
		return ERROR;
	}
	// Verificacao do caminho do link
	if (recordByPathLink != FILE_NOT_EXIST) {
		if (recordByPathLink == PATH_INCORRECT)
			printf("ERRO: O caminho \"%s\" nao existe.\n", linkname);
		else if (recordByPathLink == SUCCESS)// Ha um arquivo com o mesmo nome nesse path
			printf("ERRRO: Ja existe um arquivo/diretorio/link com esse mesmo nome no caminho fornecido.\n");

		return ERROR;
	}

	// Tudo pronto para iniciar a criacao do link
	int size;
	char **pathLink = splitPath(linkname, &size);
	DWORD dirIndexBlock;
	char *lastSlash = strrchr(linkname, '/');
	if (lastSlash == linkname) // A ultima / esta na primeira posicao, ou seja eh a unica. Caminho absoluto para raiz
		dirIndexBlock = rootDirIndex;
	else if (lastSlash == NULL) // Nao ha / no nome entao o arquivo sera criado no diretorio corrente
		dirIndexBlock = currentDirIndexPointer;
	else { // O arquivo esta posicionado em algum local por ai
		// Remover nome do arquivo do path e buscar o index block do local
		int slashIndex = lastSlash - linkname;
		char *linkPath = malloc((slashIndex + 1) * sizeof(char));
		memcpy(linkPath, linkname, slashIndex);
		linkPath[slashIndex] = '\0';
		getRecordByPath(&recordLink, linkPath);
		dirIndexBlock = recordLink.indexAddress;
		free(linkPath);
	}
	// Criacao do link no disco
	recordLink = createRecord(pathLink[size-1], RECORD_LINK, dirIndexBlock);
	// Link criado, agora temos que escrever o filename no arquivo
	FILE2 handle = getFreeFileHandle();
	if (handle >= 0 && handle < MAX_FILE_OPEN) {
		openedFiles[handle].dirIndexPtr = dirIndexBlock;
		openedFiles[handle].free = false;
		openedFiles[handle].path = linkname;
		openedFiles[handle].record = recordLink;
		openedFiles[handle].pointer = 0;

		writeFile(handle, (BYTE*)filename, strlen(filename));
		updateDirRecord(openedFiles[handle]);
		openedFiles[handle].free = false;
		return SUCCESS;
	}
	printf("ERRO: Nao foi possivel terminar a criacao do link. Link incompleto\n");
	return ERROR;
}


