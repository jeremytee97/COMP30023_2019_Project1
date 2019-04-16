BIN_DIR = bin
CC = gcc
CFLAGS = -std=c99 -O3 -Wall -Wpedantic
OBJ    = server.o http.o 
EXE    = server

$(EXE): $(OBJ)
	$(CC) $(CFLAGS) -o $(EXE) $(OBJ)

.PHONY: clean mkbin

clean:
	rm -rf $(BIN_DIR)

mkbin:
	mkdir -p bin

