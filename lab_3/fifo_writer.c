#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include <signal.h>

#define FIFO_PATH "./myfifo.fifo"

int main(void)
{
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR || mkfifo(FIFO_PATH, 0600) == -1) {
        perror("FIFO setup");
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
    char *stamp = ctime(&now);
    int failed = now == (time_t)-1 || stamp == NULL;
    if (!failed && fprintf(output, "Sender: PID=%ld, time=%s",
                           (long)getpid(), stamp) < 0)
        failed = 1;
    if (fclose(output) == EOF)
        failed = 1;
    if (failed)
        unlink(FIFO_PATH);
    return failed;
}
