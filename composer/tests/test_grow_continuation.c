#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>
#include <ursatz_composer/grow.h>
#include <ursatz_composer/motive.h>
#include <ursatz_composer/plan_json.h>
#include <ursatz_composer/plan_state.h>

#include "api/api_grow.h"
#include "support/test_support.h"

#ifndef COMPOSER_FIXTURE_DIR
#error "COMPOSER_FIXTURE_DIR is not defined"
#endif

/*
 * The accelerating continuation (B4 and B5 of L3, R-462 to R-466): s77 (C major) and u51 (C minor), two bars of 4/4 each, in
 * an 8-bar sentence; the -chords fixtures are the same motifs with chords of their own, for the basic idea as written. Every setting is off by default; each is turned on through GrowContext.continuation and its effect read
 * in the plan: the segments the fragments name, their cut or diminution, the liquidated fragment 2 and the cadential idea.
 * No argument runs every case but the compose rates; "rates" runs the rates (slow); "fast" runs seed 1 of every case, the
 * rates included (test_grow_continuation_smoke).
 */

#define SEEDS 8

typedef struct {
    const char *name;
    Motive motive;
} Fixture;

static bool fast_run = false;
static Fixture FIXTURES[2];
static Fixture CHORD_FIXTURES[2]; /* the same motifs with harmony of their own */
static Motive TRIPLE_MOTIVE;      /* two bars of 3/4 in A minor with harmony of its own */

static unsigned long last_seed(void) {
    return fast_run ? 1 : SEEDS;
}

static Motive load_motive(const char *path) {
    char *text = test_read_file(path, NULL);
    Motive motive;
    MotiveError error;
    memset(&error, 0, sizeof(error));
    MotiveResult result = motive_from_json(text, NULL, &motive, &error);
    if (result != MOTIVE_OK) {
        fprintf(stderr, "%s: cannot read the motif at %s: %s\n", path, error.location, error.message);
    }
    assert(result == MOTIVE_OK);
    free(text);
    return motive;
}

static void load_fixtures(void) {
    FIXTURES[0].name = "s77";
    FIXTURES[0].motive = load_motive(COMPOSER_FIXTURE_DIR "/continuation/s77.json");
    FIXTURES[1].name = "u51";
    FIXTURES[1].motive = load_motive(COMPOSER_FIXTURE_DIR "/continuation/u51.json");
    CHORD_FIXTURES[0].name = "s77";
    CHORD_FIXTURES[0].motive = load_motive(COMPOSER_FIXTURE_DIR "/continuation/s77-chords.json");
    CHORD_FIXTURES[1].name = "u51";
    CHORD_FIXTURES[1].motive = load_motive(COMPOSER_FIXTURE_DIR "/continuation/u51-chords.json");
    TRIPLE_MOTIVE = load_motive(COMPOSER_FIXTURE_DIR "/rates/two-bar-3-4.json");
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

/* The 8-bar sentence of the motive and seed under the continuation settings (NULL for none) and the basic idea as written
   or not: the plan after grow_run. */
static PiecePlan *grow_with(const Motive *motive, unsigned long seed, const GrowContinuation *continuation,
                            bool as_written) {
    GrowRequest request;
    GrowConfig config;
    GrowOutcome outcome;
    PiecePlan *plan = NULL;
    memset(&request, 0, sizeof(request));
    request.motive = motive;
    request.key = motive_key(motive);
    request.meter = motive_meter(motive);
    request.bars = 8;
    request.pairing.beginning = PLAN_THEME_BEGINNING_PRESENTATION;
    request.pairing.ending = PLAN_THEME_ENDING_CONTINUATION;
    request.cadence = CADENCE_TYPE_PERFECT_AUTHENTIC;
    memset(&config, 0, sizeof(config));
    config.seed = seed;
    config.basic = motive;
    config.context.basic_as_written = as_written;
    if (continuation) {
        config.context.continuation = *continuation;
    }
    assert(grow_build_plan(&request, &plan) == GROW_OK);
    assert(grow_run(plan, &config, &outcome) == GROW_OK);
    grow_outcome_destroy(&outcome);
    return plan;
}

static PiecePlan *grow(const Motive *motive, unsigned long seed, const GrowContinuation *continuation) {
    return grow_with(motive, seed, continuation, false);
}

/* The motive_ref of the node `name`. */
static const PlanMotiveRef *ref_of(const PiecePlan *plan, const char *name) {
    NodeId node = find_node(plan, name);
    assert(node != PLAN_NODE_NONE);
    for (size_t i = 0; i < plan_node_constraint_count(plan, node); i++) {
        const PlanConstraint *constraint = plan_node_constraint_at(plan, node, i);
        if (constraint->kind == PLAN_CONSTRAINT_MOTIVE_REF) {
            return &constraint->params.motive;
        }
    }
    assert(!"the node has a motive_ref");
    return NULL;
}

/* The end of the notes of the motive node `name`, in whole notes. */
static Rational node_end(const PiecePlan *plan, const char *name) {
    LaneSpanId span = PLAN_LANE_SPAN_NONE;
    assert(plan_node_lane_spans(plan, find_node(plan, name), &span, 1) == 1);
    const PlanNotes *held = &plan_lane_span_payload(plan, span)->notes;
    assert(held->note_count > 0);
    const PlanNote *last = &held->notes[held->note_count - 1];
    Rational end;
    assert(rational_add(last->offset, last->duration, &end) == RATIONAL_OK);
    return end;
}

static bool end_is(const PiecePlan *plan, const char *name, long long num, long long den) {
    Rational want = {num, den};
    return rational_cmp(node_end(plan, name), want) == 0;
}

/* How many notes the segment of the motive has. */
static size_t segment_notes(const Motive *motive, const char *label) {
    for (size_t f = 0; f < motive->feature_count; f++) {
        if (strcmp(motive->features[f].label, label) == 0) {
            return motive->features[f].last - motive->features[f].first + 1;
        }
    }
    assert(!"the motif has the segment");
    return 0;
}

/* Fragment 1 and fragment 2 of the continuation: the nodes m2 and m3. */
static const char *const FRAGMENTS[2] = {"m2", "m3"};

static char *plan_text(const PiecePlan *plan) {
    PlanJsonError error;
    char *text = plan_to_json(plan, &error);
    assert(text);
    return text;
}

/* No setting changes nothing: a zero GrowContinuation, and fragment_lengths {1, 0, 0} with the rest off, are the plan of a
   context with no continuation set. */
static void test_defaults_change_nothing(void) {
    GrowContinuation zero;
    GrowContinuation whole;
    memset(&zero, 0, sizeof(zero));
    memset(&whole, 0, sizeof(whole));
    whole.fragment_lengths[GROW_FRAGMENT_SEGMENT] = 1;
    for (size_t f = 0; f < 2; f++) {
        for (unsigned long seed = 1; seed <= (fast_run ? 1 : 4); seed++) {
            const Motive *motives[2] = {&FIXTURES[f].motive, &CHORD_FIXTURES[f].motive};
            for (size_t m = 0; m < 2; m++) {
                /* faster_harmony and basic_as_written are set false explicitly: the plan is the same. */
                PiecePlan *plain = grow(motives[m], seed, NULL);
                PiecePlan *with_zero = grow_with(motives[m], seed, &zero, false);
                PiecePlan *with_whole = grow_with(motives[m], seed, &whole, false);
                char *a = plan_text(plain);
                char *b = plan_text(with_zero);
                char *c = plan_text(with_whole);
                assert(strcmp(a, b) == 0 && strcmp(a, c) == 0);
                free(a);
                free(b);
                free(c);
                plan_destroy(plain);
                plan_destroy(with_zero);
                plan_destroy(with_whole);
            }
        }
    }
}

/* With every_segment fragment 1 and fragment 2 name different segments, in every seed. */
static void test_every_segment_uses_both(void) {
    GrowContinuation every;
    memset(&every, 0, sizeof(every));
    every.every_segment = true;
    for (size_t f = 0; f < 2; f++) {
        size_t both_a_without = 0;
        for (unsigned long seed = 1; seed <= last_seed(); seed++) {
            PiecePlan *plan = grow(&FIXTURES[f].motive, seed, &every);
            assert(strcmp(ref_of(plan, "m2")->segment, ref_of(plan, "m3")->segment) != 0);
            plan_destroy(plan);
            plan = grow(&FIXTURES[f].motive, seed, NULL);
            both_a_without += strcmp(ref_of(plan, "m2")->segment, ref_of(plan, "m3")->segment) == 0 ? 1 : 0;
            plan_destroy(plan);
        }
        printf("%s: fragments from the same segment without every_segment: %zu of %lu\n", FIXTURES[f].name, both_a_without,
               last_seed());
        /* The default does draw both fragments from one segment here, so the setting is what changed it. */
        assert(both_a_without > 0);
    }
}

/* A cut fragment is its segment's leading notes: a `fragment` leading step of 2 notes or more and fewer than the segment's. */
static void check_cut(const PiecePlan *plan, const Motive *motive, const char *name) {
    const PlanMotiveRef *ref = ref_of(plan, name);
    assert(ref->has_step && ref->step.kind == MOTIVE_RELATION_FRAGMENT && ref->step.value[0] == MOTIVE_FRAGMENT_LEADING);
    assert(ref->step.value[1] >= 2 && (size_t)ref->step.value[1] < segment_notes(motive, ref->segment));
}

static void test_cut_half_bar(void) {
    GrowContinuation cut;
    memset(&cut, 0, sizeof(cut));
    cut.fragment_lengths[GROW_FRAGMENT_HALF_BAR] = 1;
    for (size_t f = 0; f < 2; f++) {
        for (unsigned long seed = 1; seed <= last_seed(); seed++) {
            PiecePlan *plan = grow(&FIXTURES[f].motive, seed, &cut);
            for (size_t k = 0; k < 2; k++) {
                check_cut(plan, &FIXTURES[f].motive, FRAGMENTS[k]);
                Rational half = {1, 2};
                assert(rational_cmp(node_end(plan, FRAGMENTS[k]), half) <= 0);
            }
            plan_destroy(plan);
        }
    }
}

/* A beat is a quarter: s77's segment b (G A G E ...) cuts to G A, its segment a (one note in a beat) falls back to half a
   bar; u51's segments are q. e h, so both fall back. */
static void test_cut_beat(void) {
    GrowContinuation cut;
    memset(&cut, 0, sizeof(cut));
    cut.fragment_lengths[GROW_FRAGMENT_BEAT] = 1;
    for (size_t f = 0; f < 2; f++) {
        for (unsigned long seed = 1; seed <= last_seed(); seed++) {
            PiecePlan *plan = grow(&FIXTURES[f].motive, seed, &cut);
            for (size_t k = 0; k < 2; k++) {
                check_cut(plan, &FIXTURES[f].motive, FRAGMENTS[k]);
                const PlanMotiveRef *ref = ref_of(plan, FRAGMENTS[k]);
                assert(ref->step.value[1] == 2);
                bool beat = f == 0 && strcmp(ref->segment, "b") == 0;
                assert(end_is(plan, FRAGMENTS[k], beat ? 1 : 1, beat ? 4 : 2));
            }
            plan_destroy(plan);
        }
    }
}

/* The melody notes of the bars [from, to) of the theme (0-based), in whole-note onsets of a 4/4 bar. */
static size_t melody_notes(const PiecePlan *plan, long from, long to) {
    NodeId theme = find_node(plan, "theme");
    LaneSpanId spans[64];
    size_t found = plan_lane_find(plan, PLAN_LANE_MELODY, 0, plan_node_span(plan, theme), spans, 64);
    size_t count = 0;
    for (size_t s = 0; s < found; s++) {
        PlanSpan span = plan_lane_span_span(plan, spans[s]);
        const PlanNotes *notes = &plan_lane_span_payload(plan, spans[s])->notes;
        for (size_t i = 0; i < notes->note_count; i++) {
            Rational onset;
            long long whole = 0;
            assert(rational_add(span.start, notes->notes[i].offset, &onset) == RATIONAL_OK);
            assert(rational_floor(onset, &whole) == RATIONAL_OK);
            count += whole >= from && whole < to ? 1 : 0;
        }
    }
    return count;
}

/* A whole-segment fragment is its segment in diminution 1:2, half its length, and the continuation's melody is busier. */
static void test_diminish_halves(void) {
    GrowContinuation diminish;
    memset(&diminish, 0, sizeof(diminish));
    diminish.diminish = true;
    for (size_t f = 0; f < 2; f++) {
        size_t before = 0;
        size_t after = 0;
        for (unsigned long seed = 1; seed <= last_seed(); seed++) {
            PiecePlan *plan = grow(&FIXTURES[f].motive, seed, &diminish);
            for (size_t k = 0; k < 2; k++) {
                const PlanMotiveRef *ref = ref_of(plan, FRAGMENTS[k]);
                assert(ref->has_step && ref->step.kind == MOTIVE_RELATION_DIMINISH);
                assert(end_is(plan, FRAGMENTS[k], 1, 2));
            }
            before += melody_notes(plan, 0, 2);
            after += melody_notes(plan, 4, 6);
            plan_destroy(plan);
        }
        printf("%s: melody notes a bar, bars 1 and 2 %.2f, bars 5 and 6 %.2f\n", FIXTURES[f].name,
               (double)before / (double)(2 * last_seed()), (double)after / (double)(2 * last_seed()));
        assert(after > before);
    }
}

/* Fragment 2 is a liquidation by segments and names a segment; the cadential idea liquidates at least as far. */
static void test_liquidate_through(void) {
    GrowContinuation liquidate;
    memset(&liquidate, 0, sizeof(liquidate));
    liquidate.liquidate = true;
    for (size_t f = 0; f < 2; f++) {
        for (unsigned long seed = 1; seed <= last_seed(); seed++) {
            PiecePlan *plan = grow(&FIXTURES[f].motive, seed, &liquidate);
            const PlanMotiveRef *second = ref_of(plan, "m3");
            const PlanMotiveRef *cadential = ref_of(plan, "m4");
            assert(second->steps >= 1 && second->segment[0] != '\0');
            assert(cadential->steps >= second->steps);
            plan_destroy(plan);
        }
    }
}

/* A theme response through api_grow_run: the development item of a cut fragment has minimum 0 (it departs from its segment,
   R-464). */
static void test_cut_departs(void) {
    GrowContinuation cut;
    memset(&cut, 0, sizeof(cut));
    cut.fragment_lengths[GROW_FRAGMENT_HALF_BAR] = 1;
    for (size_t f = 0; f < 2; f++) {
        GrowBriefText text;
        GrowSetup setup;
        memset(&text, 0, sizeof(text));
        text.key = f == 0 ? "C major" : "C minor";
        text.meter = "4/4";
        text.cadence = "PAC";
        text.into = "sentence";
        memset(&setup, 0, sizeof(setup));
        setup.text = &text;
        setup.into = GROW_INTO_SENTENCE;
        setup.pairing.beginning = PLAN_THEME_BEGINNING_PRESENTATION;
        setup.pairing.ending = PLAN_THEME_ENDING_CONTINUATION;
        setup.key = motive_key(&FIXTURES[f].motive);
        setup.meter = motive_meter(&FIXTURES[f].motive);
        setup.bars = 8;
        setup.cadence = CADENCE_TYPE_PERFECT_AUTHENTIC;
        setup.first_cadence = CADENCE_TYPE_HALF;
        setup.motive = &FIXTURES[f].motive;
        setup.suggest = 1;
        setup.seed = 1;
        setup.tempo_from = "none";
        setup.continuation = &cut;
        GrowRun run;
        ComposerError error;
        memset(&error, 0, sizeof(error));
        assert(api_grow_run(&setup, &run, &error) == COMPOSER_OK && run.made == 1);
        cJSON *document = api_grow_result_json(&setup, &run, NULL);
        assert(document);
        const cJSON *theme = cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(document, "themes"), 0);
        size_t cuts = 0;
        const cJSON *entry;
        cJSON_ArrayForEach(entry, cJSON_GetObjectItemCaseSensitive(theme, "developments")) {
            const cJSON *idea = cJSON_GetObjectItemCaseSensitive(entry, "idea");
            const cJSON *summary = cJSON_GetObjectItemCaseSensitive(entry, "summary");
            const cJSON *minimum = cJSON_GetObjectItemCaseSensitive(entry, "minimum");
            assert(cJSON_IsString(idea) && cJSON_IsString(summary));
            if (strncmp(idea->valuestring, "fragment", 8) == 0 && strstr(summary->valuestring, "fragmented") != NULL) {
                cuts++;
                assert(cJSON_IsNumber(minimum) && minimum->valuedouble == 0);
            }
        }
        assert(cuts == 2);
        cJSON_Delete(document);
        api_grow_run_destroy(&run);
    }
}

/* The chords of the theme in time order: onset (in whole notes, as a rational) and numeral. */
typedef struct {
    Rational start;
    char numeral[PLAN_ROMAN_NUMERAL_MAX];
} Chord;

static size_t chords_of(const PiecePlan *plan, Chord *out, size_t size) {
    NodeId theme = find_node(plan, "theme");
    LaneSpanId spans[64];
    size_t found = plan_lane_find(plan, PLAN_LANE_HARMONY, 0, plan_node_span(plan, theme), spans, 64);
    assert(found <= size);
    for (size_t i = 0; i < found; i++) {
        out[i].start = plan_lane_span_span(plan, spans[i]).start;
        snprintf(out[i].numeral, sizeof(out[i].numeral), "%s", plan_lane_span_payload(plan, spans[i])->harmony.roman_numeral);
    }
    /* In time order. */
    for (size_t i = 1; i < found; i++) {
        for (size_t j = i; j > 0 && rational_cmp(out[j - 1].start, out[j].start) > 0; j--) {
            Chord swap = out[j];
            out[j] = out[j - 1];
            out[j - 1] = swap;
        }
    }
    return found;
}

/* With faster_harmony the continuation of 4/4 changes chord on beats 1 and 3 of bars 5 to 7, no two neighbors the same, and
   it has more chord changes than the presentation; in 3/4 a bar is one cell and the plan is the plan without it. */
static void test_faster_harmony_changes_each_cell(void) {
    GrowContinuation faster;
    memset(&faster, 0, sizeof(faster));
    faster.faster_harmony = true;
    static const Rational ONSETS[6] = {{4, 1}, {9, 2}, {5, 1}, {11, 2}, {6, 1}, {13, 2}};
    for (size_t f = 0; f < 2; f++) {
        for (unsigned long seed = 1; seed <= last_seed(); seed++) {
            PiecePlan *plan = grow(&FIXTURES[f].motive, seed, &faster);
            Chord chords[64];
            size_t count = chords_of(plan, chords, 64);
            const Chord *at[6];
            for (size_t k = 0; k < 6; k++) {
                at[k] = NULL;
                for (size_t c = 0; c < count; c++) {
                    if (rational_cmp(chords[c].start, ONSETS[k]) == 0) {
                        at[k] = &chords[c];
                    }
                }
                assert(at[k]);
                assert(k == 0 || strcmp(at[k]->numeral, at[k - 1]->numeral) != 0);
            }
            size_t presentation = 0;
            size_t continuation = 0;
            for (size_t c = 0; c < count; c++) {
                (rational_cmp(chords[c].start, (Rational){4, 1}) < 0 ? &presentation : &continuation)[0]++;
            }
            assert(continuation > presentation);
            plan_destroy(plan);
        }
    }
    for (unsigned long seed = 1; seed <= last_seed(); seed++) {
        PiecePlan *with = grow(&TRIPLE_MOTIVE, seed, &faster);
        PiecePlan *without = grow(&TRIPLE_MOTIVE, seed, NULL);
        char *a = plan_text(with);
        char *b = plan_text(without);
        assert(strcmp(a, b) == 0);
        free(a);
        free(b);
        plan_destroy(with);
        plan_destroy(without);
    }
}

/* Whether the melody of the first two bars of the theme is the motif's notes exactly. */
static bool basic_idea_is(const PiecePlan *plan, const Motive *motive) {
    NodeId theme = find_node(plan, "theme");
    LaneSpanId spans[64];
    size_t found = plan_lane_find(plan, PLAN_LANE_MELODY, 0, plan_node_span(plan, theme), spans, 64);
    size_t next = 0;
    for (size_t s = 0; s < found; s++) {
        if (plan_lane_span_span(plan, spans[s]).start.num != 0) {
            continue;
        }
        const PlanNotes *notes = &plan_lane_span_payload(plan, spans[s])->notes;
        for (size_t i = 0; i < notes->note_count; i++, next++) {
            if (next >= motive->note_count || notes->notes[i].midi != motive_pitch(motive, next) ||
                rational_cmp(notes->notes[i].offset, motive->notes[next].onset) != 0 ||
                rational_cmp(notes->notes[i].duration, motive->notes[next].duration) != 0) {
                return false;
            }
        }
    }
    return next == motive->note_count;
}

/* A motif as long as the basic idea with chords of its own is the basic idea as it is; a motif with no chords is not
   changed by the setting. */
static void test_basic_as_written_keeps_the_motif(void) {
    for (size_t f = 0; f < 2; f++) {
        unsigned on = 0;
        unsigned off = 0;
        unsigned long seeds = fast_run ? 1 : 16;
        for (unsigned long seed = 1; seed <= seeds; seed++) {
            PiecePlan *written = grow_with(&CHORD_FIXTURES[f].motive, seed, NULL, true);
            PiecePlan *plain = grow_with(&CHORD_FIXTURES[f].motive, seed, NULL, false);
            on += basic_idea_is(written, &CHORD_FIXTURES[f].motive) ? 1 : 0;
            off += basic_idea_is(plain, &CHORD_FIXTURES[f].motive) ? 1 : 0;
            plan_destroy(written);
            plan_destroy(plain);
            PiecePlan *no_chords = grow_with(&FIXTURES[f].motive, seed, NULL, true);
            PiecePlan *no_chords_off = grow_with(&FIXTURES[f].motive, seed, NULL, false);
            char *a = plan_text(no_chords);
            char *b = plan_text(no_chords_off);
            assert(strcmp(a, b) == 0);
            free(a);
            free(b);
            plan_destroy(no_chords);
            plan_destroy(no_chords_off);
        }
        printf("basic_as_written %s: %u of %lu keep the motif, %u without\n", CHORD_FIXTURES[f].name, on, seeds, off);
        assert(on == seeds && off == 0);
    }
}

/* Compose rates: through api_grow_run, each setting alone and all together, seeds 1 to 16 (start seeds, each retried
   by api_grow_run), both motifs: how many compose. Measured 16 of 16 each; the basic idea as written is read on the motifs
   with chords of their own. */
#define RATE_SEEDS 16
#define MIN_COMPOSED 15

static void test_rates(void) {
    static const char *const NAMES[] = {"every_segment", "half_bar", "beat",          "diminish",
                                        "liquidate",     "all together", "faster_harmony", "basic_as_written"};
    for (int which = 0; which < 8; which++) {
        GrowContinuation settings;
        memset(&settings, 0, sizeof(settings));
        settings.faster_harmony = which == 6;
        settings.every_segment = which == 0 || which == 5;
        settings.fragment_lengths[GROW_FRAGMENT_SEGMENT] = which == 5 ? 1 : 0;
        settings.fragment_lengths[GROW_FRAGMENT_HALF_BAR] = which == 1 || which == 5 ? 1 : 0;
        settings.fragment_lengths[GROW_FRAGMENT_BEAT] = which == 2 || which == 5 ? 1 : 0;
        settings.diminish = which == 3 || which == 5;
        settings.liquidate = which == 4 || which == 5;
        for (size_t f = 0; f < 2; f++) {
            const Motive *motive = which == 7 ? &CHORD_FIXTURES[f].motive : &FIXTURES[f].motive;
            unsigned composed = 0;
            unsigned long seeds = fast_run ? 1 : RATE_SEEDS;
            for (unsigned long seed = 1; seed <= seeds; seed++) {
                GrowBriefText text;
                GrowSetup setup;
                memset(&text, 0, sizeof(text));
                text.key = f == 0 ? "C major" : "C minor";
                text.meter = "4/4";
                text.cadence = "PAC";
                text.into = "sentence";
                memset(&setup, 0, sizeof(setup));
                setup.text = &text;
                setup.into = GROW_INTO_SENTENCE;
                setup.pairing.beginning = PLAN_THEME_BEGINNING_PRESENTATION;
                setup.pairing.ending = PLAN_THEME_ENDING_CONTINUATION;
                setup.key = motive_key(motive);
                setup.meter = motive_meter(motive);
                setup.bars = 8;
                setup.cadence = CADENCE_TYPE_PERFECT_AUTHENTIC;
                setup.first_cadence = CADENCE_TYPE_HALF;
                setup.motive = motive;
                setup.suggest = 1;
                setup.seed = seed;
                setup.tempo_from = "none";
                setup.continuation = &settings;
                setup.basic_as_written = which == 7;
                GrowRun run;
                ComposerError error;
                memset(&error, 0, sizeof(error));
                composed += api_grow_run(&setup, &run, &error) == COMPOSER_OK && run.made == 1 ? 1 : 0;
                api_grow_run_destroy(&run);
            }
            printf("%-14s %s: %u of %lu composed\n", NAMES[which], FIXTURES[f].name, composed, seeds);
            assert(fast_run ? composed == 1 : composed >= MIN_COMPOSED);
        }
    }
}

int main(int argc, char **argv) {
    fast_run = argc > 1 && strcmp(argv[1], "fast") == 0;
    bool rates_only = argc > 1 && strcmp(argv[1], "rates") == 0;
    load_fixtures();
    if (!rates_only) {
        test_defaults_change_nothing();
        test_every_segment_uses_both();
        test_cut_half_bar();
        test_cut_beat();
        test_diminish_halves();
        test_liquidate_through();
        test_cut_departs();
        test_faster_harmony_changes_each_cell();
        test_basic_as_written_keeps_the_motif();
    }
    if (rates_only || fast_run) {
        test_rates();
    }
    return 0;
}
