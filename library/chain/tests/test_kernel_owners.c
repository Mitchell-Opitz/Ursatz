#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "analysis/motive_tone_roles.h"
#include "core/rational.h"
#include "identity/entity_id.h"
#include "pitch/pitch_numeric_value.h"
#include "pitch/tuning_system.h"
#include "ursatz_chain/analysis_chain.h"
#include "standard-form/analysis/analysis_motive_match.h"
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
        s.ioi[i] = rational_ratio(gaps[i], 4);
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
    StandardFormRelationMatch m;
    assert(chain_match_motive_relation(&proto, &same, &m, NULL) && m.kind == MOTIVE_OCCURRENCE_EXACT && m.score > 0.0);
    assert(chain_match_motive_relation(&proto, &moved, &m, NULL) && m.kind == MOTIVE_OCCURRENCE_REAL);
    assert(m.score > 0.0 && m.score <= 1.0);

    /* C D E C and D E F D in C major: the semitones differ (2 2 -4 / 2 1 -3), the degrees do not: tonal */
    const int semis_a[3] = {2, 2, -4};
    const int semis_b[3] = {2, 1, -3};
    StandardFormMotiveShape tonal_proto = shape(true, 3, semis_a, generic_up, gaps, 60);
    StandardFormMotiveShape tonal_cand = shape(true, 3, semis_b, generic_up, gaps, 62);
    ChainRelationContext key = {true, 0, false};
    assert(chain_match_motive_relation(&tonal_proto, &tonal_cand, &m, &key) && m.kind == MOTIVE_OCCURRENCE_TONAL);
    /* without a key the same shapes are not a transposition of a scale: no match as a tonal one */
    assert(!chain_match_motive_relation(&tonal_proto, &tonal_cand, &m, NULL) || m.kind != MOTIVE_OCCURRENCE_TONAL);
}

static void test_inverted_and_retrograde_shapes(void) {
    const long gaps[3] = {4, 4, 4};
    const int up[3] = {2, 3, 2};
    const int down[3] = {-2, -3, -2};
    const int back[3] = {-2, -3, -2};
    StandardFormMotiveShape proto = shape(false, 3, up, up, gaps, 60);
    StandardFormMotiveShape inverted = shape(false, 3, down, down, gaps, 72);
    StandardFormRelationMatch m;
    assert(chain_match_motive_relation(&proto, &inverted, &m, NULL));
    assert(m.kind == MOTIVE_OCCURRENCE_INVERTED || m.kind == MOTIVE_OCCURRENCE_RETROGRADE);

    /* an asymmetric figure: its retrograde reverses the intervals and flips them; its inversion only flips */
    const int figure[3] = {2, 3, 1};
    const int reversed[3] = {-1, -3, -2};
    const int flipped[3] = {-2, -3, -1};
    StandardFormMotiveShape p = shape(false, 3, figure, figure, gaps, 60);
    StandardFormMotiveShape r = shape(false, 3, reversed, reversed, gaps, 66);
    StandardFormMotiveShape v = shape(false, 3, flipped, flipped, gaps, 72);
    assert(chain_match_motive_relation(&p, &r, &m, NULL) && m.kind == MOTIVE_OCCURRENCE_RETROGRADE);
    assert(chain_match_motive_relation(&p, &v, &m, NULL) && m.kind == MOTIVE_OCCURRENCE_INVERTED);
}

static void test_augmentation_and_diminution_shapes(void) {
    const int steps[3] = {2, 2, -4};
    const long quarter[3] = {1, 1, 1};
    const long half[3] = {2, 2, 2};
    const long double_length[3] = {4, 4, 4};
    const long triple[3] = {3, 3, 3};
    StandardFormMotiveShape proto = shape(false, 3, steps, steps, half, 60);
    StandardFormMotiveShape augmented = shape(false, 3, steps, steps, double_length, 60);
    StandardFormMotiveShape diminished = shape(false, 3, steps, steps, quarter, 60);
    StandardFormMotiveShape by_three = shape(false, 3, steps, steps, triple, 60);
    StandardFormRelationMatch m;
    assert(chain_match_motive_relation(&proto, &augmented, &m, NULL) && m.kind == MOTIVE_OCCURRENCE_AUGMENTED);
    assert(m.score > 0.0 && m.score <= 1.0 && m.fragment_part == -1);
    assert(chain_match_motive_relation(&proto, &diminished, &m, NULL) && m.kind == MOTIVE_OCCURRENCE_DIMINISHED);
    /* only the factors 2 and 1/2 are read as an augmentation or diminution */
    assert(!chain_match_motive_relation(&proto, &by_three, &m, NULL) || (m.kind != MOTIVE_OCCURRENCE_AUGMENTED && m.kind != MOTIVE_OCCURRENCE_DIMINISHED));

    /* the form takes these through its classifier, as a kind it does not itself match */
    StandardFormMotiveMatch match;
    assert(standard_form_motive_classify_match(&proto, &augmented, 1, chain_match_motive_relation, NULL, &match) &&
           match.kind == (int)MOTIVE_OCCURRENCE_AUGMENTED && match.intervals == 3);
    assert(standard_form_motive_classify_match(&proto, &diminished, 1, chain_match_motive_relation, NULL, &match) &&
           match.kind == (int)MOTIVE_OCCURRENCE_DIMINISHED);
}

static void test_fragment_shapes(void) {
    const long gaps[4] = {1, 1, 1, 1};
    const int whole_steps[4] = {2, 2, 1, 2};
    const int prefix_steps[2] = {2, 2};
    const int suffix_steps[2] = {1, 2};
    const int middle_steps[2] = {2, 1};
    const int one_step[1] = {2};
    StandardFormMotiveShape proto = shape(false, 4, whole_steps, whole_steps, gaps, 60);
    StandardFormMotiveShape prefix = shape(false, 2, prefix_steps, prefix_steps, gaps, 60);
    StandardFormMotiveShape suffix = shape(false, 2, suffix_steps, suffix_steps, gaps, 64);
    StandardFormMotiveShape middle = shape(false, 2, middle_steps, middle_steps, gaps, 62);
    StandardFormMotiveShape single = shape(false, 1, one_step, one_step, gaps, 60);
    StandardFormRelationMatch m;
    StandardFormMotiveMatch match;

    assert(chain_match_motive_relation(&proto, &prefix, &m, NULL) && m.kind == MOTIVE_OCCURRENCE_FRAGMENTED);
    assert(m.fragment_part == 0 && m.fragment_intervals == 2);
    assert(standard_form_motive_fragment_match(&proto, &prefix, chain_match_motive_relation, NULL, &match) &&
           match.kind == (int)MOTIVE_OCCURRENCE_FRAGMENTED && match.fragment_part == 0 && match.fragment_intervals == 2 &&
           match.intervals == 2);

    assert(chain_match_motive_relation(&proto, &suffix, &m, NULL) && m.kind == MOTIVE_OCCURRENCE_FRAGMENTED);
    assert(m.fragment_part == 1 && m.fragment_intervals == 2);
    assert(standard_form_motive_fragment_match(&proto, &suffix, chain_match_motive_relation, NULL, &match) && match.fragment_part == 1);

    /* a middle part, and a part below the fewest intervals, are not claimed */
    assert(!chain_match_motive_relation(&proto, &middle, &m, NULL));
    assert(!standard_form_motive_fragment_match(&proto, &middle, chain_match_motive_relation, NULL, &match));
    assert(!standard_form_motive_fragment_match(&proto, &single, chain_match_motive_relation, NULL, &match));
    /* the whole is no fragment, and a fragment is no whole-window kind */
    assert(!standard_form_motive_fragment_match(&proto, &proto, chain_match_motive_relation, NULL, &match));
    assert(!standard_form_motive_classify_match(&proto, &prefix, 1, chain_match_motive_relation, NULL, &match));
}

static void test_refusals(void) {
    const long gaps[3] = {4, 4, 4};
    const long other_gaps[3] = {4, 2, 4};
    const int a[3] = {2, 2, 1};
    const int b[3] = {3, 2, 2};
    StandardFormMotiveShape proto = shape(false, 3, a, a, gaps, 60);
    StandardFormMotiveShape unrelated = shape(false, 3, b, b, gaps, 60);
    StandardFormMotiveShape rhythm = shape(false, 3, a, a, other_gaps, 60);
    StandardFormMotiveShape shorter = shape(false, 2, b, b, gaps, 60);    /* not a part of the prototype */
    StandardFormMotiveShape too_short = shape(false, 1, a, a, gaps, 60); /* a part, but below the fewest intervals */
    StandardFormRelationMatch m;
    assert(!chain_match_motive_relation(&proto, &unrelated, &m, NULL));
    assert(!chain_match_motive_relation(&proto, &rhythm, &m, NULL));
    assert(!chain_match_motive_relation(&proto, &shorter, &m, NULL));
    assert(!chain_match_motive_relation(&proto, &too_short, &m, NULL));
    assert(!chain_match_motive_relation(NULL, &proto, &m, NULL));
    assert(!chain_match_motive_relation(&proto, &proto, NULL, NULL));
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
    assert(config.weighting.classifier == NULL && config.form.relation_match == NULL); /* set when the chain runs */
}

int main(void) {
    test_the_classifier_is_the_owner();
    test_equal_and_transposed_shapes();
    test_inverted_and_retrograde_shapes();
    test_augmentation_and_diminution_shapes();
    test_fragment_shapes();
    test_refusals();
    test_relabelling_a_melody_line();
    test_the_chain_injects_by_default();
    printf("kernel owners: ok\n");
    return 0;
}
