#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <standard-harmony/harmony_cadence_rules.h>

#include <ursatz_composer/compose.h>
#include <ursatz_composer/form_template.h>
#include <ursatz_composer/given.h>
#include <ursatz_composer/harmony_phrase.h>
#include <ursatz_composer/lanes.h>
#include <ursatz_composer/motive.h>
#include <ursatz_composer/piece.h>
#include <ursatz_composer/plan_state.h>

#include "harmony/harmony_internal.h"
#include "melody/melody_internal.h"

/*
 * Given material placed anywhere in a plan (R-071, given.h): the stages grow the form around it and fill the rest, and
 * never change it.
 *
 *   give                 the placing calls: what they refuse, what they add, that a refused call adds nothing, the gaps
 *                        Given spans leave, and the snapshot that tells the run left them as they were
 *   smoke                the first of seeds 1 to 5 that composes with an 8-bar melody given as the B of a ternary form
 *   melody|progression|bass
 *                        seeds 1 to 10 of C major 4/4 with, in turn,
 *                          melody       an 8-bar melody given as the B of a ternary form (24 bars)
 *                          progression  I IV V I (a bar each) given at the first four bars of the A of a rounded binary
 *                          bass         an 8-bar bass line given in the B of a binary form (16 bars)
 *                        Every run that completes leaves the Given payloads byte for byte as they were, has chords that
 *                        stand on the given bass and carry the given melody, and is heard. The shares of runs that
 *                        complete, of sections heard and of cadences heard where planned are printed and fail the
 *                        test when they fall below the baselines below (docs/middle-out.md).
 */

#define SEEDS 10

/* The melody's downbeats are tones of the chords the harmony operators can write there: the tonic, then a predominant, the dominant and the tonic. */
static const char MELODY[] = "C5:h E5:h G5:q E5:q C5:h E5:h G5:h G5:q F5:q E5:h "
                             "E5:h C5:h A4:q B4:q C5:h D5:h B4:h C5:w";
static const char BASS[] = "C3:w C3:w C3:w C3:w C3:w F3:w G3:w C3:w";
static const char *const PROGRESSION[] = {"I", "IV", "V", "I"};

typedef enum {
    SCENARIO_MELODY,
    SCENARIO_PROGRESSION,
    SCENARIO_BASS,
    SCENARIO_COUNT
} ScenarioKind;

static const char *const SCENARIO_NAMES[] = {"melody", "progression", "bass"};

/*
 * The shares measured (docs/middle-out.md) of the runs of seeds 1 to 10, by scenario: runs that complete, sections
 * heard (a theme heard in the section), and cadences heard where planned (the last cadence heard in a section is the
 * type planned, in the section's last bar). A share fails the test only when it falls below its baseline.
 */
typedef struct {
    size_t completed;
    size_t sections_heard;
    size_t cadences;
} Baseline;

/* Of 10 runs and, for the melody and the progression, 30 sections (3 a run); the bass has 2 sections a run. */
static const Baseline BASELINES[SCENARIO_COUNT] = {{10, 10, 12}, {10, 5, 22}, {9, 18, 16}};

/* --- the plans ----------------------------------------------------------------------- */

static Motive notes_of(const char *text) {
    MotiveOptions options;
    Motive motive;
    MotiveError error;
    memset(&options, 0, sizeof(options));
    options.has_meter = true;
    options.meter.beats_per_bar = 4;
    options.meter.beat_unit = 4;
    assert(motive_from_notes_text(text, &options, &motive, &error) == MOTIVE_OK);
    return motive;
}

static NodeId section_with_letter(const PiecePlan *plan, const char *letter) {
    NodeId root = plan_root(plan);
    for (size_t i = 0; i < plan_node_child_count(plan, root); i++) {
        NodeId node = plan_node_child_at(plan, root, i);
        for (size_t c = 0; c < plan_node_constraint_count(plan, node); c++) {
            const PlanConstraint *constraint = plan_node_constraint_at(plan, node, c);
            if (constraint->kind == PLAN_CONSTRAINT_SECTION_LETTER && strcmp(constraint->params.letter, letter) == 0) {
                return node;
            }
        }
    }
    return PLAN_NODE_NONE;
}

static PieceRequest request_of(FormTemplateKind form, unsigned long seed) {
    PieceRequest request;
    memset(&request, 0, sizeof(request));
    request.form = form;
    request.key.tonic_pitch_class = 0;
    request.key.mode = PLAN_MODE_MAJOR;
    request.meter.beats_per_bar = 4;
    request.meter.beat_unit = 4;
    request.seed = seed;
    return request;
}

/* The plan of a scenario with its material placed. */
static PiecePlan *plan_of(ScenarioKind kind, unsigned long seed) {
    FormTemplateKind form = kind == SCENARIO_MELODY ? FORM_TEMPLATE_TERNARY
                            : kind == SCENARIO_PROGRESSION ? FORM_TEMPLATE_ROUNDED_BINARY
                                                           : FORM_TEMPLATE_BINARY;
    PieceRequest request = request_of(form, seed);
    request.given_melodies = kind == SCENARIO_MELODY;
    PiecePlan *plan = NULL;
    assert(piece_build_plan(&request, &plan) == PIECE_OK);
    NodeId section = section_with_letter(plan, kind == SCENARIO_PROGRESSION ? "A" : "B");
    assert(section != PLAN_NODE_NONE);
    if (kind == SCENARIO_MELODY) {
        Motive melody = notes_of(MELODY);
        assert(plan_give_melody(plan, section, 0, melody.notes, melody.note_count) == GIVEN_OK);
    } else if (kind == SCENARIO_PROGRESSION) {
        assert(plan_give_progression(plan, section, 0, PROGRESSION, 4, 2) == GIVEN_OK);
    } else {
        Motive bass = notes_of(BASS);
        assert(plan_give_bass(plan, section, 0, bass.notes, bass.note_count) == GIVEN_OK);
    }
    return plan;
}

/* --- the placing calls ------------------------------------------------------------------ */

static Rational bars(long count) {
    Rational time;
    assert(rational_make(count, 1, &time) == RATIONAL_OK);
    return time;
}

static size_t given_count(const PiecePlan *plan, PlanLaneKind lane) {
    size_t count = 0;
    for (LaneSpanId id = 1; id <= plan_lane_span_count(plan); id++) {
        count += plan_lane_span_lane(plan, id) == lane && plan_lane_span_state(plan, id) == PLAN_STATE_GIVEN &&
                 plan_node_kind(plan, plan_lane_span_owner(plan, id)) == PLAN_NODE_SECTION;
    }
    return count;
}

static void test_refusals(void) {
    PieceRequest request = request_of(FORM_TEMPLATE_TERNARY, 1);
    PiecePlan *plan = NULL;
    assert(piece_build_plan(&request, &plan) == PIECE_OK);
    NodeId b = section_with_letter(plan, "B");
    Motive melody = notes_of(MELODY);
    Motive bass = notes_of(BASS);
    assert(b != PLAN_NODE_NONE);
    size_t spans = plan_lane_span_count(plan);

    /* Not a theme section: the form, the material, a node that is not there, a null plan. */
    assert(plan_give_melody(plan, plan_root(plan), 0, melody.notes, melody.note_count) == GIVEN_ERR_NOT_SECTION);
    assert(plan_give_melody(plan, 99999, 0, melody.notes, melody.note_count) == GIVEN_ERR_NOT_SECTION);
    NodeId material = PLAN_NODE_NONE;
    for (size_t i = 0; i < plan_node_child_count(plan, plan_root(plan)); i++) {
        const char *name = plan_node_name(plan, plan_node_child_at(plan, plan_root(plan), i));
        material = name && strcmp(name, "material") == 0 ? plan_node_child_at(plan, plan_root(plan), i) : material;
    }
    assert(material != PLAN_NODE_NONE);
    assert(plan_give_melody(plan, material, 0, melody.notes, melody.note_count) == GIVEN_ERR_NOT_SECTION);
    assert(plan_give_melody(NULL, b, 0, melody.notes, melody.note_count) == GIVEN_ERR_NULL);

    /* A bar outside the section, and material that would cross its end. */
    assert(plan_give_melody(plan, b, 8, melody.notes, melody.note_count) == GIVEN_ERR_BAD_BAR);
    assert(plan_give_melody(plan, b, 1, melody.notes, melody.note_count) == GIVEN_ERR_PAST_END);
    assert(plan_give_bass(plan, b, 4, bass.notes, bass.note_count) == GIVEN_ERR_PAST_END);
    const char *five[] = {"I", "V", "I", "V", "I"};
    assert(plan_give_progression(plan, b, 5, five, 5, 2) == GIVEN_ERR_PAST_END);

    /* Notes that cannot be placed. */
    assert(plan_give_melody(plan, b, 0, melody.notes, 0) == GIVEN_ERR_BAD_NOTES);
    assert(plan_give_melody(plan, b, 0, NULL, 4) == GIVEN_ERR_BAD_NOTES);
    PlanNote bad[2] = {melody.notes[0], melody.notes[1]};
    bad[0].midi_pitch = 200;
    assert(plan_give_melody(plan, b, 0, bad, 2) == GIVEN_ERR_BAD_NOTES);
    bad[0] = melody.notes[0];
    bad[1].offset = bad[0].offset; /* overlaps the first */
    assert(plan_give_melody(plan, b, 0, bad, 2) == GIVEN_ERR_BAD_NOTES);
    /* A bass line is one unbroken line: a rest in it, or at its start, is refused. */
    bad[0] = melody.notes[1];
    assert(plan_give_bass(plan, b, 0, bad, 1) == GIVEN_ERR_BAD_NOTES);
    bad[0] = bass.notes[0];
    bad[1] = bass.notes[2];
    assert(plan_give_bass(plan, b, 0, bad, 2) == GIVEN_ERR_BAD_NOTES);

    /* Numerals: one that cannot be read refuses the whole progression, and nothing of it is added. */
    const char *unreadable[] = {"I", "IV", "X", "I"};
    assert(plan_give_progression(plan, b, 0, unreadable, 4, 2) == GIVEN_ERR_BAD_NUMERAL);
    assert(plan_give_progression(plan, b, 0, PROGRESSION, 4, 0) == GIVEN_ERR_BAD_NUMERAL);
    assert(plan_give_progression(plan, b, 0, PROGRESSION, 0, 2) == GIVEN_ERR_BAD_NUMERAL);
    assert(plan_give_progression(plan, b, 0, NULL, 4, 2) == GIVEN_ERR_BAD_NUMERAL);

    /* Every refusal left the plan as it was. */
    assert(plan_lane_span_count(plan) == spans);
    assert(given_count(plan, PLAN_LANE_HARMONY) == 0);
    assert(given_count(plan, PLAN_LANE_MELODY) == 0);
    assert(given_count(plan, PLAN_LANE_BASS) == 0);
    plan_destroy(plan);
}

static void test_placing(void) {
    PieceRequest request = request_of(FORM_TEMPLATE_TERNARY, 1);
    PiecePlan *plan = NULL;
    assert(piece_build_plan(&request, &plan) == PIECE_OK);
    NodeId b = section_with_letter(plan, "B");
    Motive melody = notes_of("E5:h G5:h G5:q F5:q E5:h E5:h C5:h A4:q B4:q C5:h"); /* 4 bars */
    PlanSpan section = plan_node_span(plan, b);

    /* A melody from the third bar: one Given span, 4 bars long, owned by the section. */
    assert(plan_give_melody(plan, b, 2, melody.notes, melody.note_count) == GIVEN_OK);
    LaneSpanId ids[4];
    assert(plan_lane_find(plan, PLAN_LANE_MELODY, 0, section, ids, 4) == 1);
    assert(plan_lane_span_state(plan, ids[0]) == PLAN_STATE_GIVEN);
    assert(plan_lane_span_owner(plan, ids[0]) == b);
    PlanSpan span = plan_lane_span_span(plan, ids[0]);
    Rational expected;
    assert(rational_add(section.start, bars(2), &expected) == RATIONAL_OK);
    assert(rational_cmp(span.start, expected) == 0 && rational_cmp(span.length, bars(4)) == 0);
    assert(plan_lane_span_payload(plan, ids[0])->notes.note_count == melody.note_count);

    /* The same lane is taken there: a second melody that overlaps it is refused, one that does not is not. */
    Motive bar = notes_of("C5:w");
    assert(plan_give_melody(plan, b, 5, bar.notes, bar.note_count) == GIVEN_ERR_OVERLAP);
    assert(plan_give_melody(plan, b, 6, bar.notes, bar.note_count) == GIVEN_OK);

    /* The gaps: the two bars before, and the last bar (the two Given spans touch). */
    PlanSpan gaps[4];
    assert(given_gaps(plan, PLAN_LANE_MELODY, 0, section, gaps, 4) == 2);
    assert(rational_cmp(gaps[0].start, section.start) == 0 && rational_cmp(gaps[0].length, bars(2)) == 0);
    assert(rational_add(section.start, bars(7), &expected) == RATIONAL_OK);
    assert(rational_cmp(gaps[1].start, expected) == 0 && rational_cmp(gaps[1].length, bars(1)) == 0);
    assert(given_touches(plan, PLAN_LANE_MELODY, 0, section));
    assert(!given_touches(plan, PLAN_LANE_BASS, 0, section));
    /* A lane nothing Given is in is its own gap; a span covered wholly has none. */
    assert(given_gaps(plan, PLAN_LANE_BASS, 0, section, gaps, 4) == 1);
    assert(rational_cmp(gaps[0].length, section.length) == 0);
    assert(given_gaps(plan, PLAN_LANE_MELODY, 0, plan_lane_span_span(plan, ids[0]), gaps, 4) == 0);

    /* A progression: a Given span a chord, in the key's mode, with each chord's function. */
    const char *chords[] = {"I", "V", "vi", "V7/V"};
    assert(plan_give_progression(plan, b, 0, chords, 4, 1) == GIVEN_OK);
    LaneSpanId harmony[8];
    assert(plan_lane_find(plan, PLAN_LANE_HARMONY, 0, section, harmony, 8) == 4);
    for (size_t i = 0; i < 4; i++) {
        const PlanLanePayload *payload = plan_lane_span_payload(plan, harmony[i]);
        assert(plan_lane_span_state(plan, harmony[i]) == PLAN_STATE_GIVEN);
        assert(strcmp(payload->harmony.roman_numeral, chords[i]) == 0);
        assert(payload->harmony.function != PLAN_FUNCTION_UNKNOWN);
    }
    assert(plan_lane_span_payload(plan, harmony[0])->harmony.function == PLAN_FUNCTION_TONIC);
    assert(plan_lane_span_payload(plan, harmony[1])->harmony.function == PLAN_FUNCTION_DOMINANT);

    /* The snapshot: a plan with the same material, and the plan without it. */
    GivenSnapshot *snapshot = given_snapshot(plan);
    assert(snapshot);
    GivenReport runs[8];
    size_t run_count = given_snapshot_report(snapshot, plan, runs, 8);
    assert(run_count == 2); /* the melody (two spans that touch are one run) and the progression */
    for (size_t i = 0; i < run_count; i++) {
        assert(runs[i].unchanged);
    }
    assert(runs[0].lane == PLAN_LANE_MELODY && runs[0].spans == 2);
    assert(runs[1].lane == PLAN_LANE_HARMONY && runs[1].spans == 4);
    PiecePlan *bare = NULL;
    assert(piece_build_plan(&request, &bare) == PIECE_OK);
    run_count = given_snapshot_report(snapshot, bare, runs, 8);
    assert(run_count == 2);
    for (size_t i = 0; i < run_count; i++) {
        assert(!runs[i].unchanged);
    }
    given_snapshot_destroy(snapshot);
    plan_destroy(bare);
    plan_destroy(plan);
}

/* A progression or a melody that crosses the change of key inside a section is refused. */
static void test_key_change(void) {
    bool found = false;
    for (unsigned long seed = 1; seed <= 20 && !found; seed++) {
        PieceRequest request = request_of(FORM_TEMPLATE_TERNARY, seed);
        PiecePlan *plan = NULL;
        request.modulate = true;
        assert(piece_build_plan(&request, &plan) == PIECE_OK);
        NodeId root = plan_root(plan);
        for (size_t i = 0; i < plan_node_child_count(plan, root) && !found; i++) {
            NodeId section = plan_node_child_at(plan, root, i);
            PlanSpan span = plan_node_span(plan, section);
            const char *name = plan_node_name(plan, section);
            if (name && strcmp(name, "material") == 0) {
                continue;
            }
            if (plan_lane_find(plan, PLAN_LANE_KEY, 0, span, NULL, 0) == 2) {
                /* A melody from the second bar to the end crosses the half. */
                Motive melody = notes_of("C5:w C5:w C5:w C5:w C5:w C5:w C5:w");
                assert(plan_give_melody(plan, section, 1, melody.notes, melody.note_count) == GIVEN_ERR_KEY);
                assert(given_count(plan, PLAN_LANE_MELODY) == 0);
                /* The first half is one key: it is accepted. */
                Motive half = notes_of("C5:w C5:w C5:w C5:w");
                assert(plan_give_melody(plan, section, 0, half.notes, half.note_count) == GIVEN_OK);
                found = true;
            }
        }
        plan_destroy(plan);
    }
    printf("key change: %s\n", found ? "refused across the change" : "no plan with a change inside a section found");
}

static void test_give(void) {
    test_refusals();
    test_placing();
    test_key_change();
}

/* --- the runs ----------------------------------------------------------------------------- */

/* Whether the last cadence heard in a section is the one planned for it and arrives in its last bar. */
static bool cadence_as_planned(const PiecePlan *plan, const ComposeHeard *heard) {
    PlanCadenceType planned = PLAN_CADENCE_PAC;
    bool has_planned = false;
    for (size_t c = 0; c < plan_node_constraint_count(plan, heard->section) && !has_planned; c++) {
        const PlanConstraint *constraint = plan_node_constraint_at(plan, heard->section, c);
        if (constraint->kind == PLAN_CONSTRAINT_CADENCE) {
            planned = constraint->params.cadence;
            has_planned = true;
        }
    }
    if (!has_planned || heard->heard_result != FORM_HEAR_OK || heard->heard.cadence_count == 0) {
        return false;
    }
    const FormHeardCadence *last = &heard->heard.cadences[heard->heard.cadence_count - 1];
    PlanSpan section = plan_node_span(plan, heard->section);
    Rational end;
    Rational last_bar;
    assert(plan_span_end(section, &end) == PLAN_OK);
    assert(rational_make(end.num - (long)end.den, end.den, &last_bar) == RATIONAL_OK); /* a bar of 4/4 before the end */
    return (CadenceType)last->type == harmony_cadence_type_of(planned) && rational_cmp(last->arrival, last_bar) >= 0 &&
           rational_cmp(last->arrival, end) < 0;
}

/* The melody's notes that start a bar are tones of the chord there; the chords at the bass notes stand on them. */
static void check_given_fits(const PiecePlan *plan, ScenarioKind kind) {
    PlanLaneKind lane = kind == SCENARIO_MELODY ? PLAN_LANE_MELODY : PLAN_LANE_BASS;
    PlanKey key = {0, PLAN_MODE_MAJOR};
    Rational half = {1, 2}; /* a harmony cell of 4/4 */
    if (kind == SCENARIO_PROGRESSION) {
        return;
    }
    for (LaneSpanId id = 1; id <= plan_lane_span_count(plan); id++) {
        if (plan_lane_span_lane(plan, id) != lane || plan_lane_span_state(plan, id) != PLAN_STATE_GIVEN ||
            plan_node_kind(plan, plan_lane_span_owner(plan, id)) != PLAN_NODE_SECTION) {
            continue;
        }
        const PlanNotes *notes = &plan_lane_span_payload(plan, id)->notes;
        PlanSpan span = plan_lane_span_span(plan, id);
        for (size_t n = 0; n < notes->note_count; n++) {
            Rational onset;
            LaneSpanId chords[2];
            assert(rational_add(span.start, notes->notes[n].offset, &onset) == RATIONAL_OK);
            /* A bar starts where the onset is a whole number of whole notes (4/4 from the start of the piece). */
            if (onset.den != 1) {
                continue;
            }
            PlanSpan at = {onset, {1, 4096}};
            assert(plan_lane_find(plan, PLAN_LANE_HARMONY, 0, at, chords, 2) == 1);
            PhraseChord chord;
            assert(harmony_read_chord(plan, chords[0], key.mode, half, &chord));
            if (lane == PLAN_LANE_MELODY) {
                unsigned mask;
                unsigned seventh;
                assert(melody_chord_mask(&key, &chord, &mask, &seventh));
                assert((mask >> (notes->notes[n].midi_pitch % 12)) & 1u);
            } else if (rational_cmp(plan_lane_span_span(plan, chords[0]).start, onset) == 0) {
                int pc;
                assert(harmony_chord_bass_pc(&key, &chord, &pc));
                assert(pc == notes->notes[n].midi_pitch % 12);
            }
        }
    }
}

typedef struct {
    size_t runs;
    size_t completed;
    size_t sections;
    size_t sections_heard;
    size_t cadences;
} Share;

static Share run_scenario(ScenarioKind kind, unsigned long first, unsigned long last, bool stop_at_first) {
    Share share;
    memset(&share, 0, sizeof(share));
    for (unsigned long seed = first; seed <= last; seed++) {
        PiecePlan *plan = plan_of(kind, seed);
        GivenSnapshot *snapshot = given_snapshot(plan);
        ComposeConfig config;
        ComposeOutcome outcome;
        memset(&config, 0, sizeof(config));
        config.seed = seed;
        PieceResult result = compose_run(plan, &config, &outcome);
        share.runs++;
        if (result == PIECE_OK) {
            GivenReport runs[8];
            size_t run_count = given_snapshot_report(snapshot, plan, runs, 8);
            share.completed++;
            assert(run_count >= 1);
            for (size_t i = 0; i < run_count; i++) {
                assert(runs[i].unchanged); /* the Given payloads are as they were, byte for byte */
            }
            check_given_fits(plan, kind);
            for (size_t s = 0; s < outcome.section_count; s++) {
                share.sections++;
                share.sections_heard += outcome.sections[s].heard_result == FORM_HEAR_OK && outcome.sections[s].heard.count > 0;
                share.cadences += cadence_as_planned(plan, &outcome.sections[s]);
            }
        } else {
            /* A stuck run still leaves what was given as it was. */
            GivenReport runs[8];
            size_t run_count = given_snapshot_report(snapshot, plan, runs, 8);
            for (size_t i = 0; i < run_count; i++) {
                assert(runs[i].unchanged);
            }
        }
        given_snapshot_destroy(snapshot);
        compose_outcome_destroy(&outcome);
        plan_destroy(plan);
        if (stop_at_first && result == PIECE_OK) {
            break;
        }
    }
    return share;
}

static void test_smoke(void) {
    Share share = run_scenario(SCENARIO_MELODY, 1, 5, true);
    assert(share.completed == 1);
    assert(share.sections_heard > 0);
}

/* The partial case: a melody given in the middle of the section; the ideas that begin or end in it are made from its first bar. */
static void test_partial(void) {
    bool composed = false;
    for (unsigned long seed = 1; seed <= 10 && !composed; seed++) {
        PieceRequest request = request_of(FORM_TEMPLATE_TERNARY, seed);
        request.given_melodies = 1;
        PiecePlan *plan = NULL;
        assert(piece_build_plan(&request, &plan) == PIECE_OK);
        NodeId b = section_with_letter(plan, "B");
        /* Bars 4 to 7: its downbeats are tones of the tonic, the tonic, a predominant and the dominant. */
        Motive melody = notes_of("E5:h G5:h G5:q F5:q E5:h A4:q B4:q C5:h D5:h B4:h");
        assert(plan_give_melody(plan, b, 3, melody.notes, melody.note_count) == GIVEN_OK);
        GivenSnapshot *snapshot = given_snapshot(plan);
        ComposeConfig config;
        ComposeOutcome outcome;
        memset(&config, 0, sizeof(config));
        config.seed = seed;
        if (compose_run(plan, &config, &outcome) == PIECE_OK) {
            composed = true;
            GivenReport runs[4];
            assert(given_snapshot_report(snapshot, plan, runs, 4) == 1 && runs[0].unchanged);
            /* The motive made from the first bar of the given melody is in the material, Given. */
            bool node_found = false;
            for (NodeId id = 1; id <= plan_node_count(plan); id++) {
                const char *name = plan_node_name(plan, id);
                if (name && strncmp(name, "given ", 6) == 0) {
                    node_found = true;
                    assert(plan_node_kind(plan, id) == PLAN_NODE_MOTIVE && plan_node_state(plan, id) == PLAN_STATE_GIVEN);
                }
            }
            assert(node_found);
            /* The melody is written before it and after it, every melody span written or Given. */
            PlanSpan section = plan_node_span(plan, b);
            LaneSpanId spans[16];
            size_t count = plan_lane_find(plan, PLAN_LANE_MELODY, 0, section, spans, 16);
            assert(count >= 3 && count <= 16);
            for (size_t i = 0; i < count; i++) {
                assert(plan_lane_span_state(plan, spans[i]) != PLAN_STATE_CONSTRAINED);
            }
        }
        given_snapshot_destroy(snapshot);
        compose_outcome_destroy(&outcome);
        plan_destroy(plan);
    }
    assert(composed);
}

static void test_sweep(ScenarioKind kind) {
    Share share = run_scenario(kind, 1, SEEDS, false);
    const Baseline *baseline = &BASELINES[kind];
    printf("%s, C major 4/4, seeds 1 to %d: completed %zu of %zu; sections heard %zu of %zu; cadences heard as planned "
           "%zu of %zu\n",
           SCENARIO_NAMES[kind], SEEDS, share.completed, share.runs, share.sections_heard, share.sections, share.cadences,
           share.sections);
    assert(share.completed >= baseline->completed);
    assert(share.sections_heard >= baseline->sections_heard);
    assert(share.cadences >= baseline->cadences);
}

int main(int argc, char **argv) {
    const char *mode = argc > 1 ? argv[1] : "give";
    if (strcmp(mode, "give") == 0) {
        test_give();
    } else if (strcmp(mode, "smoke") == 0) {
        test_smoke();
    } else if (strcmp(mode, "partial") == 0) {
        test_partial();
    } else if (strcmp(mode, "melody") == 0) {
        test_sweep(SCENARIO_MELODY);
    } else if (strcmp(mode, "progression") == 0) {
        test_sweep(SCENARIO_PROGRESSION);
    } else if (strcmp(mode, "bass") == 0) {
        test_sweep(SCENARIO_BASS);
    } else {
        fprintf(stderr, "unknown mode '%s'\n", mode);
        return 1;
    }
    return 0;
}
