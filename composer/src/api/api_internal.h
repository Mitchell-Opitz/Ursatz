#ifndef URSATZ_COMPOSER_API_INTERNAL_H
#define URSATZ_COMPOSER_API_INTERNAL_H

#include <stdbool.h>

#include <cJSON.h>
#include <ursatz_composer/composer_api.h>

/* Parses the request text into *out (the caller deletes it). Checks "api_version" when present. On failure sets error and returns the status. */
ComposerStatus api_parse_request(const char *text, cJSON **out, ComposerError *error);

/* Sets error and returns status. path may be NULL or "". */
ComposerStatus api_fail(ComposerError *error, ComposerStatus status, const char *path, const char *format, ...);

/* Prints the value unformatted into out->json and deletes it. Returns COMPOSER_OK or COMPOSER_ERR_ALLOC (with error set). */
ComposerStatus api_respond_json(ComposerResponse *out, cJSON *value);

/* Reports progress when the job has a callback; returns true when the job is cancelled. A NULL job never cancels. */
bool composer_job_step(const ComposerJob *job, const char *stage, unsigned done, unsigned total);

#endif
