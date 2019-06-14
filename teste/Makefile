#
# Makefile para programas de teste
# 

CC=gcc
LIB_DIR=../lib

all: formata2

formata2: formata2.c $(LIB_DIR)/libt2fs.a
	$(CC) -o formata2 formata2.c -L$(LIB_DIR) -lt2fs -lm -Wall

clean:
	rm -rf formata2 *.o *~

