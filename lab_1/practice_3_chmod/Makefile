CC = cc
CFLAGS = -std=c11 -O2 -Wall -Wextra -Wpedantic
PROGRAMS = mychmod

.PHONY: all clean
all: $(PROGRAMS)

%: %.c common.h
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -f $(PROGRAMS)
