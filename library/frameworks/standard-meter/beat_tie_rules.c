#include "beat_tie_rules.h"

#include "beat_grid.h"
#include "rule_pair.h"

static const char *const NAMES[STANDARD_METER_BEAT_TIE_RULE_COUNT] = {"beat-tie-beat-split"};

static RulePairTable g_table = RULE_PAIR_TABLE_INIT(standard_meter_framework, "standard-meter",
    NAMES, STANDARD_METER_BEAT_TIE_RULE_COUNT);

URSATZ_RULE_ACCESSORS(standard_meter_beat_tie, StandardMeterBeatTieRule, STANDARD_METER_BEAT_TIE_RULE_COUNT, g_table)

bool standard_meter_beat_split(const TimeSignature *time_signature, const Duration *start, const Duration *length,
                               Duration **out_first) {
    if (time_signature == NULL || start == NULL || length == NULL || duration_numerator(length) == 0) {
        return false;
    }

    unsigned beat_index;
    bool on_beat;
    if (!standard_meter_beat_at(time_signature, start, &beat_index, &on_beat) || on_beat) {
        return false; /* outside the measure, or starting on a beat */
    }

    Duration *next_beat = NULL;
    Duration *end = NULL;
    Duration *first = NULL;
    bool split = false;

    if (standard_meter_beat_start(time_signature, beat_index + 1, &next_beat) &&
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
