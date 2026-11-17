#ifndef URSATZ_TEST_H
#define URSATZ_TEST_H

/* The test support every part shares: checks, files, temp paths and JSON. Built as the library ursatz_test_support
 * (tests/CMakeLists.txt); a part's own helpers stay in its tests/support folder. */

#include <stdbool.h>
#include <stddef.h>

#include <cJSON.h>

/* ---- checks ----------------------------------------------------------------------------------------------- */

/* The number of failed checks so far. */
extern int test_failure_count;

/* Counts a failure and prints "FAIL <file>:<line>: <text>" to stderr when ok is false. Returns ok. */
bool test_check(bool ok, const char *file, int line, const char *text);

/* The same with a printf style message in place of the condition text. Returns ok. */
bool test_checkf(bool ok, const char *file, int line, const char *format, ...)
#if defined(__GNUC__)
    __attribute__((format(printf, 4, 5)))
#endif
    ;

#define CHECK(cond) test_check((cond), __FILE__, __LINE__, #cond)
#define CHECKF(cond, ...) test_checkf((cond), __FILE__, __LINE__, __VA_ARGS__)

/* Prints "PASS: <what>" (for the tests that print every passed check). */
void test_pass(const char *what);

/* Prints "<name>: ok" or "<name>: <n> failure(s)" and returns 0, or 1 when a check failed. Call it last in main. */
int test_finish(const char *name);

/* ---- goldens ----------------------------------------------------------------------------------------------- */

/* One switch updates every golden: URSATZ_UPDATE_GOLDENS set to anything but "" or "0" (cmake --build build --target
 * update-goldens sets it for the tests labelled golden). A golden test in update mode writes its goldens instead of
 * comparing and ends with test_goldens_updated, so a run with the switch never passes. */

/* True when URSATZ_UPDATE_GOLDENS is set to a non-empty value other than "0". */
bool test_update_goldens(void);

/* Ends an update run: prints "<name>: goldens updated (URSATZ_UPDATE_GOLDENS)" and returns 2 for main to return. When
 * a check failed on the way (a golden that could not be written) it returns what test_finish returns instead. */
int test_goldens_updated(const char *name);

/* ---- files ------------------------------------------------------------------------------------------------ */

/* The whole file as a NUL-terminated buffer the caller frees; NULL when it cannot be read. size may be NULL. */
char *test_read_file(const char *path, size_t *size);

/* The same, but prints the path and aborts when the file cannot be read. */
char *test_must_read_file(const char *path, size_t *size);

/* Writes size bytes to the file; false on failure. */
bool test_write_file(const char *path, const char *data, size_t size);

/* True when the path exists. */
bool test_file_exists(const char *path);

/* "<TMPDIR or /tmp>/ursatz_test_<pid>_<name>", a heap string the caller frees, so parallel test processes never
 * share a file. */
char *test_temp_path(const char *name);

/* ---- JSON ------------------------------------------------------------------------------------------------- */

/* The file parsed as JSON, or NULL when it cannot be read or parsed. The caller deletes it. */
cJSON *test_read_json(const char *path);

/* The member of an object, or NULL. */
const cJSON *test_json_member(const cJSON *object, const char *name);

/* A number member, or fallback when it is missing or not a number. */
double test_json_number(const cJSON *object, const char *name, double fallback);

/* A string member, or "" when it is missing or not a string. */
const char *test_json_string(const cJSON *object, const char *name);

/* True when the two values are equal (object keys compare case-sensitive). Otherwise false, and where gets the path of
 * the first difference, for example "sections[2].key" ("" for the root). */
bool test_json_equal(const cJSON *expected, const cJSON *actual, char *where, size_t where_size);

#endif
