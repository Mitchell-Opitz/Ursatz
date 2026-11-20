#ifndef URSATZ_COMPOSER_API_INTERNAL_H
#define URSATZ_COMPOSER_API_INTERNAL_H

#include <stdbool.h>

#include <cJSON.h>
#include <theory/tonality/common_practice_materials.h>
#include <ursatz_composer/composer_api.h>
#include <ursatz_composer/corpus_seed.h>
#include <ursatz_composer/export.h>
#include <ursatz_composer/fill_target.h>
#include <plan/plan_lanes.h>
#include <ursatz_composer/preferences.h>
#include <ursatz_composer/motive.h>
#include <ursatz_composer/regenerate.h>
#include <ursatz_composer/style.h>
#include "util/job.h"

/* Parses the request text into *out (the caller deletes it). Checks "api_version" when present. On failure sets error and returns the status. */
UrsatzStatus api_parse_request(const char *text, cJSON **out, UrsatzError *error);

/* Sets error and returns status. path may be NULL or "". */
UrsatzStatus api_fail(UrsatzError *error, UrsatzStatus status, const char *path, const char *format, ...);

/* A corpus seed that failed: `what` is what the passages were to give (a motive, or the chords for numerals), path the member. */
UrsatzStatus api_seed_failure(CorpusSeedResult result, const char *what, const char *path, UrsatzError *error);

/*
 * An export that failed: ALLOC for an allocation failure, REQUEST for the results a request can cause (no content, not
 * representable, too many voices; the error names `path`), INTERNAL otherwise. The message is `what`, then ": " and the detail's text.
 */
UrsatzStatus api_export_failure(UrsatzError *error, PlanExportResult result, const char *path, const char *what,
                                const PlanExportError *detail);

/*
 * For a key text the request parser refused: when it names a material the Composer does not compose in (request_key_refusal), sets
 * error to that message at path and returns true; false when it does not, and the caller reports the key as unknown.
 */
bool api_fail_unsupported_key(UrsatzError *error, const char *path, const KnowledgeSnapshot *knowledge, const char *key_text);

/* What a request's "material" (or old "mode") member asked for. */
typedef enum { API_MATERIAL_PICKED, API_MATERIAL_STYLE, API_MATERIAL_RANDOM } ApiMaterialKind;
#define API_MATERIAL_ALLOW_STYLE 1u
#define API_MATERIAL_ALLOW_RANDOM 2u

/*
 * Reads the "material" member of a request object, or the old "mode" member (the two together are refused). *text is the
 * value, NULL when neither is present (an error when required), and *member names the member read ("material" or "mode").
 * base NULL reads as api_member_string (paths "/<member>"); a base reads as api_request_string (paths "<base>/<member>").
 */
bool api_read_material(const cJSON *object, const char *base, bool required, const char **text, const char **member,
                       UrsatzError *error);

/*
 * Resolves the text api_read_material gave. "style" (allow API_MATERIAL_ALLOW_STYLE) and "random" (API_MATERIAL_ALLOW_RANDOM) set
 * *kind and leave *material NULL. Under "material" the text is a K1 id, name, key word or alias of a material the Composer
 * composes in; under the old "mode" only major and minor (any case), as before. path is the member's path, for the refusal.
 */
UrsatzStatus api_resolve_material(const KnowledgeSnapshot *knowledge, const char *member, const char *path, const char *text,
                                    unsigned allow, const PitchMaterial **material, ApiMaterialKind *kind, UrsatzError *error);

/*
 * Checks the call context an entry takes first: URSATZ_OK when context and its knowledge are present, else
 * URSATZ_ERR_REQUEST with error set to "a knowledge snapshot is required" (no path). The entry zeroes its response first.
 * materials, when not NULL, gets the common-practice materials of the snapshot, resolved once for the call
 * (common_practice_materials_resolve); a snapshot that lacks one is URSATZ_ERR_REQUEST with the message that names it.
 */
UrsatzStatus api_require_knowledge(const ComposerContext *context, CommonPracticeMaterials *materials, UrsatzError *error);

/* Sets error to "out of memory" (URSATZ_ERR_ALLOC, no path) and returns the status. */
UrsatzStatus api_fail_alloc(UrsatzError *error);

/* Adds item to object under name and returns true; the item is deleted when object or item is NULL or it cannot be added. */
bool api_add(cJSON *object, const char *name, cJSON *item);

/* Prints the value unformatted into out->json and deletes it. Returns URSATZ_OK or URSATZ_ERR_ALLOC (with error set). */
UrsatzStatus api_respond_json(ComposerResponse *out, cJSON *value);

/*
 * Member readers. Each reads the member name of object: absent or null leaves *out as it is and sets *present (when not
 * NULL) to false, returning true; a wrong type or an invalid value sets error (path "/<name>") and returns false.
 */
bool api_member_string(const cJSON *object, const char *name, const char **out, bool *present, UrsatzError *error);
bool api_member_bool(const cJSON *object, const char *name, bool *out, bool *present, UrsatzError *error);
/* A whole number, or a string "3/2" (request_parse_ratio). */
bool api_member_rational(const cJSON *object, const char *name, Rational *out, bool *present, UrsatzError *error);
/*
 * A whole number from minimum to maximum, given as a JSON number or as a string of digits ("3"). A value that is not one
 * is refused with the message "<name> must be a whole number from <minimum> to <maximum>, not '<value>'".
 */
bool api_member_whole(const cJSON *object, const char *name, unsigned long minimum, unsigned long maximum,
                      unsigned long *out, bool *present, UrsatzError *error);

/*
 * Loads the style model file at path into *out, without the named categories (none when count is 0); the caller destroys
 * it. A category that is not one the Analyzer defines is URSATZ_ERR_REQUEST with path_without, a file that cannot be
 * read is URSATZ_ERR_UNSATISFIABLE with path_file; both messages say "cannot read style '<path>': <reason>".
 */
UrsatzStatus api_load_style(const char *path, const char *const *without, size_t without_count, const char *path_file,
                              const char *path_without, ComposerStyle **out, UrsatzError *error);

/*
 * The optional "preferences" member (docs/preferences.md): *out gets the defaults with the members present applied, and
 * *present (when not NULL) whether the request had the member. A member that is not valid sets error to
 * URSATZ_ERR_REQUEST with the path "/preferences" followed by the reader's path (for example "/preferences/melody/keep")
 * and returns false.
 */
bool api_read_preferences(const cJSON *request, ComposerPreferences *out, bool *present, UrsatzError *error);

/* A seed from the clock, for a request that gives none. */
unsigned long api_default_seed(void);

/*
 * The "tempo" member of the request (quarter notes per minute, 20 to 400, a whole number, a fraction string like
 * "195/2", or null) into options->tempo_numerator and tempo_denominator; absent or null leaves them as they are.
 */
UrsatzStatus api_read_tempo(const cJSON *request, PlanExportOptions *options, UrsatzError *error);

/*
 * Reads a motive from the request object (docs/api.md, "Motive requests"): "input" {"format", "form", "text"}, "meter",
 * "key" (read through the knowledge snapshot), "pickup", and the extras of api_read_motive_extras. base is the JSON Pointer of the object in the request ("" for
 * the request itself), which the error paths start with. Returns URSATZ_OK; URSATZ_ERR_REQUEST for a member that is
 * not valid (error path set); URSATZ_ERR_UNSATISFIABLE for text that is not a motive of the kind asked for. options
 * (may be NULL) gets the options the motive was read with.
 */
UrsatzStatus api_read_motive(const cJSON *request, const char *base, const KnowledgeSnapshot *knowledge, Motive *out,
                               MotiveOptions *options, UrsatzError *error);

/* As api_read_motive for a motive that arrives as the bytes of a Standard MIDI File: the request has no "input". */
UrsatzStatus api_read_motive_bytes(const cJSON *request, const char *base, const KnowledgeSnapshot *knowledge,
                                     const unsigned char *bytes, size_t size, Motive *out, MotiveOptions *options,
                                     UrsatzError *error);

/*
 * The motive's own harmony ("harmony"), labelled features ("features") and role ("role") of object, into options.
 * meter is the motive's meter, which the chord onsets are counted in; it may be NULL only when there is no harmony.
 */
UrsatzStatus api_read_motive_extras(const cJSON *object, const char *base, const PlanMeter *meter,
                                      MotiveOptions *options, UrsatzError *error);

/*
 * The motives of a theme or piece request (docs/theme-request.md): a motive object {"degrees", "pickup", "harmony",
 * "features", "role", "form", "tone_overrides"} read in the key and meter. what names the motive in the messages ("motive
 * 'A'"), base is the JSON Pointer of the object. role forces the role when not NULL. Returns URSATZ_OK, or
 * URSATZ_ERR_REQUEST naming the member.
 */
UrsatzStatus api_read_degrees_motive(const KnowledgeSnapshot *knowledge, const char *what, const char *base, const cJSON *object, const char *role,
                                       const PlanMeter *meter, const SpelledKey *key, Motive *out, UrsatzError *error);
/* The GROW_FUNCTION_* bits a form's "functions" list names. */
UrsatzStatus api_read_functions(const char *what, const char *base, const cJSON *functions, unsigned *out,
                                  UrsatzError *error);
/* A string member of a request object (null or absent: *out is NULL, an error when required), as the theme request reads it. */
bool api_request_string(const cJSON *object, const char *base, const char *name, bool required, const char **out,
                        UrsatzError *error);
/* The "pickup" member as text: a string, or a number of beats formatted into text. NULL for none. */
bool api_request_pickup(const cJSON *object, const char *base, char *text, size_t size, const char **out,
                        UrsatzError *error);

/* --- the node and lane a request works on (api_target.c) ----------------------------------------------- */

/* "harmony", "bass", "melody" or "inner", any case. */
bool api_lane_from_name(const char *name, PlanLaneKind *out);
/* The lane in words: "harmony", "bass", "melody", "inner voice". */
const char *api_lane_label(PlanLaneKind lane);
/*
 * The targets under node: the node itself, or with a lane every span of it under the node (all the voices for inner), into
 * a new array the caller frees. URSATZ_ERR_UNSATISFIABLE when the node has no span of the lane.
 */
UrsatzStatus api_lane_targets(const Plan *plan, NodeId node, bool has_lane, PlanLaneKind lane, FillTarget **out,
                                size_t *count, UrsatzError *error);
/* Why a regenerate result refused the target (what names it: "section B"): URSATZ_ERR_UNSATISFIABLE with the message. */
UrsatzStatus api_refused(RegenerateResult result, const char *what, UrsatzError *error);

/* The first node of the plan with the name, or PLAN_NODE_NONE. */
NodeId api_find_node(const Plan *plan, const char *name);

/* A number written with %g and read back, so a double in a result has no float noise. */
cJSON *api_number_g(double value);

/* A rational as a number (num / den), for a tempo or a density in a result. */
cJSON *api_ratio_number(const Rational *ratio);

/* True when the object has the member and it is not null. */
bool api_has_member(const cJSON *object, const char *name);

/* --- the outputs a theme or grow request asks for ---------------------------------------------------- */

typedef struct {
    bool midi;
    bool musicxml;
    bool plans;
    bool layers; /* the realization and the reading of the first plan, in "realization" and "reading" */
    int explain; /* 0 none, 1 text, 2 json */
    PlanExportOptions export_options;
} ApiOutputOptions;

/*
 * Adds "realization" (the work of the plan as the MIDI and MusicXML are written from it, realization.v1) and "reading" (the Composer's
 * hearing of the plan over that work, reading.v1, its target the realization's digest) to document. URSATZ_ERR_UNSATISFIABLE when the plan
 * cannot be rendered or heard, URSATZ_ERR_ALLOC when out of memory.
 */
UrsatzStatus api_layers_json(const KnowledgeSnapshot *knowledge, const Plan *plan, const PlanExportOptions *options, cJSON *document,
                               UrsatzError *error);

/* "midi", "musicxml", "plain_midi", "plans", "layers" and "explain" ("text" or "json"). */
UrsatzStatus api_read_output_options(const cJSON *request, ApiOutputOptions *out, UrsatzError *error);

/* --- reading a request without running it (composer_api_validate) --------------------------------------- */

/* Reads the request as its entry does and stops before composing: URSATZ_OK, or the status the entry would give. */
UrsatzStatus api_theme_validate(const KnowledgeSnapshot *knowledge, const CommonPracticeMaterials *materials, const cJSON *request,
                                  UrsatzError *error);
/* As api_theme_validate for a random motive request; filled (a copy of request) gets the seed and the drawn mode and meter. */
UrsatzStatus api_random_validate(const KnowledgeSnapshot *knowledge, const cJSON *request, cJSON *filled, UrsatzError *error);

#endif
