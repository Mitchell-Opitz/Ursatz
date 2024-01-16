#include <math.h>

#include "analysis_resolution.h"
#include "analysis_test_util.h"
#include "identity/entity_id.h"
#include "pitch/tuning_system.h"

/* Seventh and leading tone resolution on separated voices. The fixture's segments are quarter notes; each test gives a
   chord per segment (degree, intervals, inversion) and the four voices' MIDI pitches, top voice first. */

#define VOICES 4

typedef struct {
    HarmonyVoiceCell cells[FIXTURE_MAX * VOICES];
    PitchNumericValue *values[FIXTURE_MAX * VOICES];
    char ids[FIXTURE_MAX * VOICES][16];
    HarmonyVoiceGrid grid;
    TuningSystem *tuning;
} Voices;

static void voices_init(Voices *v, size_t segments, const int rows[][VOICES]) {
    memset(v, 0, sizeof(*v));
    EntityID *id = NULL;
    assert(entity_id_create("midi-tuning", &id) == ENTITY_ID_OK);
    assert(tuning_system_create(id, "MIDI note number", &v->tuning) == TUNING_SYSTEM_OK);
    for (size_t s = 0; s < segments; s++) {
        for (size_t voice = 0; voice < VOICES; voice++) {
            size_t i = s * VOICES + voice;
            if (rows[s][voice] == 0) {
                continue;
            }
            assert(pitch_numeric_value_create(v->tuning, rows[s][voice], 1, &v->values[i]) == PITCH_NUMERIC_VALUE_OK);
            v->cells[i].present = true;
            v->cells[i].pitch = v->values[i];
            v->cells[i].confidence = 1.0;
            v->cells[i].attacked = true;
            snprintf(v->ids[i], sizeof(v->ids[i]), "n%zu-%zu", s, voice);
            v->cells[i].note_id = v->ids[i];
        }
    }
    v->grid.segment_count = segments;
    v->grid.voice_count = VOICES;
    v->grid.cells = v->cells;
}

static void voices_destroy(Voices *v) {
    for (size_t i = 0; i < FIXTURE_MAX * VOICES; i++) {
        pitch_numeric_value_destroy(v->values[i]);
    }
    tuning_system_destroy(v->tuning);
}

/* V7 then I, in C major: the V7 over G in the bass (root position) then the I over C. */
static HarmonyResolutions run(Fixture *f, Voices *v, const HarmonyResolutionConfig *config, HarmonySegment **segments) {
    assert(harmony_analysis_prepare(f->inputs, f->count, segments) == HARMONY_ANALYSIS_OK);
    HarmonyResolutions found;
    assert(harmony_analysis_check_resolutions(*segments, f->count, &v->grid, config, &found) == HARMONY_RESOLUTION_OK);
    return found;
}

static void v7_then_i(Fixture *f) {
    fixture_init(f);
    fixture_add(f, 5, SEVENTH_DOMINANT, 4, 0, 5, 7);
    fixture_add(f, 1, TRIAD_MAJOR, 3, 0, 1, 1);
}

static void test_v7_resolving_correctly_gives_no_violation(void) {
    Fixture f;
    v7_then_i(&f);
    /* S B4 (ti) A F4 (fa) T D4 B G3 -> S C5, A E4, T C4, B C3 */
    const int rows[][VOICES] = {{71, 65, 62, 55}, {72, 64, 60, 48}};
    Voices v;
    voices_init(&v, 2, rows);
    HarmonySegment *segments = NULL;
    HarmonyResolutions found = run(&f, &v, NULL, &segments);
    assert(found.count == 0 && found.items == NULL);
    harmony_resolutions_destroy(&found);

    /* with the debug flag the two satisfied resolutions are output */
    HarmonyResolutionConfig debug = harmony_resolution_config_default();
    debug.emit_satisfied = true;
    found = run(&f, &v, &debug, &segments);
    assert(found.count == 2);
    for (size_t i = 0; i < found.count; i++) {
        assert(found.items[i].satisfied);
    }
    harmony_resolutions_destroy(&found);
    harmony_analysis_destroy(segments);
    voices_destroy(&v);
    fixture_destroy(&f);
}

static void test_an_unresolved_seventh_is_flagged(void) {
    Fixture f;
    v7_then_i(&f);
    const int rows[][VOICES] = {{71, 65, 62, 55}, {72, 65, 60, 48}}; /* the alto holds its F over the C chord */
    Voices v;
    voices_init(&v, 2, rows);
    HarmonySegment *segments = NULL;
    HarmonyResolutions found = run(&f, &v, NULL, &segments);
    assert(found.count == 1);
    const HarmonyResolutionResult *r = &found.items[0];
    assert(r->rule == HARMONY_RESOLUTION_SEVENTH_RESOLVES_DOWN && r->voice == 1 && !r->satisfied);
    assert(r->segment_from == 0 && r->segment_to == 1);
    assert(structure_event_id_count(r->shell) == 2);
    assert(strcmp(entity_id_value(interpretation_target_id(r->interpretation)),
                  entity_id_value(entity_id(structure_entity(r->shell)))) == 0);
    assert(interpretation_framework_reference(r->interpretation) != NULL);
    HarmonyResolutionClaim claim;
    assert(harmony_resolution_claim_read(r->interpretation, &claim));
    assert(strcmp(claim.rule_name, "seventh-resolves-down") == 0 && claim.voice == 1 && !claim.satisfied);
    harmony_resolutions_destroy(&found);
    harmony_analysis_destroy(segments);
    voices_destroy(&v);
    fixture_destroy(&f);
}

static void test_the_leading_tone_in_the_soprano(void) {
    Fixture f;
    v7_then_i(&f);
    const int falling[][VOICES] = {{71, 65, 62, 55}, {67, 64, 60, 48}}; /* ti falls to sol in the soprano */
    Voices v;
    voices_init(&v, 2, falling);
    HarmonySegment *segments = NULL;
    HarmonyResolutions found = run(&f, &v, NULL, &segments);
    assert(found.count == 1 && found.items[0].rule == HARMONY_RESOLUTION_LEADING_TONE_RESOLVES &&
           found.items[0].voice == 0);
    harmony_resolutions_destroy(&found);
    harmony_analysis_destroy(segments);
    voices_destroy(&v);
    fixture_destroy(&f);
}

static void test_the_leading_tone_in_an_inner_voice(void) {
    Fixture f;
    v7_then_i(&f);
    /* a complete V7: S D5 (fifth), A F4 (seventh), T B3 (ti), B G3; the tenor drops to sol: allowed */
    const int drop[][VOICES] = {{74, 65, 59, 55}, {72, 64, 55, 48}};
    Voices v;
    voices_init(&v, 2, drop);
    HarmonySegment *segments = NULL;
    HarmonyResolutions found = run(&f, &v, NULL, &segments);
    assert(found.count == 0);
    harmony_resolutions_destroy(&found);
    harmony_analysis_destroy(segments);
    voices_destroy(&v);

    /* an incomplete V7 (no fifth): the same drop is not allowed */
    const int incomplete[][VOICES] = {{67, 65, 59, 55}, {67, 64, 55, 48}};
    voices_init(&v, 2, incomplete);
    found = run(&f, &v, NULL, &segments);
    assert(found.count == 1 && found.items[0].rule == HARMONY_RESOLUTION_LEADING_TONE_RESOLVES &&
           found.items[0].voice == 2);
    harmony_resolutions_destroy(&found);
    harmony_analysis_destroy(segments);
    voices_destroy(&v);

    /* rising to do in the inner voice is fine */
    const int rising[][VOICES] = {{74, 65, 59, 55}, {72, 64, 60, 48}};
    voices_init(&v, 2, rising);
    found = run(&f, &v, NULL, &segments);
    assert(found.count == 0);
    harmony_resolutions_destroy(&found);
    harmony_analysis_destroy(segments);
    voices_destroy(&v);
    fixture_destroy(&f);
}

static void test_the_leading_tone_chord_and_non_tonic_resolutions(void) {
    /* vii-diminished (B D F) to I: the root is the leading tone; the soprano rises, the bass too */
    Fixture f;
    fixture_init(&f);
    fixture_add(&f, 7, TRIAD_DIM, 3, 0, 7, 7);
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 0, 1, 1);
    const int rows[][VOICES] = {{83, 74, 65, 59}, {84, 76, 67, 60}};
    Voices v;
    voices_init(&v, 2, rows);
    HarmonySegment *segments = NULL;
    HarmonyResolutions found = run(&f, &v, NULL, &segments);
    assert(found.count == 0);
    harmony_resolutions_destroy(&found);
    harmony_analysis_destroy(segments);
    voices_destroy(&v);
    fixture_destroy(&f);

    /* V to vi: the leading tone is not required to rise (the next chord is not a tonic) */
    fixture_init(&f);
    fixture_add(&f, 5, TRIAD_MAJOR, 3, 0, 5, 7);
    fixture_add(&f, 6, TRIAD_MINOR, 3, 0, 6, 1);
    const int deceptive[][VOICES] = {{71, 67, 62, 55}, {69, 64, 60, 57}};
    voices_init(&v, 2, deceptive);
    found = run(&f, &v, NULL, &segments);
    assert(found.count == 0);
    harmony_resolutions_destroy(&found);
    harmony_analysis_destroy(segments);
    voices_destroy(&v);
    fixture_destroy(&f);
}

static void test_the_same_chord_over_several_segments_is_judged_where_it_leaves(void) {
    Fixture f;
    fixture_init(&f);
    fixture_add(&f, 5, SEVENTH_DOMINANT, 4, 0, 5, 7);
    fixture_add(&f, 5, SEVENTH_DOMINANT, 4, 0, 5, 7); /* the V7 again: the same chord */
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 0, 1, 1);
    const int rows[][VOICES] = {{71, 65, 62, 55}, {71, 65, 62, 55}, {72, 65, 60, 48}};
    Voices v;
    voices_init(&v, 3, rows);
    HarmonySegment *segments = NULL;
    HarmonyResolutions found = run(&f, &v, NULL, &segments);
    assert(found.count == 1 && found.items[0].segment_from == 1 && found.items[0].segment_to == 2);
    harmony_resolutions_destroy(&found);
    harmony_analysis_destroy(segments);
    voices_destroy(&v);
    fixture_destroy(&f);
}

static void test_uncertainty_and_bad_grids(void) {
    Fixture f;
    v7_then_i(&f);
    f.inputs[0].uncertainty = 0.2;
    f.inputs[1].uncertainty = 0.1;
    const int rows[][VOICES] = {{71, 65, 62, 55}, {72, 65, 60, 48}};
    Voices v;
    voices_init(&v, 2, rows);
    v.cells[1].confidence = 0.5; /* the alto's F */
    HarmonySegment *segments = NULL;
    HarmonyResolutions found = run(&f, &v, NULL, &segments);
    assert(found.count == 1);
    /* 1 - 0.5 * (1 - 0.2) */
    assert(fabs(found.items[0].uncertainty - 0.6) < 1e-12);
    harmony_resolutions_destroy(&found);

    HarmonyVoiceGrid wrong = v.grid;
    wrong.segment_count = 5;
    assert(harmony_analysis_check_resolutions(segments, f.count, &wrong, NULL, &found) ==
           HARMONY_RESOLUTION_ERR_GRID_MISMATCH);
    harmony_analysis_destroy(segments);
    voices_destroy(&v);
    fixture_destroy(&f);
}

int main(void) {
    test_v7_resolving_correctly_gives_no_violation();
    test_an_unresolved_seventh_is_flagged();
    test_the_leading_tone_in_the_soprano();
    test_the_leading_tone_in_an_inner_voice();
    test_the_leading_tone_chord_and_non_tonic_resolutions();
    test_the_same_chord_over_several_segments_is_judged_where_it_leaves();
    test_uncertainty_and_bad_grids();
    printf("standard-harmony analysis resolution: all tests passed\n");
    return 0;
}
