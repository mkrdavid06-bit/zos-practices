CC = cc
CFLAGS = -std=c11 -O2 -Wall -Wextra -Wpedantic

.PHONY: all clean
all: fork_demo

fork_demo: main.c
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -f fork_demo
