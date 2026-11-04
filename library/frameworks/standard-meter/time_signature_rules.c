#include "time_signature_rules.h"

#include "rule_pair.h"

static const char *const NAMES[STANDARD_METER_TIME_SIGNATURE_RULE_COUNT] = {
    "time-signature-simple-beats", "time-signature-beat-unit", "time-signature-common-cut",
    "time-signature-not-a-fraction"};

static RulePairTable g_table = RULE_PAIR_TABLE_INIT(standard_meter_framework, "standard-meter",
    NAMES, STANDARD_METER_TIME_SIGNATURE_RULE_COUNT);

URSATZ_RULE_ACCESSORS(standard_meter_time_signature, StandardMeterTimeSignatureRule, STANDARD_METER_TIME_SIGNATURE_RULE_COUNT, g_table)

bool standard_meter_beats_are_simple(unsigned beats) {
    return beats >= 2 && beats <= 4;
}

bool standard_meter_beat_unit_is_usual(const TimeSignature *time_signature) {
    if (time_signature == NULL) {
        return false;
    }
    unsigned number = time_signature_bottom_number(time_signature);
    return number == 2 || number == 4 || number == 8;
}

bool standard_meter_is_simple(const TimeSignature *time_signature) {
    /* Every TimeSignature bottom number is already 1, 2, 4, 8, or 16. */
    return time_signature != NULL && standard_meter_beats_are_simple(time_signature_top_number(time_signature));
}

TimeSignatureSymbol standard_meter_signature_symbol(const TimeSignature *time_signature) {
    if (time_signature == NULL) {
        return TIME_SIGNATURE_SYMBOL_NONE;
    }
    unsigned beats = time_signature_top_number(time_signature);
    unsigned number = time_signature_bottom_number(time_signature);
    if (beats == 4 && number == 4) {
        return TIME_SIGNATURE_SYMBOL_COMMON;
    }
    if (beats == 2 && number == 2) {
        return TIME_SIGNATURE_SYMBOL_CUT;
    }
    return TIME_SIGNATURE_SYMBOL_NONE;
}

bool standard_meter_symbol_signature(TimeSignatureSymbol symbol, TimeSignature **out) {
    unsigned beats;
    int exponent;
    switch (symbol) {
    case TIME_SIGNATURE_SYMBOL_COMMON:
        beats = 4;
        exponent = 2;
        break;
    case TIME_SIGNATURE_SYMBOL_CUT:
        beats = 2;
        exponent = 1;
        break;
    default:
        return false;
    }

    NoteValue *beat_unit = NULL;
    if (note_value_create(exponent, &beat_unit) != NOTE_VALUE_OK) {
        return false;
    }
    TimeSignatureResult result = time_signature_create(beats, beat_unit, out);
    note_value_destroy(beat_unit);
    return result == TIME_SIGNATURE_OK;
}

bool standard_meter_same_measure_length(const TimeSignature *a, const TimeSignature *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    Duration *length_a = NULL;
    Duration *length_b = NULL;
    bool same = false;
    if (time_signature_measure_duration(a, &length_a) == DURATION_OK &&
        time_signature_measure_duration(b, &length_b) == DURATION_OK) {
        same = duration_equals(length_a, length_b);
    }
    duration_destroy(length_a);
    duration_destroy(length_b);
    return same;
}
