#include "common.h"
#include <fcntl.h>

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s FIFO_PATH\n", argv[0]);
        return EXIT_FAILURE;
    }
    const char *path = argv[1];
    /* Refuse existing paths, so no pre-existing file is removed. */
    if (mkfifo(path, 0600) == -1) { perror("mkfifo"); return EXIT_FAILURE; }
    printf("FIFO created: %s; waiting for sender.\n", path);
    fflush(stdout);
    int fd;
    do { fd = open(path, O_RDONLY); } while (fd == -1 && errno == EINTR);
    if (fd == -1) {
        perror("open"); unlink(path); return EXIT_FAILURE;
    }
    int result = receive_message(fd, 10);
    close(fd);
    if (unlink(path) == -1) { perror("unlink"); result = -1; }
    return result == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
