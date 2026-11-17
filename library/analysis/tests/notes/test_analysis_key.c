#include "analysis/notes/analysis_key.h"

#include <assert.h>
#include <stdio.h>

#include "model/pitch/pitch_arithmetic.h"
#include "model/time/musical_time.h"
#include "test_knowledge.h"

#define MAJOR (test_materials()->major)
#define MINOR (test_materials()->natural_minor)

static void test_from_fifths_all_keys(void) {
    int count = 0;
    for (int mode = 0; mode <= 1; mode++) {
        const PitchMaterial *material = mode == 0 ? MAJOR : MINOR;
        for (int fifths = -7; fifths <= 7; fifths++) {
            AnalysisKey key;
            assert(analysis_key_from_fifths(fifths, material, &key));
            assert(key.material == material);
            assert(key.id == NULL);
            /* The relative major of a minor key is a minor third above its tonic. */
            int tonic = analysis_key_tonic_pitch_class(&key);
            int major_tonic = (tonic + (mode == 1 ? 3 : 0)) % 12;
            AnalysisKey major;
            assert(analysis_key_from_fifths(fifths, MAJOR, &major));
            assert(analysis_key_tonic_pitch_class(&major) == major_tonic);
            count++;
        }
    }
    assert(count == 30);
}

static void test_known_keys(void) {
    AnalysisKey key;
    assert(analysis_key_from_fifths(0, MAJOR, &key));
    assert(key.tonic_letter == PITCH_LETTER_C && key.tonic_accidental == 0);
    assert(analysis_key_from_fifths(0, MINOR, &key));
    assert(key.tonic_letter == PITCH_LETTER_A && key.tonic_accidental == 0);
    assert(analysis_key_from_fifths(3, MAJOR, &key));
    assert(key.tonic_letter == PITCH_LETTER_A && key.tonic_accidental == 0);
    assert(analysis_key_from_fifths(3, MINOR, &key));
    assert(key.tonic_letter == PITCH_LETTER_F && key.tonic_accidental == 1);
    assert(analysis_key_from_fifths(-3, MAJOR, &key));
    assert(key.tonic_letter == PITCH_LETTER_E && key.tonic_accidental == -1);
    assert(analysis_key_from_fifths(-3, MINOR, &key));
    assert(key.tonic_letter == PITCH_LETTER_C && key.tonic_accidental == 0);
    assert(analysis_key_from_fifths(7, MAJOR, &key));
    assert(key.tonic_letter == PITCH_LETTER_C && key.tonic_accidental == 1);
    assert(analysis_key_from_fifths(-7, MAJOR, &key));
    assert(key.tonic_letter == PITCH_LETTER_C && key.tonic_accidental == -1);
    assert(analysis_key_tonic_pitch_class(&key) == 11);
}

static void test_from_fifths_rejects(void) {
    AnalysisKey key = {"keep", PITCH_LETTER_D, 0, MINOR};
    assert(!analysis_key_from_fifths(8, MAJOR, &key));
    assert(!analysis_key_from_fifths(-8, MINOR, &key));
    assert(!analysis_key_from_fifths(0, NULL, &key));
    assert(!analysis_key_from_fifths(0, MAJOR, NULL));
    assert(key.id != NULL && key.tonic_letter == PITCH_LETTER_D && key.material == MINOR);
}

static void test_tonic_pitch_class(void) {
    AnalysisKey c = {NULL, PITCH_LETTER_C, 0, MAJOR};
    AnalysisKey fs = {NULL, PITCH_LETTER_F, 1, MINOR};
    AnalysisKey bb = {NULL, PITCH_LETTER_B, -1, MAJOR};
    assert(analysis_key_tonic_pitch_class(&c) == 0);
    assert(analysis_key_tonic_pitch_class(&fs) == 6);
    assert(analysis_key_tonic_pitch_class(&bb) == 10);
    assert(analysis_key_tonic_pitch_class(NULL) == -1);
}

static void test_equals(void) {
    AnalysisKey a = {"one", PITCH_LETTER_D, 0, MAJOR};
    AnalysisKey b = {"two", PITCH_LETTER_D, 0, MAJOR};
    assert(analysis_key_equals(&a, &b));
    AnalysisKey minor = {"one", PITCH_LETTER_D, 0, MINOR};
    assert(!analysis_key_equals(&a, &minor));
    /* A key with no material is not the key of a material (two with none are the same). */
    AnalysisKey no_material = {"one", PITCH_LETTER_D, 0, NULL};
    assert(!analysis_key_equals(&a, &no_material) && analysis_key_equals(&no_material, &no_material));
    /* The same pitch class spelled differently is a different key. */
    AnalysisKey sharp = {NULL, PITCH_LETTER_C, 1, MAJOR};
    AnalysisKey flat = {NULL, PITCH_LETTER_D, -1, MAJOR};
    assert(analysis_key_tonic_pitch_class(&sharp) == analysis_key_tonic_pitch_class(&flat));
    assert(!analysis_key_equals(&sharp, &flat));
    assert(!analysis_key_equals(&a, NULL));
    assert(!analysis_key_equals(NULL, &a));
}

static void test_key_event(void) {
    MusicalTime time;
    assert(musical_time_make(3, 2, &time) == MUSICAL_TIME_OK);
    AnalysisKeyEvent event = {time, {NULL, PITCH_LETTER_G, 0, MAJOR}};
    assert(musical_time_same(event.time, time) && analysis_key_tonic_pitch_class(&event.key) == 7);
}

int main(void) {
    test_from_fifths_all_keys();
    test_known_keys();
    test_from_fifths_rejects();
    test_tonic_pitch_class();
    test_equals();
    test_key_event();
    printf("test_analysis_key: ok\n");
    return 0;
}
