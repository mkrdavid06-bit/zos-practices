CC = cc
CFLAGS = -std=c11 -O2 -Wall -Wextra -Wpedantic
PROGRAMS = mycat mygrep myls mychmod

.PHONY: all clean
all: $(PROGRAMS)

%: %.c
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -f $(PROGRAMS)
