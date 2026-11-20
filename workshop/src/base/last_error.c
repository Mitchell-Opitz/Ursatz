#include "last_error.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static _Thread_local UrsatzError g_last;

UrsatzStatus workshop_fail(UrsatzStatus status, const char *message) {
    memset(&g_last, 0, sizeof(g_last));
    if (message) {
        ursatz_error_set(&g_last, status, "", "%s", message);
    }
    return status;
}

UrsatzStatus workshop_fail_with(UrsatzStatus status, const UrsatzError *error) {
    memset(&g_last, 0, sizeof(g_last));
    if (error) {
        g_last = *error;
    }
    return status;
}

const UrsatzError *workshop_last_error(void) {
    return &g_last;
}

void workshop_error_text(UrsatzError *error, const char *text) {
    if (error) {
        ursatz_error_set(error, URSATZ_OK, "", "%s", text ? text : "");
    }
}

void workshop_error_printf(UrsatzError *error, const char *format, ...) {
    if (error) {
        va_list args;
        va_start(args, format);
        vsnprintf(error->message, sizeof(error->message), format, args);
        va_end(args);
        error->path[0] = '\0';
    }
}
