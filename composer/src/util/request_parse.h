#ifndef URSATZ_COMPOSER_REQUEST_PARSE_H
#define URSATZ_COMPOSER_REQUEST_PARSE_H

#include <stdbool.h>
#include <stddef.h>

#include <ursatz_composer/constraint.h>
#include <ursatz_composer/lanes.h>
#include <ursatz_composer/piece_plan.h>
#include <ursatz_composer/style.h>

/* The values a request or a command line names, as text: shared by the CLI and the API (src/api). Private: not installed. */

bool request_equals_ignore_case(const char *a, const char *b);

/* A whole number from 0 to maximum, digits only. */
bool request_parse_whole_number(const char *text, unsigned long maximum, unsigned long *out);

/* "3/2" or "3": a weight, both parts whole numbers up to 4294967295 and the denominator at least 1. */
bool request_parse_ratio(const char *text, ComposerRatio *out);

/* A tonic letter, an optional # or b, whitespace, then major or minor ("D minor", "F# major"); any case for the mode. */
bool request_parse_key(const char *text, AnalysisKey *key);

/* "3/4", "6/8": 1 to 64 beats over a beat unit of 1, 2, 4, 8 or 16. The meters the harmony operators support are checked by the caller (harmony_meter_supported). */
bool request_parse_meter(const char *text, PlanMeter *meter);

/* PAC, IAC, HC, DC or PC, any case. */
bool request_parse_cadence(const char *text, CadenceType *cadence);

/* A pickup length in beats: "1", "3/2". beat_unit is the meter's. */
bool request_parse_pickup(const char *text, int beat_unit, Rational *out);

#endif
