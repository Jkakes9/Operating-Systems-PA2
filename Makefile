CC=gcc
CFLAGS=-Wall -Wextra -pthread

OBJS=chash.o hash_table.o

all: chash

chash: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

chash.o: chash.c hash_table.h
hash_table.o: hash_table.c hash_table.h

clean:
	 rm -f $(OBJS) chash hash.log

.PHONY: all clean
