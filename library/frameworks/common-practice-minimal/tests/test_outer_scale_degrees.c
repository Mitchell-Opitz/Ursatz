#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/framework_reference.h"
#include "interpretation/interpretation.h"
#include "interpretation/uncertainty.h"
#include "outer_scale_degrees.h"
#include "pitch/tuning_system.h"

/*
 * Scale degree exposure tests: the degree of a soprano and a bass note in a major and a minor key, an
 * altered degree, an absent line (no ScaleDegree), and what each ScaleDegree's Interpretation carries.
 */

static EntityID *make_id(const char *value) {
    EntityID *id = NULL;
    assert(entity_id_create(value, &id) == ENTITY_ID_OK);
    return id;
}

/* Builds OuterVoiceLines by hand: line pitches (MIDI), with a negative value meaning an absent entry. */
static OuterVoiceLines make_lines(const int *soprano, const int *bass, size_t count) {
    OuterVoiceLines lines;
    lines.count = count;
    lines.soprano = (OuterVoiceEntry *)calloc(count, sizeof(OuterVoiceEntry));
    lines.bass = (OuterVoiceEntry *)calloc(count, sizeof(OuterVoiceEntry));
    TuningSystem *tuning = NULL;
    assert(tuning_system_create(make_id("midi"), "MIDI note number", &tuning) == TUNING_SYSTEM_OK);
    for (size_t i = 0; i < count; i++) {
        for (int line = 0; line < 2; line++) {
            OuterVoiceEntry *entry = line == 0 ? &lines.soprano[i] : &lines.bass[i];
            int pitch = line == 0 ? soprano[i] : bass[i];
            entry->segment_index = i;
            MusicalTime *start = NULL;
            MusicalTime *end = NULL;
            assert(musical_time_create((long)i, 4, &start) == MUSICAL_TIME_OK);
            assert(musical_time_create((long)i + 1, 4, &end) == MUSICAL_TIME_OK);
            assert(time_span_create(start, end, TIME_SPAN_BOUNDARY_LEFT_CLOSED, &entry->span) == TIME_SPAN_OK);
            musical_time_destroy(start);
            musical_time_destroy(end);
            if (pitch >= 0) {
                char id[32];
                snprintf(id, sizeof(id), "%s-%zu", line == 0 ? "s" : "b", i);
                entry->present = true;
                entry->note_id = make_id(id);
                assert(pitch_numeric_value_create(tuning, pitch, 1, &entry->pitch) == PITCH_NUMERIC_VALUE_OK);
            }
        }
    }
    tuning_system_destroy(tuning);
    return lines;
}

static Key *make_key(CommonPracticeKeyMode mode, const char *tonic) {
    Key *key = NULL;
    assert(common_practice_build_key(mode, tonic, make_id("work"), &key) == KEY_OK);
    return key;
}

static void assert_label(const ScaleDegree *degree, const char *label) {
    assert(degree != NULL);
    assert(strcmp(scale_degree_label(degree), label) == 0);
}

static void test_degrees_in_a_major_key(void) {
    /* C major soprano C D E F G A B, and a bass C C C F G G C. */
    int soprano[7] = {72, 74, 76, 77, 79, 81, 83};
    int bass[7] = {48, 48, 48, 53, 55, 55, 48};
    OuterVoiceLines lines = make_lines(soprano, bass, 7);
    Key *key = make_key(COMMON_PRACTICE_KEY_MAJOR, "C");
    CommonPracticeOuterDegreeList degrees = {0};
    assert(common_practice_outer_scale_degrees(&lines, key, 0, COMMON_PRACTICE_KEY_MAJOR, &degrees) ==
           COMMON_PRACTICE_OUTER_OK);
    assert(degrees.count == 7);
    const char *soprano_labels[7] = {"1", "2", "3", "4", "5", "6", "7"};
    const char *bass_labels[7] = {"1", "1", "1", "4", "5", "5", "1"};
    for (size_t i = 0; i < 7; i++) {
        assert_label(degrees.items[i].soprano, soprano_labels[i]);
        assert_label(degrees.items[i].bass, bass_labels[i]);
    }
    common_practice_outer_degrees_destroy(&degrees);
    outer_voice_lines_destroy(&lines);
    key_destroy(key);
}

static void test_degrees_in_a_transposed_major_key(void) {
    /* E-flat major (tonic 3): Eb4 is 1, Bb4 is 5, D5 is 7, Ab4 is 4. */
    int soprano[4] = {63, 70, 74, 68};
    int bass[4] = {51, 46, 46, 56};
    OuterVoiceLines lines = make_lines(soprano, bass, 4);
    Key *key = make_key(COMMON_PRACTICE_KEY_MAJOR, "Eb");
    CommonPracticeOuterDegreeList degrees = {0};
    assert(common_practice_outer_scale_degrees(&lines, key, 3, COMMON_PRACTICE_KEY_MAJOR, &degrees) ==
           COMMON_PRACTICE_OUTER_OK);
    assert_label(degrees.items[0].soprano, "1");
    assert_label(degrees.items[1].soprano, "5");
    assert_label(degrees.items[2].soprano, "7");
    assert_label(degrees.items[3].soprano, "4");
    assert_label(degrees.items[0].bass, "1");
    assert_label(degrees.items[1].bass, "5");
    assert_label(degrees.items[3].bass, "4");
    common_practice_outer_degrees_destroy(&degrees);
    outer_voice_lines_destroy(&lines);
    key_destroy(key);
}

static void test_altered_degrees(void) {
    /* In C major: F# is #4, Bb is b7, Db is b2, Ab is b6, Eb is b3. */
    int soprano[5] = {78, 70, 73, 80, 75};
    int bass[5] = {48, 48, 48, 48, 48};
    OuterVoiceLines lines = make_lines(soprano, bass, 5);
    Key *key = make_key(COMMON_PRACTICE_KEY_MAJOR, "C");
    CommonPracticeOuterDegreeList degrees = {0};
    assert(common_practice_outer_scale_degrees(&lines, key, 0, COMMON_PRACTICE_KEY_MAJOR, &degrees) ==
           COMMON_PRACTICE_OUTER_OK);
    const char *expected[5] = {"#4", "b7", "b2", "b6", "b3"};
    /* 80 is G#/Ab: offset 8 above C, which the major table spells b6. */
    for (size_t i = 0; i < 5; i++) {
        assert_label(degrees.items[i].soprano, expected[i]);
    }
    common_practice_outer_degrees_destroy(&degrees);
    outer_voice_lines_destroy(&lines);
    key_destroy(key);
}

static void test_degrees_in_a_minor_key_including_the_raised_seventh(void) {
    /* A minor (tonic 9), natural minor: A 1, C 3, F 6, G 7 are diatonic; G# is #7 and F# is #6, altered. */
    int soprano[6] = {69, 72, 77, 79, 80, 78};
    int bass[6] = {45, 45, 45, 45, 52, 52};
    OuterVoiceLines lines = make_lines(soprano, bass, 6);
    Key *key = make_key(COMMON_PRACTICE_KEY_MINOR, "A");
    CommonPracticeOuterDegreeList degrees = {0};
    assert(common_practice_outer_scale_degrees(&lines, key, 9, COMMON_PRACTICE_KEY_MINOR, &degrees) ==
           COMMON_PRACTICE_OUTER_OK);
    const char *soprano_labels[6] = {"1", "3", "6", "7", "#7", "#6"};
    for (size_t i = 0; i < 6; i++) {
        assert_label(degrees.items[i].soprano, soprano_labels[i]);
    }
    assert_label(degrees.items[0].bass, "1");
    assert_label(degrees.items[4].bass, "5"); /* E in the bass */
    common_practice_outer_degrees_destroy(&degrees);
    outer_voice_lines_destroy(&lines);
    key_destroy(key);
}

static void test_an_absent_line_has_no_scale_degree(void) {
    int soprano[3] = {72, -1, 76};
    int bass[3] = {48, 48, -1};
    OuterVoiceLines lines = make_lines(soprano, bass, 3);
    Key *key = make_key(COMMON_PRACTICE_KEY_MAJOR, "C");
    CommonPracticeOuterDegreeList degrees = {0};
    assert(common_practice_outer_scale_degrees(&lines, key, 0, COMMON_PRACTICE_KEY_MAJOR, &degrees) ==
           COMMON_PRACTICE_OUTER_OK);
    assert(degrees.items[0].soprano != NULL && degrees.items[0].bass != NULL);
    assert(degrees.items[1].soprano == NULL && degrees.items[1].bass != NULL);
    assert(degrees.items[2].soprano != NULL && degrees.items[2].bass == NULL);
    common_practice_outer_degrees_destroy(&degrees);
    outer_voice_lines_destroy(&lines);
    key_destroy(key);
}

static void test_each_degree_carries_a_framework_reference_and_its_note(void) {
    int soprano[1] = {79};
    int bass[1] = {43};
    OuterVoiceLines lines = make_lines(soprano, bass, 1);
    Key *key = make_key(COMMON_PRACTICE_KEY_MAJOR, "C");
    CommonPracticeOuterDegreeList degrees = {0};
    assert(common_practice_outer_scale_degrees(&lines, key, 0, COMMON_PRACTICE_KEY_MAJOR, &degrees) ==
           COMMON_PRACTICE_OUTER_OK);

    const ScaleDegree *degree = degrees.items[0].soprano;
    const Interpretation *interpretation = scale_degree_interpretation(degree);
    assert(interpretation_framework_reference(interpretation) != NULL); /* required of a ScaleDegree */
    assert(strcmp(entity_id_value(interpretation_target_id(interpretation)), "s-0") == 0);
    assert(strcmp(entity_id_value(interpretation_context_id(interpretation)),
                  entity_id_value(interpretation_id(key_interpretation(key)))) == 0);
    assert(uncertainty_scalar_confidence_numerator(interpretation_uncertainty(interpretation)) == 1 &&
           uncertainty_scalar_confidence_denominator(interpretation_uncertainty(interpretation)) == 1);
    assert(scale_degree_pitch(degree) != NULL);
    assert(strcmp(interpretation_author(interpretation), "common-practice-minimal-v1") == 0);

    /* The soprano's and the bass's Interpretations have different ids. */
    assert(!entity_id_equals(interpretation_id(interpretation),
                             interpretation_id(scale_degree_interpretation(degrees.items[0].bass))));
    common_practice_outer_degrees_destroy(&degrees);
    outer_voice_lines_destroy(&lines);
    key_destroy(key);
}

static void test_the_label_function_directly(void) {
    assert(strcmp(common_practice_scale_degree_label(0, 0, COMMON_PRACTICE_KEY_MAJOR), "1") == 0);
    assert(strcmp(common_practice_scale_degree_label(6, 0, COMMON_PRACTICE_KEY_MAJOR), "#4") == 0);
    assert(strcmp(common_practice_scale_degree_label(2, 9, COMMON_PRACTICE_KEY_MINOR), "b2") == 0 ||
           strcmp(common_practice_scale_degree_label(2, 9, COMMON_PRACTICE_KEY_MINOR), "2") != NULL);
    assert(common_practice_scale_degree_label(12, 0, COMMON_PRACTICE_KEY_MAJOR) == NULL);
    assert(common_practice_scale_degree_label(0, -1, COMMON_PRACTICE_KEY_MAJOR) == NULL);
    assert(common_practice_scale_degree_label(0, 0, (CommonPracticeKeyMode)7) == NULL);
}

static void test_rejects_bad_input(void) {
    int soprano[1] = {72};
    int bass[1] = {48};
    OuterVoiceLines lines = make_lines(soprano, bass, 1);
    Key *key = make_key(COMMON_PRACTICE_KEY_MAJOR, "C");
    CommonPracticeOuterDegreeList degrees = {NULL, 7};
    assert(common_practice_outer_scale_degrees(NULL, key, 0, COMMON_PRACTICE_KEY_MAJOR, &degrees) ==
           COMMON_PRACTICE_OUTER_ERR_NULL_ARGUMENT);
    assert(common_practice_outer_scale_degrees(&lines, NULL, 0, COMMON_PRACTICE_KEY_MAJOR, &degrees) ==
           COMMON_PRACTICE_OUTER_ERR_NULL_ARGUMENT);
    assert(common_practice_outer_scale_degrees(&lines, key, 12, COMMON_PRACTICE_KEY_MAJOR, &degrees) ==
           COMMON_PRACTICE_OUTER_ERR_BAD_TONIC);
    assert(common_practice_outer_scale_degrees(&lines, key, 0, (CommonPracticeKeyMode)7, &degrees) ==
           COMMON_PRACTICE_OUTER_ERR_UNSUPPORTED_MODE);
    assert(degrees.items == NULL && degrees.count == 7); /* untouched */

    /* A half-semitone position is not supported. */
    OuterVoiceLines fractional = make_lines(soprano, bass, 1);
    pitch_numeric_value_destroy(fractional.soprano[0].pitch);
    TuningSystem *tuning = NULL;
    assert(tuning_system_create(make_id("midi"), "MIDI note number", &tuning) == TUNING_SYSTEM_OK);
    assert(pitch_numeric_value_create(tuning, 145, 2, &fractional.soprano[0].pitch) == PITCH_NUMERIC_VALUE_OK);
    tuning_system_destroy(tuning);
    assert(common_practice_outer_scale_degrees(&fractional, key, 0, COMMON_PRACTICE_KEY_MAJOR, &degrees) ==
           COMMON_PRACTICE_OUTER_ERR_UNSUPPORTED_PITCH);
    outer_voice_lines_destroy(&fractional);

    common_practice_outer_degrees_destroy(NULL);
    outer_voice_lines_destroy(&lines);
    key_destroy(key);
}

int main(void) {
    test_degrees_in_a_major_key();
    test_degrees_in_a_transposed_major_key();
    test_altered_degrees();
    test_degrees_in_a_minor_key_including_the_raised_seventh();
    test_an_absent_line_has_no_scale_degree();
    test_each_degree_carries_a_framework_reference_and_its_note();
    test_the_label_function_directly();
    test_rejects_bad_input();
    printf("test_outer_scale_degrees: ok\n");
    return 0;
}
