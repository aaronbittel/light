CC = gcc
CFLAGS = -Wall -Wextra -Wconversion -pedantic -std=gnu11 -ggdb
ANALYZE_FLAGS = -fanalyzer

main: main.c

run: main
	./main

analyze: main
	$(CC) $(CFLAGS) $(ANALYZE_FLAGS) main.c -o main

hot:
	ls *.c *.h | entr -c sh -c 'make --no-print-directory -B main_test main && ./main_test'

clean:
	rm main

.PHONY = clean run hot analyze
