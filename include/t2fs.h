

#ifndef __LIBT2FS___
#define __LIBT2FS___
#include <stdbool.h>
#include <string.h>

#define	SECTOR_SIZE	256
#define INIT_BYTE_PART_TABLE 8

#define INVALID_BLOCK_PTR '\0'
#define SECTOR_ERROR -31
#define	INVALID_PTR	-1
#define NULL_INDEX_POINTER 0
#define MAX_FILE_OPEN 10

#define SUCCESS 0
#define ERROR -1


typedef int FILE2;
typedef int DIR2;

typedef unsigned char BYTE;
typedef unsigned short int WORD;
typedef unsigned int DWORD;

#define FILE_NAME_SIZE 31  // Tamanho fixo do nome do arquivo ou diretorio


#pragma pack(push, 1)
/* Informacoes relevantes sobre a particao */
typedef struct{
	DWORD firstSectorAddress;
	DWORD lastSectorAddress;
	DWORD indexBlocksStart;
	DWORD dataBlocksStart;
	DWORD numberOfPointers;
	DWORD blockSize;
} PART_INFO;

/* Registro com as informacoes do superbloco */ 
typedef struct {
	char id[4]; // Identifcacao do sistema de arquivos. Ex.: T2FS
	//WORD dataBlockBitmapSize; // Quantidade de blocos para armazenar o bitmap de blocos de dados
	//WORD indexBlockBitmapSize; // Quantidade de blocos para armazenar o bitmap de arquivos de indice
	WORD bitmapSectorsSize; // Quantidade de setores para armazenar os dois bitmaps;
	WORD dataBlockAreaSize; // Quantidade de blocos da area de blocos de dado
	WORD indexBlockAreaSize; // Quantidade de blocos da area de blocos de indice
	WORD blockSize; // Quantidade de setores de cada bloco
	DWORD partitionSize; // Quantidade de blocos destiandos a particao do T2FS
	DWORD numberOfPointers; //Quantidade de ponteiros no bloco de indice
} SUPERBLOCK;

typedef struct blockPointer{
	BYTE valid;
	DWORD blockPointer;
} BLOCK_POINTER;

/* Registro de diretorio  */
#define RECORD_INVALID 0x00 // Entrada do arquivo de diretorio invalida
#define RECORD_REGULAR 0x01 // Entrada para arquivo regular
#define RECORD_DIR 0x02 // Entrada para diretorio
#define RECORD_LINK 0x03 // Entrada para softlinks
//Informações de ofset em termos de byte  da struct DIR_RECORD.
//SE MUDAR A STRUCT MUDE OS OFFSETS CORRETAMENTE TAMBÉM
#define DIR_ENTRY_OFFSET 1
#define BLOCK_FILE_SIZE_OFFSET DIR_ENTRY_OFFSET + FILE_NAME_SIZE + 1
#define INDEX_ADDRESS_OFFSET BLOCK_FILE_SIZE_OFFSET + 4
typedef struct {
	BYTE type;
	char name[FILE_NAME_SIZE + 1];
	DWORD blockFileSize;
	DWORD indexAddress;
} DIR_RECORD;

/** Registro com as informa��es da entrada de diret�rio, lida com readdir2 */
#define MAX_FILE_NAME_SIZE 255
typedef struct {
    char    name[MAX_FILE_NAME_SIZE+1]; /* Nome do arquivo cuja entrada foi lida do disco      */
    BYTE    fileType;                   /* Tipo do arquivo: regular (0x01) ou diret�rio (0x02) */
    DWORD   fileSize;                   /* Numero de bytes do arquivo                          */
} DIRENT2;

/* Handler para arquivos abertos */
typedef struct {
	bool free; // Entrada no handler está livre ou não
	DIR_RECORD record;
	char *path;
	DWORD pointer;
} HANDLER;
#pragma pack(pop)



/********************************************************************************/
/************************************ PRIVATE ***********************************/
/********************************************************************************/

// Le informacoes sobre os limites da particao contidos no MBR
bool readPartInfoSectors();

// Le informacoes sobre enderecos iniciais dos blocos de indice e dados contidos no Superbloco
bool readPartInfoBlocks();

// Usado na formatacao para zerar todas posicoes do disco
void cleanDisk();

// Realiza os calculos para preencer as informacoes da estrutura de superbloco e retorna-la
SUPERBLOCK createSuperblock(int sectorsPerBlock);

// Retorna registor do diretorio dado seu nome e bloco de indice a ser pesquiasdo
int getRecordByName(DIR_RECORD *record, DWORD indexPointer, char *name);

// Retorna o registro de diretorio apontado pelo pelo path
int getRecordByPath(DIR_RECORD *record, char *path);

// Inicializa a estrutura de partInfo, faz uma chamada para readPartInfoSectors() e para readpartInfoBlocks();
bool initPartInfo();

// Inicializa as globais usadas para gerenciar o diretorio raiz, basicamente o bloco de indice da raiz
bool initRootDir();

// Faz a chamada para inicializar todas outras estruturas (initPartInfo() e initRootDir() e tambem inicializa o HANDLER de arquivos abertos
bool init();

// Realiza a criacao de um novo record/registro no diretorio corrente
DIR_RECORD createRecord(char *filename, int type);

//Armazena em block o bloco apontado em pointer dado o offset de inicio da área de blocos usadae tamanho do bloco.
int getBlockByPointer(BYTE *block,DWORD pointer, DWORD offset, int blockSize);

//Armazena em indexBlock o bloco de indices apontado por pointer
int getIndexBlockByPointer(BYTE *indexBlock,DWORD pointer);

//Armazena em dataBlock o bloco de dados apontado por pointer
int getDataBlockByPointer(BYTE *dataBlock,DWORD pointer);

/********************************************************************************/
/************************************ PUBLIC ************************************/
/********************************************************************************/

/*-----------------------------------------------------------------------------
Fun��o: Usada para identificar os desenvolvedores do T2FS.
	Essa fun��o copia um string de identifica��o para o ponteiro indicado por "name".
	Essa c�pia n�o pode exceder o tamanho do buffer, informado pelo par�metro "size".
	O string deve ser formado apenas por caracteres ASCII (Valores entre 0x20 e 0x7A) e terminado por �\0�.
	O string deve conter o nome e n�mero do cart�o dos participantes do grupo.

Entra:	name -> buffer onde colocar o string de identifica��o.
	size -> tamanho do buffer "name" (n�mero m�ximo de bytes a serem copiados).

Sa�da:	Se a opera��o foi realizada com sucesso, a fun��o retorna "0" (zero).
	Em caso de erro, ser� retornado um valor diferente de zero.
-----------------------------------------------------------------------------*/
int identify2 (char *name, int size);


/*-----------------------------------------------------------------------------
Fun��o:	Formata um disco virtual

Entra:	sectors_per_block -> tamanho de um bloco de dados em setores

Sa�da:	Se a opera��o foi realizada com sucesso, a fun��o retorna "0" (zero).
	Em caso de erro, ser� retornado um valor diferente de zero.
-----------------------------------------------------------------------------*/
int format2(int sectors_per_block);


/*-----------------------------------------------------------------------------
Fun��o: Criar um novo arquivo.
	O nome desse novo arquivo � aquele informado pelo par�metro "filename".
	O contador de posi��o do arquivo (current pointer) deve ser colocado na posi��o zero.
	Caso j� exista um arquivo ou diret�rio com o mesmo nome, a fun��o dever� retornar um erro de cria��o.
	A fun��o deve retornar o identificador (handle) do arquivo.
	Esse handle ser� usado em chamadas posteriores do sistema de arquivo para fins de manipula��o do arquivo criado.

Entra:	filename -> nome do arquivo a ser criado.

Sa�da:	Se a opera��o foi realizada com sucesso, a fun��o retorna o handle do arquivo (n�mero positivo).
	Em caso de erro, deve ser retornado um valor negativo.
-----------------------------------------------------------------------------*/
FILE2 create2 (char *filename);


/*-----------------------------------------------------------------------------
Fun��o:	Apagar um arquivo do disco.
	O nome do arquivo a ser apagado � aquele informado pelo par�metro "filename".

Entra:	filename -> nome do arquivo a ser apagado.

Sa�da:	Se a opera��o foi realizada com sucesso, a fun��o retorna "0" (zero).
	Em caso de erro, ser� retornado um valor diferente de zero.
-----------------------------------------------------------------------------*/
int delete2 (char *filename);


/*-----------------------------------------------------------------------------
Fun��o:	Abre um arquivo existente no disco.
	O nome desse novo arquivo � aquele informado pelo par�metro "filename".
	Ao abrir um arquivo, o contador de posi��o do arquivo (current pointer) deve ser colocado na posi��o zero.
	A fun��o deve retornar o identificador (handle) do arquivo.
	Esse handle ser� usado em chamadas posteriores do sistema de arquivo para fins de manipula��o do arquivo criado.
	Todos os arquivos abertos por esta chamada s�o abertos em leitura e em escrita.
	O ponto em que a leitura, ou escrita, ser� realizada � fornecido pelo valor current_pointer (ver fun��o seek2).

Entra:	filename -> nome do arquivo a ser apagado.

Sa�da:	Se a opera��o foi realizada com sucesso, a fun��o retorna o handle do arquivo (n�mero positivo)
	Em caso de erro, deve ser retornado um valor negativo
-----------------------------------------------------------------------------*/
FILE2 open2 (char *filename);


/*-----------------------------------------------------------------------------
Fun��o:	Fecha o arquivo identificado pelo par�metro "handle".

Entra:	handle -> identificador do arquivo a ser fechado

Sa�da:	Se a opera��o foi realizada com sucesso, a fun��o retorna "0" (zero).
	Em caso de erro, ser� retornado um valor diferente de zero.
-----------------------------------------------------------------------------*/
int close2 (FILE2 handle);


/*-----------------------------------------------------------------------------
Fun��o:	Realiza a leitura de "size" bytes do arquivo identificado por "handle".
	Os bytes lidos s�o colocados na �rea apontada por "buffer".
	Ap�s a leitura, o contador de posi��o (current pointer) deve ser ajustado para o byte seguinte ao �ltimo lido.

Entra:	handle -> identificador do arquivo a ser lido
	buffer -> buffer onde colocar os bytes lidos do arquivo
	size -> n�mero de bytes a serem lidos

Sa�da:	Se a opera��o foi realizada com sucesso, a fun��o retorna o n�mero de bytes lidos.
	Se o valor retornado for menor do que "size", ent�o o contador de posi��o atingiu o final do arquivo.
	Em caso de erro, ser� retornado um valor negativo.
-----------------------------------------------------------------------------*/
int read2 (FILE2 handle, char *buffer, int size);


/*-----------------------------------------------------------------------------
Fun��o:	Realiza a escrita de "size" bytes no arquivo identificado por "handle".
	Os bytes a serem escritos est�o na �rea apontada por "buffer".
	Ap�s a escrita, o contador de posi��o (current pointer) deve ser ajustado para o byte seguinte ao �ltimo escrito.

Entra:	handle -> identificador do arquivo a ser escrito
	buffer -> buffer de onde pegar os bytes a serem escritos no arquivo
	size -> n�mero de bytes a serem escritos

Sa�da:	Se a opera��o foi realizada com sucesso, a fun��o retorna o n�mero de bytes efetivamente escritos.
	Em caso de erro, ser� retornado um valor negativo.
-----------------------------------------------------------------------------*/
int write2 (FILE2 handle, char *buffer, int size);


/*-----------------------------------------------------------------------------
Fun��o:	Fun��o usada para truncar um arquivo.
	Remove do arquivo todos os bytes a partir da posi��o atual do contador de posi��o (CP)
	Todos os bytes a partir da posi��o CP (inclusive) ser�o removidos do arquivo.
	Ap�s a opera��o, o arquivo dever� contar com CP bytes e o ponteiro estar� no final do arquivo

Entra:	handle -> identificador do arquivo a ser truncado

Sa�da:	Se a opera��o foi realizada com sucesso, a fun��o retorna "0" (zero).
	Em caso de erro, ser� retornado um valor diferente de zero.
-----------------------------------------------------------------------------*/
int truncate2 (FILE2 handle);


/*-----------------------------------------------------------------------------
Fun��o:	Reposiciona o contador de posi��es (current pointer) do arquivo identificado por "handle".
	A nova posi��o � determinada pelo par�metro "offset".
	O par�metro "offset" corresponde ao deslocamento, em bytes, contados a partir do in�cio do arquivo.
	Se o valor de "offset" for "-1", o current_pointer dever� ser posicionado no byte seguinte ao final do arquivo,
		Isso � �til para permitir que novos dados sejam adicionados no final de um arquivo j� existente.

Entra:	handle -> identificador do arquivo a ser escrito
	offset -> deslocamento, em bytes, onde posicionar o "current pointer".

Sa�da:	Se a opera��o foi realizada com sucesso, a fun��o retorna "0" (zero).
	Em caso de erro, ser� retornado um valor diferente de zero.
-----------------------------------------------------------------------------*/
int seek2 (FILE2 handle, DWORD offset);


/*-----------------------------------------------------------------------------
Fun��o:	Criar um novo diret�rio.
	O caminho desse novo diret�rio � aquele informado pelo par�metro "pathname".
		O caminho pode ser ser absoluto ou relativo.
	S�o considerados erros de cria��o quaisquer situa��es em que o diret�rio n�o possa ser criado.
		Isso inclui a exist�ncia de um arquivo ou diret�rio com o mesmo "pathname".

Entra:	pathname -> caminho do diret�rio a ser criado

Sa�da:	Se a opera��o foi realizada com sucesso, a fun��o retorna "0" (zero).
	Em caso de erro, ser� retornado um valor diferente de zero.
-----------------------------------------------------------------------------*/
int mkdir2 (char *pathname);


/*-----------------------------------------------------------------------------
Fun��o:	Apagar um subdiret�rio do disco.
	O caminho do diret�rio a ser apagado � aquele informado pelo par�metro "pathname".
	S�o considerados erros quaisquer situa��es que impe�am a opera��o.
		Isso inclui:
			(a) o diret�rio a ser removido n�o est� vazio;
			(b) "pathname" n�o existente;
			(c) algum dos componentes do "pathname" n�o existe (caminho inv�lido);
			(d) o "pathname" indicado n�o � um arquivo;

Entra:	pathname -> caminho do diret�rio a ser criado

Sa�da:	Se a opera��o foi realizada com sucesso, a fun��o retorna "0" (zero).
	Em caso de erro, ser� retornado um valor diferente de zero.
-----------------------------------------------------------------------------*/
int rmdir2 (char *pathname);


/*-----------------------------------------------------------------------------
Fun��o:	Altera o diret�rio atual de trabalho (working directory).
		O caminho desse diret�rio � informado no par�metro "pathname".
		S�o considerados erros:
			(a) qualquer situa��o que impe�a a realiza��o da opera��o
			(b) n�o exist�ncia do "pathname" informado.

Entra:	pathname -> caminho do novo diret�rio de trabalho.

Sa�da:	Se a opera��o foi realizada com sucesso, a fun��o retorna "0" (zero).
		Em caso de erro, ser� retornado um valor diferente de zero.
-----------------------------------------------------------------------------*/
int chdir2 (char *pathname);


/*-----------------------------------------------------------------------------
Fun��o:	Informa o diret�rio atual de trabalho.
		O "pathname" do diret�rio de trabalho deve ser copiado para o buffer indicado por "pathname".
			Essa c�pia n�o pode exceder o tamanho do buffer, informado pelo par�metro "size".
		S�o considerados erros:
			(a) quaisquer situa��es que impe�am a realiza��o da opera��o
			(b) espa�o insuficiente no buffer "pathname", cujo tamanho est� informado por "size".

Entra:	pathname -> buffer para onde copiar o pathname do diret�rio de trabalho
		size -> tamanho do buffer pathname

Sa�da:	Se a opera��o foi realizada com sucesso, a fun��o retorna "0" (zero).
		Em caso de erro, ser� retornado um valor diferente de zero.
-----------------------------------------------------------------------------*/
int getcwd2 (char *pathname, int size);


/*-----------------------------------------------------------------------------
Fun��o:	Abre um diret�rio existente no disco.
	O caminho desse diret�rio � aquele informado pelo par�metro "pathname".
	Se a opera��o foi realizada com sucesso, a fun��o:
		(a) deve retornar o identificador (handle) do diret�rio
		(b) deve posicionar o ponteiro de entradas (current entry) na primeira posi��o v�lida do diret�rio "pathname".
	O handle retornado ser� usado em chamadas posteriores do sistema de arquivo para fins de manipula��o do diret�rio.

Entra:	pathname -> caminho do diret�rio a ser aberto

Sa�da:	Se a opera��o foi realizada com sucesso, a fun��o retorna o identificador do diret�rio (handle).
	Em caso de erro, ser� retornado um valor negativo.
-----------------------------------------------------------------------------*/
DIR2 opendir2 (char *pathname);


/*-----------------------------------------------------------------------------
Fun��o:	Realiza a leitura das entradas do diret�rio identificado por "handle".
	A cada chamada da fun��o � lida a entrada seguinte do diret�rio representado pelo identificador "handle".
	Algumas das informa��es dessas entradas devem ser colocadas no par�metro "dentry".
	Ap�s realizada a leitura de uma entrada, o ponteiro de entradas (current entry) deve ser ajustado para a pr�xima entrada v�lida, seguinte � �ltima lida.
	S�o considerados erros:
		(a) qualquer situa��o que impe�a a realiza��o da opera��o
		(b) t�rmino das entradas v�lidas do diret�rio identificado por "handle".

Entra:	handle -> identificador do diret�rio cujas entradas deseja-se ler.
	dentry -> estrutura de dados onde a fun��o coloca as informa��es da entrada lida.

Sa�da:	Se a opera��o foi realizada com sucesso, a fun��o retorna "0" (zero).
	Em caso de erro, ser� retornado um valor diferente de zero ( e "dentry" n�o ser� v�lido)
-----------------------------------------------------------------------------*/
int readdir2 (DIR2 handle, DIRENT2 *dentry);


/*-----------------------------------------------------------------------------
Fun��o:	Fecha o diret�rio identificado pelo par�metro "handle".

Entra:	handle -> identificador do diret�rio que se deseja fechar (encerrar a opera��o).

Sa�da:	Se a opera��o foi realizada com sucesso, a fun��o retorna "0" (zero).
	Em caso de erro, ser� retornado um valor diferente de zero.
-----------------------------------------------------------------------------*/
int closedir2 (DIR2 handle);


/*-----------------------------------------------------------------------------
Fun��o:	Cria um link simb�lico

Entra:	linkname -> nome do link
		filename -> nome do arquivo apontado pelo link

Sa�da:	Se a opera��o foi realizada com sucesso, a fun��o retorna "0" (zero).
	Em caso de erro, ser� retornado um valor diferente de zero.
-----------------------------------------------------------------------------*/
int ln2(char *linkname, char *filename);







#endif
