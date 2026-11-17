#include "model/time/note_value.h"

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
