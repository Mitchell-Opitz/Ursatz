#include "semantics/analysis_key.h"

#include <assert.h>
#include <stdio.h>

#include "pitch/pitch_arithmetic.h"
#include "time/musical_time.h"

static void test_from_fifths_all_keys(void) {
    int count = 0;
    for (int mode = KEY_STRUCTURE_MODE_MAJOR; mode <= KEY_STRUCTURE_MODE_MINOR; mode++) {
        for (int fifths = -7; fifths <= 7; fifths++) {
            AnalysisKey key;
            assert(analysis_key_from_fifths(fifths, (KeyStructureMode)mode, &key));
            assert(key.mode == (KeyStructureMode)mode);
            assert(key.id == NULL);
            /* The relative major of a minor key is a minor third above its tonic. */
            int tonic = analysis_key_tonic_pitch_class(&key);
            int major_tonic = (tonic + (mode == KEY_STRUCTURE_MODE_MINOR ? 3 : 0)) % 12;
            AnalysisKey major;
            assert(analysis_key_from_fifths(fifths, KEY_STRUCTURE_MODE_MAJOR, &major));
            assert(analysis_key_tonic_pitch_class(&major) == major_tonic);
            count++;
        }
    }
    assert(count == 30);
}

static void test_known_keys(void) {
    AnalysisKey key;
    assert(analysis_key_from_fifths(0, KEY_STRUCTURE_MODE_MAJOR, &key));
    assert(key.tonic_letter == PITCH_LETTER_C && key.tonic_accidental == 0);
    assert(analysis_key_from_fifths(0, KEY_STRUCTURE_MODE_MINOR, &key));
    assert(key.tonic_letter == PITCH_LETTER_A && key.tonic_accidental == 0);
    assert(analysis_key_from_fifths(3, KEY_STRUCTURE_MODE_MAJOR, &key));
    assert(key.tonic_letter == PITCH_LETTER_A && key.tonic_accidental == 0);
    assert(analysis_key_from_fifths(3, KEY_STRUCTURE_MODE_MINOR, &key));
    assert(key.tonic_letter == PITCH_LETTER_F && key.tonic_accidental == 1);
    assert(analysis_key_from_fifths(-3, KEY_STRUCTURE_MODE_MAJOR, &key));
    assert(key.tonic_letter == PITCH_LETTER_E && key.tonic_accidental == -1);
    assert(analysis_key_from_fifths(-3, KEY_STRUCTURE_MODE_MINOR, &key));
    assert(key.tonic_letter == PITCH_LETTER_C && key.tonic_accidental == 0);
    assert(analysis_key_from_fifths(7, KEY_STRUCTURE_MODE_MAJOR, &key));
    assert(key.tonic_letter == PITCH_LETTER_C && key.tonic_accidental == 1);
    assert(analysis_key_from_fifths(-7, KEY_STRUCTURE_MODE_MAJOR, &key));
    assert(key.tonic_letter == PITCH_LETTER_C && key.tonic_accidental == -1);
    assert(analysis_key_tonic_pitch_class(&key) == 11);
}

static void test_from_fifths_rejects(void) {
    AnalysisKey key = {"keep", PITCH_LETTER_D, 0, KEY_STRUCTURE_MODE_MINOR};
    assert(!analysis_key_from_fifths(8, KEY_STRUCTURE_MODE_MAJOR, &key));
    assert(!analysis_key_from_fifths(-8, KEY_STRUCTURE_MODE_MINOR, &key));
    assert(!analysis_key_from_fifths(0, (KeyStructureMode)5, &key));
    assert(!analysis_key_from_fifths(0, KEY_STRUCTURE_MODE_MAJOR, NULL));
    assert(key.id != NULL && key.tonic_letter == PITCH_LETTER_D && key.mode == KEY_STRUCTURE_MODE_MINOR);
}

static void test_tonic_pitch_class(void) {
    AnalysisKey c = {NULL, PITCH_LETTER_C, 0, KEY_STRUCTURE_MODE_MAJOR};
    AnalysisKey fs = {NULL, PITCH_LETTER_F, 1, KEY_STRUCTURE_MODE_MINOR};
    AnalysisKey bb = {NULL, PITCH_LETTER_B, -1, KEY_STRUCTURE_MODE_MAJOR};
    assert(analysis_key_tonic_pitch_class(&c) == 0);
    assert(analysis_key_tonic_pitch_class(&fs) == 6);
    assert(analysis_key_tonic_pitch_class(&bb) == 10);
    assert(analysis_key_tonic_pitch_class(NULL) == -1);
}

static void test_equals(void) {
    AnalysisKey a = {"one", PITCH_LETTER_D, 0, KEY_STRUCTURE_MODE_MAJOR};
    AnalysisKey b = {"two", PITCH_LETTER_D, 0, KEY_STRUCTURE_MODE_MAJOR};
    assert(analysis_key_equals(&a, &b));
    AnalysisKey minor = {"one", PITCH_LETTER_D, 0, KEY_STRUCTURE_MODE_MINOR};
    assert(!analysis_key_equals(&a, &minor));
    /* The same pitch class spelled differently is a different key. */
    AnalysisKey sharp = {NULL, PITCH_LETTER_C, 1, KEY_STRUCTURE_MODE_MAJOR};
    AnalysisKey flat = {NULL, PITCH_LETTER_D, -1, KEY_STRUCTURE_MODE_MAJOR};
    assert(analysis_key_tonic_pitch_class(&sharp) == analysis_key_tonic_pitch_class(&flat));
    assert(!analysis_key_equals(&sharp, &flat));
    assert(!analysis_key_equals(&a, NULL));
    assert(!analysis_key_equals(NULL, &a));
}

static void test_key_event(void) {
    MusicalTime *time = NULL;
    assert(musical_time_create(3, 2, &time) == MUSICAL_TIME_OK);
    AnalysisKeyEvent event = {time, {NULL, PITCH_LETTER_G, 0, KEY_STRUCTURE_MODE_MAJOR}};
    assert(event.time == time && analysis_key_tonic_pitch_class(&event.key) == 7);
    musical_time_destroy(time);
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
