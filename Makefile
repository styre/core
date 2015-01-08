parser: parser.c utf8.c tokenizer.c
	$(CC) -Wall -g -std=c99 -o parser utf8.c parser.c tokenizer.c

all: parser
