
/**
*/
#include <stdlib.h>
#include "../include/t2fs.h"
#include "../include/apidisk.h"
#include "../include/buffer_control.h"


/********************************************************************************/
/************************************ PRIVATE ***********************************/
/********************************************************************************/
bool initialized = false;	// Estado do sistema de arquivos: inicializado ou nao para correto funcionamento

char *currentPath;	// Caminho corrente do sistema de arquivos
DWORD currentDirIndexPointer; // Ponteiro de bloco de indice de diretorio associado ao caminho corrente
INDEX_BLOCK *rootDirIndex = NULL; // Bloco de indice da raiz

PART_INFO partInfo;	// Estrutura que armazenara enderecos e limites da particao
HANDLER openFiles[MAX_FILE_OPEN];	// Estrutura armazenadora das informacoes dos arquivos abertos

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
		//TODO
		partInfo.dataBlocksStart = 0;
		partInfo.indexBlocksStart = 0;

		//Adição do número de ponteiros de indice
		partInfo.numberOfPointers = superblock.numberOfPointers;

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

	int size = partInfo.lastSectorAddress - partInfo.firstSectorAddress + 1;
	int totalBlocks = (size / sectorsPerBlock) - 1; // Um é destinado para o superbloco
	int utilBlocks = (totalBlocks * sectorsPerBlock * SECTOR_SIZE * 8) / (sectorsPerBlock * SECTOR_SIZE * 8 + 1);
	int totalSectorsBitmap = ceil((float)utilBlocks / (float)(SECTOR_SIZE * 8));
	// Se for necessario apenas 1 setor para o bitmap, sera necessario separar os dois tipos de bitmaps
	// isso acerreta em mudanças nos valores de blocos ja calculados.
	// TODO
	int indexBitmapSize = totalSectorsBitmap / 3;
	int indexBlocksSize = utilBlocks / 3; //Numero de blocos


	//superblock.id = {'T', '2', 'F', 'S'};
	superblock.dataBlockBitmapSize = totalSectorsBitmap - indexBitmapSize;
	superblock.indexBlockBitmapSize = indexBitmapSize;
	superblock.indexBlockAreaSize = indexBlocksSize;
	superblock.blockSize = sectorsPerBlock;
	superblock.partitionSize = utilBlocks + 1; // Em blocos
	superblock.numberOfPointers = sectorsPerBlock * SECTOR_SIZE * 8/(int) sizeof(DWORD);

	return superblock;
}


DIR_RECORD *getRecordByName(char *name) {
	//TODO
}

bool initPartInfo() {
	return (readPartInfoSectors() == true && readPartInfoBlocks() == true);
}

bool initRootDir() {
	rootDirIndex = getIndexBlockByNumber(0);
	
	if (rootDirIndex != NULL)
		return true;
	
	return false;
}

bool init() {
	if (initPartInfo() == true && initRootDir() == true) {
		int i;
		for (i = 0; i < MAX_FILE_OPEN; i++) {
			openFiles[i].free = true;
			//...
		}

		currentPath = "/";
		currentRecord = getRecordByName(".");

		initialized = true;
	}

	return false;
}


FILE2 createRecord(char *filename, int type) {
	
	return -1;
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
	BYTE bitmapBuffer[SECTOR_SIZE] = {0};
	if (readPartInfoSectors() == true) {
		cleanDisk();
		
		// Escreve o superbloco 
		SUPERBLOCK superblock = createSuperblock(sectors_per_block);
		memcpy(buffer, &superblock, sizeof(SUPERBLOCK)); // so terá sentido se for usado mesmo endian na estrutura e no array
		
		if (write_sector(partInfo.firstSectorAddress, buffer) != 0)
			return ERROR;
		
		// Escreve o bitmap de indices e dados
		int i;
		for (i = 0; i < (superblock.dataBlockBitmapSize + superblock.indexBlockBitmapSize); i++);
			if (write_sector(sectors_per_block, bitmapBuffer) != 0)
				return ERROR;

		// Definindo ultimas informacoes sobre a particao
		partInfo.dataBlocksStart = 0; //TODO
		partInfo.indexBlocksStart = 0; //TODO

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


