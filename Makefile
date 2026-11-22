CC = gcc
CFLAGS = -Wall -Wextra -Wconversion -pedantic -std=gnu11 -ggdb
ANALYZE_FLAGS = -fanalyzer

main: main.c

run: main
	./main

analyze: main
	$(CC) $(CFLAGS) $(ANALYZE_FLAGS) main.c -o main

clean:
	rm main

.PHONY = clean run
