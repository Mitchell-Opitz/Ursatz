#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/given.h>
#include <ursatz_composer/grow.h>
#include <ursatz_composer/melody_fill.h>
#include <ursatz_composer/motive.h>
#include <ursatz_composer/preferences.h>
#include <plan/plan_state.h>

#include "melody/melody_internal.h"
#include "support/test_knowledge.h"
#include "support/test_support.h"
#include "structure/plan_util.h"
#include "ursatz_test.h"

#ifndef COMPOSER_FIXTURE_DIR
#error "COMPOSER_FIXTURE_DIR is not defined"
#endif

/*
 * The overrides of the melody stage (R-672): waive names hard rules the stage does not hold, reverse names soft groups
 * whose score counts what a candidate breaks.
 *   (a) waive [] and reverse [] read from a document give the same melodies as the defaults, on the sentence of
 *       test_melody_preferences;
 *   (b) a melody with parallel fifths against the bass is refused by default and kept with parallel-fifths waived;
 *   (c) with a leaps weight of 5, a stepwise and a leaping melody swap their order when leaps is reversed.
 * (b) and (c) judge hand-made candidates over a two-bar plan in 4/4 in C major: bars of whole notes over a bass.
 */

#define CHORDS 9
#define SEEDS 3
#define BEAT_TICKS 4L

/* ------------------------------------------------------------ (a) the sentence */

static Motive load_motive(void) {
    char *text = test_must_read_file(COMPOSER_FIXTURE_DIR "/motifs/test-motif-6-8.json", NULL);
    Motive motive;
    MotiveError error;
    memset(&error, 0, sizeof(error));
    assert(text);
    assert(motive_from_json(text, test_motive_options(), &motive, &error) == MOTIVE_OK);
    free(text);
    return motive;
}

static Plan *compose(const Motive *motive, unsigned long seed, const MelodyFillParams *melody) {
    static const char *const NUMERALS[CHORDS] = {"I", "vi", "IV", "V", "I", "vi", "IV", "V", "I"};
    static const long long STARTS[CHORDS] = {0, 6, 12, 18, 24, 30, 36, 39, 42};
    static const long long LENGTHS[CHORDS] = {6, 6, 6, 6, 6, 6, 3, 3, 6};
    GivenChord chords[CHORDS];
    for (size_t i = 0; i < CHORDS; i++) {
        chords[i].numeral = NUMERALS[i];
        chords[i].onset = rational_ratio(STARTS[i], 8);
        chords[i].length = rational_ratio(LENGTHS[i], 8);
    }
    GrowRequest request;
    memset(&request, 0, sizeof(request));
    request.motive = motive;
    request.key = test_key(0, "major");
    request.meter.beats_per_bar = 6;
    request.meter.beat_unit = 8;
    request.bars = 8;
    request.pairing.beginning = SECTION_BEGINNING_PRESENTATION;
    request.pairing.ending = SECTION_ENDING_CONTINUATION;
    request.cadence = CADENCE_TYPE_PERFECT_AUTHENTIC;
    request.chords = chords;
    request.chord_count = CHORDS;
    Plan *plan = NULL;
    GrowOutcome outcome;
    GrowConfig config;
    memset(&config, 0, sizeof(config));
    config.knowledge = test_knowledge();
    config.seed = seed;
    config.basic = motive;
    config.melody = melody;
    assert(grow_build_plan_checked(&request, &plan, NULL) == GROW_OK);
    GrowResult result = grow_run(plan, &config, &outcome);
    assert(result == GROW_OK && !outcome.stuck);
    grow_outcome_destroy(&outcome);
    return plan;
}

static uint64_t mix(uint64_t hash, long long value) {
    hash ^= (uint64_t)value;
    return hash * 1099511628211ull;
}

static uint64_t melody_hash(const Plan *plan) {
    uint64_t hash = 14695981039346656037ull;
    for (NodeId node = 1; node <= plan_node_count(plan); node++) {
        LaneSpanId span;
        if (!plan_melody_span_of(plan, node, &span)) {
            continue;
        }
        const PlanNotes *notes = &plan_lane_span_payload(plan, span)->notes;
        hash = mix(hash, (long long)node);
        for (size_t n = 0; n < notes->note_count; n++) {
            hash = mix(hash, notes->notes[n].midi);
            hash = mix(hash, notes->notes[n].offset.num);
            hash = mix(hash, notes->notes[n].offset.den);
            hash = mix(hash, notes->notes[n].duration.num);
            hash = mix(hash, notes->notes[n].duration.den);
        }
    }
    return hash;
}

static void explicit_empty_lists_change_nothing(void) {
    ComposerPreferences preferences;
    char path[COMPOSER_PREFERENCES_PATH_MAX];
    char message[COMPOSER_PREFERENCES_MESSAGE_MAX];
    cJSON *document = cJSON_Parse("{\"melody\":{\"waive\":[],\"reverse\":[]}}");
    assert(document && composer_preferences_read(document, &preferences, path, sizeof(path), message, sizeof(message)));
    cJSON_Delete(document);
    assert(preferences.melody.waive == 0 && preferences.melody.reverse == 0);
    Motive motive = load_motive();
    for (unsigned long seed = 1; seed <= SEEDS; seed++) {
        Plan *plain = compose(&motive, seed, NULL);
        Plan *explicit_lists = compose(&motive, seed, &preferences.melody);
        assert(melody_hash(plain) == melody_hash(explicit_lists));
        plan_destroy(plain);
        plan_destroy(explicit_lists);
    }
}

/* ------------------------------------------------------------ (b), (c) hand-made candidates */

typedef struct {
    const char *numeral;
    long start; /* in beats */
    int bass;
} Bar;

static Rational beats(long count) {
    return rational_ratio(count, 4);
}

static LaneSpanId add_notes(Plan *plan, NodeId owner, PlanLaneKind lane, PlanSpan span, int pitch, PlanState state) {
    PlanNote note;
    PlanLanePayload payload;
    LaneSpanId id;
    memset(&payload, 0, sizeof(payload));
    memset(&note, 0, sizeof(note));
    note.midi = pitch;
    note.offset = beats(0);
    note.duration = span.length;
    payload.notes.notes = &note;
    payload.notes.note_count = 1;
    assert(plan_add_lane_span(plan, owner, lane, 0, span, &payload, &id) == PLAN_OK);
    if (state != PLAN_STATE_CONSTRAINED) {
        assert(plan_lane_span_set_state(plan, id, state) == PLAN_OK);
    }
    return id;
}

/* The context of a two-bar melody target over two bars of whole-note bass, read from a plan. */
static void read_context(const Bar bars[2], MelodyContext *context, Plan **plan_out) {
    PlanSpan form = {beats(0), beats(16)};
    PlanSpan section = {beats(0), beats(8)};
    PlanLanePayload payload;
    LaneSpanId id;
    NodeId form_node;
    NodeId owner;
    Plan *plan = plan_create();
    assert(plan_add_node(plan, PLAN_NODE_NONE, PLAN_NODE_FORM, form, "form", &form_node) == PLAN_OK);
    assert(plan_add_node(plan, form_node, PLAN_NODE_SECTION, section, "section", &owner) == PLAN_OK);
    memset(&payload, 0, sizeof(payload));
    payload.meter.beats_per_bar = 4;
    payload.meter.beat_unit = 4;
    assert(plan_add_lane_span(plan, form_node, PLAN_LANE_METER, 0, form, &payload, &id) == PLAN_OK);
    assert(plan_lane_span_set_state(plan, id, PLAN_STATE_GIVEN) == PLAN_OK);
    memset(&payload, 0, sizeof(payload));
    payload.region = test_key(0, "major");
    assert(plan_add_lane_span(plan, form_node, PLAN_LANE_REGION, 0, form, &payload, &id) == PLAN_OK);
    assert(plan_lane_span_set_state(plan, id, PLAN_STATE_GIVEN) == PLAN_OK);
    for (size_t i = 0; i < 2; i++) {
        PlanSpan span = {beats(bars[i].start), beats(4)};
        memset(&payload, 0, sizeof(payload));
        strcpy(payload.harmony.roman_numeral, bars[i].numeral);
        assert(plan_add_lane_span(plan, owner, PLAN_LANE_HARMONY, 0, span, &payload, &id) == PLAN_OK);
        assert(plan_lane_span_set_state(plan, id, PLAN_STATE_REALIZED) == PLAN_OK);
        add_notes(plan, owner, PLAN_LANE_BASS, span, bars[i].bass, PLAN_STATE_REALIZED);
    }
    assert(plan_node_set_state(plan, owner, PLAN_STATE_REALIZED) == PLAN_OK);
    assert(plan_node_set_state(plan, form_node, PLAN_STATE_GIVEN) == PLAN_OK);
    memset(context, 0, sizeof(*context));
    assert(melody_phrase_read(plan, owner, section, test_key(0, "major"), test_composer_chords(), context));
    *plan_out = plan;
}

/* Two whole notes, one a bar, on the two chords of the context. */
static MelodyCandidate *candidate(const MelodyContext *context, int first, int second, const MelodyFillParams *params) {
    MelodyCandidate *melody = calloc(1, sizeof(*melody));
    assert(melody);
    melody->magic = MELODY_PAYLOAD_MAGIC;
    melody->context = *context;
    melody->params = *params;
    melody->count = 2;
    const int pitches[2] = {first, second};
    for (size_t i = 0; i < 2; i++) {
        melody->notes[i].onset = beats((long)i * 4);
        melody->notes[i].duration = beats(4);
        melody->notes[i].pitch = pitches[i];
        melody->notes[i].motive_index = -1;
        melody->notes[i].strong = true;
        melody->notes[i].chord = i;
    }
    return melody;
}

static const char *judge_hard(const MelodyCandidate *melody) {
    PhraseVerdict verdict;
    assert(melody_judge(melody, &verdict));
    return verdict.hard;
}

/* a/b against c/d: the sign of a/b - c/d, from integers. */
static int compare_scores(unsigned held_a, unsigned total_a, unsigned held_b, unsigned total_b) {
    unsigned long left = (unsigned long)held_a * total_b;
    unsigned long right = (unsigned long)held_b * total_a;
    return left > right ? 1 : left < right ? -1 : 0;
}

static void judge_score(const MelodyCandidate *melody, unsigned *held, unsigned *total) {
    PhraseVerdict verdict;
    assert(melody_judge(melody, &verdict) && !verdict.hard);
    *held = verdict.soft_held;
    *total = verdict.soft_total;
}

static void parallel_fifths_are_waivable(void) {
    /* C3 then D3 under G4 then A4: a fifth and a fifth, both voices moving up by step. */
    const Bar bars[2] = {{"I", 0, 48}, {"ii", 4, 50}};
    MelodyContext context;
    Plan *plan;
    read_context(bars, &context, &plan);
    MelodyFillParams params = melody_fill_params_default();
    MelodyCandidate *melody = candidate(&context, 67, 69, &params);
    const char *refused = judge_hard(melody);
    assert(refused && strcmp(refused, "parallel-fifths") == 0);

    /* Waived, it is kept: the same melody scored, nothing else changed. */
    params.waive = 1u << MELODY_RULE_PARALLEL_FIFTHS;
    melody->params = params;
    assert(judge_hard(melody) == NULL);
    /* Another rule waived leaves it refused; every rule waived keeps it. */
    params.waive = 1u << MELODY_RULE_PARALLEL_OCTAVES;
    melody->params = params;
    assert(judge_hard(melody) != NULL);
    params.waive = (1u << MELODY_RULE_COUNT) - 1u;
    melody->params = params;
    assert(judge_hard(melody) == NULL);
    free(melody);
    plan_destroy(plan);
}

static void reversing_leaps_swaps_the_order(void) {
    /* Both melodies are chord tones of I over a held C3; one moves by a third, the other by a fifth. */
    const Bar bars[2] = {{"I", 0, 48}, {"I", 4, 48}};
    MelodyContext context;
    Plan *plan;
    read_context(bars, &context, &plan);
    MelodyFillParams params = melody_fill_params_default();
    params.leap_weight = 5;
    MelodyCandidate *step = candidate(&context, 64, 67, &params);
    MelodyCandidate *leap = candidate(&context, 60, 67, &params);
    unsigned step_held;
    unsigned step_total;
    unsigned leap_held;
    unsigned leap_total;
    judge_score(step, &step_held, &step_total);
    judge_score(leap, &leap_held, &leap_total);
    assert(compare_scores(step_held, step_total, leap_held, leap_total) > 0);

    params.reverse = 1u << MELODY_GROUP_LEAPS;
    step->params = params;
    leap->params = params;
    judge_score(step, &step_held, &step_total);
    judge_score(leap, &leap_held, &leap_total);
    assert(compare_scores(step_held, step_total, leap_held, leap_total) < 0);
    /* Reversing a group with no weight counts nothing: against_bass reversed leaves the totals as they were. */
    params.reverse = 0;
    step->params = params;
    unsigned before_held;
    unsigned before_total;
    judge_score(step, &before_held, &before_total);
    params.reverse = 1u << MELODY_GROUP_SEAM;
    step->params = params;
    judge_score(step, &step_held, &step_total);
    assert(step_total == before_total && step_held == before_held);
    free(step);
    free(leap);
    plan_destroy(plan);
}

int main(void) {
    explicit_empty_lists_change_nothing();
    parallel_fifths_are_waivable();
    reversing_leaps_swaps_the_order();
    printf("test_melody_overrides: ok\n");
    return 0;
}
