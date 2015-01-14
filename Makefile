parser: parser.c utf8.c tokenizer.c token.c buffer.c
	$(CC) -Wall -g -std=c99 -o parser utf8.c parser.c tokenizer.c token.c buffer.c

all: parser
