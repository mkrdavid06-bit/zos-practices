#ifndef LAB_COMMON_H
#define LAB_COMMON_H
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

static inline int show_time(const char *label)
{
    time_t now = time(NULL);
    struct tm local;
    char stamp[32];
    if (now == (time_t)-1 || localtime_r(&now, &local) == NULL ||
        strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S", &local) == 0) {
        fputs("Cannot format current time.\n", stderr);
        return -1;
    }
    printf("%s: time=%s, PID=%ld\n", label, stamp, (long)getpid());
    return 0;
}

static inline int send_message(int fd)
{
    time_t now = time(NULL);
    struct tm local;
    char stamp[32], message[256];
    if (now == (time_t)-1 || localtime_r(&now, &local) == NULL ||
        strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S", &local) == 0)
        return -1;
    int n = snprintf(message, sizeof(message),
                     "Sender: time=%s, PID=%ld\n", stamp, (long)getpid());
    if (n < 0 || (size_t)n >= sizeof(message)) return -1;
    size_t done = 0;
    while (done < (size_t)n) {
        ssize_t written = write(fd, message + done, (size_t)n - done);
        if (written < 0 && errno == EINTR) continue;
        if (written <= 0) { perror("write"); return -1; }
        done += (size_t)written;
    }
    printf("Sent: %s", message);
    return 0;
}

static inline int receive_message(int fd, unsigned delay)
{
    char message[256];
    size_t used = 0;
    for (;;) {
        if (used == sizeof(message) - 1) {
            fputs("Message is too long.\n", stderr);
            return -1;
        }
        ssize_t n = read(fd, message + used, sizeof(message) - 1 - used);
        if (n < 0 && errno == EINTR) continue;
        if (n < 0) { perror("read"); return -1; }
        if (n == 0) break;
        used += (size_t)n;
    }
    if (used == 0) { fputs("Empty message.\n", stderr); return -1; }
    message[used] = '\0';
    struct timespec remaining = {(time_t)delay, 0};
    while (nanosleep(&remaining, &remaining) == -1) {
        if (errno != EINTR) { perror("nanosleep"); return -1; }
    }
    if (show_time("Receiver") == -1) return -1;
    printf("Received: %s", message);
    return 0;
}
#endif
