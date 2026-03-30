CC = gcc
CFLAGS = -fopenmp -Wall -Wextra -O2
LDFLAGS = -lm

all: main

main: src/main.c
	$(CC) $(CFLAGS) src/main.c -o main $(LDFLAGS)

clean:
	rm -f main