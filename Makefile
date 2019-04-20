CC = gcc
CFLAGS = -g -std=c99 -O3 -Wall -Wpedantic 
DEPS = http.h
OBJ    = server.o http.o 
EXE    = server

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

$(EXE): $(OBJ)
	$(CC) $(CFLAGS) -o $(EXE) $(OBJ)