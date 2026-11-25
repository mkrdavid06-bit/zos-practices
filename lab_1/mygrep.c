#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "Usage: mygrep PATTERN [FILE ...]\n");
        return 2;
    }
    regex_t pattern;
    if (regcomp(&pattern, argv[1][0] ? argv[1] : "^", REG_NOSUB) != 0) {
        fputs("Invalid pattern\n", stderr);
        return 2;
    }

    char *line = NULL;
    size_t size = 0;
    int files = argc - 2, found = 0, failed = 0;
    for (int i = 0; i < (files ? files : 1); i++) {
        char *path = files ? argv[i + 2] : "-";
        FILE *file = strcmp(path, "-") == 0 ? stdin : fopen(path, "r");
        if (file == NULL) {
            perror(path);
            failed = 1;
            continue;
        }
        while (getline(&line, &size, file) != -1) {
            line[strcspn(line, "\n")] = '\0';
            if (regexec(&pattern, line, 0, NULL, 0) == 0) {
                if (files > 1)
                    printf("%s:", strcmp(path, "-") == 0 ? "(standard input)" : path);
                puts(line);
                found = 1;
            }
        }
        if (ferror(file)) {
            perror(path);
            failed = 1;
        }
        if (file != stdin && fclose(file) == EOF)
            failed = 1;
    }
    free(line);
    regfree(&pattern);
    if (fflush(stdout) == EOF)
        failed = 1;
    return failed ? 2 : found ? 0 : 1;
}
