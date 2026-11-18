#define _XOPEN_SOURCE 700
#define _DARWIN_C_SOURCE 1
#include "common.h"
#include <dirent.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <locale.h>
#include <wchar.h>
#ifdef __linux__
#include <sys/sysmacros.h>
#elif defined(__APPLE__)
#include <sys/types.h>
#endif

typedef struct {
    char *name, *path;
    struct stat st;
} Entry;
typedef struct { size_t links, owner, group, size; } Widths;
static int status, use_color;

static void *allocate(size_t size)
{
    void *p = malloc(size);
    if (!p) { perror("myls: malloc"); exit(1); }
    return p;
}

static char *copy(const char *s)
{
    char *p = allocate(strlen(s) + 1); strcpy(p, s); return p;
}

static int compare(const void *a, const void *b)
{
    const Entry *x = a, *y = b;
    int c = strcoll(x->name, y->name);
    return c ? c : strcmp(x->name, y->name);
}

static size_t display_width(const char *s)
{
    mbstate_t state = {0}; size_t n = 0;
    while (*s) {
        wchar_t wc;
        size_t k = mbrtowc(&wc, s, MB_CUR_MAX, &state);
        if (k == (size_t)-1 || k == (size_t)-2) {
            memset(&state, 0, sizeof(state)); ++s; ++n;
        } else {
            int w = wcwidth(wc); n += w > 0 ? (size_t)w : 0; s += k;
        }
    }
    return n;
}

static void permissions(mode_t m, char out[11])
{
    out[0] = S_ISDIR(m) ? 'd' : S_ISLNK(m) ? 'l' : S_ISCHR(m) ? 'c' :
             S_ISBLK(m) ? 'b' : S_ISFIFO(m) ? 'p' : S_ISSOCK(m) ? 's' : '-';
    const char *bits = "rwxrwxrwx";
    for (int i = 0; i < 9; ++i) out[i + 1] = (m & (0400 >> i)) ? bits[i] : '-';
    if (m & S_ISUID) out[3] = m & S_IXUSR ? 's' : 'S';
    if (m & S_ISGID) out[6] = m & S_IXGRP ? 's' : 'S';
    if (m & S_ISVTX) out[9] = m & S_IXOTH ? 't' : 'T';
    out[10] = '\0';
}

static void identity(const Entry *e, char owner[256], char group[256])
{
    struct passwd *pw = getpwuid(e->st.st_uid);
    if (pw) snprintf(owner, 256, "%s", pw->pw_name);
    else snprintf(owner, 256, "%lu", (unsigned long)e->st.st_uid);
    struct group *gr = getgrgid(e->st.st_gid);
    if (gr) snprintf(group, 256, "%s", gr->gr_name);
    else snprintf(group, 256, "%lu", (unsigned long)e->st.st_gid);
}

static void size_string(const Entry *e, char out[64])
{
    if (S_ISCHR(e->st.st_mode) || S_ISBLK(e->st.st_mode))
        snprintf(out, 64, "%u, %u", (unsigned)major(e->st.st_rdev), (unsigned)minor(e->st.st_rdev));
    else snprintf(out, 64, "%lld", (long long)e->st.st_size);
}

static void name_print(const Entry *e)
{
    const char *color = S_ISLNK(e->st.st_mode) ? "36" : S_ISDIR(e->st.st_mode) ? "34" :
        S_ISREG(e->st.st_mode) && (e->st.st_mode & 0111) ? "32" : NULL;
    if (use_color && color) printf("\033[%sm", color);
    fputs(e->name, stdout);
    if (use_color && color) fputs("\033[0m", stdout);
}

static void print_long(const Entry *e, Widths w)
{
    char mode[11], owner[256], group[256], size[64], date[128];
    permissions(e->st.st_mode, mode); identity(e, owner, group); size_string(e, size);
    time_t now = time(NULL);
    struct tm *tm = localtime(&e->st.st_mtime);
    double age = difftime(now, e->st.st_mtime);
    if (tm) strftime(date, sizeof(date), age < -3600 || age > 15778476 ?
                      "%b %e  %Y" : "%b %e %H:%M", tm);
    else strcpy(date, "??? ??  ????");
    printf("%s %*llu %s", mode, (int)w.links, (unsigned long long)e->st.st_nlink, owner);
    for (size_t i = display_width(owner); i < w.owner; ++i) putchar(' ');
    printf(" %s", group);
    for (size_t i = display_width(group); i < w.group; ++i) putchar(' ');
    printf(" %*s %s ", (int)w.size, size, date);
    name_print(e);
    if (S_ISLNK(e->st.st_mode)) {
        size_t cap = e->st.st_size > 0 ? (size_t)e->st.st_size + 1 : 256;
        for (;;) {
            char *target = allocate(cap + 1);
            ssize_t n = readlink(e->path, target, cap);
            if (n < 0) { file_error("myls", e->path); status = 1; free(target); break; }
            if ((size_t)n < cap) { target[n] = '\0'; printf(" -> %s", target); free(target); break; }
            free(target); cap *= 2;
        }
    }
    putchar('\n');
}

static void print_entries(Entry *entries, size_t n, int long_form, int directory)
{
    if (n > 1) qsort(entries, n, sizeof(*entries), compare);
    Widths w = {1, 0, 0, 1}; unsigned long long blocks = 0;
    for (size_t i = 0; i < n; ++i) {
        char owner[256], group[256], size[64], links[32];
        identity(&entries[i], owner, group); size_string(&entries[i], size);
        snprintf(links, sizeof(links), "%llu", (unsigned long long)entries[i].st.st_nlink);
        if (strlen(links) > w.links) w.links = strlen(links);
        if (display_width(owner) > w.owner) w.owner = display_width(owner);
        if (display_width(group) > w.group) w.group = display_width(group);
        if (strlen(size) > w.size) w.size = strlen(size);
        blocks += (unsigned long long)entries[i].st.st_blocks;
    }
    if (long_form && directory) {
#ifdef __APPLE__
        printf("total %llu\n", blocks);
#else
        printf("total %llu\n", (blocks + 1) / 2);
#endif
    }
    if (long_form) {
        for (size_t i = 0; i < n; ++i) print_long(&entries[i], w);
    } else if (!isatty(STDOUT_FILENO)) {
        for (size_t i = 0; i < n; ++i) { name_print(&entries[i]); putchar('\n'); }
    } else if (n) {
        struct winsize ws; size_t width = 80;
        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col) width = ws.ws_col;
        size_t cols = n, rows = 0;
        size_t *widths = allocate(n * sizeof(*widths));
        /* ls fills columns vertically. Choose the largest fitting layout. */
        for (; cols; --cols) {
            rows = (n + cols - 1) / cols;
            memset(widths, 0, n * sizeof(*widths));
            for (size_t i = 0; i < n; ++i) {
                size_t col = i / rows, k = display_width(entries[i].name);
                if (k > widths[col]) widths[col] = k;
            }
            size_t total = cols > 1 ? 2 * (cols - 1) : 0;
            for (size_t j = 0; j < cols; ++j) total += widths[j];
            if (total <= width || cols == 1) break;
        }
        for (size_t row = 0; row < rows; ++row) {
            for (size_t col = 0; col < cols && col * rows + row < n; ++col) {
                size_t i = col * rows + row; name_print(&entries[i]);
                if ((col + 1) * rows + row < n)
                    for (size_t k = display_width(entries[i].name); k < widths[col] + 2; ++k) putchar(' ');
            }
            putchar('\n');
        }
        free(widths);
    }
}

static void list_directory(const char *path, int all, int long_form)
{
    DIR *dir = opendir(path);
    if (!dir) { file_error("myls", path); status = 1; return; }
    Entry *entries = NULL; size_t n = 0;
    for (;;) {
        errno = 0; struct dirent *de = readdir(dir);
        if (!de) { if (errno) { file_error("myls", path); status = 1; } break; }
        if (!all && de->d_name[0] == '.') continue;
        char *full = allocate(strlen(path) + strlen(de->d_name) + 2);
        sprintf(full, "%s/%s", path, de->d_name);
        struct stat st;
        if (lstat(full, &st) < 0) { file_error("myls", full); status = 1; free(full); continue; }
        Entry *next = realloc(entries, (n + 1) * sizeof(*entries));
        if (!next) { perror("myls: realloc"); exit(1); }
        entries = next; entries[n++] = (Entry){copy(de->d_name), full, st};
    }
    if (closedir(dir) < 0) { file_error("myls", path); status = 1; }
    print_entries(entries, n, long_form, 1);
    for (size_t i = 0; i < n; ++i) { free(entries[i].name); free(entries[i].path); }
    free(entries);
}

int main(int argc, char **argv)
{
    setlocale(LC_ALL, "");
    char **args = flag_arguments(argc, argv); int count = 0;
    while (args[count]) ++count;
    int all = 0, long_form = 0, opt;
    while ((opt = getopt(count, args, "la")) != -1) {
        if (opt == 'l') long_form = 1;
        else if (opt == 'a') all = 1;
        else { fprintf(stderr, "Usage: myls [-la] [PATH ...]\n"); free(args); return 1; }
    }
    use_color = isatty(STDOUT_FILENO);
    int operands = count - optind;
    if (!operands) { list_directory(".", all, long_form); free(args); return status; }
    Entry *files = allocate((size_t)operands * sizeof(*files));
    Entry *dirs = allocate((size_t)operands * sizeof(*dirs)); size_t nf = 0, nd = 0;
    for (int i = optind; i < count; ++i) {
        struct stat st;
        if (lstat(args[i], &st) < 0) { file_error("myls", args[i]); status = 1; continue; }
        /* Without -l, a command-line symlink to a directory is followed. */
        if (!long_form && S_ISLNK(st.st_mode)) {
            struct stat target;
            if (stat(args[i], &target) == 0 && S_ISDIR(target.st_mode)) st = target;
        }
        Entry e = {args[i], args[i], st};
        if (S_ISDIR(st.st_mode)) dirs[nd++] = e; else files[nf++] = e;
    }
    print_entries(files, nf, long_form, 0);
    qsort(dirs, nd, sizeof(*dirs), compare);
    for (size_t i = 0; i < nd; ++i) {
        if (nf || i) putchar('\n');
        if (operands > 1) printf("%s:\n", dirs[i].name);
        list_directory(dirs[i].path, all, long_form);
    }
    free(files); free(dirs); free(args);
    if (fflush(stdout) == EOF || ferror(stdout)) { file_error("myls", "stdout"); status = 1; }
    return status;
}
