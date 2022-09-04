#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "semantics/phrase_model_label.h"

static void test_validity(void) {
    for (int i = 0; i < PHRASE_MODEL_LABEL_COUNT; i++) {
        assert(phrase_model_label_valid((PhraseModelLabel)i));
    }
    assert(!phrase_model_label_valid(PHRASE_MODEL_LABEL_COUNT));
    assert(!phrase_model_label_valid((PhraseModelLabel)-1));
}

static void test_names(void) {
    assert(strcmp(phrase_model_label_name(PHRASE_MODEL_LABEL_TONIC_BEGINNING), "tonic-beginning") == 0);
    assert(strcmp(phrase_model_label_name(PHRASE_MODEL_LABEL_PREDOMINANT), "predominant") == 0);
    assert(strcmp(phrase_model_label_name(PHRASE_MODEL_LABEL_DOMINANT), "dominant") == 0);
    assert(strcmp(phrase_model_label_name(PHRASE_MODEL_LABEL_TONIC_ENDING), "tonic-ending") == 0);
    assert(phrase_model_label_name(PHRASE_MODEL_LABEL_COUNT) == NULL);
}

static void test_abbreviations(void) {
    assert(strcmp(phrase_model_label_abbreviation(PHRASE_MODEL_LABEL_TONIC_BEGINNING), "Tb") == 0);
    assert(strcmp(phrase_model_label_abbreviation(PHRASE_MODEL_LABEL_PREDOMINANT), "PD") == 0);
    assert(strcmp(phrase_model_label_abbreviation(PHRASE_MODEL_LABEL_DOMINANT), "D") == 0);
    assert(strcmp(phrase_model_label_abbreviation(PHRASE_MODEL_LABEL_TONIC_ENDING), "Te") == 0);
    assert(phrase_model_label_abbreviation(PHRASE_MODEL_LABEL_COUNT) == NULL);
    assert(phrase_model_label_abbreviation((PhraseModelLabel)-1) == NULL);
}

static void test_phrase_order(void) {
    assert(PHRASE_MODEL_LABEL_TONIC_BEGINNING < PHRASE_MODEL_LABEL_PREDOMINANT);
    assert(PHRASE_MODEL_LABEL_PREDOMINANT < PHRASE_MODEL_LABEL_DOMINANT);
    assert(PHRASE_MODEL_LABEL_DOMINANT < PHRASE_MODEL_LABEL_TONIC_ENDING);
}

int main(void) {
    test_validity();
    test_names();
    test_abbreviations();
    test_phrase_order();
    printf("test_phrase_model_label: ok\n");
    return 0;
}
