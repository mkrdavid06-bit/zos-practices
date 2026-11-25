#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <regex.h>
#include <locale.h>

static void file_error(const char *tool, const char *path)
{
    fprintf(stderr, "%s: %s: %s\n", tool, path, strerror(errno));
}

int main(int argc, char **argv)
{
    setlocale(LC_ALL, "");
    int opt, insensitive = 0, invert = 0, numbers = 0, count = 0;
    int extended = 0, quiet = 0;
    while ((opt = getopt(argc, argv, "ivncEq")) != -1) {
        switch (opt) {
        case 'i': insensitive = 1; break;
        case 'v': invert = 1; break;
        case 'n': numbers = 1; break;
        case 'c': count = 1; break;
        case 'E': extended = 1; break;
        case 'q': quiet = 1; break;
        default: return 2;
        }
    }
    if (optind >= argc) {
        fprintf(stderr, "Usage: mygrep [-ivncEq] PATTERN [FILE ...]\n");
        return 2;
    }
    regex_t re;
    const char *pattern = argv[optind++];
    
    int rc = regcomp(&re, *pattern ? pattern : "^", REG_NOSUB |
                     (insensitive ? REG_ICASE : 0) | (extended ? REG_EXTENDED : 0));
    if (rc) {
        char error[256]; regerror(rc, &re, error, sizeof(error));
        fprintf(stderr, "mygrep: %s\n", error); return 2;
    }
    int files = argc - optind, matched = 0, failed = 0;
    char *line = NULL; size_t capacity = 0;
    for (int i = 0; i < (files ? files : 1); ++i) {
        const char *path = files ? argv[optind + i] : "-";
        const char *label = !strcmp(path, "-") ? "(standard input)" : path;
        FILE *in = !strcmp(path, "-") ? stdin : fopen(path, "r");
        if (!in) { file_error("mygrep", path); failed = 1; continue; }
        ssize_t length; unsigned long long lineno = 0, hits = 0;
        while ((length = getline(&line, &capacity, in)) >= 0) {
            ++lineno;
            int newline = length && line[length - 1] == '\n';
            if (newline) line[length - 1] = '\0';
            rc = regexec(&re, line, 0, NULL, 0);
            if (rc && rc != REG_NOMATCH) {
                char error[256]; regerror(rc, &re, error, sizeof(error));
                fprintf(stderr, "mygrep: %s\n", error); failed = 1; break;
            }
            int selected = (rc == 0) != invert;
            if (selected) {
                matched = 1; ++hits;
                if (quiet) {
                    if (in != stdin) fclose(in);
                    free(line); regfree(&re); return 0;
                }
                if (!count) {
                    if (files > 1) printf("%s:", label);
                    if (numbers) printf("%llu:", lineno);
                    if (newline) line[length - 1] = '\n';
                    fwrite(line, 1, (size_t)length, stdout);
                    if (!newline) putchar('\n');
                }
            }
        }
        if (ferror(in)) { file_error("mygrep", path); failed = 1; }
        if (count) {
            if (files > 1) printf("%s:", label);
            printf("%llu\n", hits);
        }
        if (in != stdin && fclose(in) == EOF) { file_error("mygrep", path); failed = 1; }
    }
    free(line); regfree(&re);
    if (fflush(stdout) == EOF || ferror(stdout)) { file_error("mygrep", "stdout"); failed = 1; }
    return failed ? 2 : matched ? 0 : 1;
}
