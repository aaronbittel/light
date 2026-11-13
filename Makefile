CFLAGS = -Wall -Wextra -pedantic -ggdb

main: main.c

.PHONY = clean

clean:
	rm main
