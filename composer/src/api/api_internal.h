#ifndef URSATZ_COMPOSER_API_INTERNAL_H
#define URSATZ_COMPOSER_API_INTERNAL_H

#include <stdbool.h>

#include <cJSON.h>
#include <ursatz_composer/composer_api.h>
#include <ursatz_composer/lanes.h>
#include <ursatz_composer/style.h>

/* Parses the request text into *out (the caller deletes it). Checks "api_version" when present. On failure sets error and returns the status. */
ComposerStatus api_parse_request(const char *text, cJSON **out, ComposerError *error);

/* Sets error and returns status. path may be NULL or "". */
ComposerStatus api_fail(ComposerError *error, ComposerStatus status, const char *path, const char *format, ...);

/* Prints the value unformatted into out->json and deletes it. Returns COMPOSER_OK or COMPOSER_ERR_ALLOC (with error set). */
ComposerStatus api_respond_json(ComposerResponse *out, cJSON *value);

/* Reports progress when the job has a callback; returns true when the job is cancelled. A NULL job never cancels. */
bool composer_job_step(const ComposerJob *job, const char *stage, unsigned done, unsigned total);

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

#endif
