#
# Makefile ESQUELETO
#
# DEVE ter uma regra "all" para geração da biblioteca
# regra "clean" para remover todos os objetos gerados.
#
# NECESSARIO adaptar este esqueleto de makefile para suas necessidades.
#
# 

CC=gcc
LIB_DIR=./lib
INC_DIR=./include
BIN_DIR=./bin
SRC_DIR=./src

all: bitmap.o buffer_control.o t2fs.o libt2fs

bitmap.o: 
	$(CC) -o $(BIN_DIR)/bitmap.o -c $(SRC_DIR)/bitmap.c -Wall

buffer_control.o: 
	$(CC) -o $(BIN_DIR)/buffer_control.o -c $(SRC_DIR)/buffer_control.c -Wall

t2fs.o: 
	$(CC) -o $(BIN_DIR)/t2fs.o -c $(SRC_DIR)/t2fs.c -Wall

libt2fs: bitmap.o buffer_control.o t2fs.o
	ar crs $(LIB_DIR)/libt2fs.a $(BIN_DIR)/t2fs.o $(BIN_DIR)/bitmap.o $(BIN_DIR)/buffer_control.o $(LIB_DIR)/apidisk.o

clean:
	rm -rf $(LIB_DIR)/*.a $(BIN_DIR)/*.o $(SRC_DIR)/*~ $(INC_DIR)/*~ *~


