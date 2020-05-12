#include "pitch/clef.h"

#include <stdlib.h>
#include <string.h>
#include "core/value.h"

struct Clef {
    Value *value;
};

typedef struct {
    ClefSign sign;
    int line;
} ClefContent;

static bool sign_is_valid(ClefSign sign) {
    return sign == CLEF_SIGN_G || sign == CLEF_SIGN_F || sign == CLEF_SIGN_C;
}

ClefResult clef_create(ClefSign sign, int line, Clef **out) {
    if (!sign_is_valid(sign)) {
        return CLEF_ERR_INVALID_SIGN;
    }
    if (line < CLEF_MIN_LINE || line > CLEF_MAX_LINE) {
        return CLEF_ERR_INVALID_LINE;
    }

    /* memset first: value_equals compares raw bytes, so struct padding
       must be zeroed. */
    ClefContent content;
    memset(&content, 0, sizeof(content));
    content.sign = sign;
    content.line = line;

    Clef *clef = (Clef *)malloc(sizeof(Clef));
    if (clef == NULL) {
        return CLEF_ERR_ALLOC;
    }

    if (value_create("Clef", &content, sizeof(content), &clef->value) != VALUE_OK) {
        free(clef);
        return CLEF_ERR_ALLOC;
    }

    *out = clef;
    return CLEF_OK;
}

ClefResult clef_create_treble(Clef **out) {
    return clef_create(CLEF_SIGN_G, 2, out);
}

ClefResult clef_create_bass(Clef **out) {
    return clef_create(CLEF_SIGN_F, 4, out);
}

ClefResult clef_create_alto(Clef **out) {
    return clef_create(CLEF_SIGN_C, 3, out);
}

ClefResult clef_create_tenor(Clef **out) {
    return clef_create(CLEF_SIGN_C, 4, out);
}

void clef_destroy(Clef *clef) {
    if (clef == NULL) {
        return;
    }
    value_destroy(clef->value);
    free(clef);
}

static const ClefContent *clef_content(const Clef *clef) {
    return (const ClefContent *)value_content(clef->value, NULL);
}

ClefSign clef_sign(const Clef *clef) {
    return clef_content(clef)->sign;
}

int clef_line(const Clef *clef) {
    return clef_content(clef)->line;
}

PitchLetter clef_reference_letter(const Clef *clef) {
    switch (clef_content(clef)->sign) {
        case CLEF_SIGN_G:
            return PITCH_LETTER_G;
        case CLEF_SIGN_F:
            return PITCH_LETTER_F;
        case CLEF_SIGN_C:
        default:
            return PITCH_LETTER_C;
    }
}

int clef_reference_octave(const Clef *clef) {
    switch (clef_content(clef)->sign) {
        case CLEF_SIGN_G:
            return 4;
        case CLEF_SIGN_F:
            return 3;
        case CLEF_SIGN_C:
        default:
            return 4;
    }
}

bool clef_equals(const Clef *a, const Clef *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    return value_equals(a->value, b->value);
}
