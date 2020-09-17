#include "beat_tie_rules.h"

#include "rule_pair.h"

static const char *const NAMES[STANDARD_METER_BEAT_TIE_RULE_COUNT] = {"beat-tie-beat-split"};

static StandardMeterRulePair g_pairs[STANDARD_METER_BEAT_TIE_RULE_COUNT];
static bool g_built[STANDARD_METER_BEAT_TIE_RULE_COUNT];

static bool valid_rule(StandardMeterBeatTieRule which) {
    return which >= 0 && which < STANDARD_METER_BEAT_TIE_RULE_COUNT;
}

static void ensure_built(StandardMeterBeatTieRule which) {
    if (!valid_rule(which) || g_built[which]) {
        return;
    }
    g_built[which] = true;
    standard_meter_build_rule_pair(NAMES[which], &g_pairs[which]);
}

const Rule *standard_meter_beat_tie_rule(StandardMeterBeatTieRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].rule : NULL;
}

const Constraint *standard_meter_beat_tie_constraint(StandardMeterBeatTieRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].constraint : NULL;
}

bool standard_meter_beat_split(const TimeSignature *time_signature, const Duration *start, const Duration *length,
                               Duration **out_first) {
    if (time_signature == NULL || start == NULL || length == NULL || duration_numerator(length) == 0) {
        return false;
    }

    /* beat k (0-based) starts at k / 2^e of a whole note; start is n/d. */
    unsigned long beat_scale = 1UL << time_signature_beat_unit_exponent(time_signature);
    unsigned long scaled = (unsigned long)duration_numerator(start) * beat_scale;
    unsigned long denominator = duration_denominator(start);
    unsigned long beat_index = scaled / denominator;
    if (beat_index >= time_signature_beats(time_signature) || scaled % denominator == 0) {
        return false; /* outside the measure, or starting on a beat */
    }

    Duration *next_beat = NULL;
    Duration *end = NULL;
    Duration *first = NULL;
    bool split = false;

    if (duration_create((long)(beat_index + 1), beat_scale, &next_beat) == DURATION_OK &&
        duration_add(start, length, &end) == DURATION_OK) {
        /* runs past the boundary: end > next_beat */
        bool past = duration_numerator(end) * (long)duration_denominator(next_beat) >
                    duration_numerator(next_beat) * (long)duration_denominator(end);
        split = past && duration_subtract(next_beat, start, &first) == DURATION_OK;
    }

    duration_destroy(next_beat);
    duration_destroy(end);
    if (split) {
        *out_first = first;
    }
    return split;
}
