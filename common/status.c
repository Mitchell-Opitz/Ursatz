#include "common/status.h"

#include <stdarg.h>
#include <stdio.h>

const char *ursatz_status_name(UrsatzStatus status) {
    switch (status) {
    case URSATZ_OK:
        return "ok";
    case URSATZ_ERR_REQUEST:
        return "request";
    case URSATZ_ERR_UNSATISFIABLE:
        return "unsatisfiable";
    case URSATZ_ERR_NOT_FOUND:
        return "not-found";
    case URSATZ_ERR_CONFLICT:
        return "conflict";
    case URSATZ_ERR_FORMAT:
        return "format";
    case URSATZ_ERR_IO:
        return "io";
    case URSATZ_ERR_CANCELLED:
        return "cancelled";
    case URSATZ_ERR_ALLOC:
        return "alloc";
    case URSATZ_ERR_INTERNAL:
        return "internal";
    }
    return "internal";
}

UrsatzStatus ursatz_error_set(UrsatzError *error, UrsatzStatus status, const char *path, const char *format, ...) {
    if (error) {
        va_list args;
        va_start(args, format);
        vsnprintf(error->message, sizeof(error->message), format, args);
        va_end(args);
        snprintf(error->path, sizeof(error->path), "%s", path ? path : "");
    }
    return status;
}
