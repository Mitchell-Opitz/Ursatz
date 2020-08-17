#include "time/note_value.h"

#include <stdlib.h>

struct NoteValue {
    int exponent;
    unsigned dots;
};

NoteValueResult note_value_create_dotted(int exponent, unsigned dots, NoteValue **out) {
    if (exponent < NOTE_VALUE_MIN_EXPONENT || exponent > NOTE_VALUE_MAX_EXPONENT) {
        return NOTE_VALUE_ERR_EXPONENT_OUT_OF_RANGE;
    }
    if (dots > NOTE_VALUE_MAX_DOTS) {
        return NOTE_VALUE_ERR_TOO_MANY_DOTS;
    }

    NoteValue *note_value = (NoteValue *)malloc(sizeof(NoteValue));
    if (note_value == NULL) {
        return NOTE_VALUE_ERR_ALLOC;
    }
    note_value->exponent = exponent;
    note_value->dots = dots;

    *out = note_value;
    return NOTE_VALUE_OK;
}

NoteValueResult note_value_create(int exponent, NoteValue **out) {
    return note_value_create_dotted(exponent, 0, out);
}

void note_value_destroy(NoteValue *note_value) {
    free(note_value);
}

int note_value_exponent(const NoteValue *note_value) {
    return note_value->exponent;
}

unsigned note_value_dots(const NoteValue *note_value) {
    return note_value->dots;
}

/*
 * Undotted: 1/2^e (a breve, e = -1, is 2). With d dots the factor is
 * (2^(d+1) - 1) / 2^d, so the length is (2^(d+1) - 1) / 2^(e+d), or for
 * a breve 2 * (2^(d+1) - 1) / 2^d.
 */
DurationResult note_value_duration(const NoteValue *note_value, Duration **out) {
    unsigned dots = note_value->dots;
    long dot_numerator = (1L << (dots + 1)) - 1;
    if (note_value->exponent < 0) {
        return duration_create(2 * dot_numerator, 1UL << dots, out);
    }
    return duration_create(dot_numerator, 1UL << ((unsigned)note_value->exponent + dots), out);
}

bool note_value_equals(const NoteValue *a, const NoteValue *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    return a->exponent == b->exponent && a->dots == b->dots;
}
