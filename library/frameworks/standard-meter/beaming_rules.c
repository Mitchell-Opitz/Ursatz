#include "beaming_rules.h"

#include "beat_grid.h"
#include "meter_class_rules.h"
#include "rule_pair.h"

static const char *const NAMES[STANDARD_METER_BEAMING_RULE_COUNT] = {"beaming-beamable", "beaming-notes-per-beat",
                                                                      "beaming-within-beat"};

static StandardMeterRulePair g_pairs[STANDARD_METER_BEAMING_RULE_COUNT];
static bool g_built[STANDARD_METER_BEAMING_RULE_COUNT];

static bool valid_rule(StandardMeterBeamingRule which) {
    return which >= 0 && which < STANDARD_METER_BEAMING_RULE_COUNT;
}

static void ensure_built(StandardMeterBeamingRule which) {
    if (!valid_rule(which) || g_built[which]) {
        return;
    }
    g_built[which] = true;
    standard_meter_build_rule_pair(NAMES[which], &g_pairs[which]);
}

const Rule *standard_meter_beaming_rule(StandardMeterBeamingRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].rule : NULL;
}

const Constraint *standard_meter_beaming_constraint(StandardMeterBeamingRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].constraint : NULL;
}

#define EIGHTH_EXPONENT 3

bool standard_meter_value_beamable(const NoteValue *note_value) {
    return note_value != NULL && note_value_exponent(note_value) >= EIGHTH_EXPONENT;
}

bool standard_meter_notes_per_beat(const TimeSignature *time_signature, const NoteValue *note_value,
                                   unsigned long *out_count) {
    if (time_signature == NULL || note_value == NULL || note_value_dots(note_value) != 0) {
        return false;
    }
    Duration *beat = NULL;
    Duration *value = NULL;
    bool ok = false;

    if (standard_meter_beat_duration(time_signature, &beat) && note_value_duration(note_value, &value) == DURATION_OK) {
        /* beat / value = (beat_n * value_d) / (beat_d * value_n); it must
           come out as a whole number of values of at least one. */
        unsigned long numerator = (unsigned long)duration_numerator(beat) * duration_denominator(value);
        unsigned long denominator = duration_denominator(beat) * (unsigned long)duration_numerator(value);
        if (numerator >= denominator && numerator % denominator == 0) {
            *out_count = numerator / denominator;
            ok = true;
        }
    }

    duration_destroy(beat);
    duration_destroy(value);
    return ok;
}

bool standard_meter_beam_group_within_beat(const TimeSignature *time_signature, const Duration *start,
                                           const Duration *length) {
    if (time_signature == NULL || start == NULL || length == NULL || duration_numerator(length) == 0) {
        return false;
    }

    unsigned beat_index;
    bool on_beat;
    if (!standard_meter_beat_at(time_signature, start, &beat_index, &on_beat)) {
        return false;
    }

    /* the group must end by the start of the next beat */
    Duration *end = NULL;
    Duration *next_beat = NULL;
    bool within = false;
    if (duration_add(start, length, &end) == DURATION_OK &&
        standard_meter_beat_start(time_signature, beat_index + 1, &next_beat)) {
        within = duration_numerator(end) * (long)duration_denominator(next_beat) <=
                 duration_numerator(next_beat) * (long)duration_denominator(end);
    }

    duration_destroy(end);
    duration_destroy(next_beat);
    return within;
}
