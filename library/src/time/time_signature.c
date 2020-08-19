#include "time/time_signature.h"

#include <stdlib.h>

struct TimeSignature {
    unsigned beats;
    int beat_unit_exponent;
};

TimeSignatureResult time_signature_create(unsigned beats, const NoteValue *beat_unit, TimeSignature **out) {
    if (beat_unit == NULL) {
        return TIME_SIGNATURE_ERR_NULL_BEAT_UNIT;
    }
    if (beats < 1 || beats > TIME_SIGNATURE_MAX_BEATS) {
        return TIME_SIGNATURE_ERR_BEATS_OUT_OF_RANGE;
    }
    if (note_value_dots(beat_unit) != 0) {
        return TIME_SIGNATURE_ERR_BEAT_UNIT_DOTTED;
    }
    int exponent = note_value_exponent(beat_unit);
    if (exponent < TIME_SIGNATURE_MIN_BEAT_UNIT_EXPONENT || exponent > TIME_SIGNATURE_MAX_BEAT_UNIT_EXPONENT) {
        return TIME_SIGNATURE_ERR_BEAT_UNIT_OUT_OF_RANGE;
    }

    TimeSignature *time_signature = (TimeSignature *)malloc(sizeof(TimeSignature));
    if (time_signature == NULL) {
        return TIME_SIGNATURE_ERR_ALLOC;
    }
    time_signature->beats = beats;
    time_signature->beat_unit_exponent = exponent;

    *out = time_signature;
    return TIME_SIGNATURE_OK;
}

void time_signature_destroy(TimeSignature *time_signature) {
    free(time_signature);
}

unsigned time_signature_beats(const TimeSignature *time_signature) {
    return time_signature->beats;
}

int time_signature_beat_unit_exponent(const TimeSignature *time_signature) {
    return time_signature->beat_unit_exponent;
}

unsigned time_signature_beat_unit_number(const TimeSignature *time_signature) {
    return 1u << time_signature->beat_unit_exponent;
}

bool time_signature_equals(const TimeSignature *a, const TimeSignature *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    return a->beats == b->beats && a->beat_unit_exponent == b->beat_unit_exponent;
}
