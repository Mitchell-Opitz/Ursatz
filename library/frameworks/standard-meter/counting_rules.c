#include "counting_rules.h"

#include <stdio.h>

#include "rule_pair.h"

static const char *const NAMES[STANDARD_METER_COUNTING_RULE_COUNT] = {
    "counting-beats", "counting-divisions", "counting-subdivisions", "counting-finer", "counting-silent"};

static StandardMeterRulePair g_pairs[STANDARD_METER_COUNTING_RULE_COUNT];
static bool g_built[STANDARD_METER_COUNTING_RULE_COUNT];

static bool valid_rule(StandardMeterCountingRule which) {
    return which >= 0 && which < STANDARD_METER_COUNTING_RULE_COUNT;
}

static void ensure_built(StandardMeterCountingRule which) {
    if (!valid_rule(which) || g_built[which]) {
        return;
    }
    g_built[which] = true;
    standard_meter_build_rule_pair(NAMES[which], &g_pairs[which]);
}

const Rule *standard_meter_counting_rule(StandardMeterCountingRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].rule : NULL;
}

const Constraint *standard_meter_counting_constraint(StandardMeterCountingRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].constraint : NULL;
}

/* American traditional counting, indexed by eighths of a beat. */
static const char *const AMERICAN_SYLLABLES[STANDARD_METER_COUNT_GRID] = {NULL, "ta", "e", "ta", "+", "ta", "a", "ta"};

bool standard_meter_count(const TimeSignature *time_signature, const Duration *offset, StandardMeterCount *out) {
    if (time_signature == NULL || offset == NULL) {
        return false;
    }

    /* offset is n/d of a whole note and a beat is 1/2^e of one, so the
       offset in eighths of a beat is n * 2^e * 8 / d. */
    unsigned long grid = (unsigned long)duration_numerator(offset) *
                         (1UL << time_signature_beat_unit_exponent(time_signature)) * STANDARD_METER_COUNT_GRID;
    unsigned long denominator = duration_denominator(offset);
    if (grid % denominator != 0) {
        return false;
    }
    unsigned long units = grid / denominator;

    unsigned long beat_index = units / STANDARD_METER_COUNT_GRID;
    if (beat_index >= time_signature_beats(time_signature)) {
        return false;
    }

    out->beat = (unsigned)beat_index + 1;
    out->eighths = (unsigned)(units % STANDARD_METER_COUNT_GRID);
    out->syllable = AMERICAN_SYLLABLES[out->eighths];
    return true;
}

bool standard_meter_count_text(const StandardMeterCount *count, bool articulated, char *buffer, size_t size) {
    if (count == NULL || buffer == NULL) {
        return false;
    }

    char word[16];
    if (count->syllable == NULL) {
        snprintf(word, sizeof(word), "%u", count->beat);
    } else {
        snprintf(word, sizeof(word), "%s", count->syllable);
    }

    int written = articulated ? snprintf(buffer, size, "%s", word) : snprintf(buffer, size, "(%s)", word);
    return written >= 0 && (size_t)written < size;
}
