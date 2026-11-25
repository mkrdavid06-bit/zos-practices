#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "Usage: mychmod MODE FILE ...\n");
        return 1;
    }
    char *mode = argv[1], *end;
    mode_t value = 0, who = 0, bits = 0;
    int numeric = mode[0] >= '0' && mode[0] <= '9';
    int omitted = 0;
    char operation = 0;

    if (numeric) {
        errno = 0;
        long number = strtol(mode, &end, 8);
        if (errno || *end || number < 0 || number > 07777) {
            fputs("Invalid mode\n", stderr);
            return 1;
        }
        value = (mode_t)number;
    } else {
        char *p = mode;
        while (*p && strchr("ugoa", *p)) {
            switch (*p++) {
            case 'u': who |= 0700; break;
            case 'g': who |= 0070; break;
            case 'o': who |= 0007; break;
            case 'a': who |= 0777; break;
            }
        }
        if (who == 0) {
            who = 0777;
            omitted = 1;
        }
        operation = *p;
        if (operation != '+' && operation != '-' && operation != '=') {
            fputs("Invalid operation\n", stderr);
            return 1;
        }
        p++;
        while (*p) {
            switch (*p++) {
            case 'r': bits |= 0444; break;
            case 'w': bits |= 0222; break;
            case 'x': bits |= 0111; break;
            default: fputs("Invalid permission\n", stderr); return 1;
            }
        }
        mode_t mask = umask(0);
        umask(mask);
        bits &= who;
        if (omitted) {
            bits &= ~mask;
            if (operation != '=')
                who &= ~mask;
        }
    }

    int failed = 0;
    for (int i = 2; i < argc; i++) {
        struct stat st;
        if (stat(argv[i], &st) == -1) {
            perror(argv[i]);
            failed = 1;
            continue;
        }
        mode_t result = value;
        if (!numeric) {
            result = st.st_mode & 07777;
            if (operation == '+') result |= bits;
            if (operation == '-') result &= ~bits;
            if (operation == '=') result = (result & ~who) | bits;
        }
        if (chmod(argv[i], result) == -1) {
            perror(argv[i]);
            failed = 1;
        }
    }
    return failed;
}
