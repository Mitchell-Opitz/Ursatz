#ifndef WORKSHOP_HTTP_STATUS_H
#define WORKSHOP_HTTP_STATUS_H

#include <common/status.h>

/*
 * The HTTP status of a service status (R-639): OK 200; REQUEST 400, or 422 when the error names a path (a Composer refusal);
 * UNSATISFIABLE 422; NOT_FOUND 404; CONFLICT 409; CANCELLED 499; FORMAT, IO, ALLOC and INTERNAL 500. error may be NULL.
 */
int workshop_http_status(UrsatzStatus status, const UrsatzError *error);

#endif
