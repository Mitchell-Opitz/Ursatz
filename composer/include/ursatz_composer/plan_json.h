#ifndef URSATZ_COMPOSER_PLAN_JSON_H
#define URSATZ_COMPOSER_PLAN_JSON_H

#include <ursatz_composer/piece_plan.h>

/*
 * A plan as JSON. The document has the shape of an Ursatz-Analyzer analysis
 * document (schema_version 1): the node tree goes under "form", lane spans go
 * under "meter", "keys", "chords", "melody" and "texture" as claims, and
 * everything the Analyzer has no field for sits in a "composer" object on
 * each node and claim. See docs/plan-format.md.
 *
 * Reading ignores fields it does not know, but rejects unknown values in the
 * fields it owns (kind, state, lane, constraint kind) and anything malformed.
 */

#define PLAN_JSON_SCHEMA_VERSION 1
#define PLAN_JSON_PATH_MAX 128
#define PLAN_JSON_MESSAGE_MAX 160

/* Filled on failure. Pass NULL when the details are not wanted. */
typedef struct {
    /* Where the problem is, e.g. "form.children[1].span.end.den". Empty for the document as a whole. */
    char path[PLAN_JSON_PATH_MAX];
    char message[PLAN_JSON_MESSAGE_MAX];
    /* Byte offset into the text for a syntax error, otherwise -1. */
    long offset;
} PlanJsonError;

/* Returns the plan as pretty-printed JSON (2-space indent, trailing newline), or NULL with error set. Caller frees. */
char *plan_to_json(const PiecePlan *plan, PlanJsonError *error);
PlanResult plan_write_json_file(const PiecePlan *plan, const char *path, PlanJsonError *error);

/*
 * Parses a plan. On PLAN_OK, *out is a plan the caller destroys. On failure *out
 * is NULL and the result is PLAN_ERR_PARSE (not JSON), PLAN_ERR_FORMAT (JSON
 * that is not a valid plan), PLAN_ERR_IO or PLAN_ERR_ALLOC.
 */
PlanResult plan_from_json(const char *text, PiecePlan **out, PlanJsonError *error);
PlanResult plan_read_json_file(const char *path, PiecePlan **out, PlanJsonError *error);

#endif
