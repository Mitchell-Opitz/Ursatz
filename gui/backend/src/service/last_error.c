#include "last_error.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static _Thread_local UrsatzError g_last;

UrsatzStatus ursatz_gui_fail(UrsatzStatus status, const char *message) {
    memset(&g_last, 0, sizeof(g_last));
    if (message) {
        ursatz_error_set(&g_last, status, "", "%s", message);
    }
    return status;
}

UrsatzStatus ursatz_gui_fail_with(UrsatzStatus status, const UrsatzError *error) {
    memset(&g_last, 0, sizeof(g_last));
    if (error) {
        g_last = *error;
    }
    return status;
}

const UrsatzError *ursatz_gui_last_error(void) {
    return &g_last;
}

void ursatz_gui_error_text(UrsatzError *error, const char *text) {
    if (error) {
        ursatz_error_set(error, URSATZ_OK, "", "%s", text ? text : "");
    }
}

void ursatz_gui_error_printf(UrsatzError *error, const char *format, ...) {
    if (error) {
        va_list args;
        va_start(args, format);
        vsnprintf(error->message, sizeof(error->message), format, args);
        va_end(args);
        error->path[0] = '\0';
    }
}
