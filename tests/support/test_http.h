#ifndef URSATZ_TEST_HTTP_H
#define URSATZ_TEST_HTTP_H

/* A plain TCP HTTP client for the GUI backend tests (no library, one request per connection). Built as the library
 * ursatz_test_http (tests/CMakeLists.txt); only the GUI tests link it. Each test passes the port its server listens
 * on. */

#include <stddef.h>

#include <cJSON.h>

/* One exchange with 127.0.0.1:<port>. content_type may be NULL (application/json). Returns the whole response (status
 * line, headers, body) as a string the caller frees, or NULL when the connection failed. */
char *test_http_exchange(const char *port, const char *method, const char *path, const char *content_type,
                         const char *body, size_t body_size);

/* One exchange with extra header lines (each ending in "\r\n", the Content-Type among them), no base version added. The whole response as a string the
 * caller frees, or NULL when the connection failed. */
char *test_http_exchange_headers(const char *port, const char *method, const char *path, const char *headers, const char *body, size_t body_size);

/* The status code of a response, or 0 when it has none. */
int test_http_status(const char *response);

/* The body of a response ("" when there is none). Points into response. */
const char *test_http_body(const char *response);

/* An exchange with a JSON body (or NULL), the answer's body parsed as JSON (NULL when it is not). The status goes to
 * *status. The caller deletes the JSON. */
cJSON *test_http_json(const char *port, const char *method, const char *path, const char *json_body, int *status);

/* When response is a 202 with a job (R-645), polls GET /api/jobs/<id> until the job is over and returns, in place of response (freed), a
 * response holding the job's HTTP status and its answer as the body: what the route answered before the work was a job. Any other
 * response (or NULL) is returned as it is. */
char *test_http_finish(const char *port, char *response);

/* test_http_json for a request that may be accepted as a job: the job's status goes to *status and its answer is parsed. */
cJSON *test_http_json_finish(const char *port, const char *method, const char *path, const char *json_body, int *status);

/* POSTs the command object (JSON text) with a file to /api/workshop/command as the command line does: the file is the body
 * (application/octet-stream), the command is percent-encoded in X-Ursatz-Command and the upload name (already percent-encoded) is in
 * X-Filename. The whole response as a string the caller frees, or NULL when the connection failed. */
char *test_http_command_file(const char *port, const char *command_json, const char *encoded_name, const char *data, size_t size);

/* Sends library.import of the file (as actor michelle, the upload name already URL encoded) as test_http_command_file does. Waits for the import job and returns the new
 * piece's id (the caller frees), or NULL. When response is not NULL it gets the whole response (the caller frees). */
char *test_http_upload(const char *port, const char *file_path, const char *encoded_name, char **response);

/* Sleeps for ms milliseconds (waiting for a server to listen). */
void test_sleep_ms(int ms);

#endif
