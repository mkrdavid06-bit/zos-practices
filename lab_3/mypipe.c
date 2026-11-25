#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>

int main(void)
{
    int fd[2];
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR || pipe(fd) == -1) {
        perror("pipe setup");
        return 1;
    }
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        close(fd[0]);
        close(fd[1]);
        return 1;
    }
    if (pid == 0) {
        close(fd[1]);
        FILE *input = fdopen(fd[0], "r");
        if (input == NULL) {
            perror("fdopen");
            close(fd[0]);
            return 1;
        }
        char message[256];
        int failed = fgets(message, sizeof(message), input) == NULL;
        if (fclose(input) == EOF)
            failed = 1;
        if (failed) {
            fputs("Cannot read message\n", stderr);
            return 1;
        }
        unsigned seconds = 5;
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

    close(fd[0]);
    time_t now = time(NULL);
    char *stamp = ctime(&now);
    int failed = now == (time_t)-1 || stamp == NULL;
    if (!failed && dprintf(fd[1], "Sender: PID=%ld, time=%s",
                           (long)getpid(), stamp) < 0)
        failed = 1;
    if (close(fd[1]) == -1)
        failed = 1;
    int status;
    pid_t result;
    do {
        result = waitpid(pid, &status, 0);
    } while (result == -1 && errno == EINTR);
    if (result == -1) {
        perror("waitpid");
        return 1;
    }
    return failed || !WIFEXITED(status) || WEXITSTATUS(status) != 0;
}
