#include "common/files.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define PATH_BYTES 4096

static UrsatzStatus io_error(UrsatzError *error, const char *path, const char *what) {
    int code = errno;
    UrsatzStatus status = code == ENOENT ? URSATZ_ERR_NOT_FOUND : URSATZ_ERR_IO;
    return ursatz_error_set(error, status, path, "%s %s: %s", what, path, strerror(code));
}

bool common_path_exists(const char *path) {
    struct stat info;
    return stat(path, &info) == 0;
}

bool common_path_is_dir(const char *path) {
    struct stat info;
    return stat(path, &info) == 0 && S_ISDIR(info.st_mode);
}

bool common_path_join(char *out, size_t capacity, const char *a, const char *b) {
    size_t length = strlen(a);
    bool slash = length > 0 && a[length - 1] == '/';
    int written = snprintf(out, capacity, "%s%s%s", a, slash ? "" : "/", b);
    return written >= 0 && (size_t)written < capacity;
}

UrsatzStatus common_file_read(const char *path, char **data, size_t *size, UrsatzError *error) {
    *data = NULL;
    if (size) {
        *size = 0;
    }
    FILE *file = fopen(path, "rb");
    if (!file) {
        return io_error(error, path, "cannot open");
    }
    size_t capacity = 4096;
    size_t length = 0;
    char *buffer = malloc(capacity);
    while (buffer) {
        size_t got = fread(buffer + length, 1, capacity - length - 1, file);
        length += got;
        if (got == 0 || feof(file) || ferror(file)) {
            break;
        }
        if (length + 1 >= capacity) {
            char *grown = realloc(buffer, capacity * 2);
            if (!grown) {
                free(buffer);
                buffer = NULL;
                break;
            }
            buffer = grown;
            capacity *= 2;
        }
    }
    bool failed = ferror(file) != 0;
    fclose(file);
    if (!buffer) {
        return ursatz_error_set(error, URSATZ_ERR_ALLOC, path, "out of memory reading %s", path);
    }
    if (failed) {
        free(buffer);
        return ursatz_error_set(error, URSATZ_ERR_IO, path, "cannot read %s", path);
    }
    buffer[length] = '\0';
    *data = buffer;
    if (size) {
        *size = length;
    }
    return URSATZ_OK;
}

/* The folder part of path ("." when it has none). */
static bool parent_of(const char *path, char *out, size_t capacity) {
    const char *slash = strrchr(path, '/');
    if (!slash) {
        return snprintf(out, capacity, ".") < (int)capacity;
    }
    size_t length = slash == path ? 1 : (size_t)(slash - path);
    if (length + 1 > capacity) {
        return false;
    }
    memcpy(out, path, length);
    out[length] = '\0';
    return true;
}

UrsatzStatus common_dir_sync(const char *path, UrsatzError *error) {
    int descriptor = open(path, O_RDONLY);
    if (descriptor < 0) {
        return io_error(error, path, "cannot open folder");
    }
    int result = fsync(descriptor);
    int code = errno;
    close(descriptor);
    if (result != 0) {
        errno = code;
        return io_error(error, path, "cannot sync folder");
    }
    return URSATZ_OK;
}

UrsatzStatus common_file_write_atomic(const char *path, const void *data, size_t size, UrsatzError *error) {
    char temporary[PATH_BYTES];
    char folder[PATH_BYTES];
    if (snprintf(temporary, sizeof(temporary), "%s.tmp-%ld", path, (long)getpid()) >= (int)sizeof(temporary) ||
        !parent_of(path, folder, sizeof(folder))) {
        return ursatz_error_set(error, URSATZ_ERR_REQUEST, path, "the path %s is too long", path);
    }
    FILE *file = fopen(temporary, "wb");
    if (!file) {
        return io_error(error, path, "cannot write");
    }
    bool ok = size == 0 || fwrite(data, 1, size, file) == size;
    ok = fflush(file) == 0 && ok;
    ok = fsync(fileno(file)) == 0 && ok;
    ok = fclose(file) == 0 && ok;
    if (!ok) {
        UrsatzStatus status = io_error(error, path, "cannot write");
        remove(temporary);
        return status;
    }
    if (rename(temporary, path) != 0) {
        UrsatzStatus status = io_error(error, path, "cannot replace");
        remove(temporary);
        return status;
    }
    return common_dir_sync(folder, error);
}

UrsatzStatus common_file_append_sync(const char *path, const void *data, size_t size, UrsatzError *error) {
    FILE *file = fopen(path, "ab");
    if (!file) {
        return io_error(error, path, "cannot append to");
    }
    bool ok = size == 0 || fwrite(data, 1, size, file) == size;
    ok = fflush(file) == 0 && ok;
    ok = fsync(fileno(file)) == 0 && ok;
    ok = fclose(file) == 0 && ok;
    return ok ? URSATZ_OK : io_error(error, path, "cannot append to");
}

UrsatzStatus common_dir_make(const char *path, UrsatzError *error) {
    char partial[PATH_BYTES];
    size_t length = strlen(path);
    if (length == 0 || length >= sizeof(partial)) {
        return ursatz_error_set(error, URSATZ_ERR_REQUEST, path, "the folder path \"%s\" is empty or too long", path);
    }
    memcpy(partial, path, length + 1);
    for (size_t i = 1; i <= length; i++) {
        if (partial[i] != '/' && partial[i] != '\0') {
            continue;
        }
        char saved = partial[i];
        partial[i] = '\0';
        if (mkdir(partial, 0755) != 0 && errno != EEXIST) {
            return io_error(error, partial, "cannot create folder");
        }
        partial[i] = saved;
    }
    if (!common_path_is_dir(path)) {
        return ursatz_error_set(error, URSATZ_ERR_CONFLICT, path, "%s exists and is not a folder", path);
    }
    return URSATZ_OK;
}

UrsatzStatus common_dir_list(const char *path, CommonNames *out, UrsatzError *error) {
    out->items = NULL;
    out->count = 0;
    out->capacity = 0;
    DIR *directory = opendir(path);
    if (!directory) {
        return io_error(error, path, "cannot list");
    }
    struct dirent *entry;
    while ((entry = readdir(directory)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        if (!common_names_add(out, entry->d_name)) {
            closedir(directory);
            common_names_free(out);
            return ursatz_error_set(error, URSATZ_ERR_ALLOC, path, "out of memory listing %s", path);
        }
    }
    closedir(directory);
    common_names_sort(out);
    return URSATZ_OK;
}

static UrsatzStatus remove_tree(const char *path, UrsatzError *error) {
    struct stat info;
    if (lstat(path, &info) != 0) {
        return errno == ENOENT ? URSATZ_OK : io_error(error, path, "cannot inspect");
    }
    if (S_ISDIR(info.st_mode)) {
        CommonNames names;
        UrsatzStatus status = common_dir_list(path, &names, error);
        if (status != URSATZ_OK) {
            return status;
        }
        for (size_t i = 0; i < names.count && status == URSATZ_OK; i++) {
            char child[PATH_BYTES];
            if (!common_path_join(child, sizeof(child), path, names.items[i])) {
                status = ursatz_error_set(error, URSATZ_ERR_REQUEST, path, "a path under %s is too long", path);
            } else {
                status = remove_tree(child, error);
            }
        }
        common_names_free(&names);
        if (status != URSATZ_OK) {
            return status;
        }
        return rmdir(path) == 0 || errno == ENOENT ? URSATZ_OK : io_error(error, path, "cannot remove folder");
    }
    return unlink(path) == 0 || errno == ENOENT ? URSATZ_OK : io_error(error, path, "cannot remove");
}

UrsatzStatus common_dir_remove(const char *path, UrsatzError *error) {
    return remove_tree(path, error);
}
