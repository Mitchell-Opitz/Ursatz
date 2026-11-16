#include "phrase_model_label.h"

#include <stddef.h>

static const char *const ABBREVIATIONS[PHRASE_MODEL_LABEL_COUNT] = {"Tb", "PD", "D", "Te"};

bool phrase_model_label_valid(PhraseModelLabel label) {
    return label >= 0 && label < PHRASE_MODEL_LABEL_COUNT;
}

const char *phrase_model_label_abbreviation(PhraseModelLabel label) {
    return phrase_model_label_valid(label) ? ABBREVIATIONS[label] : NULL;
}
