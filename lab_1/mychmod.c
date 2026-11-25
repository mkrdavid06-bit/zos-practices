#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>

static void file_error(const char *tool, const char *path)
{
    fprintf(stderr, "%s: %s: %s\n", tool, path, strerror(errno));
}

static mode_t spread(unsigned bits, unsigned who)
{
    return (mode_t)(((who & 1) ? bits << 6 : 0) |
                    ((who & 2) ? bits << 3 : 0) |
                    ((who & 4) ? bits : 0));
}

static int symbolic(const char *text, mode_t original, mode_t mask,
                    int directory, mode_t *result)
{
    mode_t current = original & 07777;
    const char *p = text;
    if (!*p) return -1;
    for (;;) {
        unsigned who = 0;
        while (*p && strchr("ugoa", *p)) {
            switch (*p++) {
            case 'u': who |= 1; break;
            case 'g': who |= 2; break;
            case 'o': who |= 4; break;
            case 'a': who |= 7; break;
            }
        }
        int omitted = who == 0;
        if (omitted) who = 7;
        if (!*p || !strchr("+-=", *p)) return -1;
        do {
            char operation = *p++;
            mode_t add = 0;
            int mentions_s = 0;
            if (*p && strchr("ugo", *p)) {
                unsigned bits = *p == 'u' ? (current >> 6) & 7 :
                                *p == 'g' ? (current >> 3) & 7 : current & 7;
                ++p; add = spread(bits, who);
            } else {
                while (*p && strchr("rwxXst", *p)) {
                    switch (*p++) {
                    case 'r': add |= spread(4, who); break;
                    case 'w': add |= spread(2, who); break;
                    case 'x': add |= spread(1, who); break;
                    case 'X': if (directory || (current & 0111)) add |= spread(1, who); break;
                    case 's':
                        mentions_s = 1;
                        if (who & 1) add |= S_ISUID;
                        if (who & 2) add |= S_ISGID;
                        break;
                    case 't': if (who & 4) add |= S_ISVTX; break;
                    }
                }
            }
            mode_t affected = spread(7, who);
            if (who & 1) affected |= S_ISUID;
            if (who & 2) affected |= S_ISGID;
            if (who & 4) affected |= S_ISVTX;
            if (omitted) {
                add &= ~(mask & 0777);
                if (operation != '=') affected &= ~(mask & 0777);
            }
            if (operation == '+') current |= add;
            else if (operation == '-') current &= ~add;
            else {
                
                if (directory && !mentions_s) affected &= ~(S_ISUID | S_ISGID);
                current = (current & ~affected) | add;
            }
        } while (*p && strchr("+-=", *p));
        if (!*p) break;
        if (*p++ != ',' || !*p) return -1;
    }
    *result = current;
    return 0;
}

int main(int argc, char **argv)
{
    int first = 1;
    if (first < argc && !strcmp(argv[first], "--")) ++first;
    if (argc - first < 2) {
        fprintf(stderr, "Usage: mychmod [--] MODE FILE ...\n"); return 1;
    }
    const char *mode = argv[first++];
    int numeric = mode[0] >= '0' && mode[0] <= '9';
    mode_t wanted = 0, dummy, mask = umask(0); umask(mask);
    if (numeric) {
        for (const char *p = mode; *p; ++p) {
            if (*p < '0' || *p > '7' || wanted > 07777 / 8) goto invalid;
            wanted = (mode_t)(wanted * 8 + (*p - '0'));
        }
        if (wanted > 07777) goto invalid;
    } else if (symbolic(mode, 0, mask, 0, &dummy) < 0) goto invalid;
    int status = 0;
    for (int i = first; i < argc; ++i) {
        struct stat st;
        if (stat(argv[i], &st) < 0) { file_error("mychmod", argv[i]); status = 1; continue; }
        mode_t target = wanted;
        if (!numeric) symbolic(mode, st.st_mode, mask, S_ISDIR(st.st_mode), &target);
        else if (S_ISDIR(st.st_mode) && strlen(mode) <= 4)
            target |= st.st_mode & (S_ISUID | S_ISGID);
        if (chmod(argv[i], target) < 0) { file_error("mychmod", argv[i]); status = 1; }
    }
    return status;
invalid:
    fprintf(stderr, "mychmod: invalid mode: '%s'\n", mode); return 1;
}
