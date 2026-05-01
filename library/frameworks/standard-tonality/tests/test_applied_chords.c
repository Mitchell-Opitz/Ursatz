#include <math.h>

#include "adapter_test_util.h"
#include "applied_chords.h"
#include "roman_numeral_adapter.h"

/* Applied chords: the diatonic reading is re-checked against V/x, V7/x, vii-diminished/x and the fully
   diminished seventh, and relabelled only when the chromatic evidence and the resolution are there. */

#define C_MAJOR STANDARD_TONALITY_ROMAN_KEY_MAJOR
#define A_MINOR STANDARD_TONALITY_ROMAN_KEY_MINOR

static StandardTonalityChordInput make_input(const char *id, int root, int bass, const int *intervals, size_t count,
                                             int tonic, StandardTonalityRomanKeyMode mode) {
    StandardTonalityChordInput input;
    memset(&input, 0, sizeof(input));
    input.segment_id = id;
    input.has_numeral = true;
    input.root_pitch_class = root;
    input.bass_pitch_class = bass;
    input.interval_count = count;
    for (size_t i = 0; i < count; i++) {
        input.intervals[i] = intervals[i];
    }
    input.tonic_pitch_class = tonic;
    input.mode = mode;
    return input;
}

static void set_weights(StandardTonalityChordInput *input, const int *pitch_classes, size_t count) {
    input->has_weights = true;
    for (size_t i = 0; i < count; i++) {
        input->weights[pitch_classes[i]] = 1.0;
    }
}

/* Two segments: `first` (with weights) then `second`. Returns the first's result in *out. */
static StandardTonalityRomanResult *run(StandardTonalityChordInput *inputs, size_t count,
                                        const StandardTonalityAppliedConfig *config) {
    StandardTonalityRomanResult *results = NULL;
    assert(standard_tonality_build_roman_numerals_with_config(inputs, count, config, &results) ==
           STANDARD_TONALITY_ROMAN_OK);
    return results;
}

static void test_v_of_v_resolving_to_v_is_relabelled(void) {
    /* D F# A over the ii the chord identification gives (F# a leftover), then G major. */
    StandardTonalityChordInput inputs[2];
    inputs[0] = make_input("s1", 2, 2, ADAPTER_MINOR_TRIAD, 3, 0, C_MAJOR);
    int d_fs_a[3] = {2, 6, 9};
    set_weights(&inputs[0], d_fs_a, 3);
    inputs[1] = make_input("s2", 7, 7, ADAPTER_MAJOR_TRIAD, 3, 0, C_MAJOR);

    StandardTonalityRomanResult *results = run(inputs, 2, NULL);
    assert(results[0].has_numeral && strcmp(results[0].claim.label, "V/V") == 0);
    assert(results[0].reading.applied && results[0].reading.applied_kind == APPLIED_CHORD_KIND_SECONDARY_DOMINANT);
    assert(results[0].claim.applied && results[0].claim.applied_target_degree == 5 && results[0].claim.degree == 5);
    assert(results[0].claim.applied_kind == (int)APPLIED_CHORD_KIND_SECONDARY_DOMINANT);
    assert(!results[0].deceptive && results[0].resolution_index == 1);

    /* The link to the resolution segment is a Relationship, predicate "resolves-to". */
    assert(results[0].resolves_to != NULL);
    assert(strcmp(relationship_predicate_name(results[0].resolves_to), "resolves-to") == 0);
    assert(strcmp(entity_id_value(relationship_source_id(results[0].resolves_to)), "s1") == 0);
    assert(strcmp(entity_id_value(relationship_target_id(results[0].resolves_to)), "s2") == 0);

    /* The resolution itself is the plain V and carries no link. */
    assert(strcmp(results[1].claim.label, "V") == 0 && results[1].resolves_to == NULL);

    StandardTonalityRomanClaim claim;
    assert(standard_tonality_roman_claim_read(results[0].interpretation, &claim) && claim.applied);
    standard_tonality_roman_results_destroy(results, 2);
}

static void test_same_chromatic_chord_not_resolving_keeps_diatonic_label(void) {
    StandardTonalityChordInput inputs[2];
    inputs[0] = make_input("s1", 2, 2, ADAPTER_MINOR_TRIAD, 3, 0, C_MAJOR);
    int d_fs_a[3] = {2, 6, 9};
    set_weights(&inputs[0], d_fs_a, 3);
    inputs[1] = make_input("s2", 0, 0, ADAPTER_MAJOR_TRIAD, 3, 0, C_MAJOR); /* to C: not G */

    StandardTonalityRomanResult *results = run(inputs, 2, NULL);
    assert(strcmp(results[0].claim.label, "ii") == 0 && !results[0].reading.applied && results[0].resolves_to == NULL);
    standard_tonality_roman_results_destroy(results, 2);

    /* The last segment has nothing to resolve to. */
    results = run(inputs, 1, NULL);
    assert(strcmp(results[0].claim.label, "ii") == 0);
    standard_tonality_roman_results_destroy(results, 1);
}

static void test_seventh_and_inversion(void) {
    StandardTonalityChordInput inputs[2];
    inputs[0] = make_input("s1", 2, 6, ADAPTER_MINOR_TRIAD, 3, 0, C_MAJOR); /* read as ii, F# in the bass */
    int d_fs_a_c[4] = {2, 6, 9, 0};
    set_weights(&inputs[0], d_fs_a_c, 4);
    inputs[1] = make_input("s2", 7, 7, ADAPTER_MAJOR_TRIAD, 3, 0, C_MAJOR);
    StandardTonalityRomanResult *results = run(inputs, 2, NULL);
    assert(strcmp(results[0].claim.label, "V6/5/V") == 0 && results[0].claim.seventh && results[0].claim.inversion == 1);
    standard_tonality_roman_results_destroy(results, 2);

    inputs[0].bass_pitch_class = 2;
    results = run(inputs, 2, NULL);
    assert(strcmp(results[0].claim.label, "V7/V") == 0);
    standard_tonality_roman_results_destroy(results, 2);
}

static void test_leading_tone_chord_of_v(void) {
    StandardTonalityChordInput inputs[2];
    inputs[0] = make_input("s1", 9, 6, ADAPTER_MINOR_TRIAD, 3, 0, C_MAJOR); /* F# A C over a read vi */
    int fs_a_c[3] = {6, 9, 0};
    set_weights(&inputs[0], fs_a_c, 3);
    inputs[1] = make_input("s2", 7, 7, ADAPTER_MAJOR_TRIAD, 3, 0, C_MAJOR);
    StandardTonalityRomanResult *results = run(inputs, 2, NULL);
    assert(strcmp(results[0].claim.label, "vii\xC2\xB0/V") == 0);
    assert(results[0].reading.applied_kind == APPLIED_CHORD_KIND_SECONDARY_LEADING_TONE);
    standard_tonality_roman_results_destroy(results, 2);

    inputs[0].bass_pitch_class = 9; /* A in the bass: first inversion */
    results = run(inputs, 2, NULL);
    assert(strcmp(results[0].claim.label, "vii\xC2\xB0" "6/V") == 0);
    standard_tonality_roman_results_destroy(results, 2);

    /* With the Ab: a fully diminished seventh. */
    StandardTonalityChordInput seventh[2];
    seventh[0] = make_input("s1", 9, 6, ADAPTER_MINOR_TRIAD, 3, 0, C_MAJOR);
    int fs_a_c_eb[4] = {6, 9, 0, 3};
    set_weights(&seventh[0], fs_a_c_eb, 4);
    seventh[1] = make_input("s2", 7, 7, ADAPTER_MAJOR_TRIAD, 3, 0, C_MAJOR);
    results = run(seventh, 2, NULL);
    assert(strcmp(results[0].claim.label, "vii\xC2\xB0" "7/V") == 0);
    standard_tonality_roman_results_destroy(results, 2);
}

static void test_deceptive_resolution(void) {
    StandardTonalityChordInput inputs[2];
    inputs[0] = make_input("s1", 2, 2, ADAPTER_MINOR_TRIAD, 3, 0, C_MAJOR);
    int d_fs_a[3] = {2, 6, 9};
    set_weights(&inputs[0], d_fs_a, 3);
    inputs[1] = make_input("s2", 4, 4, ADAPTER_MINOR_TRIAD, 3, 0, C_MAJOR); /* E minor: vi of G */
    StandardTonalityRomanResult *results = run(inputs, 2, NULL);
    assert(strcmp(results[0].claim.label, "V/V") == 0 && results[0].deceptive);
    standard_tonality_roman_results_destroy(results, 2);
}

static void test_thresholds_and_missing_weights(void) {
    StandardTonalityChordInput inputs[3];
    inputs[0] = make_input("s1", 2, 2, ADAPTER_MINOR_TRIAD, 3, 0, C_MAJOR);
    inputs[0].has_weights = true;
    inputs[0].weights[2] = 1.0;
    inputs[0].weights[5] = 1.0;
    inputs[0].weights[9] = 1.0;
    inputs[0].weights[6] = 0.1; /* a trace of F#: a chromatic share of about 3% */
    inputs[1] = make_input("s2", 7, 7, ADAPTER_MAJOR_TRIAD, 3, 0, C_MAJOR);
    StandardTonalityRomanResult *results = run(inputs, 2, NULL);
    assert(strcmp(results[0].claim.label, "ii") == 0);
    standard_tonality_roman_results_destroy(results, 2);

    /* A raised chromatic_min keeps the diatonic label of the clear V/V; no weights cannot be re-checked. */
    int d_fs_a[3] = {2, 6, 9};
    memset(inputs[0].weights, 0, sizeof(inputs[0].weights));
    set_weights(&inputs[0], d_fs_a, 3);
    StandardTonalityAppliedConfig strict = standard_tonality_applied_config_default();
    strict.chromatic_min = 0.5;
    results = run(inputs, 2, &strict);
    assert(strcmp(results[0].claim.label, "ii") == 0);
    standard_tonality_roman_results_destroy(results, 2);

    inputs[0].has_weights = false;
    results = run(inputs, 2, NULL);
    assert(strcmp(results[0].claim.label, "ii") == 0);
    standard_tonality_roman_results_destroy(results, 2);

    /* A repeat of the chord is looked through: V/V, V/V, V resolves from the first. */
    set_weights(&inputs[0], d_fs_a, 3);
    inputs[0].has_weights = true;
    inputs[2] = inputs[1];
    inputs[1] = inputs[0];
    inputs[1].segment_id = "s1b";
    inputs[2].segment_id = "s2";
    results = run(inputs, 3, NULL);
    assert(strcmp(results[0].claim.label, "V/V") == 0 && results[0].resolution_index == 2);
    standard_tonality_roman_results_destroy(results, 3);

    StandardTonalityAppliedConfig bad = standard_tonality_applied_config_default();
    bad.chromatic_min = -1.0;
    StandardTonalityRomanResult *none = NULL;
    assert(standard_tonality_build_roman_numerals_with_config(inputs, 3, &bad, &none) ==
           STANDARD_TONALITY_ROMAN_ERR_NULL_ARGUMENT);
}

static void test_minor_key_applied(void) {
    /* A minor: A7 (A C# E G) over a read i, resolving to D minor: V7/iv. */
    StandardTonalityChordInput inputs[2];
    inputs[0] = make_input("s1", 9, 9, ADAPTER_MINOR_TRIAD, 3, 9, A_MINOR);
    int a7[4] = {9, 1, 4, 7};
    set_weights(&inputs[0], a7, 4);
    inputs[1] = make_input("s2", 2, 2, ADAPTER_MINOR_TRIAD, 3, 9, A_MINOR);
    StandardTonalityRomanResult *results = run(inputs, 2, NULL);
    assert(strcmp(results[0].claim.label, "V7/iv") == 0 && results[0].claim.applied_target_degree == 4);
    standard_tonality_roman_results_destroy(results, 2);

    /* B D F (B diminished, ii) with E: the V in A minor is not a target of itself; V/V is B D# F# resolving to E. */
    inputs[0] = make_input("s1", 11, 11, ADAPTER_MINOR_TRIAD, 3, 9, A_MINOR);
    int b_ds_fs[3] = {11, 3, 6};
    set_weights(&inputs[0], b_ds_fs, 3);
    inputs[1] = make_input("s2", 4, 4, ADAPTER_MAJOR_TRIAD, 3, 9, A_MINOR);
    results = run(inputs, 2, NULL);
    /* B D F# is a diatonic chord of A melodic minor (its ii, on the raised sixth), so the segment has a reading to
       re-check; with the D# sounding and E following, it is the V of V. */
    assert(results[0].has_numeral && results[0].reading.applied && strcmp(results[0].claim.label, "V/V") == 0);
    standard_tonality_roman_results_destroy(results, 2);
}

int main(void) {
    test_v_of_v_resolving_to_v_is_relabelled();
    test_same_chromatic_chord_not_resolving_keeps_diatonic_label();
    test_seventh_and_inversion();
    test_leading_tone_chord_of_v();
    test_deceptive_resolution();
    test_thresholds_and_missing_weights();
    test_minor_key_applied();
    printf("standard-tonality applied chords: all tests passed\n");
    return 0;
}
