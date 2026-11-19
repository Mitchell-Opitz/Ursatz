#ifndef URSATZ_COMPOSER_REQUEST_PARSE_H
#define URSATZ_COMPOSER_REQUEST_PARSE_H

#include <stdbool.h>
#include <stddef.h>

#include <theory/tonality/common_practice_materials.h>

#include <plan/plan_constraint.h>
#include <plan/plan_lanes.h>
#include <plan/plan.h>
#include <ursatz_composer/style.h>

/* The values a request or a command line names, as text: shared by the CLI and the API (src/api). Private: not installed. */

bool request_equals_ignore_case(const char *a, const char *b);

/* A whole number from 0 to maximum, digits only. */
bool request_parse_whole_number(const char *text, unsigned long maximum, unsigned long *out);

/* "3/2" or "3": a weight, both parts whole numbers up to 4294967295 and the denominator at least 1. */
bool request_parse_ratio(const char *text, Rational *out);

/* The longest word after a key's tonic: "harmonic minor" and the like. */
#define REQUEST_KEY_WORD_MAX 64

/*
 * A tonic letter, an optional # or b, whitespace, then the word of a material the snapshot knows by its key word, id, name or alias
 * (knowledge_snapshot_material_named: "D minor", "F# major", "G natural minor"; any case for the word), the key of that material. A
 * word that names no material or a material the Composer does not compose in (composer_material_supported: anything but the major
 * and the natural minor) is refused. False for a NULL snapshot.
 */
bool request_parse_key(const char *text, const KnowledgeSnapshot *knowledge, SpelledKey *key);

/*
 * Whether the key text is a tonic and the word of a material the snapshot knows but the Composer does not compose in ("C dorian",
 * "C harmonic minor"): true, with message "the Composer writes major and minor keys; '<word>' is not one of them", for the callers to
 * report in place of "unknown key". False for any other text (a key it refuses for another reason, or one it accepts).
 */
bool request_key_refusal(const char *text, const KnowledgeSnapshot *knowledge, char *message, size_t size);

/* "3/4", "6/8": 1 to 64 beats over a beat unit of 1, 2, 4, 8 or 16. The meters the harmony operators support are checked by the caller (harmony_meter_supported). */
bool request_parse_meter(const char *text, PlanMeter *meter);

/* PAC, IAC, HC, DC or PC, any case. */
bool request_parse_cadence(const char *text, CadenceType *cadence);

/* A pickup length in beats: "1", "3/2". beat_unit is the meter's. */
bool request_parse_pickup(const char *text, int beat_unit, Rational *out);

#endif
