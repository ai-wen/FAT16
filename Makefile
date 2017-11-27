CFLAGS=$(shell pkg-config fuse --cflags)
LIBS=$(shell pkg-config fuse --libs)

CC=gcc

all: clean mount_fat16

mount_fat16: mount_fat16.o sector.o log.o fat16.o
	$(CC) -o $@ $^ $(LIBS)

mount_fat16.o: mount_fat16.c

sector.o: sector.c sector.h

log.o: log.c log.h 

fat16.o: fat16.c fat16.h

clean:
	rm -f mount_fat16 *.o
