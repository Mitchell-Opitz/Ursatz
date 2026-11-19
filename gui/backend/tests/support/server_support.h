#ifndef URSATZ_GUI_TESTS_SERVER_SUPPORT_H
#define URSATZ_GUI_TESTS_SERVER_SUPPORT_H

struct mg_context;

/* Opens a workshop on the test's working directory (its data folder) and starts the backend server on it. NULL, with a message on
 * stderr, when either fails. A test starts at most one at a time; stop it with test_gui_server_stop. */
struct mg_context *test_gui_server_start(const char *port);

/* Stops the server and closes the workshop, which releases the data folder (a test may start another afterwards). */
void test_gui_server_stop(struct mg_context *ctx);

#endif
