#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static const char *role = "parent";

static void on_exit_message(void)
{
    printf("[atexit] %s: PID=%ld, PPID=%ld\n",
           role, (long)getpid(), (long)getppid());
}

/* write() is safe in a signal handler; printf() is not. */
static void write_signal_message(const char *message, size_t length)
{
    while (length > 0) {
        ssize_t written = write(STDOUT_FILENO, message, length);
        if (written < 0 && errno == EINTR) continue;
        if (written <= 0) break;
        message += written;
        length -= (size_t)written;
    }
}

static void on_signal(int signo)
{
    int saved_errno = errno;
    if (signo == SIGINT) {
        static const char msg[] =
            "[signal] SIGINT: interrupt received; continuing.\n";
        write_signal_message(msg, sizeof(msg) - 1);
    } else if (signo == SIGTERM) {
        static const char msg[] =
            "[signal] SIGTERM: termination request received; continuing.\n";
        write_signal_message(msg, sizeof(msg) - 1);
    }
    errno = saved_errno;
}

int main(void)
{
    /* Avoid duplicated buffered output after fork(). */
    if (setvbuf(stdout, NULL, _IONBF, 0) != 0) {
        fputs("Cannot configure stdout.\n", stderr);
        return EXIT_FAILURE;
    }
    if (atexit(on_exit_message) != 0) {
        fputs("Cannot register atexit handler.\n", stderr);
        return EXIT_FAILURE;
    }

    if (signal(SIGINT, on_signal) == SIG_ERR) {
        perror("signal(SIGINT)");
        return EXIT_FAILURE;
    }
    struct sigaction action = {0};
    action.sa_handler = on_signal;
    action.sa_flags = 0;
    if (sigemptyset(&action.sa_mask) == -1 ||
        sigaction(SIGTERM, &action, NULL) == -1) {
        perror("sigaction(SIGTERM)");
        return EXIT_FAILURE;
    }

    printf("Before fork: PID=%ld, PPID=%ld\n",
           (long)getpid(), (long)getppid());
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        return EXIT_FAILURE;
    }
    if (pid == 0) {
        role = "child";
        printf("Child: fork=0, PID=%ld, PPID=%ld\n",
               (long)getpid(), (long)getppid());
        puts("Child: sending SIGINT to itself.");
        if (raise(SIGINT) != 0) {
            fputs("Cannot raise SIGINT.\n", stderr);
            return EXIT_FAILURE;
        }
        puts("Child: sending SIGTERM to itself.");
        if (raise(SIGTERM) != 0) {
            fputs("Cannot raise SIGTERM.\n", stderr);
            return EXIT_FAILURE;
        }
        puts("Child: returning exit code 7.");
        return 7;
    }

    printf("Parent: fork=%ld, PID=%ld, PPID=%ld\n",
           (long)pid, (long)getpid(), (long)getppid());
    int status;
    pid_t waited;
    do {
        waited = waitpid(pid, &status, 0);
    } while (waited == -1 && errno == EINTR);
    if (waited == -1) {
        perror("waitpid");
        return EXIT_FAILURE;
    }
    if (WIFEXITED(status)) {
        printf("Parent: child PID=%ld exited with code %d.\n",
               (long)waited, WEXITSTATUS(status));
        return EXIT_SUCCESS;
    }
    if (WIFSIGNALED(status)) {
        printf("Parent: child PID=%ld terminated by signal %d.\n",
               (long)waited, WTERMSIG(status));
    }
    return EXIT_FAILURE;
}
