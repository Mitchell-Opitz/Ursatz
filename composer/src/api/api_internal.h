#ifndef URSATZ_COMPOSER_API_INTERNAL_H
#define URSATZ_COMPOSER_API_INTERNAL_H

#include <stdbool.h>

#include <cJSON.h>
#include <ursatz_composer/composer_api.h>
#include <ursatz_composer/export.h>
#include <ursatz_composer/fill_target.h>
#include <ursatz_composer/lanes.h>
#include <ursatz_composer/motive.h>
#include <ursatz_composer/regenerate.h>
#include <ursatz_composer/style.h>
#include "util/job.h"

/* Parses the request text into *out (the caller deletes it). Checks "api_version" when present. On failure sets error and returns the status. */
ComposerStatus api_parse_request(const char *text, cJSON **out, ComposerError *error);

/* Sets error and returns status. path may be NULL or "". */
ComposerStatus api_fail(ComposerError *error, ComposerStatus status, const char *path, const char *format, ...);

/* Prints the value unformatted into out->json and deletes it. Returns COMPOSER_OK or COMPOSER_ERR_ALLOC (with error set). */
ComposerStatus api_respond_json(ComposerResponse *out, cJSON *value);

/*
 * Member readers. Each reads the member name of object: absent or null leaves *out as it is and sets *present (when not
 * NULL) to false, returning true; a wrong type or an invalid value sets error (path "/<name>") and returns false.
 */
bool api_member_string(const cJSON *object, const char *name, const char **out, bool *present, ComposerError *error);
bool api_member_bool(const cJSON *object, const char *name, bool *out, bool *present, ComposerError *error);
bool api_member_int(const cJSON *object, const char *name, long minimum, long maximum, long *out, bool *present,
                    ComposerError *error);
/* A whole number, or a string "3/2" (request_parse_ratio). */
bool api_member_rational(const cJSON *object, const char *name, ComposerRatio *out, bool *present, ComposerError *error);
/* "D minor" (request_parse_key). */
bool api_member_key(const cJSON *object, const char *name, AnalysisKey *out, bool *present, ComposerError *error);
/* "3/4" (request_parse_meter). */
bool api_member_meter(const cJSON *object, const char *name, PlanMeter *out, bool *present, ComposerError *error);
/* "PAC" (request_parse_cadence). */
bool api_member_cadence(const cJSON *object, const char *name, CadenceType *out, bool *present, ComposerError *error);
/* A number of beats of beat_unit, or a string "0", "1/2" (request_parse_pickup). */
bool api_member_pickup(const cJSON *object, const char *name, int beat_unit, Rational *out, bool *present,
                       ComposerError *error);

/*
 * A whole number from minimum to maximum, given as a JSON number or as a string of digits ("3"). A value that is not one
 * is refused with the message "<name> must be a whole number from <minimum> to <maximum>, not '<value>'".
 */
bool api_member_whole(const cJSON *object, const char *name, unsigned long minimum, unsigned long maximum,
                      unsigned long *out, bool *present, ComposerError *error);

/*
 * Loads the style model file at path into *out, without the named categories (none when count is 0); the caller destroys
 * it. A category that is not one the Analyzer defines is COMPOSER_ERR_REQUEST with path_without, a file that cannot be
 * read is COMPOSER_ERR_UNSATISFIABLE with path_file; both messages say "cannot read style '<path>': <reason>".
 */
ComposerStatus api_load_style(const char *path, const char *const *without, size_t without_count, const char *path_file,
                              const char *path_without, ComposerStyle **out, ComposerError *error);

/* A seed from the clock, for a request that gives none. */
unsigned long api_default_seed(void);

/*
 * The "tempo" member of the request (quarter notes per minute, 20 to 400, a whole number, a fraction string like
 * "195/2", or null) into options->tempo_numerator and tempo_denominator; absent or null leaves them as they are.
 */
ComposerStatus api_read_tempo(const cJSON *request, PlanExportOptions *options, ComposerError *error);

/*
 * Reads a motive from the request object (docs/api.md, "Motive requests"): "input" {"format", "form", "text"}, "meter",
 * "key", "pickup", and the extras of api_read_motive_extras. base is the JSON Pointer of the object in the request ("" for
 * the request itself), which the error paths start with. Returns COMPOSER_OK; COMPOSER_ERR_REQUEST for a member that is
 * not valid (error path set); COMPOSER_ERR_UNSATISFIABLE for text that is not a motive of the kind asked for. options
 * (may be NULL) gets the options the motive was read with.
 */
ComposerStatus api_read_motive(const cJSON *request, const char *base, Motive *out, MotiveOptions *options,
                               ComposerError *error);

/* As api_read_motive for a motive that arrives as the bytes of a Standard MIDI File: the request has no "input". */
ComposerStatus api_read_motive_bytes(const cJSON *request, const char *base, const unsigned char *bytes, size_t size,
                                     Motive *out, MotiveOptions *options, ComposerError *error);

/*
 * The motive's own harmony ("harmony"), labelled features ("features") and role ("role") of object, into options.
 * meter is the motive's meter, which the chord onsets are counted in; it may be NULL only when there is no harmony.
 */
ComposerStatus api_read_motive_extras(const cJSON *object, const char *base, const PlanMeter *meter,
                                      MotiveOptions *options, ComposerError *error);

/*
 * The motives of a theme or piece request (docs/theme-request.md): a motive object {"degrees", "pickup", "harmony",
 * "features", "role", "form", "tone_overrides"} read in the key and meter. what names the motive in the messages ("motive
 * 'A'"), base is the JSON Pointer of the object. role forces the role when not NULL. Returns COMPOSER_OK, or
 * COMPOSER_ERR_REQUEST naming the member.
 */
ComposerStatus api_read_degrees_motive(const char *what, const char *base, const cJSON *object, const char *role,
                                       const PlanMeter *meter, const AnalysisKey *key, Motive *out, ComposerError *error);
/* The GROW_FUNCTION_* bits a form's "functions" list names. */
ComposerStatus api_read_functions(const char *what, const char *base, const cJSON *functions, unsigned *out,
                                  ComposerError *error);
/* A string member of a request object (null or absent: *out is NULL, an error when required), as the theme request reads it. */
bool api_request_string(const cJSON *object, const char *base, const char *name, bool required, const char **out,
                        ComposerError *error);
/* The "pickup" member as text: a string, or a number of beats formatted into text. NULL for none. */
bool api_request_pickup(const cJSON *object, const char *base, char *text, size_t size, const char **out,
                        ComposerError *error);

/* --- the node and lane a request works on (api_target.c) ----------------------------------------------- */

/* "harmony", "bass", "melody" or "inner", any case. */
bool api_lane_from_name(const char *name, PlanLaneKind *out);
/* The lane in words: "harmony", "bass", "melody", "inner voice". */
const char *api_lane_label(PlanLaneKind lane);
/*
 * The targets under node: the node itself, or with a lane every span of it under the node (all the voices for inner), into
 * a new array the caller frees. COMPOSER_ERR_UNSATISFIABLE when the node has no span of the lane.
 */
ComposerStatus api_lane_targets(const PiecePlan *plan, NodeId node, bool has_lane, PlanLaneKind lane, FillTarget **out,
                                size_t *count, ComposerError *error);
/* Why a regenerate result refused the target (what names it: "section B"): COMPOSER_ERR_UNSATISFIABLE with the message. */
ComposerStatus api_refused(RegenerateResult result, const char *what, ComposerError *error);

/* --- reading a request without running it (composer_api_validate) --------------------------------------- */

/* Reads the request as its entry does and stops before composing: COMPOSER_OK, or the status the entry would give. */
ComposerStatus api_theme_validate(const cJSON *request, ComposerError *error);
/* As api_theme_validate for a random motive request; filled (a copy of request) gets the seed and the drawn mode and meter. */
ComposerStatus api_random_validate(const cJSON *request, cJSON *filled, ComposerError *error);

#endif
