#include <string.h>

#include "ursatz_check.h"
#include "routes/route_util.h"
#include "workshop_internal.h"

/* The one status-to-HTTP table (R-639): every status, with an error that names no path, one that names a path, and no error. */

static UrsatzError error_at(const char *path) {
    UrsatzError error;
    memset(&error, 0, sizeof(error));
    ursatz_error_set(&error, URSATZ_ERR_REQUEST, path, "the member is wrong");
    return error;
}

static void test_table(void) {
    static const struct {
        UrsatzStatus status;
        int plain;     /* no path */
        int with_path; /* the error names a path */
    } rows[] = {
        {URSATZ_OK, 200, 200},
        {URSATZ_ERR_REQUEST, 400, 422},
        {URSATZ_ERR_UNSATISFIABLE, 422, 422},
        {URSATZ_ERR_NOT_FOUND, 404, 404},
        {URSATZ_ERR_CONFLICT, 409, 409},
        {URSATZ_ERR_FORMAT, 500, 500},
        {URSATZ_ERR_IO, 500, 500},
        {URSATZ_ERR_CANCELLED, 499, 499},
        {URSATZ_ERR_ALLOC, 500, 500},
        {URSATZ_ERR_INTERNAL, 500, 500},
    };
    UrsatzError without = error_at("");
    UrsatzError with = error_at("/meter");
    for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); i++) {
        check_quiet(workshop_http_status(rows[i].status, NULL) == rows[i].plain, "a status without an error");
        check_quiet(workshop_http_status(rows[i].status, &without) == rows[i].plain, "a status whose error names no path");
        check_quiet(workshop_http_status(rows[i].status, &with) == rows[i].with_path, "a status whose error names a path");
    }
}

static void test_last_error(void) {
    check_quiet(workshop_fail(URSATZ_ERR_NOT_FOUND, "item not found") == URSATZ_ERR_NOT_FOUND, "fail returns its status");
    check_quiet(strcmp(workshop_last_error()->message, "item not found") == 0, "fail keeps the message");
    check_quiet(strcmp(ursatz_gui_failure_message("fallback"), "item not found") == 0, "the failure message is the kept one");
    workshop_fail(URSATZ_ERR_CONFLICT, NULL);
    check_quiet(workshop_last_error()->message[0] == '\0', "a failure without a message clears the last one");
    check_quiet(strcmp(ursatz_gui_failure_message("fallback"), "fallback") == 0, "the fallback answers when there is no message");

    UrsatzError given = error_at("/key");
    workshop_fail_with(URSATZ_ERR_REQUEST, &given);
    check_quiet(strcmp(workshop_last_error()->path, "/key") == 0 && strcmp(workshop_last_error()->message, "the member is wrong") == 0,
                "fail_with keeps the message and the path");

    UrsatzError text = {{0}};
    workshop_error_text(&text, "plain");
    check_quiet(strcmp(text.message, "plain") == 0 && text.path[0] == '\0', "error_text sets the message");
    workshop_error_printf(&text, "%d of %s", 3, "four");
    check_quiet(strcmp(text.message, "3 of four") == 0, "error_printf formats the message");
    workshop_error_text(NULL, "ignored");
}

int main(void) {
    test_table();
    test_last_error();
    return test_finish("gui_http_status");
}
