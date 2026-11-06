#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/given.h>
#include <ursatz_composer/grow.h>
#include <ursatz_composer/plan_state.h>

#include "grow/grow_internal.h"

/*
 * A motif as long as the idea it fills is that idea (B1 of P33): grow_motive_fit for motives of 1, 1.5, 2 and 3 bars with
 * and without a pickup in 4/4 and 3/4; a 2-bar 3/4 motive composed as the basic idea of an 8-bar presentation and
 * continuation in A minor over given chords, placed as written; the same motive refused in a 4-bar theme; a 2-bar slot
 * motive in the contrasting slot of a period.
 */

static Motive make_motive(const char *text, int beats, int unit, bool minor, int tonic, Rational pickup) {
    Motive motive;
    MotiveOptions options;
    memset(&options, 0, sizeof(options));
    options.has_meter = true;
    options.meter.beats_per_bar = beats;
    options.meter.beat_unit = unit;
    if (pickup.num != 0) {
        options.has_pickup = true;
        options.pickup = pickup;
    }
    (void)minor;
    (void)tonic;
    assert(motive_from_notes_text(text, &options, &motive, NULL) == MOTIVE_OK);
    return motive;
}

static void test_fit(void) {
    /* 4/4: a bar is 4 quarters. 1 bar, 1.5 bars, 2 bars, 3 bars. */
    static const char *const FOUR[] = {"C4:w", "C4:w C4:h", "C4:w C4:w", "C4:w C4:w C4:w"};
    static const char *const THREE[] = {"C4:h.", "C4:h. C4:h", "C4:h. C4:h.", "C4:h. C4:h. C4:h."};
    static const size_t BARS[] = {1, 2, 2, 3};
    Rational none = {0, 1};
    for (int meter = 0; meter < 2; meter++) {
        const char *const *texts = meter == 0 ? FOUR : THREE;
        int beats = meter == 0 ? 4 : 3;
        PlanMeter meter_of = {0};
        meter_of.beats_per_bar = beats;
        meter_of.beat_unit = 4;
        Rational bar = grow_bar_length(&meter_of);
        for (size_t t = 0; t < 4; t++) {
            Motive motive = make_motive(texts[t], beats, 4, false, 0, none);
            for (size_t idea = 1; idea <= 4; idea++) {
                GrowMotiveFit expected = BARS[t] < idea ? GROW_FIT_SHORTER : (BARS[t] == idea ? GROW_FIT_WHOLE : GROW_FIT_LONGER);
                assert(grow_motive_fit(&motive, bar, idea) == expected);
            }
            /* A pickup of a beat comes before the first barline: it adds no bar. */
            Rational beat = {1, 4};
            Motive picked = make_motive(texts[t], beats, 4, false, 0, beat);
            for (size_t idea = 1; idea <= 4; idea++) {
                GrowMotiveFit expected = BARS[t] < idea ? GROW_FIT_SHORTER : (BARS[t] == idea ? GROW_FIT_WHOLE : GROW_FIT_LONGER);
                assert(grow_motive_fit(&picked, bar, idea) == expected);
            }
        }
        /* A pickup of a beat before a 2-bar body: 2 bars long, though the motive is 2 bars and a beat. */
        Rational beat = {1, 4};
        Motive body = make_motive(meter == 0 ? "C4:q C4:w C4:w" : "C4:q C4:h. C4:h.", beats, 4, false, 0, beat);
        assert(grow_motive_fit(&body, bar, 1) == GROW_FIT_LONGER && grow_motive_fit(&body, bar, 2) == GROW_FIT_WHOLE &&
               grow_motive_fit(&body, bar, 3) == GROW_FIT_SHORTER);
    }
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

static bool chord_has(const char *numeral, int pitch_class) {
    /* The chords of A minor the test gives. */
    static const int I[] = {9, 0, 4};
    static const int IV[] = {2, 5, 9};
    static const int V[] = {4, 8, 11};
    const int *tones = strcmp(numeral, "i") == 0 ? I : (strcmp(numeral, "iv") == 0 ? IV : V);
    return (strcmp(numeral, "i") == 0 || strcmp(numeral, "iv") == 0 || strcmp(numeral, "V") == 0) &&
           (tones[0] == pitch_class || tones[1] == pitch_class || tones[2] == pitch_class);
}

static void test_compose(void) {
    Rational none = {0, 1};
    Motive motive = make_motive("A4:q C5:q E5:q | D5:q F5:q A4:q", 3, 4, true, 9, none);
    static const char *const NUMERALS[8] = {"i", "iv", "i", "V", "i", "iv", "V", "i"};
    GivenChord chords[8];
    Rational bar = {3, 4};
    for (size_t i = 0; i < 8; i++) {
        chords[i].numeral = NUMERALS[i];
        assert(rational_mul_int(bar, (long long)i, &chords[i].onset) == RATIONAL_OK);
        chords[i].length = bar;
    }
    GrowRequest request;
    memset(&request, 0, sizeof(request));
    request.motive = &motive;
    request.key = plan_key_of_pitch_class(9, KEY_STRUCTURE_MODE_MINOR);
    request.meter.beats_per_bar = 3;
    request.meter.beat_unit = 4;
    request.bars = 8;
    request.pairing.beginning = PLAN_THEME_BEGINNING_PRESENTATION;
    request.pairing.ending = PLAN_THEME_ENDING_CONTINUATION;
    request.cadence = CADENCE_TYPE_PERFECT_AUTHENTIC;
    request.chords = chords;
    request.chord_count = 8;

    PiecePlan *plan = NULL;
    GrowOutcome outcome;
    GrowConfig config;
    memset(&config, 0, sizeof(config));
    config.seed = 1;
    assert(grow_build_plan(&request, &plan) == GROW_OK);
    GrowResult result = grow_run(plan, &config, &outcome);
    if (result != GROW_OK) {
        fprintf(stderr, "stuck at %s\n", grow_stage_name(outcome.stuck_stage));
    }
    assert(result == GROW_OK && !outcome.stuck);
    NodeId idea = find_node(plan, "basic idea");
    PlanSpan span = plan_node_span(plan, idea);
    assert(rational_cmp(span.length, (Rational){3, 2}) == 0);
    LaneSpanId ids[8];
    size_t found = plan_lane_find(plan, PLAN_LANE_MELODY, 0, span, ids, 8);
    assert(found == 1);
    const PlanNotes *notes = &plan_lane_span_payload(plan, ids[0])->notes;
    assert(notes->note_count == motive.note_count);
    for (size_t n = 0; n < notes->note_count; n++) {
        assert(rational_cmp(notes->notes[n].offset, motive.notes[n].onset) == 0);
        assert(rational_cmp(notes->notes[n].duration, motive.notes[n].duration) == 0);
        Rational at;
        LaneSpanId chord;
        assert(rational_add(span.start, notes->notes[n].offset, &at) == RATIONAL_OK);
        assert(plan_lane_find(plan, PLAN_LANE_HARMONY, 0, (PlanSpan){at, (Rational){1, 64}}, &chord, 1) == 1);
        assert(chord_has(plan_lane_span_payload(plan, chord)->harmony.roman_numeral, notes->notes[n].midi % 12));
    }
    plan_destroy(plan);
}

/* A 2-bar motive for the cadential idea whose last note is not a tone of the cadence's chord. */
static void test_cadence_note(void) {
    Rational none = {0, 1};
    Motive basic = make_motive("A4:q C5:q E5:q | D5:q F5:q A4:q", 3, 4, true, 9, none);
    Motive cadential = make_motive("E5:q D5:q C5:q | B4:q D5:q B4:q", 3, 4, true, 9, none);
    for (unsigned long seed = 1; seed <= 10; seed++) {
        GrowRequest request;
        memset(&request, 0, sizeof(request));
        request.motive = &basic;
        request.idea_motives[GROW_IDEA_CADENTIAL] = &cadential;
        request.key = plan_key_of_pitch_class(9, KEY_STRUCTURE_MODE_MINOR);
        request.meter.beats_per_bar = 3;
        request.meter.beat_unit = 4;
        request.bars = 8;
        request.pairing.beginning = PLAN_THEME_BEGINNING_PRESENTATION;
        request.pairing.ending = PLAN_THEME_ENDING_CONTINUATION;
        request.cadence = CADENCE_TYPE_PERFECT_AUTHENTIC;
        PiecePlan *plan = NULL;
        GrowOutcome outcome;
        GrowConfig config;
        memset(&config, 0, sizeof(config));
        config.seed = seed;
        assert(grow_build_plan(&request, &plan) == GROW_OK);
        GrowResult result = grow_run(plan, &config, &outcome);
        if (result != GROW_OK) {
            fprintf(stderr, "seed %lu: stuck at %s\n", seed, grow_stage_name(outcome.stuck_stage));
        }
        assert(result == GROW_OK && !outcome.stuck);
        NodeId idea = find_node(plan, "cadential");
        PlanSpan span = plan_node_span(plan, idea);
        LaneSpanId ids[8];
        size_t found = plan_lane_find(plan, PLAN_LANE_MELODY, 0, span, ids, 8);
        assert(found == 1);
        const PlanNotes *notes = &plan_lane_span_payload(plan, ids[0])->notes;
        assert(notes->note_count == cadential.note_count);
        for (size_t n = 0; n < notes->note_count; n++) {
            assert(rational_cmp(notes->notes[n].offset, cadential.notes[n].onset) == 0);
            assert(rational_cmp(notes->notes[n].duration, cadential.notes[n].duration) == 0);
        }
        /* The last note lands on a tone of the tonic chord. */
        int pc = notes->notes[notes->note_count - 1].midi % 12;
        assert(pc == 9 || pc == 0 || pc == 4);
        plan_destroy(plan);
    }
}

/* Whole motives in other keys and meters, and with a top note repeated, compose on seeds 1 to 4. */
static void test_variety(void) {
    static const struct {
        const char *text;
        int beats;
        int unit;
        int tonic;
        KeyStructureMode mode;
    } CASES[] = {
        {"E4:q G4:e F4:e D4:q C4:q | G4:q E4:q D4:q C4:q", 4, 4, 0, KEY_STRUCTURE_MODE_MAJOR},
        {"G4:q G4:q G4:q G4:q | G4:q G4:q G4:q C5:q", 4, 4, 0, KEY_STRUCTURE_MODE_MAJOR},
        {"B4:q A4:e G4:e F#4:e D4:e | A4:q B4:e C5:e D5:q", 6, 8, 7, KEY_STRUCTURE_MODE_MAJOR},
        {"C5:q B4:q A4:q | G4:q F4:q E4:q", 3, 4, 9, KEY_STRUCTURE_MODE_MINOR},
    };
    Rational none = {0, 1};
    for (size_t c = 0; c < sizeof(CASES) / sizeof(CASES[0]); c++) {
        Motive motive = make_motive(CASES[c].text, CASES[c].beats, CASES[c].unit, false, 0, none);
        for (unsigned long seed = 1; seed <= 4; seed++) {
            GrowRequest request;
            memset(&request, 0, sizeof(request));
            request.motive = &motive;
            request.key = plan_key_of_pitch_class(CASES[c].tonic, CASES[c].mode);
            request.meter.beats_per_bar = CASES[c].beats;
            request.meter.beat_unit = CASES[c].unit;
            request.bars = 8;
            request.pairing.beginning = PLAN_THEME_BEGINNING_PRESENTATION;
            request.pairing.ending = PLAN_THEME_ENDING_CONTINUATION;
            request.cadence = CADENCE_TYPE_PERFECT_AUTHENTIC;
            PiecePlan *plan = NULL;
            GrowOutcome outcome;
            GrowConfig config;
            memset(&config, 0, sizeof(config));
            config.seed = seed;
            assert(grow_build_plan(&request, &plan) == GROW_OK);
            GrowResult result = grow_run(plan, &config, &outcome);
            if (result != GROW_OK) {
                fprintf(stderr, "case %zu seed %lu: stuck at %s\n", c, seed, grow_stage_name(outcome.stuck_stage));
            }
            assert(result == GROW_OK && !outcome.stuck);
            plan_destroy(plan);
        }
    }
}

/* The same 2-bar motive in a 4-bar theme, whose basic idea is a bar, is refused. */
static void test_four_bar_refused(void) {
    Rational none = {0, 1};
    Motive motive = make_motive("A4:q C5:q E5:q | D5:q F5:q A4:q", 3, 4, true, 9, none);
    GrowRequest request;
    memset(&request, 0, sizeof(request));
    request.motive = &motive;
    request.key = plan_key_of_pitch_class(9, KEY_STRUCTURE_MODE_MINOR);
    request.meter.beats_per_bar = 3;
    request.meter.beat_unit = 4;
    request.bars = 4;
    /* A compound basic idea is one idea: no part for a 2-bar motive to fill. */
    request.pairing.beginning = PLAN_THEME_BEGINNING_COMPOUND_BASIC_IDEA;
    request.pairing.ending = PLAN_THEME_ENDING_CONTINUATION;
    request.cadence = CADENCE_TYPE_PERFECT_AUTHENTIC;
    PiecePlan *plan = NULL;
    assert(grow_build_plan(&request, &plan) == GROW_ERR_MOTIVE_TOO_LONG && plan == NULL);
    /* A bar of it is accepted. */
    Motive bar = make_motive("A4:q C5:q E5:q", 3, 4, true, 9, none);
    request.motive = &bar;
    assert(grow_build_plan(&request, &plan) == GROW_OK && plan);
    plan_destroy(plan);
}

/* A 2-bar motive for the contrasting slot of a period fills the contrasting idea as written. */
static void test_period_slot(void) {
    Rational none = {0, 1};
    Motive basic = make_motive("A4:q C5:q E5:q | D5:q F5:q A4:q", 3, 4, true, 9, none);
    Motive contrasting = make_motive("E5:q C5:q A4:q | B4:q D5:q E5:q", 3, 4, true, 9, none);
    GrowRequest request;
    memset(&request, 0, sizeof(request));
    request.motive = &basic;
    request.slot_motives[GROW_SLOT_CONTRASTING] = &contrasting;
    request.key = plan_key_of_pitch_class(9, KEY_STRUCTURE_MODE_MINOR);
    request.meter.beats_per_bar = 3;
    request.meter.beat_unit = 4;
    request.bars = 8;
    request.pairing.beginning = PLAN_THEME_BEGINNING_ANTECEDENT;
    request.pairing.ending = PLAN_THEME_ENDING_CONSEQUENT;
    request.cadence = CADENCE_TYPE_PERFECT_AUTHENTIC;
    PiecePlan *plan = NULL;
    GrowOutcome outcome;
    GrowConfig config;
    memset(&config, 0, sizeof(config));
    config.seed = 1;
    assert(grow_build_plan(&request, &plan) == GROW_OK);
    GrowResult result = grow_run(plan, &config, &outcome);
    if (result != GROW_OK) {
        fprintf(stderr, "period: stuck at %s\n", grow_stage_name(outcome.stuck_stage));
    }
    assert(result == GROW_OK && !outcome.stuck);
    NodeId idea = find_node(plan, "contrasting idea");
    assert(idea != PLAN_NODE_NONE);
    PlanSpan span = plan_node_span(plan, idea);
    assert(rational_cmp(span.length, (Rational){3, 2}) == 0);
    LaneSpanId ids[8];
    assert(plan_lane_find(plan, PLAN_LANE_MELODY, 0, span, ids, 8) == 1);
    const PlanNotes *notes = &plan_lane_span_payload(plan, ids[0])->notes;
    assert(notes->note_count == contrasting.note_count);
    for (size_t n = 0; n < notes->note_count; n++) {
        assert(rational_cmp(notes->notes[n].offset, contrasting.notes[n].onset) == 0);
        assert(rational_cmp(notes->notes[n].duration, contrasting.notes[n].duration) == 0);
    }
    plan_destroy(plan);
}

/* A 4-bar 4/4 motif as long as the antecedent fills it whole: bars 1 to 4 are the motif's rhythm. */
static const char *const FOUR_BARS = "E4:q G4:e F4:e D4:q C4:q | G4:q E4:q D4:q C4:q | E4:q G4:q C5:q G4:q | D5:q B4:q A4:q G4:q";

static void check_whole_part(PiecePlan *plan, const Motive *motive) {
    NodeId idea = find_node(plan, "basic idea");
    assert(idea != PLAN_NODE_NONE);
    PlanSpan span = plan_node_span(plan, idea);
    assert(rational_cmp(span.length, (Rational){4, 1}) == 0);
    LaneSpanId ids[8];
    assert(plan_lane_find(plan, PLAN_LANE_MELODY, 0, span, ids, 8) == 1);
    const PlanNotes *notes = &plan_lane_span_payload(plan, ids[0])->notes;
    assert(notes->note_count == motive->note_count);
    for (size_t n = 0; n < notes->note_count; n++) {
        assert(rational_cmp(notes->notes[n].offset, motive->notes[n].onset) == 0);
        assert(rational_cmp(notes->notes[n].duration, motive->notes[n].duration) == 0);
    }
}

static GrowRequest part_request(const Motive *motive, PlanThemeBeginning beginning, PlanThemeEnding ending) {
    GrowRequest request;
    memset(&request, 0, sizeof(request));
    request.motive = motive;
    request.key = plan_key_of_pitch_class(0, KEY_STRUCTURE_MODE_MAJOR);
    request.meter.beats_per_bar = 4;
    request.meter.beat_unit = 4;
    request.bars = 8;
    request.pairing.beginning = beginning;
    request.pairing.ending = ending;
    request.cadence = CADENCE_TYPE_PERFECT_AUTHENTIC;
    return request;
}

static void test_part_length(void) {
    Rational none = {0, 1};
    Motive motive = make_motive(FOUR_BARS, 4, 4, false, 0, none);
    static const PlanThemeBeginning BEGINNINGS[] = {PLAN_THEME_BEGINNING_ANTECEDENT, PLAN_THEME_BEGINNING_PRESENTATION};
    static const PlanThemeEnding ENDINGS[] = {PLAN_THEME_ENDING_CONSEQUENT, PLAN_THEME_ENDING_CONTINUATION,
                                              PLAN_THEME_ENDING_CADENTIAL};
    for (size_t b = 0; b < 2; b++) {
        for (size_t e = 0; e < 3; e++) {
            GrowRequest request = part_request(&motive, BEGINNINGS[b], ENDINGS[e]);
            if (grow_pairing_archetype(&request.pairing) < 0) {
                continue;
            }
            for (unsigned long seed = 1; seed <= 3; seed++) {
                PiecePlan *plan = NULL;
                GrowOutcome outcome;
                GrowConfig config;
                memset(&config, 0, sizeof(config));
                config.seed = seed;
                assert(grow_build_plan(&request, &plan) == GROW_OK);
                GrowResult result = grow_run(plan, &config, &outcome);
                if (result != GROW_OK) {
                    fprintf(stderr, "part %zu/%zu seed %lu: stuck at %s\n", b, e, seed, grow_stage_name(outcome.stuck_stage));
                }
                assert(result == GROW_OK && !outcome.stuck);
                check_whole_part(plan, &motive);
                plan_destroy(plan);
            }
        }
    }
}

/* A 4-bar motif with a compound basic idea, a 3-bar motif, and a contrasting slot beside a part-length motif are refused. */
static void test_part_length_refused(void) {
    Rational none = {0, 1};
    Motive motive = make_motive(FOUR_BARS, 4, 4, false, 0, none);
    PiecePlan *plan = NULL;
    GrowRequest compound = part_request(&motive, PLAN_THEME_BEGINNING_COMPOUND_BASIC_IDEA, PLAN_THEME_ENDING_CONTINUATION);
    assert(grow_build_plan(&compound, &plan) == GROW_ERR_MOTIVE_TOO_LONG && plan == NULL);
    Motive three = make_motive("E4:q G4:q C5:q G4:q | D5:q B4:q A4:q G4:q | E4:w", 4, 4, false, 0, none);
    GrowRequest request = part_request(&three, PLAN_THEME_BEGINNING_ANTECEDENT, PLAN_THEME_ENDING_CONSEQUENT);
    assert(grow_build_plan(&request, &plan) == GROW_ERR_MOTIVE_TOO_LONG && plan == NULL);
    /* A 5-bar motif is longer than the part too. */
    Motive five = make_motive("C4:w C4:w C4:w C4:w C4:w", 4, 4, false, 0, none);
    request.motive = &five;
    assert(grow_build_plan(&request, &plan) == GROW_ERR_MOTIVE_TOO_LONG && plan == NULL);
    Motive contrasting = make_motive("E5:q C5:q A4:q G4:q", 4, 4, false, 0, none);
    request = part_request(&motive, PLAN_THEME_BEGINNING_ANTECEDENT, PLAN_THEME_ENDING_CONSEQUENT);
    request.slot_motives[GROW_SLOT_CONTRASTING] = &contrasting;
    assert(grow_build_plan(&request, &plan) == GROW_ERR_BAD_REQUEST && plan == NULL);
    request = part_request(&motive, PLAN_THEME_BEGINNING_PRESENTATION, PLAN_THEME_ENDING_CONTINUATION);
    request.idea_motives[GROW_IDEA_REPETITION] = &contrasting;
    assert(grow_build_plan(&request, &plan) == GROW_ERR_BAD_REQUEST && plan == NULL);
}

int main(void) {
    test_four_bar_refused();
    test_part_length();
    test_part_length_refused();
    test_period_slot();
    test_variety();
    test_fit();
    test_compose();
    test_cadence_note();
    puts("ok");
    return 0;
}
