#include "meter_class_rules.h"

#include "rule_pair.h"

static const char *const NAMES[STANDARD_METER_CLASS_RULE_COUNT] = {
    "class-classify", "class-beats-per-measure", "class-beat-division", "class-compound-beat",
    "class-compound-bottom"};

static StandardMeterRulePair g_pairs[STANDARD_METER_CLASS_RULE_COUNT];
static bool g_built[STANDARD_METER_CLASS_RULE_COUNT];

static bool valid_rule(StandardMeterClassRule which) {
    return which >= 0 && which < STANDARD_METER_CLASS_RULE_COUNT;
}

static void ensure_built(StandardMeterClassRule which) {
    if (!valid_rule(which) || g_built[which]) {
        return;
    }
    g_built[which] = true;
    standard_meter_build_rule_pair(NAMES[which], &g_pairs[which]);
}

const Rule *standard_meter_class_rule(StandardMeterClassRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].rule : NULL;
}

const Constraint *standard_meter_class_constraint(StandardMeterClassRule which) {
    ensure_built(which);
    return valid_rule(which) ? g_pairs[which].constraint : NULL;
}

MeterClass standard_meter_classify(const TimeSignature *time_signature) {
    MeterClass unclassified = {METER_TYPE_UNCLASSIFIED, METER_GROUPING_NONE};
    if (time_signature == NULL) {
        return unclassified;
    }

    MeterClass result;
    switch (time_signature_top_number(time_signature)) {
    case 2:
        result.type = METER_TYPE_SIMPLE;
        result.grouping = METER_GROUPING_DUPLE;
        return result;
    case 3:
        result.type = METER_TYPE_SIMPLE;
        result.grouping = METER_GROUPING_TRIPLE;
        return result;
    case 4:
        result.type = METER_TYPE_SIMPLE;
        result.grouping = METER_GROUPING_QUADRUPLE;
        return result;
    case 6:
        result.type = METER_TYPE_COMPOUND;
        result.grouping = METER_GROUPING_DUPLE;
        return result;
    case 9:
        result.type = METER_TYPE_COMPOUND;
        result.grouping = METER_GROUPING_TRIPLE;
        return result;
    case 12:
        result.type = METER_TYPE_COMPOUND;
        result.grouping = METER_GROUPING_QUADRUPLE;
        return result;
    default:
        return unclassified;
    }
}

MeterType standard_meter_type(const TimeSignature *time_signature) {
    return standard_meter_classify(time_signature).type;
}

bool standard_meter_beat_division(MeterType type, unsigned *out_division, unsigned *out_subdivision) {
    if (type == METER_TYPE_SIMPLE) {
        *out_division = 2;
        *out_subdivision = 4;
        return true;
    }
    if (type == METER_TYPE_COMPOUND) {
        *out_division = 3;
        *out_subdivision = 6;
        return true;
    }
    return false;
}

bool standard_meter_category(MeterType type, MeterGrouping grouping, unsigned *out_beats, unsigned *out_division) {
    unsigned division;
    unsigned subdivision;
    if (!standard_meter_beat_division(type, &division, &subdivision)) {
        return false;
    }
    unsigned beats;
    switch (grouping) {
    case METER_GROUPING_DUPLE:
        beats = 2;
        break;
    case METER_GROUPING_TRIPLE:
        beats = 3;
        break;
    case METER_GROUPING_QUADRUPLE:
        beats = 4;
        break;
    default:
        return false;
    }
    *out_beats = beats;
    *out_division = division;
    return true;
}

bool standard_meter_beats_per_measure(const TimeSignature *time_signature, unsigned *out_beats) {
    MeterClass meter = standard_meter_classify(time_signature);
    unsigned division;
    return standard_meter_category(meter.type, meter.grouping, out_beats, &division);
}

bool standard_meter_beat_duration(const TimeSignature *time_signature, Duration **out) {
    MeterType type = standard_meter_type(time_signature);
    if (type == METER_TYPE_UNCLASSIFIED) {
        return false;
    }
    unsigned division;
    unsigned subdivision;
    standard_meter_beat_division(type, &division, &subdivision);

    /* a beat is `division` bottom units for compound, one for simple */
    long units = type == METER_TYPE_COMPOUND ? 3 : 1;
    return duration_create(units, time_signature_bottom_number(time_signature), out) == DURATION_OK;
}

bool standard_meter_beat_note_value(const TimeSignature *time_signature, NoteValue **out) {
    MeterType type = standard_meter_type(time_signature);
    int exponent = time_signature_bottom_unit_exponent(time_signature);
    if (type == METER_TYPE_SIMPLE) {
        return note_value_create(exponent, out) == NOTE_VALUE_OK;
    }
    if (type == METER_TYPE_COMPOUND) {
        /* three bottom units = one dotted note, one step longer */
        return note_value_create_dotted(exponent - 1, 1, out) == NOTE_VALUE_OK;
    }
    return false;
}

bool standard_meter_compound_bottom_is_usual(const TimeSignature *time_signature) {
    if (standard_meter_type(time_signature) != METER_TYPE_COMPOUND) {
        return false;
    }
    unsigned number = time_signature_bottom_number(time_signature);
    return number == 4 || number == 8 || number == 16;
}
