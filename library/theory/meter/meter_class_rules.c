#include "theory/meter/meter_class_rules.h"

#include <stddef.h>

/* duple, triple or quadruple from the number of beats; none for any other count */
static MeterGrouping grouping_of_beats(size_t beats) {
    switch (beats) {
    case 2:
        return METER_GROUPING_DUPLE;
    case 3:
        return METER_GROUPING_TRIPLE;
    case 4:
        return METER_GROUPING_QUADRUPLE;
    default:
        return METER_GROUPING_NONE;
    }
}

/* The beats of a bar, one per group, when every group is the same size (the beat is the group); 0 for an additive
   meter such as 3+2, which has no beat count of its own: it counts its bottom units, as the beat owner says. */
static size_t beats_of(const TimeSignature *time_signature) {
    size_t count = time_signature_group_count(time_signature);
    unsigned first = time_signature_group_at(time_signature, 0);
    for (size_t i = 1; i < count; i++) {
        if (time_signature_group_at(time_signature, i) != first) {
            return 0;
        }
    }
    return count;
}

MeterClass standard_meter_classify(const TimeSignature *time_signature) {
    MeterClass unclassified = {METER_TYPE_UNCLASSIFIED, METER_GROUPING_NONE};
    if (time_signature == NULL) {
        return unclassified;
    }

    /* OMT I ch. 10: the beat is a group of the meter, a compound beat is three divisions, the grouping counts beats */
    size_t beats = beats_of(time_signature);
    MeterClass result;
    if (time_signature_is_compound(time_signature)) {
        result.type = METER_TYPE_COMPOUND;
        result.grouping = grouping_of_beats(beats);
        return result;
    }
    result.grouping = grouping_of_beats(beats);
    if (result.grouping == METER_GROUPING_NONE) {
        return unclassified;
    }
    result.type = METER_TYPE_SIMPLE;
    return result;
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
    if (standard_meter_type(time_signature) == METER_TYPE_UNCLASSIFIED) {
        return false;
    }
    *out_beats = (unsigned)beats_of(time_signature);
    return true;
}

bool standard_meter_beat_duration(const TimeSignature *time_signature, Duration *out) {
    if (standard_meter_type(time_signature) == METER_TYPE_UNCLASSIFIED) {
        return false;
    }
    return time_signature_beat_length(time_signature, out) == DURATION_OK;
}
