#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>

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
    int failed = fgets(message, sizeof(message), input) == NULL;
    if (fclose(input) == EOF)
        failed = 1;
    if (unlink(FIFO_PATH) == -1)
        failed = 1;
    if (failed) {
        fputs("Cannot receive message\n", stderr);
        return 1;
    }
    unsigned seconds = 10;
    while (seconds)
        seconds = sleep(seconds);
    time_t now = time(NULL);
    char *stamp = ctime(&now);
    if (now == (time_t)-1 || stamp == NULL)
        return 1;
    printf("Receiver: PID=%ld, time=%s", (long)getpid(), stamp);
    printf("Received: %s", message);
    return 0;
}
