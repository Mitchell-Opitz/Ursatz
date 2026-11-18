#ifndef URSATZ_COMPOSER_PLAN_JSON_H
#define URSATZ_COMPOSER_PLAN_JSON_H

#include <theory/knowledge/knowledge_snapshot.h>
#include <ursatz_composer/piece_plan.h>

/*
 * A plan as JSON, in the Composer's own format (R-260), described by schema/plan.v5.schema.json: "format"
 * "ursatz-composer-plan", "format_version", "metadata", the node tree under "form", and the lane spans under "meter",
 * "keys", "chords", "melody" and "texture". Keys carry a spelled "tonic" and a "material" (the id of a pitch material); a key
 * span the Composer drew from a form region also names the "relation" and "anchor" it came from. Writing always gives version
 * URSATZ_COMPOSER_PLAN_FORMAT_VERSION; reading also takes version 3 (notes have no ids: they get 1 to n in file order), version 2
 * (keys name a "mode", major or minor) and version 1 (every plan saved before it: "schema_version" 1, tonic pitch classes, motive
 * "transform" names). Version 4 gives every note an "id" (unique, from 1) and, where the Composer knows it, a "source" and an
 * "operation", and the document a "next_note_id" above every id (R-601). Version 5 gives every section node a "section" member in its "composer" object, the section description (R-607), in place of the constraints version 4 and before wrote for a section's letter, part, pairing, cadences, return, layers, pedal, construction, connective, figure and function; reading takes those and makes the description of them. See docs/plan-format.md.
 *
 * Reading ignores fields it does not know, but rejects unknown values in the
 * fields it owns (kind, state, lane, constraint kind) and anything malformed.
 */

#define URSATZ_COMPOSER_PLAN_FORMAT_VERSION 5
/* The "schema_version" of a version 1 file, which is only read. */
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

/*
 * Parses a plan. The keys of the plan are made of the snapshot's common-practice materials (a file names the id of the material of each key).
 * On PLAN_OK, *out is a plan the caller destroys; the snapshot must outlive it. On failure *out is NULL and the result is
 * PLAN_ERR_NULL (no text, no out, or no snapshot with the four materials), PLAN_ERR_PARSE (not JSON), PLAN_ERR_FORMAT (JSON
 * that is not a valid plan) or PLAN_ERR_ALLOC.
 */
PlanResult plan_from_json(const KnowledgeSnapshot *knowledge, const char *text, PiecePlan **out, PlanJsonError *error);

/*
 * Resolves the key spans a plan names only by a relation and an anchor (R-597), where a read plan is composed: each such span, in
 * the order the key lane runs, takes the key its K6 relation entry reaches from its anchor, region_resolve against the plan's home
 * key (the first key span) or the key span before it, and then holds that key beside its relation like a span the Composer drew.
 * A plan with no such span is untouched. The plan's keys are made of the snapshot's materials. Returns PLAN_OK, PLAN_ERR_NULL (no
 * plan, no snapshot or no snapshot with the four materials) or PLAN_ERR_FORMAT with error saying why, at the path of the span:
 * the first span has no anchor, the snapshot has no such relation, the relation does not apply to its anchor (the relation's own
 * message), or it reaches a material the Composer does not compose in (a key is major or natural minor). A refusal leaves the plan
 * as it was.
 */
PlanResult plan_resolve_key_spans(PiecePlan *plan, const KnowledgeSnapshot *knowledge, PlanJsonError *error);

/* plan_from_json, then plan_resolve_key_spans: the plan a caller composes from. On failure *out is NULL. */
PlanResult plan_from_json_resolved(const KnowledgeSnapshot *knowledge, const char *text, PiecePlan **out, PlanJsonError *error);

#endif
