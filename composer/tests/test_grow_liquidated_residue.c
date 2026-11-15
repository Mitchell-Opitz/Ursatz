#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/grow.h>
#include <ursatz_composer/lanes.h>
#include <ursatz_composer/motive.h>
#include <ursatz_composer/motive_analysis.h>
#include <ursatz_composer/plan_state.h>

#include "api/api_grow.h"
#include "support/test_support.h"

#ifndef COMPOSER_FIXTURE_DIR
#error "COMPOSER_FIXTURE_DIR is not defined"
#endif

/*
 * Liquidated residues (B3 of P36, R-345, R-350). For the Test Motif and the one-bar 4/4 motif as 8-bar sentences, the
 * two-bar 3/4 motif as an 8-bar period, and the one-bar 4/4 and two-bar 3/4 motifs as 4-bar sentences, seeds 1 to 10:
 * every cadential idea's motive ref has steps and a segment; the idea's realized melody opens with that segment's first
 * two durations (or both halved) and its first interval in scale steps; no note of it is shorter than half the motif's
 * shortest; its last note is the tonic (PAC). Unit cases: the one-bar 4/4 motif's residue is its head (E 1/4, G 1/8) and a
 * C close; a motif whose segments are each one note composes no cadential ending.
 */

#define SEEDS 10

typedef struct {
    const char *name;
    const char *file;
    size_t bars;
    PlanThemeBeginning beginning;
    PlanThemeEnding ending;
} Case;

static const Case CASES[] = {
    {"test motif sentence", COMPOSER_FIXTURE_DIR "/motifs/test-motif-6-8.json", 8, PLAN_THEME_BEGINNING_PRESENTATION,
     PLAN_THEME_ENDING_CONTINUATION},
    {"two-bar 3/4 period", COMPOSER_FIXTURE_DIR "/rates/two-bar-3-4.json", 8, PLAN_THEME_BEGINNING_ANTECEDENT,
     PLAN_THEME_ENDING_CONSEQUENT},
    {"one-bar 4/4 sentence", COMPOSER_FIXTURE_DIR "/rates/one-bar-4-4.json", 8, PLAN_THEME_BEGINNING_PRESENTATION,
     PLAN_THEME_ENDING_CONTINUATION},
    {"one-bar 4/4 4-bar sentence", COMPOSER_FIXTURE_DIR "/rates/one-bar-4-4.json", 4, PLAN_THEME_BEGINNING_PRESENTATION,
     PLAN_THEME_ENDING_CONTINUATION},
    {"two-bar 3/4 4-bar sentence", COMPOSER_FIXTURE_DIR "/rates/two-bar-3-4.json", 4, PLAN_THEME_BEGINNING_PRESENTATION,
     PLAN_THEME_ENDING_CONTINUATION},
};

static Motive load_motive(const char *path) {
    char *text = test_read_file(path, NULL);
    Motive motive;
    assert(motive_from_json(text, NULL, &motive, NULL) == MOTIVE_OK);
    free(text);
    return motive;
}

static NodeId find_node(const PiecePlan *plan, const char *name) {
    for (NodeId id = 1; id <= plan_node_count(plan); id++) {
        const char *found = plan_node_name(plan, id);
        if (found && strcmp(found, name) == 0) {
            return id;
        }
    }
    return PLAN_NODE_NONE;
}

static const PlanMotiveRef *ref_of(const PiecePlan *plan, NodeId node) {
    for (size_t i = 0; i < plan_node_constraint_count(plan, node); i++) {
        const PlanConstraint *constraint = plan_node_constraint_at(plan, node, i);
        if (constraint->kind == PLAN_CONSTRAINT_MOTIVE_REF) {
            return &constraint->params.motive;
        }
    }
    return NULL;
}

/* The melody notes a node owns (its melody span), or 0. */
static size_t melody_of(const PiecePlan *plan, NodeId node, const PlanNote **notes) {
    LaneSpanId spans[8];
    size_t count = plan_node_lane_spans(plan, node, spans, 8);
    for (size_t i = 0; i < count && i < 8; i++) {
        if (plan_lane_span_lane(plan, spans[i]) == PLAN_LANE_MELODY) {
            *notes = plan_lane_span_payload(plan, spans[i])->notes.notes;
            return plan_lane_span_payload(plan, spans[i])->notes.note_count;
        }
    }
    return 0;
}

/* The scale position of a pitch: octave * 7 + degree, a chromatic pitch on the degree below (the raised 7th of minor on 7). */
static long scale_position(int pitch, int tonic, bool minor) {
    static const int MAJOR[7] = {0, 2, 4, 5, 7, 9, 11};
    static const int MINOR[7] = {0, 2, 3, 5, 7, 8, 10};
    int from = pitch - tonic;
    long octave = from >= 0 ? from / 12 : -((-from + 11) / 12);
    int pc = from - (int)octave * 12;
    int degree = 0;
    for (int d = 0; d < 7; d++) {
        degree = (minor ? MINOR : MAJOR)[d] <= pc ? d : degree;
    }
    return octave * 7 + degree;
}

static const MotiveSketchFeature *segment_named(const MotiveAnalysis *analysis, const char *label) {
    for (size_t s = 0; s < analysis->segment_count; s++) {
        if (strcmp(analysis->segments[s].label, label) == 0) {
            return &analysis->segments[s];
        }
    }
    return NULL;
}

static bool same_durations(const PlanNote *notes, const Motive *motive, size_t first, bool halved) {
    for (size_t i = 0; i < 2; i++) {
        Rational want = motive->notes[first + i].duration;
        if (halved) {
            want.den *= 2;
        }
        if (rational_cmp(notes[i].duration, want) != 0) {
            return false;
        }
    }
    return true;
}

/* Checks one cadential idea of a composed theme; the motive node is m<slot>. */
static void check_idea(const PiecePlan *plan, const Motive *motive, const MotiveAnalysis *analysis, const char *idea,
                       const char *motive_name, int tonic, bool minor, Rational shortest) {
    NodeId node = find_node(plan, motive_name);
    NodeId idea_node = find_node(plan, idea);
    assert(node != PLAN_NODE_NONE && idea_node != PLAN_NODE_NONE);
    const PlanMotiveRef *ref = ref_of(plan, node);
    assert(ref && ref->steps >= 1 && ref->segment[0] != '\0');
    const MotiveSketchFeature *segment = segment_named(analysis, ref->segment);
    assert(segment && segment->last > segment->first);
    const PlanNote *notes = NULL;
    size_t count = melody_of(plan, idea_node, &notes);
    assert(count >= 2);
    assert(same_durations(notes, motive, segment->first, false) || same_durations(notes, motive, segment->first, true));
    long head = scale_position(motive->notes[segment->first + 1].pitch, tonic, minor) -
                scale_position(motive->notes[segment->first].pitch, tonic, minor);
    assert(scale_position(notes[1].midi, tonic, minor) - scale_position(notes[0].midi, tonic, minor) == head);
    for (size_t i = 0; i < count; i++) {
        Rational doubled = notes[i].duration;
        doubled.num *= 2;
        assert(rational_cmp(doubled, shortest) >= 0);
    }
    assert(((notes[count - 1].midi - tonic) % 12 + 12) % 12 == 0);
}

static void compose(const Case *test, unsigned long seed, GrowRun *run, Motive *motive) {
    static GrowBriefText text;
    *motive = load_motive(test->file);
    AnalysisKey key = motive_key(motive);
    memset(&text, 0, sizeof(text));
    text.cadence = "PAC";
    GrowSetup setup;
    memset(&setup, 0, sizeof(setup));
    setup.text = &text;
    setup.into = test->beginning == PLAN_THEME_BEGINNING_ANTECEDENT ? GROW_INTO_PERIOD : GROW_INTO_HYBRID;
    setup.pairing.beginning = test->beginning;
    setup.pairing.ending = test->ending;
    setup.key = key;
    setup.meter = motive_meter(motive);
    setup.bars = test->bars;
    setup.cadence = CADENCE_TYPE_PERFECT_AUTHENTIC;
    setup.first_cadence = CADENCE_TYPE_HALF;
    setup.motive = motive;
    setup.suggest = 1;
    setup.development = GROW_DEVELOP_BALANCED;
    setup.seed = seed;
    ComposerError error;
    memset(&error, 0, sizeof(error));
    ComposerStatus status = api_grow_run(&setup, run, &error);
    if (status != COMPOSER_OK || run->made != 1) {
        fprintf(stderr, "%s seed %lu: no theme\n", test->name, seed);
    }
    assert(status == COMPOSER_OK && run->made == 1);
}

static void test_cadential_ideas(void) {
    for (size_t c = 0; c < sizeof(CASES) / sizeof(CASES[0]); c++) {
        const Case *test = &CASES[c];
        size_t ideas = 0;
        for (unsigned long seed = 1; seed <= SEEDS; seed++) {
            GrowRun run;
            Motive motive;
            compose(test, seed, &run, &motive);
            MotiveAnalysis analysis;
            assert(motive_analyze(&motive, &analysis));
            Rational shortest = motive.notes[0].duration;
            for (size_t i = 1; i < motive.note_count; i++) {
                shortest = rational_cmp(motive.notes[i].duration, shortest) < 0 ? motive.notes[i].duration : shortest;
            }
            AnalysisKey key = motive_key(&motive);
            int tonic = analysis_key_tonic_pitch_class(&key);
            bool minor = key.mode == KEY_STRUCTURE_MODE_MINOR;
            const PiecePlan *plan = run.themes[0].plan;
            if (test->ending == PLAN_THEME_ENDING_CONTINUATION) {
                check_idea(plan, &motive, &analysis, "cadential", "m4", tonic, minor, shortest);
                ideas++;
            } else if (test->ending == PLAN_THEME_ENDING_CADENTIAL) {
                check_idea(plan, &motive, &analysis, "cadential 1", "m2", tonic, minor, shortest);
                check_idea(plan, &motive, &analysis, "cadential 2", "m3", tonic, minor, shortest);
                ideas += 2;
            } else {
                /* A period has no cadential idea: nothing is liquidated. */
                assert(find_node(plan, "cadential") == PLAN_NODE_NONE);
            }
            api_grow_run_destroy(&run);
        }
        printf("%s: %zu cadential ideas keep a segment over %d seeds\n", test->name, ideas, SEEDS);
    }
}

/* The one-bar 4/4 motif (E q, G e, C e, B h, one segment) keeps its head and closes on C: a head residue (R-350). */
static void test_head_residue(void) {
    GrowRun run;
    Motive motive;
    compose(&CASES[2], 1, &run, &motive);
    const PiecePlan *plan = run.themes[0].plan;
    NodeId node = find_node(plan, "liquidated 3");
    assert(node != PLAN_NODE_NONE);
    const PlanNote *notes = NULL;
    assert(melody_of(plan, node, &notes) == 3);
    assert(notes[0].midi == 64 && rational_cmp(notes[0].duration, rational_ratio(1, 4)) == 0);
    assert(notes[1].midi == 67 && rational_cmp(notes[1].duration, rational_ratio(1, 8)) == 0);
    assert(notes[2].midi % 12 == 0);
    api_grow_run_destroy(&run);
}

/* A motif whose segments are each one long note has no residue: no seed composes a cadential ending. */
static void test_no_residue(void) {
    static GrowBriefText text;
    Motive motive;
    MotiveOptions options;
    memset(&options, 0, sizeof(options));
    options.has_meter = true;
    options.meter.beats_per_bar = 4;
    options.meter.beat_unit = 4;
    options.has_features = true;
    options.feature_count = 2;
    strcpy(options.features[0].label, "a");
    options.features[0].first = 0;
    options.features[0].last = 0;
    strcpy(options.features[1].label, "b");
    options.features[1].first = 1;
    options.features[1].last = 1;
    assert(motive_from_notes_text("C5:h G4:h", &options, &motive, NULL) == MOTIVE_OK);
    AnalysisKey key = plan_key_of_pitch_class(0, KEY_STRUCTURE_MODE_MAJOR);
    motive_set_key(&motive, &key, MOTIVE_SKETCH_KEY_GIVEN);
    memset(&text, 0, sizeof(text));
    text.cadence = "PAC";
    GrowSetup setup;
    memset(&setup, 0, sizeof(setup));
    setup.text = &text;
    setup.into = GROW_INTO_HYBRID;
    setup.pairing.beginning = PLAN_THEME_BEGINNING_PRESENTATION;
    setup.pairing.ending = PLAN_THEME_ENDING_CADENTIAL;
    setup.key = key;
    setup.meter = options.meter;
    setup.bars = 8;
    setup.cadence = CADENCE_TYPE_PERFECT_AUTHENTIC;
    setup.first_cadence = CADENCE_TYPE_HALF;
    setup.motive = &motive;
    setup.suggest = 1;
    setup.development = GROW_DEVELOP_BALANCED;
    setup.seed = 1;
    GrowRun run;
    ComposerError error;
    memset(&error, 0, sizeof(error));
    ComposerStatus status = api_grow_run(&setup, &run, &error);
    assert(status != COMPOSER_OK || run.made == 0);
    api_grow_run_destroy(&run);
}

int main(void) {
    test_cadential_ideas();
    test_head_residue();
    test_no_residue();
    return 0;
}
