CC = gcc
CFLAGS = -Wall -Wextra -Wconversion -pedantic -std=gnu11 -ggdb
ANALYZE_FLAGS = -fanalyzer

NAME  = main
BIN   = bin
SRC   = src
EXEC  = $(BIN)/$(NAME)
VPATH = $(SRC)

BIN_SRCS = $(wildcard $(SRC)/*.c)
BINS = $(patsubst $(SRC)/%.c,$(BIN)/%,$(BIN_SRCS))

all: $(BINS)

$(BIN)/%: $(SRC)/%.c
	$(CC) $(CFLAGS) $< -o $@

run: $(EXEC)
	@./$(EXEC)

watch: $(BIN_SRCS)
	@find $(SRC) -type f | entr -c $(MAKE) --no-print-directory all

analyze:
	$(CC) $(CFLAGS) $(ANALYZE_FLAGS) $(SRC)/$(NAME).c -o $(EXEC)

test: $(BIN)/main $(BIN)/main_test
	@$(BIN)/main_test

test-watch:
	@find $(SRC) -type f | entr -c $(MAKE) --no-print-directory test

clean:
	@$(RM) -r $(BIN)/*

.PHONY = all run watch analyze test test-watch clean
