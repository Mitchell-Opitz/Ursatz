#ifndef URSATZ_GUI_TESTS_SERVER_SUPPORT_H
#define URSATZ_GUI_TESTS_SERVER_SUPPORT_H

struct mg_context;

/* Opens a workshop on the test's working directory (its data folder) and starts the backend server on it. NULL, with a message on
 * stderr, when either fails. A test starts at most one at a time; stop it with test_gui_server_stop. */
struct mg_context *test_gui_server_start(const char *port);

/* Stops the server and closes the workshop, which releases the data folder (a test may start another afterwards). */
void test_gui_server_stop(struct mg_context *ctx);

#include <stdbool.h>
#include <stddef.h>

#include <cJSON.h>

/* The folder of an item in the working directory's data folder (projects/<prj>/items/<id>); false when no project holds it. */
bool test_gui_item_dir(const char *id, char *out, size_t size);

/* The folder of one version of an item (projects/<prj>/items/<id>/v<n>). */
bool test_gui_version_dir(const char *id, int version, char *out, size_t size);

/* Sends the command name as actor "michelle" to POST /api/workshop/command on 127.0.0.1:<port>: {command, actor, target (when not NULL or empty),
 * base_version (when >= 0), params (params_json, {} when NULL)}. *http_status is the response's status and *reply its body parsed (NULL when it is
 * not JSON; the caller deletes it; either may be NULL). True when the exchange happened and the body parsed. */
bool test_gui_command(const char *port, const char *name, const char *target, int base_version, const char *params_json, int *http_status,
                      cJSON **reply);

/* test_gui_command for a command that may be accepted as a job (202): waits for the job and answers with its status and result, as a route
 * answered before the work was a job (test_http_finish). */
bool test_gui_command_finish(const char *port, const char *name, const char *target, int base_version, const char *params_json, int *http_status,
                             cJSON **reply);

/* A command with a file, as the command line sends it: the body is the file (application/octet-stream), the command object is percent-encoded in
 * X-Ursatz-Command and the file's name (already percent-encoded) is in X-Filename. Waits for a job as test_gui_command_finish does. */
bool test_gui_command_file(const char *port, const char *name, const char *target, int base_version, const char *params_json,
                           const char *encoded_filename, const char *data, size_t size, int *http_status, cJSON **reply);

/* test_gui_command with the reply as the result (NULL when it was not JSON), as the tests of the routes read an answer. A command on an item
 * ("item." and a target) gives the version the item has now as base_version (test_gui_item_version), as the page does with every write; a test of
 * the refusal calls test_gui_command with its own. test_gui_send_finish waits for a job as test_gui_command_finish does. */
cJSON *test_gui_send(const char *port, const char *name, const char *target, const char *params_json, int *http_status);
cJSON *test_gui_send_finish(const char *port, const char *name, const char *target, const char *params_json, int *http_status);

/* The current version of the item id as GET /api/materials/items/<id> says, -1 when it has none (a version to give as base_version). */
int test_gui_item_version(const char *port, const char *id);

#endif
