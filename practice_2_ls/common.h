#ifndef MY_UTILS_COMMON_H
#define MY_UTILS_COMMON_H

#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

/* Move flag-only options before operands, including on BSD/macOS getopt.
 * The caller owns the returned pointer array, but not its strings. */
static inline char **flag_arguments(int argc, char **argv)
{
    char **out = calloc((size_t)argc + 2, sizeof(*out));
    if (!out) { perror("calloc"); exit(1); }
    int n = 0, end = 0;
    out[n++] = argv[0];
    for (int i = 1; i < argc; ++i) {
        if (!end && !strcmp(argv[i], "--")) { end = 1; continue; }
        if (!end && argv[i][0] == '-' && argv[i][1]) out[n++] = argv[i];
    }
    out[n++] = "--";
    end = 0;
    for (int i = 1; i < argc; ++i) {
        if (!end && !strcmp(argv[i], "--")) { end = 1; continue; }
        if (end || argv[i][0] != '-' || !argv[i][1]) out[n++] = argv[i];
    }
    return out;
}

static inline void file_error(const char *tool, const char *path)
{
    fprintf(stderr, "%s: %s: %s\n", tool, path, strerror(errno));
}
#endif
