#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <plan/plan_constraint.h>
#include <plan/plan_state.h>
#include <ursatz_composer/fill_loop.h>
#include <ursatz_composer/melody_fill.h>
#include <ursatz_composer/motive.h>
#include <ursatz_composer/motive_blueprint.h>

#include "support/test_knowledge.h"
#include "support/test_support.h"

/*
 * The melody stage over a blueprint (R-746): a rhythm-only motive is realized with its onsets, an anchor's note takes its pitch,
 * an event is placed as its non-chord tone, and the climax is the one highest note. One phrase of four bars in 4/4, C major,
 * over given chords and bass (I IV V I), the melody one Constrained span holding the motive.
 */

#define SEEDS 10
#define BARS 4
/* The pitch classes of the chords: C E G, C F A, D G B. */
#define PCS_I 0x091u
#define PCS_IV 0x221u
#define PCS_V 0x884u

static const struct {
    const char *numeral;
    int bass;
    unsigned pcs;
} CHORDS[BARS] = {{"I", 48, PCS_I}, {"IV", 53, PCS_IV}, {"V", 55, PCS_V}, {"I", 48, PCS_I}};

/* The rhythm of the motive, the whole phrase: 5 + 3 + 4 + 1 notes. */
#define RHYTHM "x:q x:e x:e x:q x:q | x:h x:q x:q | x:q. x:e x:q x:q | x:w"
#define RHYTHM_NOTES 13

typedef struct {
    Plan *plan;
    NodeId form;
    NodeId section;
    LaneSpanId target;
    PlanSpan span;
} Piece;

static Rational fraction(long num, unsigned long den) {
    Rational time;
    assert(rational_make(num, den, &time) == RATIONAL_OK);
    return time;
}

static SpelledKey c_major(void) {
    return test_key_of((TestKeySpec){PITCH_LETTER_C, 0, HARMONY_MODE_MAJOR});
}

static MotiveOptions options_of(void) {
    MotiveOptions options;
    memset(&options, 0, sizeof(options));
    options.knowledge = test_knowledge();
    options.has_meter = true;
    options.meter = (PlanMeter){4, 4, 0};
    options.has_key = true;
    options.key = c_major();
    return options;
}

static void add_notes(Plan *plan, NodeId owner, PlanLaneKind lane, PlanSpan span, int pitch, PlanState state, LaneSpanId *out) {
    PlanNote note = plan_note(pitch, fraction(0, 1), span.length);
    PlanLanePayload payload;
    LaneSpanId id;
    memset(&payload, 0, sizeof(payload));
    payload.notes.notes = &note;
    payload.notes.note_count = 1;
    assert(plan_add_lane_span(plan, owner, lane, 0, span, &payload, &id) == PLAN_OK);
    if (state != PLAN_STATE_CONSTRAINED) {
        assert(plan_lane_span_set_state(plan, id, state) == PLAN_OK);
    }
    if (out) {
        *out = id;
    }
}

/*
 * The form of eight bars: the phrase's section over the first four (chords, a bass note a bar, the melody target), and the
 * material section after it holding the motive, rhythm-only or full.
 */
static Piece build(bool rhythm_only) {
    Piece piece;
    PlanLanePayload payload;
    LaneSpanId id;
    PlanSpan form = {fraction(0, 1), fraction(2 * BARS, 1)};
    PlanSpan after = {fraction(BARS, 1), fraction(BARS, 1)};
    memset(&piece, 0, sizeof(piece));
    piece.span = (PlanSpan){fraction(0, 1), fraction(BARS, 1)};
    piece.plan = plan_create();
    assert(plan_add_node(piece.plan, PLAN_NODE_NONE, PLAN_NODE_FORM, form, "form", &piece.form) == PLAN_OK);
    assert(plan_add_node(piece.plan, piece.form, PLAN_NODE_SECTION, piece.span, "section", &piece.section) == PLAN_OK);
    memset(&payload, 0, sizeof(payload));
    payload.meter = (PlanMeter){4, 4, 0};
    assert(plan_add_lane_span(piece.plan, piece.form, PLAN_LANE_METER, 0, form, &payload, &id) == PLAN_OK);
    assert(plan_lane_span_set_state(piece.plan, id, PLAN_STATE_GIVEN) == PLAN_OK);
    memset(&payload, 0, sizeof(payload));
    payload.region = c_major();
    assert(plan_add_lane_span(piece.plan, piece.form, PLAN_LANE_REGION, 0, form, &payload, &id) == PLAN_OK);
    assert(plan_lane_span_set_state(piece.plan, id, PLAN_STATE_GIVEN) == PLAN_OK);
    for (size_t bar = 0; bar < BARS; bar++) {
        PlanSpan span = {fraction((long)bar, 1), fraction(1, 1)};
        memset(&payload, 0, sizeof(payload));
        strcpy(payload.harmony.roman_numeral, CHORDS[bar].numeral);
        assert(plan_add_lane_span(piece.plan, piece.section, PLAN_LANE_HARMONY, 0, span, &payload, &id) == PLAN_OK);
        assert(plan_lane_span_set_state(piece.plan, id, PLAN_STATE_REALIZED) == PLAN_OK);
        add_notes(piece.plan, piece.section, PLAN_LANE_BASS, span, CHORDS[bar].bass, PLAN_STATE_REALIZED, NULL);
    }
    MotiveOptions options = options_of();
    Motive motive;
    MotiveError error;
    NodeId material;
    NodeId phrase;
    NodeId idea;
    NodeId source;
    assert(plan_add_node(piece.plan, piece.form, PLAN_NODE_SECTION, after, "material", &material) == PLAN_OK);
    assert(plan_add_node(piece.plan, material, PLAN_NODE_PHRASE, after, NULL, &phrase) == PLAN_OK);
    assert(plan_add_node(piece.plan, phrase, PLAN_NODE_IDEA, after, NULL, &idea) == PLAN_OK);
    if (rhythm_only) {
        assert(motive_from_rhythm_text(RHYTHM, &options, &motive, &error) == MOTIVE_OK);
        assert(composer_rhythm_motive_attach(piece.plan, idea, after.start, &motive, "rhythm", &source, NULL) == PLAN_OK);
    } else {
        assert(motive_from_notes_text("E4:q G4:e F4:e D4:q C4:q", &options, &motive, &error) == MOTIVE_OK);
        assert(motive_attach(piece.plan, idea, after.start, &motive, "motive", &source, NULL) == PLAN_OK);
    }
    assert(plan_node_set_state(piece.plan, material, PLAN_STATE_REALIZED) == PLAN_OK);
    assert(plan_node_set_state(piece.plan, phrase, PLAN_STATE_REALIZED) == PLAN_OK);
    assert(plan_node_set_state(piece.plan, idea, PLAN_STATE_REALIZED) == PLAN_OK);
    /* The target, with its motive. */
    PlanConstraint ref;
    memset(&ref, 0, sizeof(ref));
    ref.kind = PLAN_CONSTRAINT_MOTIVE_REF;
    ref.params.motive.motive = source;
    add_notes(piece.plan, piece.section, PLAN_LANE_MELODY, piece.span, 60, PLAN_STATE_CONSTRAINED, &piece.target);
    assert(plan_lane_span_add_constraint(piece.plan, piece.target, &ref) == PLAN_OK);
    assert(plan_node_set_state(piece.plan, piece.section, PLAN_STATE_REALIZED) == PLAN_OK);
    assert(plan_node_set_state(piece.plan, piece.form, PLAN_STATE_REALIZED) == PLAN_OK);
    return piece;
}

static void add_anchor(Piece *piece, Rational offset, int midi) {
    PlanConstraint constraint;
    memset(&constraint, 0, sizeof(constraint));
    constraint.kind = PLAN_CONSTRAINT_ANCHOR;
    constraint.params.anchor = (PlanAnchor){offset, midi};
    assert(plan_lane_span_add_constraint(piece->plan, piece->target, &constraint) == PLAN_OK);
}

static void add_event(Piece *piece, Rational offset, PlanEventType type) {
    PlanConstraint constraint;
    memset(&constraint, 0, sizeof(constraint));
    constraint.kind = PLAN_CONSTRAINT_EVENT;
    constraint.params.event = (PlanEvent){offset, type};
    assert(plan_lane_span_add_constraint(piece->plan, piece->target, &constraint) == PLAN_OK);
}

static void add_climax(Piece *piece, Rational offset, int midi) {
    PlanConstraint constraint;
    memset(&constraint, 0, sizeof(constraint));
    constraint.kind = PLAN_CONSTRAINT_CLIMAX;
    constraint.params.climax = (PlanClimax){offset, midi};
    assert(plan_lane_span_add_constraint(piece->plan, piece->target, &constraint) == PLAN_OK);
}

/* Fills the melody with the seed; the notes of the target when it completes, else NULL. */
static const PlanNotes *fill(Piece *piece, unsigned long seed) {
    FillOperatorRegistry *registry = fill_registry_create();
    FillConfig config;
    FillOutcome outcome;
    memset(&config, 0, sizeof(config));
    config.seed = seed;
    config.checker = melody_fill_checker;
    assert(registry && melody_fill_register(registry, NULL, test_composer_chords()) == FILL_OK);
    assert(fill_run(piece->plan, registry, &config, &outcome, NULL) == FILL_OK);
    bool complete = outcome.status == FILL_STATUS_COMPLETE;
    fill_outcome_destroy(&outcome);
    fill_registry_destroy(registry);
    return complete ? &plan_lane_span_payload(piece->plan, piece->target)->notes : NULL;
}

/* The note of the melody that starts at an offset, or -1. */
static int note_at(const PlanNotes *notes, Rational offset) {
    for (size_t i = 0; i < notes->note_count; i++) {
        if (rational_cmp(notes->notes[i].offset, offset) == 0) {
            return (int)i;
        }
    }
    return -1;
}

/* The pitch classes of the chord sounding at an offset (a chord a bar). */
static unsigned chord_at(Rational offset) {
    return CHORDS[offset.num / (long)offset.den].pcs;
}

/* The rhythm-only motive's notes are the melody's, onset for onset: the stage gives them pitches and fills nothing after them. */
static void test_rhythm_only(void) {
    MotiveOptions options = options_of();
    Motive rhythm;
    MotiveError error;
    size_t completed = 0;
    assert(motive_from_rhythm_text(RHYTHM, &options, &rhythm, &error) == MOTIVE_OK);
    assert(rhythm.note_count == RHYTHM_NOTES);
    for (unsigned long seed = 1; seed <= SEEDS; seed++) {
        Piece piece = build(true);
        const PlanNotes *notes = fill(&piece, seed);
        if (notes) {
            bool moves = false;
            completed++;
            assert(notes->note_count == RHYTHM_NOTES);
            for (size_t i = 0; i < RHYTHM_NOTES; i++) {
                assert(rational_cmp(notes->notes[i].offset, rhythm.notes[i].onset) == 0);
                assert(rational_cmp(notes->notes[i].duration, rhythm.notes[i].duration) == 0);
                /* No link to the motive: the notes are the stage's own. */
                assert(notes->notes[i].operation != PLAN_NOTE_FROM_MOTIVE);
                moves = moves || notes->notes[i].midi != notes->notes[0].midi;
            }
            /* The melody is not the rhythm's degree 1 throughout. */
            for (size_t i = 0; getenv("SHOW") && i < RHYTHM_NOTES; i++) {
                fprintf(stderr, "%d%s", notes->notes[i].midi, i + 1 < RHYTHM_NOTES ? " " : "\n");
            }
            assert(moves);
        }
        plan_destroy(piece.plan);
    }
    assert(completed >= SEEDS / 2);
}

/* An anchor's note takes its pitch: E5 on the fourth note (beat 3 of bar 1). */
static void test_anchor(void) {
    size_t completed = 0;
    for (unsigned long seed = 1; seed <= SEEDS; seed++) {
        Piece piece = build(true);
        add_anchor(&piece, fraction(1, 2), 76);
        add_anchor(&piece, fraction(3, 2), 72);
        const PlanNotes *notes = fill(&piece, seed);
        if (notes) {
            completed++;
            assert(notes->notes[note_at(notes, fraction(1, 2))].midi == 76);
            assert(notes->notes[note_at(notes, fraction(3, 2))].midi == 72);
        }
        plan_destroy(piece.plan);
    }
    assert(completed >= SEEDS / 2);
}

/* An appoggiatura on the downbeat of bar 2 (over IV): not a chord tone, and a step from the note after it. */
static void test_event(void) {
    size_t completed = 0;
    for (unsigned long seed = 1; seed <= SEEDS; seed++) {
        Piece piece = build(true);
        add_event(&piece, fraction(1, 1), PLAN_EVENT_APPOGGIATURA);
        const PlanNotes *notes = fill(&piece, seed);
        if (notes) {
            completed++;
            int i = note_at(notes, fraction(1, 1));
            int pitch = notes->notes[i].midi;
            int step = notes->notes[i + 1].midi - pitch;
            assert(!((chord_at(fraction(1, 1)) >> (pitch % 12)) & 1u));
            assert(step != 0 && step >= -2 && step <= 2);
        }
        plan_destroy(piece.plan);
    }
    assert(completed >= SEEDS / 2);
    /* An anchor and an event at one offset: the note takes the anchor's pitch and is the event (no chord tone of IV needed). */
    completed = 0;
    for (unsigned long seed = 1; seed <= SEEDS; seed++) {
        Piece piece = build(true);
        add_anchor(&piece, fraction(1, 1), 74);
        add_event(&piece, fraction(1, 1), PLAN_EVENT_APPOGGIATURA);
        const PlanNotes *notes = fill(&piece, seed);
        if (notes) {
            completed++;
            assert(notes->notes[note_at(notes, fraction(1, 1))].midi == 74);
        }
        plan_destroy(piece.plan);
    }
    assert(completed >= 1);
}

/* The climax G5 on the downbeat of bar 3 (over V): that note has it, and no other note reaches it. */
static void test_climax(void) {
    size_t completed = 0;
    for (unsigned long seed = 1; seed <= SEEDS; seed++) {
        Piece piece = build(true);
        add_climax(&piece, fraction(2, 1), 79);
        const PlanNotes *notes = fill(&piece, seed);
        if (notes) {
            completed++;
            int at = note_at(notes, fraction(2, 1));
            assert(notes->notes[at].midi == 79);
            for (size_t i = 0; i < notes->note_count; i++) {
                assert((int)i == at || notes->notes[i].midi < 79);
            }
        }
        plan_destroy(piece.plan);
    }
    assert(completed >= SEEDS / 2);
}

/* Without the constraints, a full motive composes as before: seed 3's notes, pinned. */
static const int PINNED[] = {76, 79, 76, 72, 72, 69, 71, 72, 72, 77, 74, 79, 81, 83, 79, 76, 77, 79, 79, 72};

static void test_unchanged(void) {
    Piece piece = build(false);
    const PlanNotes *notes = fill(&piece, 3);
    assert(notes);
    assert(notes->note_count == sizeof(PINNED) / sizeof(PINNED[0]));
    for (size_t i = 0; i < notes->note_count; i++) {
        assert(notes->notes[i].midi == PINNED[i]);
    }
    plan_destroy(piece.plan);
}

int main(void) {
    test_unchanged();
    test_rhythm_only();
    test_anchor();
    test_event();
    test_climax();
    printf("test_melody_blueprint: ok\n");
    return 0;
}
