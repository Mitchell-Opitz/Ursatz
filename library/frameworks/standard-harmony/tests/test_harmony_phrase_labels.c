#include <assert.h>
#include <stdio.h>

#include "harmony_phrase_labels.h"
#include "harmony_test_util.h"

#define TB PHRASE_MODEL_LABEL_TONIC_BEGINNING
#define PD PHRASE_MODEL_LABEL_PREDOMINANT
#define D PHRASE_MODEL_LABEL_DOMINANT
#define TE PHRASE_MODEL_LABEL_TONIC_ENDING

#define PAC CADENCE_TYPE_PERFECT_AUTHENTIC
#define IAC CADENCE_TYPE_IMPERFECT_AUTHENTIC
#define HC CADENCE_TYPE_HALF

#define I CH(1)
#define IV CH(4)
#define II6 CHI(2, 1)
#define V CH(5)
#define V7 CH7(5)
#define VI CH(6)

static bool label(HarmonyMode mode, const HarmonyChord *chords, size_t count, CadenceType cadence,
                  HarmonyPhraseLabels *out) {
    HarmonyPhrase phrase = {mode, chords, count};
    return harmony_phrase_model_label(&phrase, cadence, out);
}

/* The labels carried, in chord order: the indexes of Tb, PD, D, and Te, or -1. */
static void expect(const HarmonyPhraseLabels *labels, int tb, int pd, int d, int te) {
    assert(harmony_phrase_labels_index_of(labels, TB) == tb);
    assert(harmony_phrase_labels_index_of(labels, PD) == pd);
    assert(harmony_phrase_labels_index_of(labels, D) == d);
    assert(harmony_phrase_labels_index_of(labels, TE) == te);
    int carried = (tb >= 0) + (pd >= 0) + (d >= 0) + (te >= 0);
    int count = 0;
    for (size_t i = 0; i < labels->chord_count; i++) {
        count += labels->has_label[i];
    }
    assert(count == carried); /* no chord carries a label that is not one of these */
}

static void test_authentic_cadence(void) {
    const HarmonyChord phrase[] = {I, IV, V, I};
    for (int mode = 0; mode < HARMONY_MODE_COUNT; mode++) {
        for (int c = 0; c < 2; c++) {
            HarmonyPhraseLabels labels;
            assert(label((HarmonyMode)mode, phrase, COUNT_OF(phrase), c == 0 ? PAC : IAC, &labels));
            assert(labels.chord_count == 4);
            expect(&labels, 0, 1, 2, 3);
        }
    }
}

static void test_half_cadence_has_no_tonic_ending(void) {
    const HarmonyChord phrase[] = {I, II6, V};
    HarmonyPhraseLabels labels;
    assert(label(HARMONY_MODE_MAJOR, phrase, COUNT_OF(phrase), HC, &labels));
    expect(&labels, 0, 1, 2, -1);
}

static void test_pd_is_the_first_strong_predominant_of_the_run(void) {
    HarmonyPhraseLabels labels;
    /* IV then ii6: PD on the IV. */
    const HarmonyChord both[] = {I, IV, II6, V, I};
    assert(label(HARMONY_MODE_MAJOR, both, COUNT_OF(both), PAC, &labels));
    expect(&labels, 0, 1, 3, 4);
    /* A weak predominant first: the PD is the first strong one. */
    const HarmonyChord weak_first[] = {I, VI, IV, V, I};
    assert(label(HARMONY_MODE_MAJOR, weak_first, COUNT_OF(weak_first), PAC, &labels));
    expect(&labels, 0, 2, 3, 4);
    /* Only weak predominants: no PD. */
    const HarmonyChord weak_only[] = {I, VI, V, I};
    assert(label(HARMONY_MODE_MAJOR, weak_only, COUNT_OF(weak_only), PAC, &labels));
    expect(&labels, 0, -1, 2, 3);
    /* No predominant at all. */
    const HarmonyChord none[] = {I, V, I};
    assert(label(HARMONY_MODE_MAJOR, none, COUNT_OF(none), PAC, &labels));
    expect(&labels, 0, -1, 1, 2);
}

static void test_only_the_run_before_the_dominant_counts(void) {
    HarmonyPhraseLabels labels;
    /* A strong predominant earlier in the phrase, with a tonic between it and the V. */
    const HarmonyChord earlier[] = {I, IV, I, V, I};
    assert(label(HARMONY_MODE_MAJOR, earlier, COUNT_OF(earlier), PAC, &labels));
    expect(&labels, 0, -1, 3, 4);
    /* The run is the one just before the dominant: the later IV, not the earlier. */
    const HarmonyChord two_runs[] = {I, II6, I, IV, V, I};
    assert(label(HARMONY_MODE_MAJOR, two_runs, COUNT_OF(two_runs), PAC, &labels));
    expect(&labels, 0, 3, 4, 5);
}

static void test_a_plagal_iv_before_tonic_gets_no_label(void) {
    /* (IV) goes to I, so it is not in the run of predominants before the dominant. */
    const HarmonyChord phrase[] = {I, IV, I, IV, V, I};
    for (int mode = 0; mode < HARMONY_MODE_COUNT; mode++) {
        HarmonyPhraseLabels labels;
        assert(label((HarmonyMode)mode, phrase, COUNT_OF(phrase), PAC, &labels));
        expect(&labels, 0, 3, 4, 5);
        assert(!labels.has_label[1]);
    }
}

static void test_la_in_the_bass_labels(void) {
    for (int mode = 0; mode < HARMONY_MODE_COUNT; mode++) {
        HarmonyMode m = (HarmonyMode)mode;
        HarmonyPhraseLabels labels;
        /* vi connects tonic to a strong predominant: the PD label goes on the strong one. */
        const HarmonyChord connecting[] = {I, VI, II6, V, I};
        assert(label(m, connecting, COUNT_OF(connecting), PAC, &labels));
        expect(&labels, 0, 2, 3, 4);
        /* vi as a stand-in for a strong predominant: only a weak one, so no PD. */
        const HarmonyChord stand_in[] = {I, VI, V, I};
        assert(label(m, stand_in, COUNT_OF(stand_in), PAC, &labels));
        expect(&labels, 0, -1, 2, 3);
        /* IV6 has the function of IV, a strong predominant. */
        const HarmonyChord iv6[] = {I, CHI(4, 1), V, I};
        assert(label(m, iv6, COUNT_OF(iv6), PAC, &labels));
        expect(&labels, 0, 1, 2, 3);
    }
}

static void test_the_mediant_is_a_weak_predominant(void) {
    for (int mode = 0; mode < HARMONY_MODE_COUNT; mode++) {
        HarmonyMode m = (HarmonyMode)mode;
        HarmonyPhraseLabels labels;
        /* iii goes through a strong predominant: the PD label goes on the strong one. */
        const HarmonyChord through[] = {I, CH(3), IV, V, I};
        assert(label(m, through, COUNT_OF(through), PAC, &labels));
        expect(&labels, 0, 2, 3, 4);
        /* iii directly to V: only a weak predominant, so no PD. */
        const HarmonyChord direct[] = {I, CH(3), V, I};
        assert(label(m, direct, COUNT_OF(direct), PAC, &labels));
        expect(&labels, 0, -1, 2, 3);
    }
}

static void test_cadential_six_four_gets_no_label(void) {
    HarmonyPhraseLabels labels;
    const HarmonyChord phrase[] = {I, IV, CADENTIAL_CHORD, V, I};
    assert(label(HARMONY_MODE_MAJOR, phrase, COUNT_OF(phrase), PAC, &labels));
    expect(&labels, 0, 1, 3, 4); /* D on the V, PD found past the 6/4, nothing on the 6/4 */
    assert(!labels.has_label[2]);

    /* The same in a half cadence, with a V7. */
    const HarmonyChord half[] = {I, II6, CADENTIAL_CHORD, V7};
    assert(label(HARMONY_MODE_MAJOR, half, COUNT_OF(half), HC, &labels));
    expect(&labels, 0, 1, 3, -1);

    /* A 6/4 straight after the tonic: no predominant. */
    const HarmonyChord no_pd[] = {I, CADENTIAL_CHORD, V, I};
    assert(label(HARMONY_MODE_MAJOR, no_pd, COUNT_OF(no_pd), PAC, &labels));
    expect(&labels, 0, -1, 2, 3);
}

static void test_dominant_seventh(void) {
    HarmonyPhraseLabels labels;
    const HarmonyChord phrase[] = {I, IV, V7, I};
    assert(label(HARMONY_MODE_MAJOR, phrase, COUNT_OF(phrase), PAC, &labels));
    expect(&labels, 0, 1, 2, 3);
}

static void test_tonic_beginning_and_prolongation(void) {
    HarmonyPhraseLabels labels;
    /* A prolongation of tonic: only the first chord is Tb. */
    const HarmonyChord prolonged[] = {I, CH7I(5, 1), I, IV, V, I};
    assert(label(HARMONY_MODE_MAJOR, prolonged, COUNT_OF(prolonged), PAC, &labels));
    expect(&labels, 0, 3, 4, 5);
    assert(!labels.has_label[1] && !labels.has_label[2]);

    /* A delayed or omitted tonic: the phrase begins on a predominant, or on V. */
    const HarmonyChord delayed[] = {IV, V, I};
    assert(label(HARMONY_MODE_MAJOR, delayed, COUNT_OF(delayed), PAC, &labels));
    expect(&labels, -1, 0, 1, 2);
    const HarmonyChord from_dominant[] = {V, I};
    assert(label(HARMONY_MODE_MAJOR, from_dominant, COUNT_OF(from_dominant), PAC, &labels));
    expect(&labels, -1, -1, 0, 1);
    const HarmonyChord just_v[] = {V};
    assert(label(HARMONY_MODE_MAJOR, just_v, COUNT_OF(just_v), HC, &labels));
    expect(&labels, -1, -1, 0, -1);
    const HarmonyChord tonic_then_v[] = {I, V};
    assert(label(HARMONY_MODE_MAJOR, tonic_then_v, COUNT_OF(tonic_then_v), HC, &labels));
    expect(&labels, 0, -1, 1, -1);
}

static void test_minor(void) {
    HarmonyPhraseLabels labels;
    const HarmonyChord phrase[] = {I, IV, V, I};
    assert(label(HARMONY_MODE_MINOR, phrase, COUNT_OF(phrase), PAC, &labels));
    expect(&labels, 0, 1, 2, 3);
    /* In minor VII (degree 7, no leading tone) is a weak predominant. */
    const HarmonyChord weak[] = {I, CH(7), V, I};
    assert(label(HARMONY_MODE_MINOR, weak, COUNT_OF(weak), PAC, &labels));
    expect(&labels, 0, -1, 2, 3);
}

static void test_the_phrase_must_end_as_the_cadence_says(void) {
    HarmonyPhraseLabels labels;
    labels.chord_count = 99;
    const HarmonyChord plagal[] = {I, IV, I};
    const HarmonyChord ends_on_dominant[] = {I, IV, V};
    const HarmonyChord ends_on_tonic[] = {I, IV, V, I};
    const HarmonyChord ends_on_six_four[] = {I, IV, CADENTIAL_CHORD};
    const HarmonyChord leading_tone[] = {I, IV, LEADING_TONE_CHORD, I};
    const HarmonyChord single_tonic[] = {I};
    assert(!label(HARMONY_MODE_MAJOR, plagal, COUNT_OF(plagal), PAC, &labels));
    assert(!label(HARMONY_MODE_MAJOR, ends_on_dominant, COUNT_OF(ends_on_dominant), PAC, &labels));
    assert(!label(HARMONY_MODE_MAJOR, ends_on_tonic, COUNT_OF(ends_on_tonic), HC, &labels));
    assert(!label(HARMONY_MODE_MAJOR, plagal, COUNT_OF(plagal), HC, &labels));
    assert(!label(HARMONY_MODE_MAJOR, ends_on_six_four, COUNT_OF(ends_on_six_four), HC, &labels));
    assert(!label(HARMONY_MODE_MAJOR, leading_tone, COUNT_OF(leading_tone), PAC, &labels));
    assert(!label(HARMONY_MODE_MAJOR, single_tonic, COUNT_OF(single_tonic), PAC, &labels));
    assert(labels.chord_count == 99); /* untouched */
}

static void test_a_subverted_cadence_is_not_labeled(void) {
    HarmonyPhraseLabels labels;
    const HarmonyChord phrase[] = {I, IV, V, I};
    assert(!label(HARMONY_MODE_MAJOR, phrase, COUNT_OF(phrase), CADENCE_TYPE_SUBVERTED, &labels));
    assert(!label(HARMONY_MODE_MAJOR, phrase, COUNT_OF(phrase), CADENCE_TYPE_COUNT, &labels));
}

static void test_invalid_input(void) {
    HarmonyPhraseLabels labels;
    labels.chord_count = 99;
    const HarmonyChord ok[] = {I, IV, V, I};
    const HarmonyChord bad_chord[] = {I, CH(9), V, I};
    HarmonyChord many[HARMONY_PHRASE_LABELS_MAX + 1];
    for (size_t i = 0; i < COUNT_OF(many); i++) {
        many[i] = I;
    }
    many[COUNT_OF(many) - 2] = V;
    HarmonyPhrase phrase = {HARMONY_MODE_COUNT, ok, COUNT_OF(ok)};
    assert(!harmony_phrase_model_label(&phrase, PAC, &labels));
    assert(!label(HARMONY_MODE_MAJOR, bad_chord, COUNT_OF(bad_chord), PAC, &labels));
    assert(!label(HARMONY_MODE_MAJOR, many, COUNT_OF(many), PAC, &labels));
    assert(!label(HARMONY_MODE_MAJOR, ok, COUNT_OF(ok), PAC, NULL));
    assert(!harmony_phrase_model_label(NULL, PAC, &labels));
    assert(labels.chord_count == 99);

    /* index_of: a missing or invalid label, or no labels, is -1. */
    HarmonyPhraseLabels good;
    assert(label(HARMONY_MODE_MAJOR, ok, COUNT_OF(ok), PAC, &good));
    assert(harmony_phrase_labels_index_of(&good, PHRASE_MODEL_LABEL_COUNT) == -1);
    assert(harmony_phrase_labels_index_of(NULL, D) == -1);
}

int main(void) {
    test_authentic_cadence();
    test_half_cadence_has_no_tonic_ending();
    test_pd_is_the_first_strong_predominant_of_the_run();
    test_only_the_run_before_the_dominant_counts();
    test_a_plagal_iv_before_tonic_gets_no_label();
    test_la_in_the_bass_labels();
    test_the_mediant_is_a_weak_predominant();
    test_cadential_six_four_gets_no_label();
    test_dominant_seventh();
    test_tonic_beginning_and_prolongation();
    test_minor();
    test_the_phrase_must_end_as_the_cadence_says();
    test_a_subverted_cadence_is_not_labeled();
    test_invalid_input();
    printf("test_harmony_phrase_labels: ok\n");
    return 0;
}
