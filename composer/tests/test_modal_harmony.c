#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <plan/plan_constraint.h>
#include <plan/plan_state.h>
#include <ursatz_composer/fill_loop.h>
#include <ursatz_composer/harmony_phrase.h>

#include "harmony/harmony_internal.h"
#include "support/test_knowledge.h"
#include "util/util.h"

/*
 * The harmony layer in the church modes (R-725): the triads stacked from the material, the numerals relative to it, the modal
 * phrase and its cadence, and what a modal key refuses. A phrase is one harmony span over a meter and a key lane, as in
 * test_harmony_phrase; the harmony operators fill it with the checker.
 */

#define MAX_CHORDS 16
#define SEEDS 12

typedef struct {
    const char *word;
    const char *qualities; /* M major, m minor, d diminished, on the degrees 1 to 7 */
    const char *pac_end[2]; /* the numerals the PAC ends with */
    const char *hc_end;     /* the numeral a half cadence ends on */
    int forbidden[2];       /* pitch classes the mode does not have on C (-1 for none) */
} Mode;

static const Mode MODES[] = {
    {"lydian", "MMmdMmm", {"II", "I"}, "II", {5, 10}},
    {"mixolydian", "MmdMmmM", {"VII", "I"}, "VII", {11, 4 + 8}},
    {"dorian", "mmMMmdM", {"IV", "i"}, "IV", {11, 8}},
    {"phrygian", "mMMmdMm", {"II", "i"}, "II", {2, 9}},
    {"locrian", "dMmmMMm", {"V6", "io"}, "V", {7, 2}},
};
#define MODE_COUNT (sizeof(MODES) / sizeof(MODES[0]))

static Rational fraction(long num, unsigned long den) {
    Rational time;
    assert(rational_make(num, den, &time) == RATIONAL_OK);
    return time;
}

static const char *quality_text(StandardTonalityTriadQuality quality) {
    return quality == STANDARD_TONALITY_TRIAD_MAJOR ? "M" : quality == STANDARD_TONALITY_TRIAD_MINOR ? "m" : quality == STANDARD_TONALITY_TRIAD_DIMINISHED ? "d" : "?";
}

static void test_stacked_triads(void) {
    for (size_t m = 0; m < MODE_COUNT; m++) {
        const PitchMaterial *material = test_material(MODES[m].word);
        assert(pitch_material_is_modal_key(&test_composer_chords()->materials, material));
        for (int degree = 1; degree <= 7; degree++) {
            int root;
            int intervals[3];
            StandardTonalityTriadQuality quality;
            assert(pitch_material_triad_on(material, degree, &root, intervals, &quality));
            assert(root == material->steps[degree - 1]);
            assert(intervals[0] == 0);
            assert(strncmp(quality_text(quality), &MODES[m].qualities[degree - 1], 1) == 0);
        }
        int root;
        assert(!pitch_material_triad_on(material, 0, &root, NULL, NULL));
        assert(!pitch_material_triad_on(material, 8, &root, NULL, NULL));
    }
    /* Lydian on C: C D e f#o G a b; dorian: c d Eb F g ao Bb; locrian: co Db eb f Gb Ab bb. */
    int root;
    int intervals[3];
    assert(pitch_material_triad_on(test_material("lydian"), 4, &root, intervals, NULL) && root == 6 && intervals[1] == 3 && intervals[2] == 6);
    assert(pitch_material_triad_on(test_material("dorian"), 3, &root, intervals, NULL) && root == 3 && intervals[1] == 4 && intervals[2] == 7);
    assert(pitch_material_triad_on(test_material("locrian"), 1, &root, intervals, NULL) && root == 0 && intervals[1] == 3 && intervals[2] == 6);
    /* The major and the natural minor are not modal keys. */
    assert(!pitch_material_is_modal_key(&test_composer_chords()->materials, test_material("major")));
    assert(!pitch_material_is_modal_key(&test_composer_chords()->materials, test_material("minor")));
    assert(!pitch_material_is_modal_key(&test_composer_chords()->materials, NULL));
}

static void test_numerals(void) {
    const ComposerChords *chords = test_composer_chords();
    HarmonyChord chord;
    char roman[PLAN_ROMAN_NUMERAL_MAX];
    SpelledKey dorian = test_key(0, "dorian");
    SpelledKey mixolydian = test_key(0, "mixolydian");
    SpelledKey locrian = test_key(0, "locrian");
    HarmonyKind kind = harmony_kind(chords, &dorian);
    assert(kind.modal == test_material("dorian") && kind.cadence != NULL && kind.cadence->approach_count == 1 && kind.cadence->approach_degrees[0] == 4);
    assert(harmony_roman_parse_in(&kind, "IV", &chord) && chord.degree == 4);
    assert(harmony_roman_format_in(&kind, &chord, roman) && strcmp(roman, "IV") == 0);
    assert(harmony_roman_parse_in(&kind, "i", &chord) && chord.degree == 1);
    assert(harmony_roman_parse_in(&kind, "ii", &chord) && chord.degree == 2);
    /* Another quality than the stacked triad's is a borrowed chord (B6): V from the major, iv from the natural minor. */
    assert(harmony_roman_parse_in(&kind, "V", &chord) && chord.degree == 5 && chord.borrowed_from == test_material("major"));
    assert(harmony_roman_parse_in(&kind, "iv", &chord) && chord.degree == 4 && chord.borrowed_from == test_material("minor"));
    assert(harmony_roman_parse_in(&kind, "vio", &chord) && pitch_material_avoids(kind.modal, chord.degree)); /* readable, never written */
    assert(harmony_roman_parse_in(&kind, "bII", &chord) && chord.degree == 2 && chord.borrowed_from == test_material("phrygian"));
    assert(!harmony_roman_parse_in(&kind, "V7", &chord));
    assert(!harmony_roman_parse_in(&kind, "V/V", &chord));
    kind = harmony_kind(chords, &mixolydian);
    assert(harmony_roman_parse_in(&kind, "v", &chord) && chord.degree == 5);
    assert(harmony_roman_parse_in(&kind, "VII", &chord) && chord.degree == 7 && !chord.leading_tone);
    assert(!harmony_roman_parse_in(&kind, "viio", &chord));
    kind = harmony_kind(chords, &locrian);
    assert(harmony_roman_parse_in(&kind, "io", &chord) && chord.degree == 1);
    assert(harmony_roman_format_in(&kind, &chord, roman) && strcmp(roman, "io") == 0);
    assert(!harmony_roman_parse_in(&kind, "i", &chord));
    /* A major key reads as before. */
    SpelledKey major = test_key(0, "major");
    kind = harmony_kind(chords, &major);
    assert(kind.modal == NULL && harmony_roman_parse_in(&kind, "V", &chord) && chord.borrowed_from == NULL);
    assert(harmony_roman_parse_in(&kind, "v", &chord) && chord.borrowed_from == test_material("mixolydian")); /* borrowed (B6), from the nearest mode */
}

static void test_functions(void) {
    const ComposerChords *chords = test_composer_chords();
    SpelledKey dorian = test_key(0, "dorian");
    HarmonyKind kind = harmony_kind(chords, &dorian);
    HarmonicFunction function;
    HarmonyChord chord;
    memset(&chord, 0, sizeof(chord));
    chord.degree = 1;
    assert(harmony_function_in(&kind, &chord, &function) && function == HARMONIC_FUNCTION_TONIC);
    chord.degree = 4;
    assert(harmony_function_in(&kind, &chord, &function) && function == HARMONIC_FUNCTION_DOMINANT);
    chord.degree = 2;
    assert(harmony_function_in(&kind, &chord, &function) && function == HARMONIC_FUNCTION_PREDOMINANT);
    chord.degree = 6; /* the avoided vio */
    assert(!harmony_function_in(&kind, &chord, &function));
    chord.degree = 4;
    chord.seventh = true;
    assert(!harmony_function_in(&kind, &chord, &function));
}

static Plan *phrase_plan(const SpelledKey *key, int bars, CadenceType cadence, LaneSpanId *target, PlanSpan *span) {
    Plan *plan = plan_create();
    NodeId owner;
    PlanLanePayload payload;
    LaneSpanId id;
    span->start = fraction(0, 1);
    span->length = fraction(bars, 1);
    assert(plan_add_node(plan, PLAN_NODE_NONE, PLAN_NODE_FORM, *span, NULL, &owner) == PLAN_OK);
    assert(plan_node_set_state(plan, owner, PLAN_STATE_GIVEN) == PLAN_OK);
    memset(&payload, 0, sizeof(payload));
    payload.meter.beats_per_bar = 4;
    payload.meter.beat_unit = 4;
    assert(plan_add_lane_span(plan, owner, PLAN_LANE_METER, 0, *span, &payload, &id) == PLAN_OK);
    assert(plan_lane_span_set_state(plan, id, PLAN_STATE_GIVEN) == PLAN_OK);
    memset(&payload, 0, sizeof(payload));
    payload.region = *key;
    assert(plan_add_lane_span(plan, owner, PLAN_LANE_REGION, 0, *span, &payload, &id) == PLAN_OK);
    assert(plan_lane_span_set_state(plan, id, PLAN_STATE_GIVEN) == PLAN_OK);
    memset(&payload, 0, sizeof(payload));
    strcpy(payload.harmony.roman_numeral, "I");
    assert(plan_add_lane_span(plan, owner, PLAN_LANE_HARMONY, 0, *span, &payload, target) == PLAN_OK);
    PlanConstraint constraint = {PLAN_CONSTRAINT_CADENCE, {0}};
    constraint.params.cadence = cadence;
    assert(plan_lane_span_add_constraint(plan, *target, &constraint) == PLAN_OK);
    return plan;
}

static bool run_phrase(Plan *plan, unsigned long seed) {
    FillOperatorRegistry *registry = fill_registry_create();
    FillConfig config;
    FillOutcome outcome;
    memset(&config, 0, sizeof(config));
    config.seed = seed;
    config.checker = harmony_phrase_checker;
    assert(harmony_phrase_register(registry, test_composer_chords()) == FILL_OK);
    assert(fill_run(plan, registry, &config, &outcome, NULL) == FILL_OK);
    fill_registry_destroy(registry);
    bool complete = outcome.status == FILL_STATUS_COMPLETE;
    fill_outcome_destroy(&outcome);
    return complete;
}

/* Checks the written chords of one phrase: each numeral parses back in the key, every pitch class is in the mode, none is avoided. */
static size_t check_phrase(const Plan *plan, PlanSpan span, const SpelledKey *key, const Mode *mode, const char **numerals) {
    const ComposerChords *chords = test_composer_chords();
    HarmonyKind kind = harmony_kind(chords, key);
    LaneSpanId ids[MAX_CHORDS];
    size_t count = plan_lane_find(plan, PLAN_LANE_HARMONY, 0, span, ids, MAX_CHORDS);
    assert(count >= 2 && count <= MAX_CHORDS);
    for (size_t i = 0; i < count; i++) {
        const PlanHarmony *harmony = &plan_lane_span_payload(plan, ids[i])->harmony;
        HarmonyChord chord;
        int root_pc;
        int intervals[4];
        size_t interval_count;
        assert(plan_lane_span_state(plan, ids[i]) == PLAN_STATE_REALIZED);
        assert(harmony_roman_parse_in(&kind, harmony->roman_numeral, &chord));
        assert(!pitch_material_avoids(kind.modal, chord.degree));
        assert(composer_chord_tones(chords, key, &chord, &root_pc, intervals, &interval_count));
        for (size_t k = 0; k < interval_count; k++) {
            int pc = (root_pc + intervals[k]) % 12;
            assert(pitch_material_contains(kind.modal, pc));
            assert(pc != mode->forbidden[0] && pc != mode->forbidden[1]);
        }
        numerals[i] = harmony->roman_numeral;
        assert(harmony->has_function);
    }
    return count;
}

static void test_phrases(void) {
    for (size_t m = 0; m < MODE_COUNT; m++) {
        SpelledKey key = test_key(0, MODES[m].word);
        for (unsigned long seed = 1; seed <= SEEDS; seed++) {
            for (int half = 0; half < 2; half++) {
                LaneSpanId target;
                PlanSpan span;
                const char *numerals[MAX_CHORDS];
                Plan *plan = phrase_plan(&key, 8, half ? CADENCE_TYPE_HALF : CADENCE_TYPE_PERFECT_AUTHENTIC, &target, &span);
                assert(run_phrase(plan, seed));
                size_t count = check_phrase(plan, span, &key, &MODES[m], numerals);
                assert(MODES[m].qualities[0] != 'd' ? numerals[0][0] != 'v' || numerals[0][0] == MODES[m].pac_end[1][0] : true);
                if (half) {
                    assert(strcmp(numerals[count - 1], MODES[m].hc_end) == 0);
                } else {
                    assert(strcmp(numerals[count - 2], MODES[m].pac_end[0]) == 0);
                    assert(strcmp(numerals[count - 1], MODES[m].pac_end[1]) == 0);
                }
                plan_destroy(plan);
            }
        }
    }
}

static void test_refusals(void) {
    const ComposerChords *chords = test_composer_chords();
    SpelledKey dorian = test_key(0, "dorian");
    char message[160];
    assert(harmony_cadence_allowed(chords, &dorian, CADENCE_TYPE_PERFECT_AUTHENTIC, message, sizeof(message)));
    assert(harmony_cadence_allowed(chords, &dorian, CADENCE_TYPE_HALF, message, sizeof(message)));
    assert(!harmony_cadence_allowed(chords, &dorian, CADENCE_TYPE_DECEPTIVE, message, sizeof(message)));
    assert(strcmp(message, "the dorian cadence is the only one written in C dorian; DC and PC need a dominant") == 0);
    assert(!harmony_cadence_allowed(chords, &dorian, CADENCE_TYPE_PLAGAL, message, sizeof(message)));
    SpelledKey major = test_key(0, "major");
    assert(harmony_cadence_allowed(chords, &major, CADENCE_TYPE_DECEPTIVE, message, sizeof(message)));

    /* A deceptive cadence asked of a C dorian phrase: the operator does not apply. */
    LaneSpanId target;
    PlanSpan span;
    PhraseRequest request;
    Plan *plan = phrase_plan(&dorian, 8, CADENCE_TYPE_DECEPTIVE, &target, &span);
    assert(!harmony_phrase_read_request(plan, target, chords, &request));
    plan_destroy(plan);
    plan = phrase_plan(&dorian, 8, CADENCE_TYPE_PERFECT_AUTHENTIC, &target, &span);
    assert(harmony_phrase_read_request(plan, target, chords, &request) && request.modal == test_material("dorian"));
    plan_destroy(plan);
}

int main(void) {
    test_stacked_triads();
    test_numerals();
    test_functions();
    test_phrases();
    test_refusals();
    printf("test_modal_harmony passed\n");
    return 0;
}
