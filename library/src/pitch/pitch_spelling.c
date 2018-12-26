#include "pitch/pitch_spelling.h"

#include <stdlib.h>
#include <string.h>
#include "core/value.h"

struct PitchSpelling {
    Value *value;
};

typedef struct {
    PitchLetter letter;
    int accidental;
    int octave;
} PitchSpellingContent;

static bool letter_is_valid(PitchLetter letter) {
    return letter == PITCH_LETTER_A || letter == PITCH_LETTER_B || letter == PITCH_LETTER_C ||
           letter == PITCH_LETTER_D || letter == PITCH_LETTER_E || letter == PITCH_LETTER_F ||
           letter == PITCH_LETTER_G;
}

PitchSpellingResult pitch_spelling_create(PitchLetter letter, int accidental, int octave, PitchSpelling **out) {
    if (!letter_is_valid(letter)) {
        return PITCH_SPELLING_ERR_INVALID_LETTER;
    }

    /* memset first: struct padding is uninitialized otherwise, and
       value_equals compares raw bytes, so stray padding could make
       field-equal spellings compare unequal. */
    PitchSpellingContent content;
    memset(&content, 0, sizeof(content));
    content.letter = letter;
    content.accidental = accidental;
    content.octave = octave;

    PitchSpelling *spelling = (PitchSpelling *)malloc(sizeof(PitchSpelling));
    if (spelling == NULL) {
        return PITCH_SPELLING_ERR_ALLOC;
    }

    if (value_create("PitchSpelling", &content, sizeof(content), &spelling->value) != VALUE_OK) {
        free(spelling);
        return PITCH_SPELLING_ERR_ALLOC;
    }

    *out = spelling;
    return PITCH_SPELLING_OK;
}

void pitch_spelling_destroy(PitchSpelling *spelling) {
    if (spelling == NULL) {
        return;
    }
    value_destroy(spelling->value);
    free(spelling);
}

bool pitch_spelling_equals(const PitchSpelling *a, const PitchSpelling *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    return value_equals(a->value, b->value);
}

static const PitchSpellingContent *pitch_spelling_content(const PitchSpelling *spelling) {
    return (const PitchSpellingContent *)value_content(spelling->value, NULL);
}

PitchLetter pitch_spelling_letter(const PitchSpelling *spelling) {
    return pitch_spelling_content(spelling)->letter;
}

int pitch_spelling_accidental(const PitchSpelling *spelling) {
    return pitch_spelling_content(spelling)->accidental;
}

int pitch_spelling_octave(const PitchSpelling *spelling) {
    return pitch_spelling_content(spelling)->octave;
}