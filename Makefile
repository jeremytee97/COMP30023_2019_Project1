CC = gcc
CFLAGS = -g -std=c99 -O3 -Wall -Wpedantic 
DEPS = http.h
OBJ    = image_tagger.o http.o 
EXE    = image_tagger

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

$(EXE): $(OBJ)
	$(CC) $(CFLAGS) -o $(EXE) $(OBJ)

clean:
	rm -f *.o image_tagger