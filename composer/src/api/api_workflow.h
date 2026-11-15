#ifndef URSATZ_COMPOSER_API_WORKFLOW_H
#define URSATZ_COMPOSER_API_WORKFLOW_H

#include <stdbool.h>
#include <stddef.h>

#include <ursatz_composer/compose.h>
#include <ursatz_composer/export.h>
#include <ursatz_composer/form_spec.h>
#include <ursatz_composer/given.h>
#include <ursatz_composer/piece_plan.h>

#include "api_internal.h"

/* What the plan workflow entries (plan, fill, lock, regenerate, variations, harmonize) share. */

#define API_SEED_MAX 4294967295UL
/* How many seeds an entry tries, one after another from the one given, before it gives up. */
#define API_SEED_TRIES 16
#define API_MAX_SECTIONS 16

/* --- the sections of a plan ------------------------------------------------------------------------ */

/* One theme section of a piece plan, as the entries report it. */
typedef struct {
    NodeId node;
    char name[24];
    char letter[PLAN_SECTION_LETTER_MAX];
    size_t bars; /* 0 when the plan has no meter */
    bool has_key;
    AnalysisKey key; /* the key the section ends in */
    CadenceType cadences[2];
    size_t cadence_count;
    bool has_pairing;
    PlanThemePairing pairing;
    bool returns;
    char returns_letter[PLAN_SECTION_LETTER_MAX];
    PlanReturnVariation variation;
} ApiSection;

/* The sections of the piece in order, material left out; returns how many there are (at most capacity are written). */
size_t api_plan_sections(const PiecePlan *plan, ApiSection *out, size_t capacity);
/* The bars in a length of the plan's meter, rounded; 0 when the plan has no meter. */
size_t api_bars_in(const PiecePlan *plan, Rational length);
/* {"letter", "bars", "key", "cadences", "pairing", "returns"}, as the plan command lists a section. */
cJSON *api_section_json(const ApiSection *section);
/* The key as "D minor". */
const char *api_key_name(const AnalysisKey *key, char *out, size_t size);

/* --- the plan of a request ------------------------------------------------------------------------- */

/*
 * The plan object of the request member name as JSON text of the plan (plan_to_json), checked to read; the caller frees
 * it with free. Messages: "cannot read plan: <why>[ at <where>]" at /<name>, or "the input is an Ursatz-Analyzer analysis
 * document, not a plan; ..." for a document of the Analyzer.
 */
ComposerStatus api_read_plan_text(const cJSON *request, const char *name, char **text, ComposerError *error);
/* The plan of the text above. */
ComposerStatus api_plan_from_text(const char *text, const char *name, PiecePlan **out, ComposerError *error);

/* --- what a request asks to have made -------------------------------------------------------------- */

typedef struct {
    bool midi;
    bool musicxml;
    bool plain_midi;
    bool record;     /* the text of the --record file, in "record" of the document */
    int explain;     /* 0 none, 1 text, 2 json: the explanation in "explanation" of the document */
    PlanExportOptions export_options;
} ApiOutputs;

/* "midi", "musicxml", "plain_midi", "tempo" and, with workflow, "record" and "explain" ("text" or "json"). */
ComposerStatus api_read_outputs(const cJSON *request, bool workflow, ApiOutputs *out, ComposerError *error);
/* Fills plan_json, and midi and musicxml when asked, from the plan as it is in memory. */
ComposerStatus api_render_outputs(const PiecePlan *plan, const ApiOutputs *outputs, ComposerResponse *out);
/* Fills record and the "explanation" member of document (a string) when asked; outcome may be NULL. */
ComposerStatus api_record_outputs(const PiecePlan *plan, const ComposeOutcome *outcome, const ApiOutputs *outputs,
                                  cJSON *document, ComposerResponse *out);

/* --- the result of a compose ----------------------------------------------------------------------- */

#define API_MAX_GIVEN_RUNS 64

typedef struct {
    GivenReport runs[API_MAX_GIVEN_RUNS];
    size_t count;
    bool unchanged; /* every run is as it was given */
} ApiGivenRuns;

/* The "sections" array: [{"letter", "heard", ["in_scope": false,] "heard_as_requested", "cadences"}]. */
cJSON *api_heard_json(const PiecePlan *plan, const ComposeOutcome *outcome);
/*
 * What fill prints with --format json (without "files"): {"seed", "realized", <the members of extra>, "sections",
 * ["analyzer",] ["style_distance",] "returns", ["given"]}. extra (may be NULL) is an object whose members are moved in.
 */
cJSON *api_compose_document(const PiecePlan *plan, unsigned long seed, bool realized, const ComposeOutcome *outcome,
                            const ApiGivenRuns *given, cJSON *extra);
/*
 * No seed from first to last completed the plan: COMPOSER_ERR_UNSATISFIABLE, and out->json is {"stuck": {"first", "last",
 * "stage", "key_rejected", "open": [{"what", "reason"}], "more"}}.
 */
ComposerStatus api_compose_stuck(const PiecePlan *plan, const ComposeOutcome *outcome, unsigned long first,
                                 unsigned long last, ComposerResponse *out);

/* --- the node and lane a request works on ---------------------------------------------------------- */

typedef struct {
    bool has_node;
    unsigned long node;
    const char *section;
    bool has_lane;
    PlanLaneKind lane;
} ApiTarget;

/* "node" (a node id) or "section" (a form letter), exactly one, and optionally "lane". */
ComposerStatus api_read_target(const cJSON *request, ApiTarget *out, ComposerError *error);
/* The node the target names; COMPOSER_ERR_UNSATISFIABLE when the plan has none. */
ComposerStatus api_target_node(const PiecePlan *plan, const ApiTarget *target, NodeId *out, ComposerError *error);
/* The targets: the node, or with a lane every span of it under the node. Into a new array the caller frees. */
ComposerStatus api_target_list(const PiecePlan *plan, const ApiTarget *target, FillTarget **out, size_t *count,
                               ComposerError *error);
/* The target in words: "section B", "node 12", "the harmony of section B". */
void api_target_describe(const PiecePlan *plan, const ApiTarget *target, char *out, size_t size);

/*
 * The sizes the form has, for the message about one it has not: the bars its sizes come in, then their counts when they have
 * any ("16, 24 bars"), into out (size bytes).
 */
void api_form_sizes_text(const FormSpec *form, char *out, size_t size);

#endif
