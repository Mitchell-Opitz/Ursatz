#ifndef URSATZ_COMPOSER_TEST_COMPOSER_H
#define URSATZ_COMPOSER_TEST_COMPOSER_H

#include <ursatz_composer/motive.h>
#include <ursatz_composer/piece_plan.h>
#include <ursatz_composer/plan_json.h>

/*
 * File readers the tests use over the library's text entries: the library reads no file (the Composer's callers hand it
 * text). test_read_file asserts when the file cannot be read.
 */

/* The plan in the JSON file at path: test_read_file, then plan_from_json. */
PlanResult test_read_plan_file(const char *path, PiecePlan **out, PlanJsonError *error);

/* The motive in the file at path: test_read_file, then motive_load. */
MotiveResult test_load_motive_file(const char *path, MotiveFormat format, const MotiveOptions *options, Motive *out,
                                   MotiveError *error);

#endif
