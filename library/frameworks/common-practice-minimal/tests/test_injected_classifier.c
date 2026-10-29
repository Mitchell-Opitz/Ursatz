#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "weighted_chord_identification.h"

/*
 * The injected NonChordToneClassifier of CommonPracticeWeightedConfig: NULL keeps the framework rule, a classifier that labels
 * everything passing relabels the voiced leftover tones, members without a voice keep the framework rule, a refusing
 * classifier keeps it too, and the line the classifier sees is per voice, with the chord mask of each segment.
 */

#define S BEAT_STRENGTH_STRONG
#define M BEAT_STRENGTH_MEDIUM
#define W BEAT_STRENGTH_WEAK

static CommonPracticeMember note(const char *id, int pitch, bool held, double duration, BeatStrength strength,
                                 const char *voice, double onset) {
    CommonPracticeMember m;
    memset(&m, 0, sizeof(m));
    m.note_id = id;
    m.pitch = pitch;
    m.held = held;
    m.duration = duration;
    m.strength = strength;
    m.voice_id = voice;
    m.onset = onset;
    return m;
}

static CommonPracticeSegment segment(const CommonPracticeMember *members, size_t count, const char *id) {
    CommonPracticeSegment s;
    s.members = members;
    s.member_count = count;
    s.id = id;
    s.beat = 0;
    return s;
}

typedef struct {
    int calls;
    size_t last_count;
    unsigned masks[8];
    int midi[8];
    bool refuse;
} Spy;

/* Labels every note that is not in its chord as a passing tone, and records the last line it saw. */
static bool all_passing(const ToneLineNote *notes, size_t count, ToneLineLabel *out, void *context) {
    Spy *spy = (Spy *)context;
    spy->calls++;
    if (spy->refuse) {
        return false;
    }
    spy->last_count = count;
    for (size_t i = 0; i < count; i++) {
        tone_line_label_clear(&out[i]);
        bool in_chord = (notes[i].chord_pitch_classes >> (((notes[i].midi % 12) + 12) % 12) & 1u) != 0;
        out[i].chord_tone = in_chord;
        out[i].classified = true;
        out[i].type = NON_CHORD_TONE_PASSING;
        if (i < 8) {
            spy->masks[i] = notes[i].chord_pitch_classes;
            spy->midi[i] = notes[i].midi;
        }
    }
    return true;
}

/* IV, then C major with the soprano's F4 held over, then the soprano falls to E4 (a 4-3 suspension). */
static void suspension(CommonPracticeSegment *segments, CommonPracticeMember *m0, CommonPracticeMember *m1,
                       CommonPracticeMember *m2, const char *soprano_voice) {
    m0[0] = note("fb", 53, false, 0.5, S, "b", 0.0);
    m0[1] = note("a", 57, false, 0.5, S, "a", 0.0);
    m0[2] = note("c", 60, false, 0.5, S, "c", 0.0);
    m0[3] = note("f", 65, false, 0.5, S, soprano_voice, 0.0);
    m1[0] = note("cb", 48, false, 0.5, S, "b", 0.5);
    m1[1] = note("e3", 52, false, 0.5, S, "a", 0.5);
    m1[2] = note("g", 55, false, 0.5, S, "c", 0.5);
    m1[3] = note("c4", 60, false, 0.5, S, "t", 0.5);
    m1[4] = note("f", 65, true, 0.5, S, soprano_voice, 0.0);
    m2[0] = note("cb", 48, true, 0.5, S, "b", 0.5);
    m2[1] = note("e3", 52, true, 0.5, S, "a", 0.5);
    m2[2] = note("g", 55, true, 0.5, S, "c", 0.5);
    m2[3] = note("c4", 60, true, 0.5, S, "t", 0.5);
    m2[4] = note("e", 64, false, 0.5, M, soprano_voice, 1.0);
    segments[0] = segment(m0, 4, "s0");
    segments[1] = segment(m1, 5, "s1");
    segments[2] = segment(m2, 5, "s2");
}

static void test_null_classifier_keeps_the_framework_rule(void) {
    CommonPracticeMember m0[4], m1[5], m2[5];
    CommonPracticeSegment segments[3];
    suspension(segments, m0, m1, m2, "s");
    CommonPracticeWeightedConfig config = common_practice_weighted_config_default();
    assert(config.classifier == NULL && config.classifier_context == NULL);
    CommonPracticeWeightedResults with_default = {0};
    CommonPracticeWeightedResults with_null = {0};
    assert(common_practice_identify_weighted_sequence(segments, 3, 0, COMMON_PRACTICE_KEY_MAJOR, NULL, &with_default) ==
           COMMON_PRACTICE_WEIGHTED_OK);
    assert(common_practice_identify_weighted_sequence(segments, 3, 0, COMMON_PRACTICE_KEY_MAJOR, &config, &with_null) ==
           COMMON_PRACTICE_WEIGHTED_OK);
    assert(with_default.items[1].leftover_count == 1 && with_null.items[1].leftover_count == 1);
    assert(with_null.items[1].leftover[0].tone == NON_CHORD_TONE_SUSPENSION);
    assert(with_null.items[1].leftover[0].tone == with_default.items[1].leftover[0].tone);
    common_practice_weighted_results_destroy(&with_default);
    common_practice_weighted_results_destroy(&with_null);
}

static void test_a_classifier_labels_the_voiced_leftover_tones(void) {
    CommonPracticeMember m0[4], m1[5], m2[5];
    CommonPracticeSegment segments[3];
    suspension(segments, m0, m1, m2, "s");
    Spy spy;
    memset(&spy, 0, sizeof(spy));
    CommonPracticeWeightedConfig config = common_practice_weighted_config_default();
    config.classifier = all_passing;
    config.classifier_context = &spy;
    CommonPracticeWeightedResults results = {0};
    assert(common_practice_identify_weighted_sequence(segments, 3, 0, COMMON_PRACTICE_KEY_MAJOR, &config, &results) ==
           COMMON_PRACTICE_WEIGHTED_OK);
    assert(spy.calls >= 1);
    /* the held F is labelled by the classifier as a passing tone, not by the framework as a suspension */
    assert(results.items[1].leftover_count == 1);
    assert(results.items[1].leftover[0].classified && results.items[1].leftover[0].tone == NON_CHORD_TONE_PASSING);
    assert(strcmp(results.items[1].leftover[0].note_id, "f") == 0);
    /* the soprano line is F (IV), F held (I), E (I): three entries, each with the mask of the segment it sounds in */
    assert(spy.last_count == 3);
    assert(spy.midi[0] == 65 && spy.midi[1] == 65 && spy.midi[2] == 64);
    assert(spy.masks[0] != spy.masks[1] && spy.masks[1] == spy.masks[2]);
    assert((spy.masks[0] >> 5 & 1u) != 0);                   /* F is in the IV chord */
    assert((spy.masks[1] >> 0 & 1u) != 0 && (spy.masks[1] >> 5 & 1u) == 0); /* C major: C in, F out */
    common_practice_weighted_results_destroy(&results);
}

static void test_unvoiced_members_keep_the_framework_rule(void) {
    CommonPracticeMember m0[4], m1[5], m2[5];
    CommonPracticeSegment segments[3];
    suspension(segments, m0, m1, m2, NULL);
    Spy spy;
    memset(&spy, 0, sizeof(spy));
    CommonPracticeWeightedConfig config = common_practice_weighted_config_default();
    config.classifier = all_passing;
    config.classifier_context = &spy;
    CommonPracticeWeightedResults results = {0};
    assert(common_practice_identify_weighted_sequence(segments, 3, 0, COMMON_PRACTICE_KEY_MAJOR, &config, &results) ==
           COMMON_PRACTICE_WEIGHTED_OK);
    assert(spy.calls == 0);
    assert(results.items[1].leftover_count == 1 && results.items[1].leftover[0].tone == NON_CHORD_TONE_SUSPENSION);
    common_practice_weighted_results_destroy(&results);
}

static void test_a_refusing_classifier_keeps_the_framework_rule(void) {
    CommonPracticeMember m0[4], m1[5], m2[5];
    CommonPracticeSegment segments[3];
    suspension(segments, m0, m1, m2, "s");
    Spy spy;
    memset(&spy, 0, sizeof(spy));
    spy.refuse = true;
    CommonPracticeWeightedConfig config = common_practice_weighted_config_default();
    config.classifier = all_passing;
    config.classifier_context = &spy;
    CommonPracticeWeightedResults results = {0};
    assert(common_practice_identify_weighted_sequence(segments, 3, 0, COMMON_PRACTICE_KEY_MAJOR, &config, &results) ==
           COMMON_PRACTICE_WEIGHTED_OK);
    assert(spy.calls >= 1);
    assert(results.items[1].leftover_count == 1 && results.items[1].leftover[0].tone == NON_CHORD_TONE_SUSPENSION);
    common_practice_weighted_results_destroy(&results);
}

int main(void) {
    test_null_classifier_keeps_the_framework_rule();
    test_a_classifier_labels_the_voiced_leftover_tones();
    test_unvoiced_members_keep_the_framework_rule();
    test_a_refusing_classifier_keeps_the_framework_rule();
    printf("injected classifier: ok\n");
    return 0;
}
