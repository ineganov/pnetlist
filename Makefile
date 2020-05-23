
.PHONY: all


all: tokenize.c
	clang -std=c99 -g -Wall -Os -o main tokenize.c
