#include "meter_class_rules.h"

#include <stddef.h>

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

