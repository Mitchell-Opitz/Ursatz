#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/constraint.h>
#include <ursatz_composer/fill_loop.h>
#include <ursatz_composer/plan_json.h>
#include <ursatz_composer/plan_state.h>
#include <ursatz_composer/voicing.h>

#include "accompaniment_check.h"

/*
 * The voicing operator on short realized progressions, a chord a bar in 4/4: I IV V I, I ii6 V7 I in C major and
 * i iv6 V i in A minor, each with the soprano given as a written melody and with the soprano written by the voicing.
 * Every result is held to rules worked out here from the plan's lanes in plain semitones, not from the operator or from
 * the framework it uses: every chord tone sounding with nothing foreign to it, the voices in their ranges and spacing and
 * not crossed, the leading tone and the seventh not doubled, no parallel fifths or octaves, the seventh and the leading
 * tone resolved.
 */

#define SEEDS 10
#define CHORDS 4

typedef struct {
    const char *name;
    PlanKey key;
    const char *numeral[CHORDS];
    int bass[CHORDS];
    int melody[CHORDS];
    /* The pitch classes of each chord, root first, and the seventh and the leading tone of it (-1 for none). */
    int pcs[CHORDS][4];
    int pc_count[CHORDS];
    int seventh[CHORDS];
    int leading[CHORDS];
} Progression;

static const Progression PROGRESSIONS[] = {
    {"I IV V I in C major",
     {0, PLAN_MODE_MAJOR},
     {"I", "IV", "V", "I"},
     {48, 41, 43, 48},
     {76, 72, 71, 72},
     {{0, 4, 7}, {5, 9, 0}, {7, 11, 2}, {0, 4, 7}},
     {3, 3, 3, 3},
     {-1, -1, -1, -1},
     {-1, -1, 11, -1}},
    {"I ii6 V7 I in C major",
     {0, PLAN_MODE_MAJOR},
     {"I", "ii6", "V7", "I"},
     {48, 41, 43, 48},
     {76, 74, 71, 72},
     {{0, 4, 7}, {2, 5, 9}, {7, 11, 2, 5}, {0, 4, 7}},
     {3, 3, 4, 3},
     {-1, -1, 5, -1},
     {-1, -1, 11, -1}},
    {"i iv6 V i in A minor",
     {9, PLAN_MODE_MINOR},
     {"i", "iv6", "V", "i"},
     {45, 41, 52, 45},
     {72, 74, 68, 69},
     {{9, 0, 4}, {2, 5, 9}, {4, 8, 11}, {9, 0, 4}},
     {3, 3, 3, 3},
     {-1, -1, -1, -1},
     {-1, -1, 8, -1}},
};

typedef struct {
    PiecePlan *plan;
    NodeId owner;
    PlanSpan span;
    LaneSpanId alto;
} Piece;

static Rational fraction(long num, unsigned long den) {
    Rational time;
    assert(rational_make(num, den, &time) == RATIONAL_OK);
    return time;
}

static Piece build(const Progression *progression, bool write_soprano, int melody_override) {
    Piece piece;
    Rational bar = fraction(1, 1);
    PlanSpan all = {fraction(0, 1), fraction(CHORDS, 1)};
    PlanLanePayload payload;
    PlanNote notes[CHORDS];
    LaneSpanId id;
    memset(&piece, 0, sizeof(piece));
    piece.plan = plan_create();
    assert(plan_add_node(piece.plan, PLAN_NODE_NONE, PLAN_NODE_FORM, all, "form", &piece.owner) == PLAN_OK);
    piece.span = all;
    memset(&payload, 0, sizeof(payload));
    payload.meter.beats_per_bar = 4;
    payload.meter.beat_unit = 4;
    assert(plan_add_lane_span(piece.plan, piece.owner, PLAN_LANE_METER, 0, all, &payload, &id) == PLAN_OK);
    assert(plan_lane_span_set_state(piece.plan, id, PLAN_STATE_GIVEN) == PLAN_OK);
    memset(&payload, 0, sizeof(payload));
    payload.key = progression->key;
    assert(plan_add_lane_span(piece.plan, piece.owner, PLAN_LANE_KEY, 0, all, &payload, &id) == PLAN_OK);
    assert(plan_lane_span_set_state(piece.plan, id, PLAN_STATE_GIVEN) == PLAN_OK);
    for (size_t b = 0; b < CHORDS; b++) {
        PlanSpan span = {fraction((long)b, 1), bar};
        memset(&payload, 0, sizeof(payload));
        strcpy(payload.harmony.roman_numeral, progression->numeral[b]);
        payload.harmony.function = PLAN_FUNCTION_UNKNOWN;
        assert(plan_add_lane_span(piece.plan, piece.owner, PLAN_LANE_HARMONY, 0, span, &payload, &id) == PLAN_OK);
        assert(plan_lane_span_set_state(piece.plan, id, PLAN_STATE_REALIZED) == PLAN_OK);
        memset(&payload, 0, sizeof(payload));
        notes[0].midi_pitch = progression->bass[b];
        notes[0].offset = fraction(0, 1);
        notes[0].duration = bar;
        payload.notes.notes = notes;
        payload.notes.note_count = 1;
        assert(plan_add_lane_span(piece.plan, piece.owner, PLAN_LANE_BASS, 0, span, &payload, &id) == PLAN_OK);
        assert(plan_lane_span_set_state(piece.plan, id, PLAN_STATE_REALIZED) == PLAN_OK);
        if (!write_soprano) {
            notes[0].midi_pitch = b == 0 && melody_override ? melody_override : progression->melody[b];
            assert(plan_add_lane_span(piece.plan, piece.owner, PLAN_LANE_MELODY, 0, span, &payload, &id) == PLAN_OK);
            assert(plan_lane_span_set_state(piece.plan, id, PLAN_STATE_REALIZED) == PLAN_OK);
        }
    }
    assert(plan_node_set_state(piece.plan, piece.owner, PLAN_STATE_GIVEN) == PLAN_OK);

    /* The open spans: the melody when the voicing writes it, the tenor, and the alto with the constraint. */
    if (write_soprano) {
        PlanNote note = {60, fraction(0, 1), all.length};
        memset(&payload, 0, sizeof(payload));
        payload.notes.notes = &note;
        payload.notes.note_count = 1;
        assert(plan_add_lane_span(piece.plan, piece.owner, PLAN_LANE_MELODY, 0, all, &payload, &id) == PLAN_OK);
    }
    for (int voice = 2; voice >= 1; voice--) {
        PlanNote note = {55, fraction(0, 1), all.length};
        memset(&payload, 0, sizeof(payload));
        payload.notes.notes = &note;
        payload.notes.note_count = 1;
        assert(plan_add_lane_span(piece.plan, piece.owner, PLAN_LANE_INNER_VOICE, voice, all, &payload, &id) == PLAN_OK);
        if (voice == 1) {
            PlanConstraint constraint;
            memset(&constraint, 0, sizeof(constraint));
            constraint.kind = PLAN_CONSTRAINT_VOICING;
            constraint.params.voicing.write_soprano = write_soprano;
            assert(plan_lane_span_add_constraint(piece.plan, id, &constraint) == PLAN_OK);
            piece.alto = id;
        }
    }
    return piece;
}

static FillOutcome voice(PiecePlan *plan, unsigned long seed) {
    FillOperatorRegistry *registry = fill_registry_create();
    FillConfig config;
    FillOutcome outcome;
    memset(&config, 0, sizeof(config));
    config.seed = seed;
    config.checker = voicing_checker;
    assert(registry && voicing_register(registry, NULL) == FILL_OK);
    assert(fill_run(plan, registry, &config, &outcome, NULL) == FILL_OK);
    fill_registry_destroy(registry);
    return outcome;
}

/* ------------------------------------------------------- the rules */

static int pitch_at_bar(const Piece *piece, PlanLaneKind lane, int voice, size_t bar) {
    CheckNote notes[CHECK_MAX_NOTES];
    size_t count = check_notes(piece->plan, lane, voice, piece->span, notes);
    for (size_t n = 0; n < count; n++) {
        if (notes[n].onset == (long)bar * CHECK_TICKS) {
            assert(notes[n].end == notes[n].onset + CHECK_TICKS);
            return notes[n].pitch;
        }
    }
    assert(!"a note on the chord");
    return -1;
}

static bool has_pc(const int *pcs, int count, int pc) {
    for (int i = 0; i < count; i++) {
        if (pcs[i] == pc) {
            return true;
        }
    }
    return false;
}

static int signed_up(int from, int to) {
    return to - from;
}

/* Voices 0 to 3 are the soprano, alto, tenor and bass. */
static void check_voicing(const Piece *piece, const Progression *progression, bool write_soprano) {
    int voices[CHORDS][4];
    for (size_t b = 0; b < CHORDS; b++) {
        voices[b][0] = pitch_at_bar(piece, PLAN_LANE_MELODY, 0, b);
        voices[b][1] = pitch_at_bar(piece, PLAN_LANE_INNER_VOICE, 1, b);
        voices[b][2] = pitch_at_bar(piece, PLAN_LANE_INNER_VOICE, 2, b);
        voices[b][3] = pitch_at_bar(piece, PLAN_LANE_BASS, 0, b);
        assert(voices[b][3] == progression->bass[b]);
        if (!write_soprano) {
            assert(voices[b][0] == progression->melody[b]);
        }
        /* The chord: every tone sounds and nothing else does. */
        for (int v = 0; v < 4; v++) {
            assert(has_pc(progression->pcs[b], progression->pc_count[b], voices[b][v] % 12));
        }
        for (int k = 0; k < progression->pc_count[b]; k++) {
            bool sounds = false;
            for (int v = 0; v < 4; v++) {
                sounds = sounds || voices[b][v] % 12 == progression->pcs[b][k];
            }
            assert(sounds);
        }
        /* Ranges of what is written, spacing and crossing. */
        if (write_soprano) {
            assert(voices[b][0] >= 60 && voices[b][0] <= 79);
        }
        assert(voices[b][1] >= 55 && voices[b][1] <= 74);
        assert(voices[b][2] >= 48 && voices[b][2] <= 67);
        assert(voices[b][0] >= voices[b][1] && voices[b][1] >= voices[b][2] && voices[b][2] >= voices[b][3]);
        assert(voices[b][0] - voices[b][1] <= 12 && voices[b][1] - voices[b][2] <= 12 && voices[b][2] - voices[b][3] <= 19);
        /* Doubling: the leading tone and the seventh are not doubled, and a seventh chord doubles nothing. */
        for (int v = 0; v < 4; v++) {
            int same = 0;
            for (int w = 0; w < 4; w++) {
                same += voices[b][v] % 12 == voices[b][w] % 12;
            }
            if (voices[b][v] % 12 == progression->leading[b] || voices[b][v] % 12 == progression->seventh[b]) {
                assert(same == 1);
            }
            if (progression->pc_count[b] == 4) {
                assert(same == 1);
            }
        }
    }
    for (size_t b = 1; b < CHORDS; b++) {
        for (int upper = 0; upper < 4; upper++) {
            for (int lower = upper + 1; lower < 4; lower++) {
                int before = (voices[b - 1][upper] - voices[b - 1][lower]) % 12;
                int after = (voices[b][upper] - voices[b][lower]) % 12;
                int up_moved = signed_up(voices[b - 1][upper], voices[b][upper]);
                int low_moved = signed_up(voices[b - 1][lower], voices[b][lower]);
                bool perfect = before == after && (before == 7 || before == 0);
                bool same_way = (up_moved > 0 && low_moved > 0) || (up_moved < 0 && low_moved < 0);
                if (perfect && same_way) {
                    fprintf(stderr, "%s: parallels in voices %d and %d at chord %zu\n", progression->name, upper, lower, b);
                }
                assert(!(perfect && same_way));
            }
        }
        /* The seventh falls a step; the leading tone rises a step, or falls a third in an inner voice of a complete V7. */
        for (int v = 0; v < 3; v++) {
            int moved = signed_up(voices[b - 1][v], voices[b][v]);
            if (voices[b - 1][v] % 12 == progression->seventh[b - 1]) {
                assert(moved == -1 || moved == -2);
            }
            if (voices[b - 1][v] % 12 == progression->leading[b - 1]) {
                assert(moved == 1 || (v > 0 && progression->pc_count[b - 1] == 4 && (moved == -3 || moved == -4)));
            }
        }
    }
}

/* ------------------------------------------------------- the tests */

static void test_progressions(bool write_soprano) {
    for (size_t p = 0; p < sizeof(PROGRESSIONS) / sizeof(PROGRESSIONS[0]); p++) {
        for (unsigned long seed = 1; seed <= SEEDS; seed++) {
            Piece piece = build(&PROGRESSIONS[p], write_soprano, 0);
            FillOutcome outcome = voice(piece.plan, seed);
            if (outcome.status != FILL_STATUS_COMPLETE) {
                fprintf(stderr, "%s, soprano %s, seed %lu: not complete (%zu stuck)\n", PROGRESSIONS[p].name,
                        write_soprano ? "written" : "given", seed, outcome.stuck_count);
            }
            assert(outcome.status == FILL_STATUS_COMPLETE);
            check_voicing(&piece, &PROGRESSIONS[p], write_soprano);

            /* The same seed gives the same voicing. */
            Piece again = build(&PROGRESSIONS[p], write_soprano, 0);
            FillOutcome second = voice(again.plan, seed);
            char *a = plan_to_json(piece.plan, NULL);
            char *b = plan_to_json(again.plan, NULL);
            assert(a && b && strcmp(a, b) == 0);
            free(a);
            free(b);
            fill_outcome_destroy(&second);
            fill_outcome_destroy(&outcome);
            plan_destroy(again.plan);
            plan_destroy(piece.plan);
        }
    }
}

/* A soprano below the bass leaves no voicing: the alto is stuck, and so is the tenor with it, and nothing is written. */
static void test_soprano_below_the_bass(void) {
    Piece piece = build(&PROGRESSIONS[0], false, 40);
    FillOutcome outcome = voice(piece.plan, 1);
    bool alto_stuck = false;
    assert(outcome.status == FILL_STATUS_STUCK && outcome.stuck_count == 2);
    for (size_t i = 0; i < outcome.stuck_count; i++) {
        assert(outcome.stuck[i].target.kind == FILL_TARGET_LANE_SPAN);
        if (outcome.stuck[i].target.id == piece.alto) {
            assert(outcome.stuck[i].reason == FILL_STUCK_ALL_REJECTED);
            alto_stuck = true;
        } else {
            assert(outcome.stuck[i].reason == FILL_STUCK_NO_OPERATOR);
        }
    }
    assert(alto_stuck);
    assert(plan_lane_span_state(piece.plan, piece.alto) == PLAN_STATE_CONSTRAINED);
    fill_outcome_destroy(&outcome);
    plan_destroy(piece.plan);
}

int main(void) {
    test_progressions(false);
    test_progressions(true);
    test_soprano_below_the_bass();
    printf("test_voicing: ok\n");
    return 0;
}
