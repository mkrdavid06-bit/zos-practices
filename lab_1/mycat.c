#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    int flag_n = 0, flag_b = 0, flag_E = 0, opt;
    while ((opt = getopt(argc, argv, "nbE")) != -1) {
        switch (opt) {
        case 'n': flag_n = 1; break;
        case 'b': flag_b = 1; break;
        case 'E': flag_E = 1; break;
        default: return 1;
        }
    }

    int line = 1, start = 1, failed = 0;
    int files = argc - optind;
    for (int i = 0; i < (files ? files : 1); i++) {
        char *path = files ? argv[optind + i] : "-";
        FILE *file = strcmp(path, "-") == 0 ? stdin : fopen(path, "r");
        if (file == NULL) {
            perror(path);
            failed = 1;
            continue;
        }
        int c;
        while ((c = fgetc(file)) != EOF) {
            if (start && ((flag_b && c != '\n') || (flag_n && !flag_b)))
                printf("%6d\t", line++);
            if (flag_E && c == '\n')
                putchar('$');
            putchar(c);
            start = c == '\n';
        }
        if (ferror(file)) {
            perror(path);
            failed = 1;
        }
        if (file != stdin && fclose(file) == EOF) {
            perror(path);
            failed = 1;
        }
    }
    if (fflush(stdout) == EOF)
        failed = 1;
    return failed;
}
