CC = gcc
CFLAGS = -O3
LDFLAGS = -O3

all: bitset_test loadids

bitset_test: bitset.o bitset_test.o
loadids: bitset.o loadids.o

clean:
	-rm bitset_test loadids *.o

loadids.o: loadids.c bitset.h
bitset_test.o: bitset_test.c bitset.h
bitset.o: bitset.c bitset.h
