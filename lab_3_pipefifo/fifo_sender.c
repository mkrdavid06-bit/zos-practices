#include "common.h"
#include <fcntl.h>

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s FIFO_PATH\n", argv[0]);
        return EXIT_FAILURE;
    }
    struct stat info;
    if (lstat(argv[1], &info) == -1) { perror("lstat"); return EXIT_FAILURE; }
    if (!S_ISFIFO(info.st_mode)) {
        fputs("The path must refer to a FIFO.\n", stderr);
        return EXIT_FAILURE;
    }
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        perror("signal"); return EXIT_FAILURE;
    }
    int fd;
    do { fd = open(argv[1], O_WRONLY); } while (fd == -1 && errno == EINTR);
    if (fd == -1) { perror("open"); return EXIT_FAILURE; }
    int result = send_message(fd);
    close(fd);
    return result == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
