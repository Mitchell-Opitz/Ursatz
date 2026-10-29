#include "metric_position_rules.h"

#include <limits.h>

#include "beat_grid.h"
#include "core/rational.h"
#include "grouping_rules.h"
#include "meter_class_rules.h"
#include "rule_pair.h"

static const char *const NAMES[STANDARD_METER_METRIC_RULE_COUNT] = {"metric-beat-position", "metric-level",
                                                                     "metric-beat-strength"};

static RulePairTable g_table = RULE_PAIR_TABLE_INIT(standard_meter_framework, "standard-meter",
    NAMES, STANDARD_METER_METRIC_RULE_COUNT);

static bool valid_rule(StandardMeterMetricRule which) {
    return which >= 0 && which < STANDARD_METER_METRIC_RULE_COUNT;
}

const Rule *standard_meter_metric_rule(StandardMeterMetricRule which) {
    const RulePair *pair = valid_rule(which) ? rule_pair_table_get(&g_table, (size_t)which) : NULL;
    return pair != NULL ? pair->rule : NULL;
}

const Constraint *standard_meter_metric_constraint(StandardMeterMetricRule which) {
    const RulePair *pair = valid_rule(which) ? rule_pair_table_get(&g_table, (size_t)which) : NULL;
    return pair != NULL ? pair->constraint : NULL;
}

bool standard_meter_beat_grid(const TimeSignature *time_signature, Duration **out_beat, unsigned *out_beats) {
    if (time_signature == NULL || out_beat == NULL || out_beats == NULL) {
        return false;
    }
    if (standard_meter_beat_duration(time_signature, out_beat)) {
        return standard_meter_beats_per_measure(time_signature, out_beats);
    }
    if (time_signature_unit_duration(time_signature, out_beat) != DURATION_OK) {
        return false;
    }
    *out_beats = time_signature_top_number(time_signature);
    return true;
}

/* offset / beat, as a whole number of beats and the exact fraction of a beat left over. The products
   are checked: false on overflow. */
static bool split_into_beats(const Duration *offset, const Duration *beat, unsigned long long *out_index,
                             unsigned long long *out_fraction_numerator, unsigned long long *out_fraction_denominator) {
    unsigned long long offset_n = (unsigned long long)duration_numerator(offset);
    unsigned long long offset_d = duration_denominator(offset);
    unsigned long long beat_n = (unsigned long long)duration_numerator(beat);
    unsigned long long beat_d = duration_denominator(beat);
    if ((offset_n != 0 && beat_d > ULLONG_MAX / offset_n) || offset_d > ULLONG_MAX / beat_n) {
        return false;
    }
    unsigned long long numerator = offset_n * beat_d;
    unsigned long long denominator = offset_d * beat_n;
    *out_index = numerator / denominator;
    unsigned long long remainder = numerator % denominator;
    unsigned long long g = rational_gcd_unsigned(remainder, denominator);
    *out_fraction_numerator = remainder / g;
    *out_fraction_denominator = denominator / g;
    return true;
}

typedef struct {
    unsigned beat; /* 1-based */
    unsigned long long fraction_numerator;
    unsigned long long fraction_denominator;
    unsigned beats;
} Located;

static bool locate(const TimeSignature *time_signature, const MetricalPosition *position, Located *out) {
    if (time_signature == NULL || position == NULL) {
        return false;
    }
    Duration *beat = NULL;
    unsigned beats = 0;
    if (!standard_meter_beat_grid(time_signature, &beat, &beats)) {
        duration_destroy(beat);
        return false;
    }

    unsigned long long index = 0;
    unsigned long long fraction_numerator = 0;
    unsigned long long fraction_denominator = 1;
    bool split = split_into_beats(metrical_position_offset(position), beat, &index, &fraction_numerator,
                                  &fraction_denominator);
    duration_destroy(beat);
    if (!split || index >= beats) {
        return false;
    }

    out->beat = (unsigned)index + 1;
    out->fraction_numerator = fraction_numerator;
    out->fraction_denominator = fraction_denominator;
    out->beats = beats;
    return true;
}

bool standard_meter_beat_position(const TimeSignature *time_signature, const MetricalPosition *position,
                                  StandardMeterBeatPosition *out) {
    Located located;
    if (out == NULL || !locate(time_signature, position, &located)) {
        return false;
    }
    if (located.fraction_numerator > (unsigned long long)LONG_MAX ||
        located.fraction_denominator > (unsigned long long)ULONG_MAX) {
        return false;
    }
    out->beat = located.beat;
    out->subdivision_numerator = (long)located.fraction_numerator;
    out->subdivision_denominator = (unsigned long)located.fraction_denominator;
    return true;
}

/* Parts the beat divides into at the division level: 2 simple, 3 compound, 2 for an unclassified
   meter's pulse. */
static unsigned division_parts(const TimeSignature *time_signature) {
    unsigned division = 0;
    unsigned subdivision = 0;
    if (standard_meter_beat_division(standard_meter_type(time_signature), &division, &subdivision)) {
        return division;
    }
    return 2;
}

bool standard_meter_metric_level(const TimeSignature *time_signature, const MetricalPosition *position,
                                 StandardMeterLevel *out) {
    Located located;
    if (out == NULL || !locate(time_signature, position, &located)) {
        return false;
    }

    if (located.fraction_numerator == 0) {
        if (located.beat == 1) {
            *out = STANDARD_METER_LEVEL_DOWNBEAT;
        } else if (located.beat == 3 && standard_meter_classify(time_signature).grouping == METER_GROUPING_QUADRUPLE) {
            *out = STANDARD_METER_LEVEL_SECONDARY_BEAT;
        } else {
            *out = STANDARD_METER_LEVEL_BEAT;
        }
        return true;
    }

    /* Off the beat: on the division grid (k / parts of a beat) is the off-beat level, and any place
       between those points is finer. The fraction is in lowest terms, so it is k / parts exactly
       when its denominator divides parts. */
    unsigned long long parts = division_parts(time_signature);
    bool on_division = parts % located.fraction_denominator == 0;
    *out = on_division ? STANDARD_METER_LEVEL_OFFBEAT : STANDARD_METER_LEVEL_FINER;
    return true;
}

BeatStrength standard_meter_level_strength(StandardMeterLevel level) {
    switch (level) {
    case STANDARD_METER_LEVEL_DOWNBEAT:
        return BEAT_STRENGTH_STRONG;
    case STANDARD_METER_LEVEL_SECONDARY_BEAT:
        return BEAT_STRENGTH_MEDIUM;
    default:
        return BEAT_STRENGTH_WEAK;
    }
}

bool standard_meter_beat_strength(const TimeSignature *time_signature, const MetricalPosition *position,
                                  BeatStrength *out) {
    StandardMeterLevel level;
    if (out == NULL || !standard_meter_metric_level(time_signature, position, &level)) {
        return false;
    }
    *out = standard_meter_level_strength(level);
    return true;
}

bool standard_meter_accent_beats(const TimeSignature *time_signature, unsigned *beats, BeatStrength *strengths,
                                 size_t capacity, size_t *count) {
    if (time_signature == NULL || beats == NULL || count == NULL) {
        return false;
    }
    Duration *beat = NULL;
    unsigned per_measure = 0;
    if (!standard_meter_beat_grid(time_signature, &beat, &per_measure)) {
        return false;
    }

    unsigned found_beats[64];
    BeatStrength found_strengths[64];
    size_t found = 0;
    Duration *start = NULL;
    bool ok = duration_create(0, 1, &start) == DURATION_OK;
    for (unsigned index = 0; ok && index < per_measure && index < 64; index++) {
        MetricalPosition *position = NULL;
        BeatStrength strength;
        ok = metrical_position_create(1, start, &position) == METRICAL_POSITION_OK &&
             standard_meter_beat_strength(time_signature, position, &strength);
        metrical_position_destroy(position);
        if (!ok) {
            break;
        }
        if (strength == BEAT_STRENGTH_STRONG || strength == BEAT_STRENGTH_MEDIUM) {
            found_beats[found] = index;
            found_strengths[found] = strength;
            found++;
        }
        Duration *next = NULL;
        ok = duration_add(start, beat, &next) == DURATION_OK;
        duration_destroy(start);
        start = next;
    }
    duration_destroy(start);
    duration_destroy(beat);
    if (!ok || found > capacity) {
        return false;
    }
    for (size_t i = 0; i < found; i++) {
        beats[i] = found_beats[i];
        if (strengths != NULL) {
            strengths[i] = found_strengths[i];
        }
    }
    *count = found;
    return true;
}
