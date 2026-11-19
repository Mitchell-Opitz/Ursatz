#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <unistd.h>

#include <common/files.h>

#include "workshop_internal.h"

#define LOCK_NAME ".ursatz.lock"

UrsatzStatus workshop_data_lock_acquire(const char *data_dir, int *descriptor, UrsatzError *error) {
    *descriptor = -1;
    char path[1100];
    if (!common_path_join(path, sizeof(path), data_dir, LOCK_NAME)) {
        return ursatz_error_set(error, URSATZ_ERR_REQUEST, data_dir, "the data folder path is too long");
    }
    int file = open(path, O_RDWR | O_CREAT | O_CLOEXEC, 0644);
    if (file < 0) {
        return ursatz_error_set(error, URSATZ_ERR_IO, path, "cannot open the lock file %s: %s", path, strerror(errno));
    }
    if (flock(file, LOCK_EX | LOCK_NB) != 0) {
        int busy = errno == EWOULDBLOCK;
        char holder[32] = "";
        ssize_t got = pread(file, holder, sizeof(holder) - 1, 0);
        holder[got > 0 ? got : 0] = '\0';
        close(file);
        if (!busy) {
            return ursatz_error_set(error, URSATZ_ERR_IO, path, "cannot lock %s: %s", path, strerror(errno));
        }
        long pid = strtol(holder, NULL, 10);
        if (pid > 0) {
            return ursatz_error_set(error, URSATZ_ERR_CONFLICT, data_dir, "the data folder %s is in use by another Ursatz process (pid %ld)",
                                    data_dir, pid);
        }
        return ursatz_error_set(error, URSATZ_ERR_CONFLICT, data_dir, "the data folder %s is in use by another Ursatz process", data_dir);
    }
    char pid_text[32];
    int length = snprintf(pid_text, sizeof(pid_text), "%ld\n", (long)getpid());
    if (ftruncate(file, 0) != 0 || pwrite(file, pid_text, (size_t)length, 0) != length) {
        flock(file, LOCK_UN);
        close(file);
        return ursatz_error_set(error, URSATZ_ERR_IO, path, "cannot write the lock file %s: %s", path, strerror(errno));
    }
    *descriptor = file;
    return URSATZ_OK;
}

void workshop_data_lock_release(int descriptor) {
    if (descriptor >= 0) {
        flock(descriptor, LOCK_UN);
        close(descriptor);
    }
}
