#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include <signal.h>
#include <errno.h>

#define FIFO_PATH "./myfifo.fifo"

int main(void)
{
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        perror("signal");
        return 1;
    }
    if (mkfifo(FIFO_PATH, 0600) == -1) {
        perror("mkfifo");
        return 1;
    }
    puts("FIFO created. Start fifo_reader in another terminal.");
    fflush(stdout);
    FILE *output = fopen(FIFO_PATH, "w");
    if (output == NULL) {
        perror("fopen");
        unlink(FIFO_PATH);
        return 1;
    }
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    char stamp[32];
    int failed = 0;
    if (now == (time_t)-1 || tm == NULL ||
        strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S", tm) == 0) {
        failed = 1;
    } else if (fprintf(output, "Sender: PID=%ld, time=%s\n",
                       (long)getpid(), stamp) < 0) {
        perror("fprintf");
        failed = 1;
    }
    if (fclose(output) == EOF) {
        perror("fclose");
        failed = 1;
    }
    if (failed)
        unlink(FIFO_PATH);
    return failed;
}
