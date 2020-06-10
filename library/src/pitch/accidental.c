#include "pitch/accidental.h"

#include <stdlib.h>
#include <string.h>
#include "core/value.h"

struct Accidental {
    Value *value;
};

typedef struct {
    AccidentalKind kind;
} AccidentalContent;

static bool kind_is_valid(AccidentalKind kind) {
    return kind == ACCIDENTAL_DOUBLE_FLAT || kind == ACCIDENTAL_FLAT || kind == ACCIDENTAL_NATURAL ||
           kind == ACCIDENTAL_SHARP || kind == ACCIDENTAL_DOUBLE_SHARP;
}

static int offset_of(AccidentalKind kind) {
    switch (kind) {
        case ACCIDENTAL_DOUBLE_FLAT:
            return -2;
        case ACCIDENTAL_FLAT:
            return -1;
        case ACCIDENTAL_SHARP:
            return 1;
        case ACCIDENTAL_DOUBLE_SHARP:
            return 2;
        case ACCIDENTAL_NATURAL:
        default:
            return 0;
    }
}

AccidentalResult accidental_create(AccidentalKind kind, Accidental **out) {
    if (!kind_is_valid(kind)) {
        return ACCIDENTAL_ERR_INVALID_KIND;
    }

    /* memset first: value_equals compares raw bytes, so struct padding
       must be zeroed. */
    AccidentalContent content;
    memset(&content, 0, sizeof(content));
    content.kind = kind;

    Accidental *accidental = (Accidental *)malloc(sizeof(Accidental));
    if (accidental == NULL) {
        return ACCIDENTAL_ERR_ALLOC;
    }

    if (value_create("Accidental", &content, sizeof(content), &accidental->value) != VALUE_OK) {
        free(accidental);
        return ACCIDENTAL_ERR_ALLOC;
    }

    *out = accidental;
    return ACCIDENTAL_OK;
}

AccidentalResult accidental_from_offset(int semitone_offset, Accidental **out) {
    switch (semitone_offset) {
        case -2:
            return accidental_create(ACCIDENTAL_DOUBLE_FLAT, out);
        case -1:
            return accidental_create(ACCIDENTAL_FLAT, out);
        case 0:
            return accidental_create(ACCIDENTAL_NATURAL, out);
        case 1:
            return accidental_create(ACCIDENTAL_SHARP, out);
        case 2:
            return accidental_create(ACCIDENTAL_DOUBLE_SHARP, out);
        default:
            return ACCIDENTAL_ERR_INVALID_OFFSET;
    }
}

void accidental_destroy(Accidental *accidental) {
    if (accidental == NULL) {
        return;
    }
    value_destroy(accidental->value);
    free(accidental);
}

AccidentalKind accidental_kind(const Accidental *accidental) {
    return ((const AccidentalContent *)value_content(accidental->value, NULL))->kind;
}

int accidental_semitone_offset(const Accidental *accidental) {
    return offset_of(accidental_kind(accidental));
}

bool accidental_equals(const Accidental *a, const Accidental *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    return value_equals(a->value, b->value);
}
