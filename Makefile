CC      = clang
CFLAGS  = -std=c99 -g -Wall -O0
objects = pnetlist.o parse.o tokenize.o

.PHONY: all clean

all: $(objects)
	$(CC) -o pnetlist $(objects)

${objects}: pnetlist.h

%.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@

clean:
	@-rm $(objects)
	@-rm pnetlist
	@echo Done