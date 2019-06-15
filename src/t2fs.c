
/**
*/
#include <math.h>
#include <stdio.h>
#include <string.h>
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
	BYTE buffer[sizeof(DWORD)];
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

FILE2 getFreeHandle() {
	int i;
	for (i = 0; i < MAX_FILE_OPEN; i++) {
		if (openedFiles[i].free == true)
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
	BYTE *indexBlock[SECTOR_SIZE * partInfo.blockSize];
	BYTE *dataBlock[SECTOR_SIZE * partInfo.blockSize];
	BLOCK_POINTER bufferPointer;
	DIR_RECORD bufferRecord;

	bool fileFound = false;
	while (!fileFound) {
		getIndexBlockByPointer(indexBlock, indexPointer);
		int indexIterator;
		// Busca entradas do diretorio
		for (indexIterator = 0; indexIterator < partInfo.numberOfPointers-1; indexPointer++) {
			bufferToBLOCK_POINTER(&bufferPointer, indexIterator * sizeof(bufferPointer));
			if (bufferPointer.valid == 1) {
				getDataBlockByPointer(&dataBlock, bufferPointer.blockPointer);
				
				int i; // Busca nos registros dos blocos de dados
				for (i = 0; i < (SECTOR_SIZE * partInfo.blockSize / sizeof(DIR_RECORD)); i++) {
					bufferToDIR_RECORD(&bufferRecord, i * sizeof(bufferRecord));
					if (strcmp(bufferRecord.name, name) == 0 ) {
						*record = bufferRecord;

						return SUCCESS;
					}
				}
			}
		}
		// Checa se o encadeamento existe
		bufferToBLOCK_POINTER(&bufferPointer, indexIterator * sizeof(bufferPointer));
		if (bufferPointer.valid != 1)
			return ERROR; 
	}
}

int getRecordByPath(DIR_RECORD *record, char *path) {
	DWORD indexPointer;
	if (path[0] = '/')
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


FILE2 createRecord(char *filename, int type) {
	
	DWORD indexBlockPointer = currentDirIndexPointer;
	int blockSizeInBytes = SECTOR_SIZE * partInfo.blockSize *sizeof(BYTE);
	int indexIterator, i;
	DWORD extractedPointer;

	int numberOfDirRecords = sizeof(DIR_RECORD) / partInfo.blockSize * SECTOR_SIZE;

	//Buffers para fetch de bloco de indice e bloco de dados
	BYTE *indexBlockBuffer = malloc(blockSizeInBytes);
	BYTE *dataBlockBuffer = malloc(blockSizeInBytes);

	//Record a ser registrado 
	DIR_RECORD newRecord;
	newRecord.blockFileSize = 0;
	strcpy(newRecord.name,filename);
	newRecord.type = type;
	//TODO:Alocar bloco de indices e bloco de dados consistentes pro arquivo
	
	//Fetch do bloco de indices do diretorio corrente
	getIndexBlockByPointer(indexBlockBuffer,indexBlockPointer);

	for(indexIterator = 0; indexIterator < partInfo.numberOfPointers - 1; indexIterator++){
		extractedPointer = bufferToDWORD(indexBlockBuffer,indexIterator * sizeof(DWORD));

		if(extractedPointer == NULL_INDEX_POINTER){
			//aloca bloco de dados
			//aponta para bloco de dados
			//escreve newRecord no inicio
			//retorna
		}
		else{
			getDataBlockByPointer(dataBlockBuffer,extractedPointer);
			//Percorre as entadas de diretório no bloco
		}
	}


	

	
	return -1;
}

bool isOpened(FILE2 handle) {
	return (handle >= 0 && openedFiles[handle].free == true);
}


int readFile(FILE2 handle, BYTE *buffer, int size){
	int i;

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

	return createRecord(filename, RECORD_REGULAR);
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

	FILE2 handle = getFreeHandle();
	if (handle != -1 && strlen(filename) > 0) {
		DIR_RECORD dirRecord;
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

	return ERROR;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para realizar a leitura de uma certa quantidade
		de bytes (size) de um arquivo.
-----------------------------------------------------------------------------*/
int read2 (FILE2 handle, char *buffer, int size) {
	if (initialized == false && init() == false)
		return ERROR;

	if (isOpened(handle) == true) 
		if (readFile(handle, buffer, size) == 0)
			return SUCCESS;

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

	return ERROR;
}

/*-----------------------------------------------------------------------------
Função:	Função que abre um diretório existente no disco.
-----------------------------------------------------------------------------*/
DIR2 opendir2 (char *pathname) {
	if (initialized == false && init() == false)
		return ERROR;

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


