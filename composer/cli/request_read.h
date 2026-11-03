#ifndef URSATZ_COMPOSER_CLI_REQUEST_READ_H
#define URSATZ_COMPOSER_CLI_REQUEST_READ_H

#include <stdbool.h>
#include <stddef.h>

#include <cJSON.h>

#include <ursatz_composer/motive.h>
#include <ursatz_composer/piece_plan.h>

#include "api/api_grow.h"

/*
 * The request readers the `piece` command uses (R-173): the readers of the composer API (src/api), each printing the error
 * it found as the command line does and failing with false or a non-zero status (CLI_EXIT_USAGE).
 */
/* A member that is a string (null or absent when not required, then *out is NULL). */
bool request_member_string(const cJSON *object, const char *name, bool required, const char **out);
/* The "pickup" member as text: a string, or a number of beats formatted into text. NULL for none. */
bool request_member_pickup(const cJSON *object, char *text, size_t size, const char **out);
/* A motive of a request, a degree list in the key with its pickup, harmony, features and role (role forced when not NULL). */
int request_read_motive(const char *what, const cJSON *object, const char *role, const PlanMeter *meter, const AnalysisKey *key,
                        Motive *out);
/* The GROW_FUNCTION_* bits a form's "functions" list names. */
bool request_read_functions(const char *what, const cJSON *functions, unsigned *out);
/* The brief of a run as text lines, or the JSON member "brief":{...} (no leading comma). */
void cli_print_brief(const GrowBrief *brief, bool json);

#endif
