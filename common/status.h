#ifndef URSATZ_COMMON_STATUS_H
#define URSATZ_COMMON_STATUS_H

#include <stdbool.h>

/*
 * The one status set of the applications (R-639): the Composer, the Analyzer and the workshop answer with it, and the GUI maps it to
 * an HTTP status in one table. Library result enums stay and each application maps each one once.
 */
typedef enum {
    URSATZ_OK = 0,
    URSATZ_ERR_REQUEST,       /* malformed or invalid input */
    URSATZ_ERR_UNSATISFIABLE, /* valid, and nothing satisfies it */
    URSATZ_ERR_NOT_FOUND,
    URSATZ_ERR_CONFLICT, /* exists, still referenced, busy, or changed since it was read */
    URSATZ_ERR_FORMAT,   /* a stored or imported file that does not read */
    URSATZ_ERR_IO,
    URSATZ_ERR_CANCELLED,
    URSATZ_ERR_ALLOC,
    URSATZ_ERR_INTERNAL
} UrsatzStatus;

typedef struct {
    char message[512];
    char path[128]; /* a JSON Pointer into the request, a file path for a file error, or "" */
} UrsatzError;

/* The status as a word: "ok", "request", "unsatisfiable", "not-found", "conflict", "format", "io", "cancelled", "alloc", "internal". */
const char *ursatz_status_name(UrsatzStatus status);

/* The inverse of ursatz_status_name: true and *out set for each of its ten words, false for any other text (and *out untouched). */
bool ursatz_status_parse(const char *name, UrsatzStatus *out);

/*
 * Fills error (NULL is allowed) with the formatted message and the path (NULL is ""), and returns status, so a caller writes
 * "return ursatz_error_set(error, URSATZ_ERR_REQUEST, "/key", "the key is not a note name");".
 */
UrsatzStatus ursatz_error_set(UrsatzError *error, UrsatzStatus status, const char *path, const char *format, ...)
#if defined(__GNUC__)
    __attribute__((format(printf, 4, 5)))
#endif
    ;

#endif
