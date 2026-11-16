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

/* The status code of a response, or 0 when it has none. */
int test_http_status(const char *response);

/* The body of a response ("" when there is none). Points into response. */
const char *test_http_body(const char *response);

/* An exchange with a JSON body (or NULL), the answer's body parsed as JSON (NULL when it is not). The status goes to
 * *status. The caller deletes the JSON. */
cJSON *test_http_json(const char *port, const char *method, const char *path, const char *json_body, int *status);

/* POSTs the file to /api/analyzer/pieces with the upload name (already URL encoded) in X-Filename. Returns the new
 * piece's id (the caller frees), or NULL. When response is not NULL it gets the whole response (the caller frees). */
char *test_http_upload(const char *port, const char *file_path, const char *encoded_name, char **response);

/* Sleeps for ms milliseconds (waiting for a server to listen). */
void test_sleep_ms(int ms);

#endif
