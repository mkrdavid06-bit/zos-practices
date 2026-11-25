#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>

#define FIFO_PATH "./myfifo.fifo"

int main(void)
{
    struct stat info;
    if (lstat(FIFO_PATH, &info) == -1) {
        perror("lstat");
        return 1;
    }
    if (!S_ISFIFO(info.st_mode)) {
        fputs("The path is not a FIFO\n", stderr);
        return 1;
    }
    FILE *input = fopen(FIFO_PATH, "r");
    if (input == NULL) {
        perror("fopen");
        return 1;
    }
    char message[256];
    int failed = 0;
    if (fgets(message, sizeof(message), input) == NULL) {
        fputs("Cannot read message\n", stderr);
        failed = 1;
    }
    if (fclose(input) == EOF) {
        perror("fclose");
        failed = 1;
    }
    if (unlink(FIFO_PATH) == -1) {
        perror("unlink");
        failed = 1;
    }
    if (failed)
        return 1;
    struct timespec delay = {10, 0};
    while (nanosleep(&delay, &delay) == -1) {
        if (errno != EINTR) {
            perror("nanosleep");
            return 1;
        }
    }
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    char stamp[32];
    if (now == (time_t)-1 || tm == NULL ||
        strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S", tm) == 0)
        return 1;
    printf("Receiver: PID=%ld, time=%s\n", (long)getpid(), stamp);
    printf("Received: %s", message);
    return 0;
}
