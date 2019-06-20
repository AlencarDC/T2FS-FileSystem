#
# Makefile para programas de teste
# 

CC=gcc
LIB_DIR=../lib

all: t2shell main

t2shell: t2shell.c $(LIB_DIR)/libt2fs.a
	$(CC) -o t2shell t2shell.c -L$(LIB_DIR) -lt2fs -lm -Wall

main: main.c $(LIB_DIR)/libt2fs.a
	$(CC) -o main main.c -L$(LIB_DIR) -lt2fs -lm -Wall

clean:
	rm -rf t2shell main *.o *~

