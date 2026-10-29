#include "syncopation_rules.h"

#include "beat_grid.h"
#include "beat_tie_rules.h"
#include "rule_pair.h"

static const char *const NAMES[STANDARD_METER_SYNCOPATION_RULE_COUNT] = {
    "syncopation-off-beat", "syncopation-sustain", "syncopation-rest", "syncopation-accent",
    "syncopation-through-stronger"};

static RulePairTable g_table = RULE_PAIR_TABLE_INIT(standard_meter_framework, "standard-meter",
    NAMES, STANDARD_METER_SYNCOPATION_RULE_COUNT);

static bool valid_rule(StandardMeterSyncopationRule which) {
    return which >= 0 && which < STANDARD_METER_SYNCOPATION_RULE_COUNT;
}

const Rule *standard_meter_syncopation_rule(StandardMeterSyncopationRule which) {
    const RulePair *pair = valid_rule(which) ? rule_pair_table_get(&g_table, (size_t)which) : NULL;
    return pair != NULL ? pair->rule : NULL;
}

const Constraint *standard_meter_syncopation_constraint(StandardMeterSyncopationRule which) {
    const RulePair *pair = valid_rule(which) ? rule_pair_table_get(&g_table, (size_t)which) : NULL;
    return pair != NULL ? pair->constraint : NULL;
}

bool standard_meter_is_off_beat(const TimeSignature *time_signature, const Duration *onset) {
    unsigned index;
    bool on_beat;
    return standard_meter_beat_at(time_signature, onset, &index, &on_beat) && !on_beat;
}

bool standard_meter_syncopated_by_sustain(const TimeSignature *time_signature, const Duration *onset,
                                          const Duration *length) {
    /* starts off the beat and runs past the next beat line: the same
       shape as a note that is split and tied at the beat line */
    Duration *first = NULL;
    bool syncopated = standard_meter_beat_split(time_signature, onset, length, &first);
    duration_destroy(first);
    return syncopated;
}

bool standard_meter_syncopated_by_rest(const TimeSignature *time_signature, const Duration *rest_onset,
                                       const Duration *note_onset) {
    unsigned index;
    bool on_beat;
    return standard_meter_beat_at(time_signature, rest_onset, &index, &on_beat) && on_beat &&
           standard_meter_is_off_beat(time_signature, note_onset);
}

bool standard_meter_syncopated_by_accent(const TimeSignature *time_signature, const Duration *onset, bool accented) {
    return accented && standard_meter_is_off_beat(time_signature, onset);
}

bool standard_meter_syncopated_through_stronger(StandardMeterLevel attack, StandardMeterLevel sustained,
                                                unsigned *out_strength) {
    if (attack < STANDARD_METER_LEVEL_DOWNBEAT || attack > STANDARD_METER_LEVEL_FINER ||
        sustained < STANDARD_METER_LEVEL_DOWNBEAT || sustained > STANDARD_METER_LEVEL_FINER || sustained >= attack) {
        return false;
    }
    if (out_strength != NULL) {
        *out_strength = (unsigned)(attack - sustained);
    }
    return true;
}
