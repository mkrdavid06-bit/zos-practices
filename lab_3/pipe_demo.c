#include "common.h"
#include <sys/wait.h>

int main(void)
{
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        perror("signal"); return EXIT_FAILURE;
    }
    int fd[2];
    if (pipe(fd) == -1) { perror("pipe"); return EXIT_FAILURE; }
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork"); close(fd[0]); close(fd[1]);
        return EXIT_FAILURE;
    }
    if (pid == 0) {
        close(fd[1]);
        int result = receive_message(fd[0], 5);
        close(fd[0]);
        return result == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    close(fd[0]);
    int sent = send_message(fd[1]);
    close(fd[1]); /* EOF lets the child finish reading. */
    int status;
    pid_t waited;
    do { waited = waitpid(pid, &status, 0); }
    while (waited == -1 && errno == EINTR);
    if (waited == -1) { perror("waitpid"); return EXIT_FAILURE; }
    if (sent != 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0)
        return EXIT_FAILURE;
    return EXIT_SUCCESS;
}
