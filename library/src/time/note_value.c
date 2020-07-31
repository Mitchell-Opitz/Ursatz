#include "time/note_value.h"

#include <stdlib.h>

struct NoteValue {
    int exponent;
};

NoteValueResult note_value_create(int exponent, NoteValue **out) {
    if (exponent < NOTE_VALUE_MIN_EXPONENT || exponent > NOTE_VALUE_MAX_EXPONENT) {
        return NOTE_VALUE_ERR_EXPONENT_OUT_OF_RANGE;
    }

    NoteValue *note_value = (NoteValue *)malloc(sizeof(NoteValue));
    if (note_value == NULL) {
        return NOTE_VALUE_ERR_ALLOC;
    }
    note_value->exponent = exponent;

    *out = note_value;
    return NOTE_VALUE_OK;
}

void note_value_destroy(NoteValue *note_value) {
    free(note_value);
}

int note_value_exponent(const NoteValue *note_value) {
    return note_value->exponent;
}

DurationResult note_value_duration(const NoteValue *note_value, Duration **out) {
    if (note_value->exponent < 0) {
        return duration_create(2, 1, out);
    }
    return duration_create(1, 1UL << note_value->exponent, out);
}

bool note_value_equals(const NoteValue *a, const NoteValue *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    return a->exponent == b->exponent;
}
