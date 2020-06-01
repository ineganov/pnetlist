
.PHONY: all


all: tokenize.c
	clang -std=c99 -g -Wall -O0 -o main tokenize.c
