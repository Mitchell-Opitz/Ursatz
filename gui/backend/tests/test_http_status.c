#include <string.h>

#include "ursatz_check.h"
#include "routes/route_util.h"
#include "base/http_status.h"
#include "base/last_error.h"

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
        /* the GUI answers with the workshop's table, but for the one case below */
        check_quiet(ursatz_gui_http_status(rows[i].status, &with) == rows[i].with_path, "the GUI's answer is the workshop's");
    }
}

/* The one difference: a body past the size the route takes is 413, not 400 or 422. */
static void test_body_size(void) {
    UrsatzError size = error_at("/body/size");
    check_quiet(ursatz_gui_http_status(URSATZ_ERR_REQUEST, &size) == 413, "a REQUEST at /body/size is 413");
    check_quiet(ursatz_gui_http_status(URSATZ_ERR_NOT_FOUND, &size) == 404, "another status at /body/size is the workshop's");
    check_quiet(workshop_http_status(URSATZ_ERR_REQUEST, &size) == 422, "the workshop's table has no 413");
    check_quiet(workshop_is_call_path("/params/x") && workshop_is_call_path("/query") && workshop_is_call_path("/actor") &&
                    workshop_is_call_path("/name") && workshop_is_call_path("/command") && !workshop_is_call_path("/meter") && !workshop_is_call_path(""),
                "the call's own paths");
}

static void test_last_error(void) {
    check_quiet(workshop_fail(URSATZ_ERR_NOT_FOUND, "item not found") == URSATZ_ERR_NOT_FOUND, "fail returns its status");
    check_quiet(strcmp(workshop_last_error()->message, "item not found") == 0, "fail keeps the message");
    workshop_fail(URSATZ_ERR_CONFLICT, NULL);
    check_quiet(workshop_last_error()->message[0] == '\0', "a failure without a message clears the last one");

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
    test_body_size();
    test_last_error();
    return test_finish("gui_http_status");
}
