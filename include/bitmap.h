
#ifndef __BITMAP__
#define	__BITMAP__

#define	BITMAP_INDEX	0
#define	BITMAP_DATA 	1


/*------------------------------------------------------------------------
	Recupera o bit indicado do bitmap solicitado
Entra:
	bitmapType -> bitmap
		==0 -> i-node
		!=0 -> blocos de dados
	bitNumber -> bit a ser retornado
Retorna:
	Sucesso: valor do bit: ZERO ou UM (0 ou 1)
	Erro: número negativo
------------------------------------------------------------------------*/
int	getBitmap (int bitmapType, int bitNumber);


/*------------------------------------------------------------------------
	Seta o bit indicado do bitmap solicitado
Entra:
	bitmapType -> bitmap
		==0 -> i-node
		!=0 -> blocos de dados
	bitNumber -> bit a ser retornado
	bitValue -> valor a ser escrito no bit
		==0 -> coloca bit em 0
		!=0 -> coloca bit em 1
Retorna
	Sucesso: ZERO (0)
	Erro: número negativo
------------------------------------------------------------------------*/
int	setBitmap (int bitmapType, int bitNumber, int bitValue);


/*------------------------------------------------------------------------
	Procura no bitmap solicitado pelo valor indicado
Entra:
	bitmapType -> bitmap
		==0 -> i-node
		!=0 -> blocos de dados
	bitValue -> valor procurado
Retorna
	Sucesso
		Achou o bit: índice associado ao bit (número positivo)
		Não achou: Procura de novo
	Erro: número negativo
------------------------------------------------------------------------*/
int	searchBitmap (int bitmapType, int bitValue);

#endif