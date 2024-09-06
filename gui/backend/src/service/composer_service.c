#define _POSIX_C_SOURCE 200809L

#include "composer_service.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

extern char **environ;

#define DEFAULT_TIMEOUT_SECONDS 120
#define MAX_CAPTURED_BYTES (16u * 1024u * 1024u)

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} Capture;

static bool capture_init(Capture *capture) {
    capture->length = 0;
    capture->capacity = 4096;
    capture->data = malloc(capture->capacity);
    if (capture->data) {
        capture->data[0] = '\0';
    }
    return capture->data != NULL;
}

/* Appends a chunk; output past MAX_CAPTURED_BYTES is dropped. False only on
   allocation failure. */
static bool capture_append(Capture *capture, const char *chunk, size_t size) {
    if (capture->length >= MAX_CAPTURED_BYTES) {
        return true;
    }
    if (size > MAX_CAPTURED_BYTES - capture->length) {
        size = MAX_CAPTURED_BYTES - capture->length;
    }
    if (capture->length + size + 1 > capture->capacity) {
        size_t capacity = capture->capacity;
        while (capture->length + size + 1 > capacity) {
            capacity *= 2;
        }
        char *grown = realloc(capture->data, capacity);
        if (!grown) {
            return false;
        }
        capture->data = grown;
        capture->capacity = capacity;
    }
    memcpy(capture->data + capture->length, chunk, size);
    capture->length += size;
    capture->data[capture->length] = '\0';
    return true;
}

const char *ursatz_gui_composer_binary(void) {
    const char *path = getenv("URSATZ_GUI_COMPOSER");
#ifdef URSATZ_GUI_COMPOSER_BIN
    if (!path || path[0] == '\0') {
        path = URSATZ_GUI_COMPOSER_BIN;
    }
#endif
    if (!path || path[0] == '\0' || access(path, X_OK) != 0) {
        return NULL;
    }
    return path;
}

static int timeout_seconds(void) {
    const char *text = getenv("URSATZ_GUI_COMPOSER_TIMEOUT");
    if (text && text[0] != '\0') {
        char *end = NULL;
        long value = strtol(text, &end, 10);
        if (end && *end == '\0' && value > 0 && value < 86400) {
            return (int)value;
        }
    }
    return DEFAULT_TIMEOUT_SECONDS;
}

static long elapsed_ms(const struct timespec *since) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (long)(now.tv_sec - since->tv_sec) * 1000 + (now.tv_nsec - since->tv_nsec) / 1000000;
}

static void close_fd(int *fd) {
    if (*fd >= 0) {
        close(*fd);
        *fd = -1;
    }
}

static bool make_pipe(int fds[2]) {
    if (pipe(fds) != 0) {
        return false;
    }
    /* A close-on-exec pipe is not inherited by commands that other request
       threads start at the same time; dup2 in the child clears the flag on
       the descriptor that becomes stdout or stderr. */
    fcntl(fds[0], F_SETFD, FD_CLOEXEC);
    fcntl(fds[1], F_SETFD, FD_CLOEXEC);
    return true;
}

void ursatz_gui_composer_output_destroy(UrsatzGuiComposerOutput *out) {
    if (!out) {
        return;
    }
    free(out->stdout_text);
    free(out->stderr_text);
    out->stdout_text = NULL;
    out->stderr_text = NULL;
    out->exit_status = 0;
}

UrsatzGuiComposerResult ursatz_gui_composer_run(const char *const *args, UrsatzGuiComposerOutput *out) {
    out->exit_status = 0;
    out->stdout_text = NULL;
    out->stderr_text = NULL;

    const char *binary = ursatz_gui_composer_binary();
    if (!binary) {
        return URSATZ_GUI_COMPOSER_ERR_NO_BINARY;
    }

    size_t arg_count = 0;
    while (args && args[arg_count]) {
        arg_count++;
    }
    char **argv = malloc((arg_count + 2) * sizeof(char *));
    if (!argv) {
        return URSATZ_GUI_COMPOSER_ERR_ALLOC;
    }
    argv[0] = (char *)binary;
    for (size_t i = 0; i < arg_count; i++) {
        argv[i + 1] = (char *)args[i];
    }
    argv[arg_count + 1] = NULL;

    int out_pipe[2] = {-1, -1};
    int err_pipe[2] = {-1, -1};
    posix_spawn_file_actions_t actions;
    bool actions_ready = false;
    Capture out_capture = {NULL, 0, 0};
    Capture err_capture = {NULL, 0, 0};
    UrsatzGuiComposerResult result = URSATZ_GUI_COMPOSER_ERR_SPAWN_FAILED;
    pid_t pid = -1;

    if (!capture_init(&out_capture) || !capture_init(&err_capture)) {
        result = URSATZ_GUI_COMPOSER_ERR_ALLOC;
        goto done;
    }
    if (!make_pipe(out_pipe) || !make_pipe(err_pipe)) {
        goto done;
    }
    if (posix_spawn_file_actions_init(&actions) != 0) {
        goto done;
    }
    actions_ready = true;
    if (posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, "/dev/null", O_RDONLY, 0) != 0 ||
        posix_spawn_file_actions_adddup2(&actions, out_pipe[1], STDOUT_FILENO) != 0 ||
        posix_spawn_file_actions_adddup2(&actions, err_pipe[1], STDERR_FILENO) != 0) {
        goto done;
    }
    if (posix_spawn(&pid, binary, &actions, NULL, argv, environ) != 0) {
        pid = -1;
        goto done;
    }
    close_fd(&out_pipe[1]);
    close_fd(&err_pipe[1]);

    struct timespec started;
    clock_gettime(CLOCK_MONOTONIC, &started);
    long limit_ms = (long)timeout_seconds() * 1000;
    bool timed_out = false;
    bool alloc_failed = false;

    while (out_pipe[0] >= 0 || err_pipe[0] >= 0) {
        long remaining = limit_ms - elapsed_ms(&started);
        if (remaining <= 0) {
            timed_out = true;
            break;
        }
        struct pollfd fds[2];
        int nfds = 0;
        int which[2];
        if (out_pipe[0] >= 0) {
            fds[nfds].fd = out_pipe[0];
            fds[nfds].events = POLLIN;
            which[nfds++] = 0;
        }
        if (err_pipe[0] >= 0) {
            fds[nfds].fd = err_pipe[0];
            fds[nfds].events = POLLIN;
            which[nfds++] = 1;
        }
        int ready = poll(fds, (nfds_t)nfds, (int)(remaining > 1000 ? 1000 : remaining));
        if (ready < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }
        for (int i = 0; i < nfds; i++) {
            if (!(fds[i].revents & (POLLIN | POLLHUP | POLLERR))) {
                continue;
            }
            int *fd = which[i] == 0 ? &out_pipe[0] : &err_pipe[0];
            Capture *capture = which[i] == 0 ? &out_capture : &err_capture;
            char chunk[4096];
            ssize_t n = read(*fd, chunk, sizeof(chunk));
            if (n > 0) {
                if (!capture_append(capture, chunk, (size_t)n)) {
                    alloc_failed = true;
                }
            } else if (n == 0 || errno != EINTR) {
                close_fd(fd);
            }
        }
        if (alloc_failed) {
            break;
        }
    }

    if (timed_out || alloc_failed) {
        kill(pid, SIGKILL);
    }
    int status = 0;
    while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {
    }
    pid = -1;

    if (timed_out) {
        result = URSATZ_GUI_COMPOSER_ERR_TIMEOUT;
        goto done;
    }
    if (alloc_failed) {
        result = URSATZ_GUI_COMPOSER_ERR_ALLOC;
        goto done;
    }

    out->exit_status = WIFEXITED(status) ? WEXITSTATUS(status) : 128 + (WIFSIGNALED(status) ? WTERMSIG(status) : 0);
    out->stdout_text = out_capture.data;
    out->stderr_text = err_capture.data;
    out_capture.data = NULL;
    err_capture.data = NULL;
    result = URSATZ_GUI_COMPOSER_OK;

done:
    if (pid > 0) {
        kill(pid, SIGKILL);
        while (waitpid(pid, NULL, 0) < 0 && errno == EINTR) {
        }
    }
    if (actions_ready) {
        posix_spawn_file_actions_destroy(&actions);
    }
    close_fd(&out_pipe[0]);
    close_fd(&out_pipe[1]);
    close_fd(&err_pipe[0]);
    close_fd(&err_pipe[1]);
    free(out_capture.data);
    free(err_capture.data);
    free(argv);
    return result;
}
