#ifndef URSATZ_ANALYZER_TEST_SUPPORT_H
#define URSATZ_ANALYZER_TEST_SUPPORT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <cJSON.h>

/* A format 0 MIDI file with one track into buf; returns its size. */
size_t test_smf_type0(unsigned char *buf, uint16_t division, const unsigned char *track_body, size_t track_body_size);

/* The member of an object, or NULL. */
const cJSON *test_json_member(const cJSON *object, const char *key);

/* The member of an object; asserts that it exists. */
const cJSON *test_json_require(const cJSON *object, const char *key);

/* A number member, or -1e9 when it is missing or not a number. */
double test_json_number(const cJSON *object, const char *key);

/* A string member, or "" when it is missing or not a string. */
const char *test_json_string(const cJSON *object, const char *key);

/* The value of a {num, den} rational object. */
double test_rational_of(const cJSON *rational);

/* True when the {start, end} span of inner lies inside the span of outer. */
bool test_within(const cJSON *inner, const cJSON *outer);

/* The whole file as a NUL-terminated string the caller frees; NULL when it cannot be read. */
char *test_read_file(const char *path);

/* Writes the text to the file; false on failure. */
bool test_write_file(const char *path, const char *data);

/* Imports a file (the format is detected), runs the pipeline and returns the parsed analysis document the
   caller deletes. Asserts on any failure. */
cJSON *test_analyze_midi(const char *path);

#endif
