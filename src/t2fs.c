
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

char *currentPath;	// Caminho corrente do sistema de arquivos
DWORD currentDirIndexPointer = -1; // Ponteiro de bloco de indice de diretorio associado ao caminho corrente
DWORD rootDirIndex = -1; // Numero/endereco do Bloco de indice da raiz

PART_INFO partInfo;	// Estrutura que armazenara enderecos e limites da particao
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
	setBitmap(BITMAP_INDEX, offset, 0);

	return (partInfo.indexBlocksStart + offset);
}

DWORD getFreeDataBlock() {
	DWORD offset = searchBitmap(BITMAP_DATA, 1);
	setBitmap(BITMAP_DATA, offset, 0);
	
	return (partInfo.dataBlocksStart + offset);
}

bool readPartInfoSectors() {
	BYTE bufferSector[SECTOR_SIZE];

	if (read_sector(0, bufferSector) == 0) {
		partInfo.firstSectorAddress = bufferToDWORD(bufferSector, INIT_BYTE_PART_TABLE);
		partInfo.lastSectorAddress = bufferToDWORD(bufferSector, INIT_BYTE_PART_TABLE + sizeof(DWORD));
		return true;
	}

	return false;
}

bool readPartInfoBlocks() {
	BYTE sectorBuffer[SECTOR_SIZE];
	SUPERBLOCK superblock;

	if (read_sector(partInfo.firstSectorAddress, sectorBuffer) == 0) {
		memcpy(&superblock, sectorBuffer, sizeof(SUPERBLOCK));
		partInfo.indexBlocksStart = partInfo.firstSectorAddress + superblock.bitmapSectorsSize + 1; // Inicio + bitmap + superbloco
		partInfo.dataBlocksStart = partInfo.indexBlocksStart + (superblock.indexBlockAreaSize * superblock.blockSize);
		partInfo.numberOfPointers = superblock.numberOfPointers;
		
		return true;
	}
	
	return false;
}

bool createRootDir() {
	// Espera-se que essa funcao seja chamada logo apos uma formatacao
	// Dessa forma o bloco de indice alocado para raiz sera o primeiro
	if (rootDirIndex == -1) {
		DWORD freeIndexBlock = getFreeDataBlock(); // Espera-se que seja 0
		if (freeIndexBlock == 0) {
			DWORD freeDataBlock = getFreeDataBlock();
			
			//createRecord("..", RECORD_DIR);
			//createRecord(".", RECORD_DIR);

			return true;
		}
		printf("ERROR: O primeiro bloco de indice nao esta livre. Necesario formatar");
	}
	return false;
}

void cleanDisk() {
	int i;
	int size = partInfo.lastSectorAddress - partInfo.firstSectorAddress + 1;
	BYTE cleanBuffer[256] = {0}; 
	for (i = 0; i < size; i++)
		write_sector(partInfo.firstSectorAddress + i, cleanBuffer);
	
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

int getBlockByPointer(BYTE *block,DWORD pointer, DWORD offset, int blockSize){
	int i,j;
	BYTE sectorBuffer[SECTOR_SIZE]; 
	DWORD initialSector = offset + (pointer * (DWORD)blockSize);

	for(i = 0; i < blockSize;i++){
		//Faz leitura e verifica se houve erro ao ler, se sim, aborta e retorna cod erro.
		if (read_sector(initialSector + i,sectorBuffer) != 0){
			printf("Erro ao ler setor ao preencher bloco");
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

  // Conta ocorrencias
  while (name[i] != '\0') {
    if (name[i] == '/')
        n++;
    i++;
  }
  *size = n;

  i = 0;
  char **strings;
  strings = (char**)malloc(sizeof(char)*n);
  char *substring;
  char *nameCopy = strdup(name); // Necessaroi para strsep
  while( (substring = strsep(&nameCopy,"/")) != NULL ) {
    strings[i] = (char*) malloc(sizeof(char)*FILE_NAME_SIZE);
    strcpy(strings[i], substring);
  }

  return strings;
}

int getRecordByName(DIR_RECORD *record, DWORD indexPointer, char *name) {
	BYTE indexBlock[SECTOR_SIZE * partInfo.blockSize];
	BYTE dataBlock[SECTOR_SIZE * partInfo.blockSize];
	BLOCK_POINTER bufferPointer;
	DIR_RECORD bufferRecord;

	bool fileFound = false;
	while (!fileFound) {
		getIndexBlockByPointer(indexBlock, indexPointer);
		int indexIterator;
		// Busca entradas do diretorio
		for (indexIterator = 0; indexIterator < partInfo.numberOfPointers-1; indexPointer++) {
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
	for (i = 0; i < size; i++) {
		if (getRecordByName(&buffRecord, indexPointer, dirNames[i]) == SUCCESS)
			indexPointer = buffRecord.indexAddress;
		else
			return ERROR;
	}
	// Encontrou o record que precisava
	*record = buffRecord;
	return SUCCESS;
}

DWORD getNextDirRecordValid(DWORD indexPointer, DWORD recordPointer) {
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
	bufferPointer = bufferToBLOCK_POINTER(indexBlock, dataBlockPointer * sizeof(bufferPointer));
	if (bufferPointer.blockPointer == INVALID_BLOCK_PTR)
		return ERROR;

	bool dirRecordNotFound = true;
	int recordIterator;
	// Buscar pelos blocos de indice encadeados atras do registro valido
	do {
		// Pega o bloco de dados da busca atual e procura pelo registro valido dentro
		getDataBlockByPointer(dataBlock, bufferPointer.blockPointer);
		for (recordIterator = recordPointer; recordIterator < numberOfRecordsPerDataBlock; recordIterator++) {
			bufferRecord = bufferToDIR_RECORD(dataBlock, recordIterator * sizeof(bufferRecord));
			if (bufferRecord.type == RECORD_DIR) {
				// Achou o registro valido, calcula o seu numero equivalente
				return (recordIterator + numberOfRecordsPerDataBlock * (dataBlockNumber-1));
			}
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

bool initPartInfo() {
	return (readPartInfoSectors() == true && readPartInfoBlocks() == true);
}

bool initRootDir() {
	if (getBitmap(BITMAP_INDEX, 0)) {
		currentPath = "/";
		currentDirIndexPointer = 0;
		rootDirIndex = 0;
		return true;
	}

	return false;
}

bool init() {
	if (initPartInfo() == true && initRootDir() == true) {
		int i;
		for (i = 0; i < MAX_FILE_OPEN; i++) {
			openedFiles[i].free = true;
			//... WARNING deve inicializar os outros campos tambem?
		}

		currentPath = "/";
		

		initialized = true;
	}

	return false;
}


DIR_RECORD createRecord(char *filename, int type) {
	DWORD indexBlockPointer = currentDirIndexPointer;
	int blockSizeInBytes = SECTOR_SIZE * partInfo.blockSize *sizeof(BYTE);
	int indexIterator, i, dirIterator;
	BLOCK_POINTER extractedPtr;
	BLOCK_POINTER ptrToIndexBlock;
	bool findEmptyEntry = false;

	//Buffers para fetch de bloco de indice e bloco de dados
	BYTE *indexBlockBuffer = malloc(blockSizeInBytes);
	BYTE *dataBlockBuffer = malloc(blockSizeInBytes);

	//Record a ser registrado 
	DIR_RECORD newRecord;
	newRecord.byteFileSize = 0;
	strcpy(newRecord.name,filename);
	newRecord.type = type;
	//TODO:Alocar bloco de indices e bloco de dados consistentes pro arquivo
	
	//Fetch do bloco de indices do diretorio corrente
	getIndexBlockByPointer(indexBlockBuffer,indexBlockPointer);
	int numberOfDirRecords = partInfo.blockSize * SECTOR_SIZE / sizeof(DIR_RECORD);
	
	while(!findEmptyEntry){
		for(indexIterator = 0; indexIterator < partInfo.numberOfPointers - 1; indexIterator++){
			extractedPtr = bufferToBLOCK_POINTER(indexBlockBuffer,indexIterator * sizeof(BLOCK_POINTER));

			if((char)extractedPtr.valid == INVALID_BLOCK_PTR){
				//TODO:aloca bloco de dados
				//aponta para bloco de dados
				//escreve newRecord no inicio
				//retorna
			}
			else{
				getDataBlockByPointer(dataBlockBuffer,extractedPtr.blockPointer);
				DIR_RECORD fetchedEntry;
				//Percorre as entradas de diretório no bloco
				for (dirIterator = 0; dirIterator < numberOfDirRecords; dirIterator++){
					fetchedEntry = bufferToDIR_RECORD(dataBlockBuffer, dirIterator * sizeof(DIR_RECORD));
					if(fetchedEntry.type == RECORD_INVALID){
						//TODO:Escreve modificações no bloco
						//Escreve no disco
						//retorna
					}
				}
			}
		}
		BLOCK_POINTER ptrToIndexBlock = bufferToBLOCK_POINTER(indexBlockBuffer,(partInfo.numberOfPointers - 1)* sizeof(BLOCK_POINTER));
		if(ptrToIndexBlock.valid == INVALID_BLOCK_PTR){
			//Aloca bloco de indices
			//Faz bloco antigo apontar para o novo
			DWORD newPtrIndexBlock = 0; //alocado acima
			getIndexBlockByPointer(indexBlockBuffer,newPtrIndexBlock);
		}
		else
			getIndexBlockByPointer(indexBlockBuffer,ptrToIndexBlock.blockPointer);
		
	}
	return newRecord;
}

bool isOpened(FILE2 handle) {
	return (handle >= 0 && openedFiles[handle].free == false && handle < MAX_FILE_OPEN);
}

bool readIsWithinBoundary(HANDLER toCheck, int size){
	return (toCheck.pointer + size - 1 < toCheck.record.byteFileSize);
}


int readFile(FILE2 handle, BYTE *buffer, int size){
	bool readDone = false;
	BYTE *bufferDataBlock = malloc(sizeof(SECTOR_SIZE * partInfo.blockSize));
	BYTE *bufferIndexBlock = malloc(sizeof(SECTOR_SIZE * partInfo.blockSize));
	HANDLER archiveToRead;
	DWORD logicBlockToRead;
	DWORD bufferPointer = 0;
	DWORD offsetInBlock;
	DWORD nBytesRead = 0;
	int i;

	if(isOpened(handle)){
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
			logicBlockToRead = archiveToRead.pointer / partInfo.blockSize * SECTOR_SIZE;
			//Determina o offset dentro do bloco logico acima
			offsetInBlock = archiveToRead.pointer % partInfo.blockSize * SECTOR_SIZE;
			//Calcula o nível de indice necessário
			DWORD indexLevelNeeded = logicBlockToRead / partInfo.numberOfPointers - 1;
			//Caso o nível de indice necessario para acessar seja maior que o nível acessado
			if(indexLevelNeeded > currentIndexLevel){
				//Itera sobre os níveis de indice até achar o correto
				while (currentIndexLevel < indexLevelNeeded){
					currentIndexLevel++;
					DWORD nextIndex = bufferToBLOCK_POINTER(bufferIndexBlock, (partInfo.numberOfPointers - 1) * sizeof(BLOCK_POINTER)).blockPointer;
					getIndexBlockByPointer(bufferIndexBlock,nextIndex);
				}
			}
			//Tradução do bloco lógico para físico
			DWORD realBlockToRead = bufferToBLOCK_POINTER(bufferIndexBlock,sizeof(BLOCK_POINTER) *logicBlockToRead).blockPointer;
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
	else{
		free(bufferIndexBlock);
		free(bufferDataBlock);
		return ERROR;	
	}
}

/********************************************************************************/
/************************************ PUBLIC ************************************/
/********************************************************************************/

/*-----------------------------------------------------------------------------
Função:	Informa a identificação dos desenvolvedores do T2FS.
-----------------------------------------------------------------------------*/
int identify2 (char *name, int size) {
	return -1;
}

/*-----------------------------------------------------------------------------
Função:	Formata logicamente o disco virtual t2fs_disk.dat para o sistema de
		arquivos T2FS definido usando blocos de dados de tamanho 
		corresponde a um múltiplo de setores dados por sectors_per_block.
-----------------------------------------------------------------------------*/
int format2 (int sectors_per_block) {
	BYTE buffer[SECTOR_SIZE] = {0};

	if (readPartInfoSectors() == true) {
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
		_printPartInfo();
		
		createRootDir();
		
		initialized = true;
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

	return -1;//createRecord(filename, RECORD_REGULAR);
}

/*-----------------------------------------------------------------------------
Função:	Função usada para remover (apagar) um arquivo do disco. 
-----------------------------------------------------------------------------*/
int delete2 (char *filename) {
	if (initialized == false && init() == false)
		return ERROR;

	return ERROR;
}

/*-----------------------------------------------------------------------------
Função:	Função que abre um arquivo existente no disco.
-----------------------------------------------------------------------------*/
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

	if (isOpened(handle) == true) 
		return readFile(handle, buffer, size);
	return ERROR;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para realizar a escrita de uma certa quantidade
		de bytes (size) de  um arquivo.
-----------------------------------------------------------------------------*/
int write2 (FILE2 handle, char *buffer, int size) {
	if (initialized == false && init() == false)
		return ERROR;

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
	
	openedFiles[handle].pointer = offset;

	return ERROR;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para criar um novo diretório.
-----------------------------------------------------------------------------*/
int mkdir2 (char *pathname) {
	if (initialized == false && init() == false)
		return ERROR;

	return ERROR;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para remover (apagar) um diretório do disco.
-----------------------------------------------------------------------------*/
int rmdir2 (char *pathname) {
	if (initialized == false && init() == false)
		return ERROR;

	return ERROR;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para alterar o CP (current path)
-----------------------------------------------------------------------------*/
int chdir2 (char *pathname) {
	if (initialized == false && init() == false)
		return ERROR;
		

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
		// Busca o registro do diretorio seguindo pelo seu path
		if (getRecordByPath(&dirRecord, pathname) == SUCCESS) {
			// checar se o arquivo é DIR
			if (dirRecord.type == RECORD_DIR) {
				openedFiles[handle].free = false;
				openedFiles[handle].path = pathname;
				openedFiles[handle].record = dirRecord;
				openedFiles[handle].pointer = getNextDirRecordValid(dirRecord.indexAddress, 0);

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

	return ERROR;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para fechar um diretório.
-----------------------------------------------------------------------------*/
int closedir2 (DIR2 handle) {
	if (initialized == false && init() == false)
		return ERROR;

	return ERROR;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para criar um caminho alternativo (softlink) com
		o nome dado por linkname (relativo ou absoluto) para um 
		arquivo ou diretório fornecido por filename.
-----------------------------------------------------------------------------*/
int ln2 (char *linkname, char *filename) {
	if (initialized == false && init() == false)
		return ERROR;

	return ERROR;
}


