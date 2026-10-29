#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "analysis/motive_tone_roles.h"
#include "identity/entity_id.h"
#include "pitch/pitch_numeric_value.h"
#include "pitch/tuning_system.h"
#include "ursatz_chain/analysis_chain.h"
#include "ursatz_chain/kernel_owners.h"

/*
 * The chain's kernel-owner callbacks: the tone classifier gives motive_tone_roles_line's labels; the relation matcher recognizes an
 * equal, a transposed (real and tonal), an inverted and a retrograde motive and refuses an unrelated one; relabelling a melody line
 * puts the owner's labels on its notes; the chain injects both by default.
 */

#define C_MAJOR_TRIAD ((1u << 0) | (1u << 4) | (1u << 7))

static StandardFormMotiveShape shape(bool generic, size_t intervals, const int *semitones, const int *steps, const long *gaps,
                                     long first) {
    StandardFormMotiveShape s;
    memset(&s, 0, sizeof(s));
    s.generic = generic;
    s.intervals = intervals;
    s.first_semitone = first;
    for (size_t i = 0; i < intervals; i++) {
        s.semitones[i] = semitones[i];
        s.steps[i] = steps[i];
        s.ioi[i] = standard_form_q(gaps[i], 4);
    }
    return s;
}

static void test_the_classifier_is_the_owner(void) {
    ToneLineNote notes[3];
    ToneLineLabel chain_labels[3];
    ToneLineLabel owner_labels[3];
    const int pitch[3] = {60, 62, 64};
    for (size_t i = 0; i < 3; i++) {
        notes[i].midi = pitch[i];
        notes[i].onset = rational_ratio((long long)i, 4);
        notes[i].duration = rational_ratio(1, 4);
        notes[i].chord_pitch_classes = C_MAJOR_TRIAD;
    }
    assert(chain_classify_tones(notes, 3, chain_labels, NULL));
    assert(motive_tone_roles_line(notes, 3, owner_labels, NULL));
    for (size_t i = 0; i < 3; i++) {
        assert(chain_labels[i].chord_tone == owner_labels[i].chord_tone);
        assert(chain_labels[i].classified == owner_labels[i].classified);
        assert(chain_labels[i].type == owner_labels[i].type);
        assert(chain_labels[i].resolves_to == owner_labels[i].resolves_to);
    }
    assert(!chain_labels[1].chord_tone && chain_labels[1].classified && chain_labels[1].type == NON_CHORD_TONE_PASSING);
}

static void test_equal_and_transposed_shapes(void) {
    const long gaps[3] = {4, 4, 4};
    const int up_steps[3] = {2, 2, -4};
    const int generic_up[3] = {2, 2, -3};
    StandardFormMotiveShape proto = shape(false, 3, up_steps, up_steps, gaps, 60);
    StandardFormMotiveShape same = proto;
    StandardFormMotiveShape moved = shape(false, 3, up_steps, up_steps, gaps, 65);
    StandardFormMotiveKind kind;
    double score = 0.0;
    assert(chain_match_motive_relation(&proto, &same, &kind, &score, NULL) && kind == STANDARD_FORM_MOTIVE_EXACT && score > 0.0);
    assert(chain_match_motive_relation(&proto, &moved, &kind, &score, NULL) && kind == STANDARD_FORM_MOTIVE_REAL);
    assert(score > 0.0 && score <= 1.0);

    /* C D E C and D E F D in C major: the semitones differ (2 2 -4 / 2 1 -3), the degrees do not: tonal */
    const int semis_a[3] = {2, 2, -4};
    const int semis_b[3] = {2, 1, -3};
    StandardFormMotiveShape tonal_proto = shape(true, 3, semis_a, generic_up, gaps, 60);
    StandardFormMotiveShape tonal_cand = shape(true, 3, semis_b, generic_up, gaps, 62);
    ChainRelationContext key = {true, 0, false};
    assert(chain_match_motive_relation(&tonal_proto, &tonal_cand, &kind, &score, &key) && kind == STANDARD_FORM_MOTIVE_TONAL);
    /* without a key the same shapes are not a transposition of a scale: no match as a tonal one */
    assert(!chain_match_motive_relation(&tonal_proto, &tonal_cand, &kind, &score, NULL) || kind != STANDARD_FORM_MOTIVE_TONAL);
}

static void test_inverted_and_retrograde_shapes(void) {
    const long gaps[3] = {4, 4, 4};
    const int up[3] = {2, 3, 2};
    const int down[3] = {-2, -3, -2};
    const int back[3] = {-2, -3, -2};
    StandardFormMotiveShape proto = shape(false, 3, up, up, gaps, 60);
    StandardFormMotiveShape inverted = shape(false, 3, down, down, gaps, 72);
    StandardFormMotiveKind kind;
    double score = 0.0;
    assert(chain_match_motive_relation(&proto, &inverted, &kind, &score, NULL));
    assert(kind == STANDARD_FORM_MOTIVE_INVERTED || kind == STANDARD_FORM_MOTIVE_RETROGRADE);

    /* an asymmetric figure: its retrograde reverses the intervals and flips them; its inversion only flips */
    const int figure[3] = {2, 3, 1};
    const int reversed[3] = {-1, -3, -2};
    const int flipped[3] = {-2, -3, -1};
    StandardFormMotiveShape p = shape(false, 3, figure, figure, gaps, 60);
    StandardFormMotiveShape r = shape(false, 3, reversed, reversed, gaps, 66);
    StandardFormMotiveShape v = shape(false, 3, flipped, flipped, gaps, 72);
    assert(chain_match_motive_relation(&p, &r, &kind, &score, NULL) && kind == STANDARD_FORM_MOTIVE_RETROGRADE);
    assert(chain_match_motive_relation(&p, &v, &kind, &score, NULL) && kind == STANDARD_FORM_MOTIVE_INVERTED);
}

static void test_refusals(void) {
    const long gaps[3] = {4, 4, 4};
    const long other_gaps[3] = {4, 2, 4};
    const int a[3] = {2, 2, 1};
    const int b[3] = {3, 2, 2};
    StandardFormMotiveShape proto = shape(false, 3, a, a, gaps, 60);
    StandardFormMotiveShape unrelated = shape(false, 3, b, b, gaps, 60);
    StandardFormMotiveShape rhythm = shape(false, 3, a, a, other_gaps, 60);
    StandardFormMotiveShape shorter = shape(false, 2, a, a, gaps, 60);
    StandardFormMotiveKind kind;
    double score;
    assert(!chain_match_motive_relation(&proto, &unrelated, &kind, &score, NULL));
    assert(!chain_match_motive_relation(&proto, &rhythm, &kind, &score, NULL));
    assert(!chain_match_motive_relation(&proto, &shorter, &kind, &score, NULL));
    assert(!chain_match_motive_relation(NULL, &proto, &kind, &score, NULL));
    assert(!chain_match_motive_relation(&proto, &proto, NULL, &score, NULL));
}

static bool all_non_chord(const ToneLineNote *notes, size_t count, ToneLineLabel *out, void *context) {
    (void)notes;
    (*(int *)context)++;
    for (size_t i = 0; i < count; i++) {
        tone_line_label_clear(&out[i]);
        out[i].classified = true;
        out[i].type = NON_CHORD_TONE_APPOGGIATURA;
    }
    return true;
}

static void test_relabelling_a_melody_line(void) {
    /* a C major segment, and a melody line C D rest-free: both notes in segment 0 */
    CommonPracticeMember members[3];
    memset(members, 0, sizeof(members));
    const int chord[3] = {48, 52, 55};
    const char *ids[3] = {"c", "e", "g"};
    for (size_t i = 0; i < 3; i++) {
        members[i].note_id = ids[i];
        members[i].pitch = chord[i];
        members[i].duration = 1.0;
        members[i].strength = BEAT_STRENGTH_STRONG;
    }
    CommonPracticeSegment segment = {members, 3, "sonority-1", 0};
    CommonPracticeWeightedResult result;
    assert(common_practice_identify_weighted(&segment, 0, COMMON_PRACTICE_KEY_MAJOR, NULL, &result) == COMMON_PRACTICE_WEIGHTED_OK);
    assert(result.matched);

    EntityID *tuning_id = NULL;
    TuningSystem *tuning = NULL;
    assert(entity_id_create("tuning", &tuning_id) == ENTITY_ID_OK);
    assert(tuning_system_create(tuning_id, "MIDI note number", &tuning) == TUNING_SYSTEM_OK);
    PitchNumericValue *pitch[2];
    ChainMelodyLine melody;
    memset(&melody, 0, sizeof(melody));
    standard_counterpoint_melody_line_init(&melody.line);
    melody.segment = (size_t *)calloc(3, sizeof(size_t));
    assert(melody.segment != NULL);
    const int midi[2] = {60, 62};
    const char *note_ids[2] = {"m0", "m1"};
    for (size_t i = 0; i < 2; i++) {
        assert(pitch_numeric_value_create(tuning, midi[i], 1, &pitch[i]) == PITCH_NUMERIC_VALUE_OK);
        StandardCounterpointMelodyNote note;
        memset(&note, 0, sizeof(note));
        note.present = true;
        note.note_id = note_ids[i];
        note.pitch = pitch[i];
        note.confidence = 1.0;
        note.nct = STANDARD_COUNTERPOINT_NCT_CHORD_TONE;
        assert(standard_counterpoint_melody_line_add_note(&melody.line, &note) == STANDARD_COUNTERPOINT_MELODY_OK);
        melody.segment[i] = 0;
    }
    int calls = 0;
    assert(chain_relabel_melody_line(&melody, &result, 1, all_non_chord, &calls));
    assert(calls == 1);
    assert(melody.line.notes[0].nct == STANDARD_COUNTERPOINT_NCT_NON_CHORD_TONE);
    assert(melody.line.notes[0].nct_type == NON_CHORD_TONE_APPOGGIATURA);
    assert(melody.line.notes[1].nct == STANDARD_COUNTERPOINT_NCT_NON_CHORD_TONE);
    /* the mask was only lent to the classifier */
    assert(melody.line.notes[0].chord_pitch_classes == 0);
    /* a segment without a match gives the classifier no run: the notes keep what they held */
    melody.segment[0] = 5;
    melody.segment[1] = 5;
    melody.line.notes[0].nct = STANDARD_COUNTERPOINT_NCT_CHORD_TONE;
    calls = 0;
    assert(chain_relabel_melody_line(&melody, &result, 1, all_non_chord, &calls));
    assert(calls == 0 && melody.line.notes[0].nct == STANDARD_COUNTERPOINT_NCT_CHORD_TONE);
    assert(!chain_relabel_melody_line(NULL, &result, 1, all_non_chord, &calls));
    assert(!chain_relabel_melody_line(&melody, &result, 1, NULL, &calls));

    free(melody.segment);
    standard_counterpoint_melody_line_destroy(&melody.line);
    for (size_t i = 0; i < 2; i++) {
        pitch_numeric_value_destroy(pitch[i]);
    }
    tuning_system_destroy(tuning);
    common_practice_weighted_result_destroy(&result);
}

static void test_the_chain_injects_by_default(void) {
    AnalysisChainConfig config = analysis_chain_config_default();
    assert(config.inject_kernel_owners);
    assert(config.weighting.classifier == NULL && config.form.relation_matcher == NULL); /* set when the chain runs */
}

int main(void) {
    test_the_classifier_is_the_owner();
    test_equal_and_transposed_shapes();
    test_inverted_and_retrograde_shapes();
    test_refusals();
    test_relabelling_a_melody_line();
    test_the_chain_injects_by_default();
    printf("kernel owners: ok\n");
    return 0;
}
