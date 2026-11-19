#ifndef WORKSHOP_LAST_ERROR_H
#define WORKSHOP_LAST_ERROR_H

#include <common/status.h>

/*
 * The error of the last failed service call of this thread (R-639). A service that fails returns its UrsatzStatus through
 * workshop_fail, which keeps the message for the route that answers; the calls that took a message buffer take an UrsatzError
 * instead. B11 gives every workshop call its own UrsatzError and this goes.
 */

/* Records status and message (NULL for none) as this thread's last error and returns status. */
UrsatzStatus workshop_fail(UrsatzStatus status, const char *message);

/* Records error as this thread's last error and returns status. */
UrsatzStatus workshop_fail_with(UrsatzStatus status, const UrsatzError *error);

/* Puts text (NULL is "") into error's message and clears its path. A NULL error is left alone. */
void workshop_error_text(UrsatzError *error, const char *text);

/* workshop_error_text with a printf format. */
void workshop_error_printf(UrsatzError *error, const char *format, ...)
#if defined(__GNUC__)
    __attribute__((format(printf, 2, 3)))
#endif
    ;

/* The last error of this thread: its message is "" when the failing call gave none. Never NULL. */
const UrsatzError *workshop_last_error(void);

#endif
