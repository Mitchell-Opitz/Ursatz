#include "counting_rules.h"

#include <stdio.h>

#include "rule_pair.h"

static const char *const NAMES[STANDARD_METER_COUNTING_RULE_COUNT] = {
    "counting-beats",  "counting-divisions", "counting-subdivisions",
    "counting-finer", "counting-silent",    "counting-dots",  "counting-borrowed"};

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

/* American traditional counting, indexed by grid step within the beat. */
static const char *const SIMPLE_SYLLABLES[STANDARD_METER_COUNT_GRID_SIMPLE] = {NULL, "ta", "e", "ta",
                                                                              "+",  "ta", "a", "ta"};
static const char *const COMPOUND_SYLLABLES[STANDARD_METER_COUNT_GRID_COMPOUND] = {NULL, "ta", "la",
                                                                                  "ta",  "li", "ta"};

bool standard_meter_count(const TimeSignature *time_signature, const Duration *offset, StandardMeterCount *out) {
    if (time_signature == NULL || offset == NULL) {
        return false;
    }
    MeterType type = standard_meter_type(time_signature);
    unsigned beats;
    Duration *beat = NULL;
    if (type == METER_TYPE_UNCLASSIFIED || !standard_meter_beats_per_measure(time_signature, &beats) ||
        !standard_meter_beat_duration(time_signature, &beat)) {
        return false;
    }
    unsigned parts = type == METER_TYPE_SIMPLE ? STANDARD_METER_COUNT_GRID_SIMPLE : STANDARD_METER_COUNT_GRID_COMPOUND;

    /* offset in grid steps = offset / (beat / parts)
                            = (offset_n * beat_d * parts) / (offset_d * beat_n) */
    unsigned long numerator = (unsigned long)duration_numerator(offset) * duration_denominator(beat) * parts;
    unsigned long denominator = duration_denominator(offset) * (unsigned long)duration_numerator(beat);
    duration_destroy(beat);
    if (numerator % denominator != 0) {
        return false;
    }
    unsigned long units = numerator / denominator;

    unsigned long beat_index = units / parts;
    if (beat_index >= beats) {
        return false;
    }

    out->beat = (unsigned)beat_index + 1;
    out->position = (unsigned)(units % parts);
    out->parts = parts;
    out->syllable = type == METER_TYPE_SIMPLE ? SIMPLE_SYLLABLES[out->position] : COMPOUND_SYLLABLES[out->position];
    return true;
}

bool standard_meter_tuplet_count_syllable(const Tuplet *tuplet, unsigned index, const char **out_syllable) {
    if (tuplet == NULL || index >= tuplet_actual(tuplet)) {
        return false;
    }
    if (tuplet_actual(tuplet) == 3 && tuplet_normal(tuplet) == 2) {
        /* the compound division: the beat, then "la" and "li" */
        *out_syllable = index == 0 ? NULL : COMPOUND_SYLLABLES[index == 1 ? 2 : 4];
        return true;
    }
    if (tuplet_actual(tuplet) == 2 && tuplet_normal(tuplet) == 3) {
        /* the simple division: the beat, then "+" */
        *out_syllable = index == 0 ? NULL : SIMPLE_SYLLABLES[4];
        return true;
    }
    return false;
}

bool standard_meter_dot_silences_beat(MeterType type) {
    return type == METER_TYPE_SIMPLE;
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
