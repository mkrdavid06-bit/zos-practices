#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>

int main(void)
{
    int fd[2];
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        perror("signal");
        return 1;
    }
    if (pipe(fd) == -1) {
        perror("pipe");
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
        if (fgets(message, sizeof(message), input) == NULL) {
            fputs("Cannot read message\n", stderr);
            fclose(input);
            return 1;
        }
        if (fclose(input) == EOF) {
            perror("fclose");
            return 1;
        }
        struct timespec delay = {5, 0};
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

    close(fd[0]);
    FILE *output = fdopen(fd[1], "w");
    int failed = 0;
    if (output == NULL) {
        perror("fdopen");
        close(fd[1]);
        failed = 1;
    } else {
        time_t now = time(NULL);
        struct tm *tm = localtime(&now);
        char stamp[32];
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
    }
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
