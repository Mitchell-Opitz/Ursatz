#ifndef URSATZ_COMPOSER_TEST_COMPOSER_H
#define URSATZ_COMPOSER_TEST_COMPOSER_H

#include <stdbool.h>

#include <cJSON.h>

#include <ursatz_composer/analyzer_check.h>
#include <ursatz_composer/composer_api.h>
#include <ursatz_composer/form_hear.h>
#include <ursatz_composer/harmony_phrase.h>
#include <ursatz_composer/motive.h>
#include <plan/plan.h>
#include <ursatz_composer/piece_plan_keys.h>
#include <plan/plan_json.h>
#include <ursatz_composer/preferences.h>
#include <ursatz_composer/return_hear.h>
#include <ursatz_composer/style.h>

/*
 * File readers the tests use over the library's text entries: the library reads no file (the Composer's callers hand it
 * text). They read with test_must_read_file, which aborts when the file cannot be read.
 */

/* The plan in the JSON file at path: test_must_read_file, then plan_from_json. */
PlanResult test_read_plan_file(const char *path, Plan **out, PlanJsonError *error);

/* The motive in the file at path: test_must_read_file, then motive_load. */
MotiveResult test_load_motive_file(const char *path, MotiveFormat format, const MotiveOptions *options, Motive *out,
                                   MotiveError *error);

/* The first node of the plan (in id order) with this name, or PLAN_NODE_NONE. */
NodeId test_find_node(const Plan *plan, const char *name);

/* The first node under `node` (any depth, depth first) with this name, or PLAN_NODE_NONE. */
NodeId test_find_named(const Plan *plan, NodeId node, const char *name);

/*
 * The hearing checks over a plan, each making one hearing of its own (hearing_create) and calling the check over it
 * (form_hear_in, return_hear_in, piece_check_keys_in, analyzer_check_piece_in, composer_style_measure_in). A hearing that
 * cannot be made maps to the check's own result as the compose run does.
 */
FormHearResult test_form_hear(const Plan *plan, PlanSpan span, FormHeard *out);
ReturnHearResult test_return_hear(const Plan *plan, ReturnHeard *out);
bool test_piece_check_keys(const Plan *plan, PieceKeyCheck *out);
AnalyzerCheckResult test_analyzer_check_piece(const Plan *plan, AnalyzerCheck *out);
ComposerMeasureResult test_style_measure(const Plan *plan, const ComposerStyle *style, StyleComparison *out);

/* The name of a Composer API status, for a message. */
const char *test_status_name(ComposerStatus status);

/* The motive as JSON in the form motive_from_json reads, 2-space indent, trailing newline. NULL on failure; caller frees. */
char *test_motive_json(const Motive *motive);

/* Reads an applied chord's numeral in root position. False for any other text. */
bool test_roman_parse_applied(HarmonyMode mode, const char *roman, HarmonyAppliedChord *out);

/* Whether every setting is the default: the written documents of the preferences and of the defaults compare equal. */
bool test_preferences_are_default(const ComposerPreferences *preferences);

#endif
