CFLAGS = -Wall -Wextra -Wconversion -pedantic -std=gnu11 -ggdb

main: main.c

run: main
	./main

clean:
	rm main

.PHONY = clean run
