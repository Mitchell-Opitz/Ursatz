#include "beaming_rules.h"

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
    int steps = note_value_exponent(note_value) - time_signature_bottom_unit_exponent(time_signature);
    if (steps < 0) {
        return false;
    }
    *out_count = 1UL << steps;
    return true;
}

bool standard_meter_beam_group_within_beat(const TimeSignature *time_signature, const Duration *start,
                                           const Duration *length) {
    if (time_signature == NULL || start == NULL || length == NULL || duration_numerator(length) == 0) {
        return false;
    }

    /* beat k (0-based) spans [k, k + 1) / 2^e of a whole note; start is
       n/d, so k = floor(n * 2^e / d). */
    unsigned long beat_scale = 1UL << time_signature_bottom_unit_exponent(time_signature);
    unsigned long beat_index = ((unsigned long)duration_numerator(start) * beat_scale) / duration_denominator(start);
    if (beat_index >= time_signature_top_number(time_signature)) {
        return false;
    }

    Duration *end = NULL;
    if (duration_add(start, length, &end) != DURATION_OK) {
        return false;
    }

    /* end <= (k + 1) / 2^e  <=>  end_n * 2^e <= (k + 1) * end_d */
    bool within = (unsigned long)duration_numerator(end) * beat_scale <=
                  (beat_index + 1) * duration_denominator(end);
    duration_destroy(end);
    return within;
}
