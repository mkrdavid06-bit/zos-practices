#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

static char **flag_arguments(int argc, char **argv)
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

static void file_error(const char *tool, const char *path)
{
    fprintf(stderr, "%s: %s: %s\n", tool, path, strerror(errno));
}

int main(int argc, char **argv)
{
    char **args = flag_arguments(argc, argv);
    int count = 0;
    while (args[count]) ++count;
    int number = 0, nonblank = 0, ends = 0, opt;
    while ((opt = getopt(count, args, "nbE")) != -1) {
        switch (opt) {
        case 'n': number = 1; break;
        case 'b': nonblank = 1; break;
        case 'E': ends = 1; break;
        default: fprintf(stderr, "Usage: mycat [-nbE] [FILE ...]\n"); free(args); return 1;
        }
    }
    unsigned long long line = 1;
    int start = 1, status = 0;
    int first = optind;
    int inputs = count - first;
    for (int i = 0; i < (inputs ? inputs : 1); ++i) {
        const char *path = inputs ? args[first + i] : "-";
        FILE *in = !strcmp(path, "-") ? stdin : fopen(path, "rb");
        if (!in) { file_error("mycat", path); status = 1; continue; }
        int c;
        while ((c = fgetc(in)) != EOF) {
            if (start && ((nonblank && c != '\n') || (!nonblank && number)))
                printf("%6llu\t", line++);
            if (ends && c == '\n') putchar('$');
            putchar(c);
            start = c == '\n';
        }
        if (ferror(in)) { file_error("mycat", path); status = 1; }
        if (in != stdin && fclose(in) == EOF) { file_error("mycat", path); status = 1; }
    }
    if (fflush(stdout) == EOF || ferror(stdout)) { file_error("mycat", "stdout"); status = 1; }
    free(args);
    return status;
}
