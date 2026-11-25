#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>

static const char *role = "parent";

static void finish(void)
{
    printf("atexit: PID=%ld, PPID=%ld, role=%s\n",
           (long)getpid(), (long)getppid(), role);
}

static void handle_signal(int sig)
{
    int saved_errno = errno;
    const char *message;
    size_t length;

    if (sig == SIGINT) {
        message = "SIGINT: interrupt received\n";
        length = sizeof("SIGINT: interrupt received\n") - 1;
    } else {
        message = "SIGTERM: termination request received\n";
        length = sizeof("SIGTERM: termination request received\n") - 1;
    }

    while (length > 0) {
        ssize_t n = write(STDOUT_FILENO, message, length);
        if (n < 0 && errno == EINTR)
            continue;
        if (n <= 0)
            break;
        message += n;
        length -= (size_t)n;
    }
    errno = saved_errno;
}

int main(void)
{
    if (atexit(finish) != 0) {
        fputs("atexit failed\n", stderr);
        return 1;
    }
    if (signal(SIGINT, handle_signal) == SIG_ERR) {
        perror("signal");
        return 1;
    }

    struct sigaction action = {0};
    action.sa_handler = handle_signal;
    if (sigemptyset(&action.sa_mask) == -1 ||
        sigaction(SIGTERM, &action, NULL) == -1) {
        perror("sigaction");
        return 1;
    }

    pid_t pid = fork();
    switch (pid) {
    case -1:
        perror("fork");
        return 1;
    case 0:
        role = "child";
        printf("child: PID=%ld, PPID=%ld\n", (long)getpid(), (long)getppid());
        if (fflush(stdout) == EOF) {
            perror("fflush");
            return 1;
        }
        if (raise(SIGINT) != 0 || raise(SIGTERM) != 0) {
            fputs("raise failed\n", stderr);
            return 1;
        }
        return 7;
    default:
        printf("parent: PID=%ld, PPID=%ld, child PID=%ld\n",
               (long)getpid(), (long)getppid(), (long)pid);
        int status;
        pid_t result;
        do {
            result = waitpid(pid, &status, 0);
        } while (result == -1 && errno == EINTR);
        if (result == -1) {
            perror("waitpid");
            return 1;
        }
        if (WIFEXITED(status)) {
            printf("child exit code: %d\n", WEXITSTATUS(status));
            return 0;
        }
        if (WIFSIGNALED(status))
            printf("child terminated by signal: %d\n", WTERMSIG(status));
        return 1;
    }
}
