#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/accompaniment.h>
#include <ursatz_composer/constraint.h>
#include <ursatz_composer/fill_loop.h>
#include <ursatz_composer/melody_fill.h>
#include <ursatz_composer/motive.h>
#include <ursatz_composer/plan_json.h>
#include <ursatz_composer/plan_state.h>
#include <ursatz_composer/voicing.h>

#include "accompaniment_check.h"

/*
 * Melody, accompaniment and voicing over spans that start and end on any beat, and melody pickups (R-168). Everything
 * is in 4/4 in C major; spans are counted in beats from the start of the plan, and the expectations are worked out in
 * beats and pitch classes, apart from the code under test.
 */

#define SEEDS 8
#define TICKS_PER_BEAT 32L
/* The pitch classes of the chords of C major: C E G, C F A and D G B. */
#define C_MAJOR_I 0x091u
#define C_MAJOR_IV 0x221u
#define C_MAJOR_V 0x884u

typedef struct {
    const char *numeral;
    long start;  /* in beats from the start of the plan */
    long length; /* in beats */
    int bass;
    int melody;  /* a held note over the chord, 0 for none */
    unsigned pcs; /* the pitch classes of the chord, bit pc */
} Chord;

typedef struct {
    PiecePlan *plan;
    NodeId form;
    NodeId owner; /* what the lanes belong to: a section of the form */
    PlanSpan span; /* the melody target's span, the pickup included */
    PlanSpan body; /* what follows the pickup */
    LaneSpanId target;
} Piece;

static Rational fraction(long num, unsigned long den) {
    Rational time;
    assert(rational_make(num, den, &time) == RATIONAL_OK);
    return time;
}

static Rational beats(long count) {
    return fraction(count, 4);
}

static PlanKey c_major(void) {
    PlanKey key = {0, PLAN_MODE_MAJOR};
    return key;
}

static LaneSpanId add_notes(PiecePlan *plan, NodeId owner, PlanLaneKind lane, int voice, PlanSpan span, int pitch,
                            PlanState state) {
    PlanNote note;
    PlanLanePayload payload;
    LaneSpanId id;
    memset(&payload, 0, sizeof(payload));
    note.midi_pitch = pitch;
    note.offset = fraction(0, 1);
    note.duration = span.length;
    payload.notes.notes = &note;
    payload.notes.note_count = 1;
    assert(plan_add_lane_span(plan, owner, lane, voice, span, &payload, &id) == PLAN_OK);
    if (state != PLAN_STATE_CONSTRAINED) {
        assert(plan_lane_span_set_state(plan, id, state) == PLAN_OK);
    }
    return id;
}

/* A form of 12 bars with a section over the first 8, a meter and a key; the chords, a bass note and a melody note each. */
static Piece build(const Chord *chords, size_t count, bool with_melody) {
    Piece piece;
    PlanLanePayload payload;
    LaneSpanId id;
    PlanSpan form = {fraction(0, 1), beats(48)};
    PlanSpan section = {fraction(0, 1), beats(32)};
    memset(&piece, 0, sizeof(piece));
    piece.plan = plan_create();
    assert(plan_add_node(piece.plan, PLAN_NODE_NONE, PLAN_NODE_FORM, form, "form", &piece.form) == PLAN_OK);
    assert(plan_add_node(piece.plan, piece.form, PLAN_NODE_SECTION, section, "section", &piece.owner) == PLAN_OK);
    memset(&payload, 0, sizeof(payload));
    payload.meter.beats_per_bar = 4;
    payload.meter.beat_unit = 4;
    assert(plan_add_lane_span(piece.plan, piece.form, PLAN_LANE_METER, 0, form, &payload, &id) == PLAN_OK);
    assert(plan_lane_span_set_state(piece.plan, id, PLAN_STATE_GIVEN) == PLAN_OK);
    memset(&payload, 0, sizeof(payload));
    payload.key = c_major();
    assert(plan_add_lane_span(piece.plan, piece.form, PLAN_LANE_KEY, 0, form, &payload, &id) == PLAN_OK);
    assert(plan_lane_span_set_state(piece.plan, id, PLAN_STATE_GIVEN) == PLAN_OK);
    for (size_t i = 0; i < count; i++) {
        PlanSpan span = {beats(chords[i].start), beats(chords[i].length)};
        memset(&payload, 0, sizeof(payload));
        strcpy(payload.harmony.roman_numeral, chords[i].numeral);
        payload.harmony.has_function = false;
        assert(plan_add_lane_span(piece.plan, piece.owner, PLAN_LANE_HARMONY, 0, span, &payload, &id) == PLAN_OK);
        assert(plan_lane_span_set_state(piece.plan, id, PLAN_STATE_REALIZED) == PLAN_OK);
        add_notes(piece.plan, piece.owner, PLAN_LANE_BASS, 0, span, chords[i].bass, PLAN_STATE_REALIZED);
        if (with_melody && chords[i].melody) {
            add_notes(piece.plan, piece.owner, PLAN_LANE_MELODY, 0, span, chords[i].melody, PLAN_STATE_REALIZED);
        }
    }
    assert(plan_node_set_state(piece.plan, piece.owner, PLAN_STATE_REALIZED) == PLAN_OK);
    assert(plan_node_set_state(piece.plan, piece.form, PLAN_STATE_GIVEN) == PLAN_OK);
    return piece;
}

/* A Given motive of a bar, "E4:q G4:e F4:e D4:q C4:q", in the form after the section. */
static NodeId add_melody_motive(Piece *piece) {
    Motive motive;
    MotiveOptions options;
    NodeId section;
    NodeId phrase;
    NodeId idea;
    NodeId source;
    PlanSpan material = {beats(40), beats(4)};
    memset(&options, 0, sizeof(options));
    options.has_meter = true;
    options.meter.beats_per_bar = 4;
    options.meter.beat_unit = 4;
    assert(motive_from_notes_text("E4:q G4:e F4:e D4:q C4:q", &options, &motive, NULL) == MOTIVE_OK);
    assert(plan_add_node(piece->plan, piece->form, PLAN_NODE_SECTION, material, NULL, &section) == PLAN_OK);
    assert(plan_add_node(piece->plan, section, PLAN_NODE_PHRASE, material, NULL, &phrase) == PLAN_OK);
    assert(plan_add_node(piece->plan, phrase, PLAN_NODE_IDEA, material, NULL, &idea) == PLAN_OK);
    assert(motive_attach(piece->plan, idea, material.start, &motive, "motive", &source, NULL) == PLAN_OK);
    assert(plan_node_set_state(piece->plan, section, PLAN_STATE_REALIZED) == PLAN_OK);
    assert(plan_node_set_state(piece->plan, phrase, PLAN_STATE_REALIZED) == PLAN_OK);
    assert(plan_node_set_state(piece->plan, idea, PLAN_STATE_REALIZED) == PLAN_OK);
    return source;
}

/* The accompaniment motive of the texture tests, C4:e G4:e E4:e G4:e, as the node "acc" after the section. */
static void add_accompaniment_motive(Piece *piece) {
    Motive motive;
    NodeId section;
    NodeId phrase;
    NodeId idea;
    PlanSpan material = {beats(40), beats(4)};
    static const int PITCHES[4] = {60, 67, 64, 67};
    memset(&motive, 0, sizeof(motive));
    motive.meter.beats_per_bar = 4;
    motive.meter.beat_unit = 4;
    motive.key = c_major();
    motive.key_source = MOTIVE_KEY_GIVEN;
    motive.role = MOTIVE_ROLE_ACCOMPANIMENT;
    for (size_t n = 0; n < 4; n++) {
        motive.notes[n].midi_pitch = PITCHES[n];
        motive.notes[n].offset = fraction((long)n, 8);
        motive.notes[n].duration = fraction(1, 8);
    }
    motive.note_count = 4;
    motive.length = fraction(1, 2);
    assert(plan_add_node(piece->plan, piece->form, PLAN_NODE_SECTION, material, "material", &section) == PLAN_OK);
    assert(plan_add_node(piece->plan, section, PLAN_NODE_PHRASE, material, "material", &phrase) == PLAN_OK);
    assert(plan_add_node(piece->plan, phrase, PLAN_NODE_IDEA, material, "material", &idea) == PLAN_OK);
    assert(motive_attach(piece->plan, idea, material.start, &motive, "acc", NULL, NULL) == PLAN_OK);
    assert(plan_node_set_state(piece->plan, section, PLAN_STATE_REALIZED) == PLAN_OK);
    assert(plan_node_set_state(piece->plan, phrase, PLAN_STATE_REALIZED) == PLAN_OK);
    assert(plan_node_set_state(piece->plan, idea, PLAN_STATE_REALIZED) == PLAN_OK);
}

/* The melody span to fill, with its motive and, when pickup_32nds is not 0, a pickup of that many 32nds of a whole note. */
static void add_melody_target(Piece *piece, PlanSpan span, NodeId motive, long pickup_32nds) {
    PlanConstraint ref = {PLAN_CONSTRAINT_MOTIVE_REF, {0}};
    Rational pickup = fraction(pickup_32nds, 32);
    piece->span = span;
    piece->body = span;
    piece->target = add_notes(piece->plan, piece->owner, PLAN_LANE_MELODY, 0, span, 60, PLAN_STATE_CONSTRAINED);
    ref.params.motive.motive = motive;
    ref.params.motive.transform = PLAN_TRANSFORM_NONE;
    assert(plan_lane_span_add_constraint(piece->plan, piece->target, &ref) == PLAN_OK);
    if (pickup_32nds > 0) {
        PlanConstraint constraint;
        memset(&constraint, 0, sizeof(constraint));
        constraint.kind = PLAN_CONSTRAINT_PICKUP;
        constraint.params.pickup = pickup;
        assert(plan_lane_span_add_constraint(piece->plan, piece->target, &constraint) == PLAN_OK);
        assert(rational_add(span.start, pickup, &piece->body.start) == RATIONAL_OK);
        assert(rational_sub(span.length, pickup, &piece->body.length) == RATIONAL_OK);
    }
}

static FillOutcome run_melody(Piece *piece, unsigned long seed) {
    FillOperatorRegistry *registry = fill_registry_create();
    FillConfig config;
    FillOutcome outcome;
    memset(&config, 0, sizeof(config));
    config.seed = seed;
    config.checker = melody_fill_checker;
    assert(registry && melody_fill_register(registry, NULL) == FILL_OK);
    assert(fill_run(piece->plan, registry, &config, &outcome, NULL) == FILL_OK);
    fill_registry_destroy(registry);
    return outcome;
}

typedef struct {
    long onset; /* in ticks from the start of the plan */
    long end;
    int pitch;
} Note;

static size_t notes_of(const Piece *piece, PlanLaneKind lane, int voice, PlanSpan span, Note *out) {
    CheckNote found[CHECK_MAX_NOTES];
    size_t count = check_notes(piece->plan, lane, voice, span, found);
    for (size_t i = 0; i < count; i++) {
        out[i].onset = found[i].onset;
        out[i].end = found[i].end;
        out[i].pitch = found[i].pitch;
    }
    return count;
}

/* The chord sounding at a tick of the plan. */
static const Chord *chord_at(const Chord *chords, size_t count, long tick) {
    for (size_t i = 0; i < count; i++) {
        if (tick >= chords[i].start * TICKS_PER_BEAT && tick < (chords[i].start + chords[i].length) * TICKS_PER_BEAT) {
            return &chords[i];
        }
    }
    assert(!"a chord at the tick");
    return NULL;
}

static bool in_chord(const Chord *chord, int pitch) {
    return (chord->pcs >> (pitch % 12)) & 1u;
}

/* ------------------------------------------------------------ melody */

typedef void (*Check)(const Chord *chords, size_t count, const Piece *piece);

/* Fills the melody target for each seed, checks the ones that complete, and returns how many did. */
static size_t fill_seeds(const Chord *chords, size_t count, PlanSpan span, long pickup_32nds, Check check) {
    size_t completed = 0;
    for (unsigned long seed = 0; seed < SEEDS; seed++) {
        Piece piece = build(chords, count, false);
        NodeId motive = add_melody_motive(&piece);
        add_melody_target(&piece, span, motive, pickup_32nds);
        FillOutcome outcome = run_melody(&piece, seed);
        if (outcome.status == FILL_STATUS_COMPLETE) {
            completed++;
            check(chords, count, &piece);
        }
        fill_outcome_destroy(&outcome);
        plan_destroy(piece.plan);
    }
    return completed;
}

/* The melody tiles its span, every strong beat of the barline grid holds a chord tone, and the checker agrees. */
static void check_beat_melody(const Chord *chords, size_t count, const Piece *piece) {
    Note notes[CHECK_MAX_NOTES];
    size_t found = notes_of(piece, PLAN_LANE_MELODY, 0, piece->span, notes);
    long start = check_ticks(piece->span.start);
    long end = start + check_ticks(piece->span.length);
    assert(found > 0 && notes[0].onset == start && notes[found - 1].end == end);
    for (size_t n = 0; n < found; n++) {
        assert(n == 0 || notes[n].onset == notes[n - 1].end);
        /* A cell of 4/4 is two beats from a barline: beats 1 and 3 are strong. */
        if (notes[n].onset % (2 * TICKS_PER_BEAT) == 0) {
            assert(in_chord(chord_at(chords, count, notes[n].onset), notes[n].pitch));
        }
        assert(notes[n].pitch >= 60 && notes[n].pitch <= 79);
    }
    const char *violation = "unread";
    assert(melody_check_lane(piece->plan, piece->span, c_major(), NULL, &violation));
    assert(violation == NULL);
}

/* A span of 7 beats starting on beat 3 holds three cells and a part cell of a beat as its tail. */
static void test_seven_beats_from_beat_three(void) {
    static const Chord chords[4] = {{"I", 2, 2, 48, 0, C_MAJOR_I},
                                    {"I", 4, 2, 48, 0, C_MAJOR_I},
                                    {"V", 6, 2, 43, 0, C_MAJOR_V},
                                    {"I", 8, 1, 48, 0, C_MAJOR_I}};
    PlanSpan span = {beats(2), beats(7)};
    assert(fill_seeds(chords, 4, span, 0, check_beat_melody) > 0);
}

/* A span of 7 beats starting on beat 2 has a lead of a beat before its first cell. */
static void test_seven_beats_from_beat_two(void) {
    static const Chord chords[4] = {{"I", 1, 1, 48, 0, C_MAJOR_I},
                                    {"I", 2, 2, 48, 0, C_MAJOR_I},
                                    {"V", 4, 2, 43, 0, C_MAJOR_V},
                                    {"I", 6, 2, 48, 0, C_MAJOR_I}};
    PlanSpan span = {beats(1), beats(7)};
    assert(fill_seeds(chords, 4, span, 0, check_beat_melody) > 0);
}

/* The metre reads strong beats from the barlines: a span starting on beat 3 starts on one, on beat 2 it does not. */
static void test_strong_beats(void) {
    static const Chord from_three[2] = {{"I", 2, 4, 48, 0, C_MAJOR_I}, {"V", 6, 2, 43, 0, C_MAJOR_V}};
    static const Chord from_two[3] = {{"I", 1, 1, 48, 0, C_MAJOR_I}, {"IV", 2, 4, 41, 0, C_MAJOR_IV},
                                      {"V", 6, 1, 43, 0, C_MAJOR_V}};
    for (int variant = 0; variant < 2; variant++) {
        Piece piece = variant ? build(from_two, 3, false) : build(from_three, 2, false);
        NodeId motive = add_melody_motive(&piece);
        PlanSpan span = {beats(variant ? 1 : 2), beats(6)};
        MelodyContext *context = malloc(sizeof(*context));
        add_melody_target(&piece, span, motive, 0);
        assert(context && melody_context_read(piece.plan, piece.target, context));
        assert(melody_on_cell(context, POSITION_ZERO) == (variant == 0));
        /* Beat 3 of the plan is strong either way. */
        assert(melody_on_cell(context, beats(2 - (variant ? 1 : 2))));
        assert(!melody_on_cell(context, beats(3 - (variant ? 1 : 2))));
        free(context);
        plan_destroy(piece.plan);
    }
}

/* The pickup's notes fill it, and lead into the first downbeat note by a step or a chord tone of the chord before. */
static void check_pickup_melody(const Chord *chords, size_t count, const Piece *piece) {
    Note notes[CHECK_MAX_NOTES];
    size_t found = notes_of(piece, PLAN_LANE_MELODY, 0, piece->span, notes);
    long start = check_ticks(piece->span.start);
    long body = check_ticks(piece->body.start);
    size_t pickup = 0;
    long covered = 0;
    assert(found > 0 && notes[0].onset == start);
    while (pickup < found && notes[pickup].onset < body) {
        assert(notes[pickup].end <= body);
        covered += notes[pickup].end - notes[pickup].onset;
        pickup++;
    }
    /* One to four notes that fill the pickup, then the first note of the phrase on the downbeat. */
    assert(pickup >= 1 && pickup <= 4 && covered == body - start);
    assert(pickup < found && notes[pickup].onset == body);
    int last = notes[pickup - 1].pitch;
    int first = notes[pickup].pitch;
    bool step = abs(last - first) >= 1 && abs(last - first) <= 2;
    /* At the start of the plan there is no chord before; elsewhere the chord sounding when the pickup starts. */
    bool tone = start > 0 && in_chord(chord_at(chords, count, start), last);
    assert(step || tone);
    for (size_t n = 0; n < pickup; n++) {
        assert(notes[n].pitch >= 60 && notes[n].pitch <= 79);
    }
    /* The checker, over the phrase that follows the pickup, agrees. */
    const char *violation = "unread";
    assert(melody_check_lane(piece->plan, piece->body, c_major(), NULL, &violation));
    assert(violation == NULL);
}

static const Chord PICKUP_CHORDS[5] = {{"I", 0, 4, 48, 0, C_MAJOR_I},
                                       {"I", 4, 2, 48, 0, C_MAJOR_I},
                                       {"I", 6, 2, 48, 0, C_MAJOR_I},
                                       {"V", 8, 2, 43, 0, C_MAJOR_V},
                                       {"I", 10, 2, 48, 0, C_MAJOR_I}};

/* A pickup of 1/4 before the downbeat of bar 2: one note. */
static void test_pickup_quarter(void) {
    PlanSpan span = {beats(3), beats(9)};
    assert(fill_seeds(PICKUP_CHORDS, 5, span, 8, check_pickup_melody) > 0);
}

/* A pickup of 3/8 before the downbeat of bar 2: three eighths. */
static void test_pickup_three_eighths(void) {
    size_t completed = 0;
    for (unsigned long seed = 0; seed < SEEDS; seed++) {
        Piece piece = build(PICKUP_CHORDS, 5, false);
        NodeId motive = add_melody_motive(&piece);
        PlanSpan span = {fraction(5, 8), fraction(19, 8)};
        add_melody_target(&piece, span, motive, 12);
        FillOutcome outcome = run_melody(&piece, seed);
        if (outcome.status == FILL_STATUS_COMPLETE) {
            Note notes[CHECK_MAX_NOTES];
            size_t found = notes_of(&piece, PLAN_LANE_MELODY, 0, span, notes);
            completed++;
            /* Three eighths from 5/8, then the first note of the phrase at beat 5. */
            assert(found > 3 && notes[0].onset == 80);
            for (size_t n = 0; n < 3; n++) {
                assert(notes[n].end - notes[n].onset == 16);
            }
            assert(notes[3].onset == 4 * TICKS_PER_BEAT);
            check_pickup_melody(PICKUP_CHORDS, 5, &piece);
        }
        fill_outcome_destroy(&outcome);
        plan_destroy(piece.plan);
    }
    assert(completed > 0);
}

/* A pickup at the start of the plan has no chord before it, so it leads in by step. */
static void test_pickup_at_start(void) {
    static const Chord chords[4] = {{"I", 1, 1, 48, 0, C_MAJOR_I},
                                    {"I", 2, 2, 48, 0, C_MAJOR_I},
                                    {"V", 4, 2, 43, 0, C_MAJOR_V},
                                    {"I", 6, 2, 48, 0, C_MAJOR_I}};
    PlanSpan span = {beats(0), beats(8)};
    assert(fill_seeds(chords, 4, span, 8, check_pickup_melody) > 0);
}

/* A pickup is a multiple of 1/32 of a whole note, and not a bar or more of the longest meter. */
static void test_pickup_constraint(void) {
    static const long NUM[4] = {1, 1, 0, 3};
    static const unsigned long DEN[4] = {3, 64, 4, 2};
    static const PlanResult EXPECTED[4] = {PLAN_ERR_BAD_PAYLOAD, PLAN_ERR_BAD_PAYLOAD, PLAN_ERR_BAD_PAYLOAD, PLAN_OK};
    Piece piece = build(PICKUP_CHORDS, 5, false);
    NodeId motive = add_melody_motive(&piece);
    PlanSpan span = {beats(3), beats(9)};
    PlanConstraint pickup;
    memset(&pickup, 0, sizeof(pickup));
    pickup.kind = PLAN_CONSTRAINT_PICKUP;
    add_melody_target(&piece, span, motive, 0);
    for (size_t i = 0; i < 4; i++) {
        pickup.params.pickup.num = NUM[i];
        pickup.params.pickup.den = DEN[i];
        assert(plan_lane_span_add_constraint(piece.plan, piece.target, &pickup) == EXPECTED[i]);
    }
    plan_destroy(piece.plan);
}

/* A pickup as long as the bar is accepted as a constraint, and the melody operator does not apply to it. */
static void test_pickup_bar_long(void) {
    Piece piece = build(PICKUP_CHORDS, 5, false);
    NodeId motive = add_melody_motive(&piece);
    PlanSpan span = {beats(0), beats(12)};
    MelodyContext *context = malloc(sizeof(*context));
    add_melody_target(&piece, span, motive, 32);
    assert(context && !melody_context_read(piece.plan, piece.target, context));
    free(context);
    plan_destroy(piece.plan);
}

/* The pickup constraint round-trips through the plan JSON. */
static void test_pickup_json(void) {
    Piece piece = build(PICKUP_CHORDS, 5, false);
    NodeId motive = add_melody_motive(&piece);
    PlanSpan span = {beats(3), beats(9)};
    PlanJsonError error;
    add_melody_target(&piece, span, motive, 8);
    char *text = plan_to_json(piece.plan, &error);
    assert(text && strstr(text, "\"kind\": \"pickup\"") && strstr(text, "\"length\": \"1/4\""));
    PiecePlan *parsed = NULL;
    assert(plan_from_json(text, &parsed, &error) == PLAN_OK);
    char *again = plan_to_json(parsed, &error);
    assert(again && strcmp(text, again) == 0);
    LaneSpanId melody;
    assert(plan_lane_find(parsed, PLAN_LANE_MELODY, 0, span, &melody, 1) == 1);
    bool found = false;
    for (size_t c = 0; c < plan_lane_span_constraint_count(parsed, melody); c++) {
        const PlanConstraint *constraint = plan_lane_span_constraint_at(parsed, melody, c);
        if (constraint->kind == PLAN_CONSTRAINT_PICKUP) {
            assert(constraint->params.pickup.num == 1 && constraint->params.pickup.den == 4);
            found = true;
        }
    }
    assert(found);
    /* A length that is not written as n/d is a format error. */
    char *broken = strdup(text);
    char *at = broken ? strstr(broken, "\"length\": \"1/4\"") : NULL;
    assert(at);
    at[12] = 'x';
    PiecePlan *rejected = NULL;
    assert(plan_from_json(broken, &rejected, &error) != PLAN_OK && rejected == NULL);
    free(broken);
    free(again);
    free(text);
    plan_destroy(parsed);
    plan_destroy(piece.plan);
}

/* ----------------------------------------------------------- texture */

static FillOutcome accompany(PiecePlan *plan, unsigned long seed) {
    FillOperatorRegistry *registry = fill_registry_create();
    FillConfig config;
    FillOutcome outcome;
    memset(&config, 0, sizeof(config));
    config.seed = seed;
    config.checker = accompaniment_checker;
    assert(registry && accompaniment_register(registry, NULL) == FILL_OK);
    assert(fill_run(plan, registry, &config, &outcome, NULL) == FILL_OK);
    fill_registry_destroy(registry);
    return outcome;
}

/* The open inner voices of an accompaniment over the span: voice 1 with the constraint, then more. */
static void open_voices(Piece *piece, PlanSpan span, PlanAccompanimentPattern pattern, int voices) {
    for (int voice = 1; voice <= voices; voice++) {
        LaneSpanId id =
            add_notes(piece->plan, piece->owner, PLAN_LANE_INNER_VOICE, voice, span, 55, PLAN_STATE_CONSTRAINED);
        if (voice == 1) {
            PlanConstraint constraint;
            memset(&constraint, 0, sizeof(constraint));
            constraint.kind = PLAN_CONSTRAINT_ACCOMPANIMENT;
            constraint.params.accompaniment.pattern = pattern;
            constraint.params.accompaniment.texture = -1;
            if (pattern == PLAN_ACCOMPANIMENT_MOTIVE) {
                strcpy(constraint.params.accompaniment.motive_node, "acc");
            }
            assert(plan_lane_span_add_constraint(piece->plan, id, &constraint) == PLAN_OK);
        }
    }
}

/* One chord of 3 beats starting on beat 2: a lead of a beat and a cell of two. */
static void test_accompaniment_three_beats(void) {
    static const Chord chord = {"I", 1, 3, 48, 76, C_MAJOR_I};
    static const PlanAccompanimentPattern PATTERNS[4] = {PLAN_ACCOMPANIMENT_BLOCK, PLAN_ACCOMPANIMENT_ARPEGGIATED,
                                                         PLAN_ACCOMPANIMENT_ALBERTI, PLAN_ACCOMPANIMENT_MOTIVE};
    for (size_t p = 0; p < 4; p++) {
        size_t completed = 0;
        for (unsigned long seed = 0; seed < SEEDS; seed++) {
            Piece piece = build(&chord, 1, true);
            PlanSpan span = {beats(1), beats(3)};
            Note top[CHECK_MAX_NOTES];
            Note others[CHECK_MAX_NOTES];
            if (PATTERNS[p] == PLAN_ACCOMPANIMENT_MOTIVE) {
                add_accompaniment_motive(&piece);
            }
            open_voices(&piece, span, PATTERNS[p], PATTERNS[p] == PLAN_ACCOMPANIMENT_BLOCK ? 3 : 1);
            FillOutcome outcome = accompany(piece.plan, seed);
            if (outcome.status == FILL_STATUS_COMPLETE) {
                completed++;
                size_t count = notes_of(&piece, PLAN_LANE_INNER_VOICE, 1, span, top);
                assert(check_accompaniment(piece.plan, span, c_major(), PLAN_ACCOMPANIMENT_DEFAULT_LOW,
                                           PLAN_ACCOMPANIMENT_DEFAULT_HIGH) > 0);
                switch (PATTERNS[p]) {
                case PLAN_ACCOMPANIMENT_BLOCK:
                    /* Three beats from the chord's start, the three voices over each. */
                    assert(count == 3);
                    for (size_t n = 0; n < 3; n++) {
                        assert(top[n].onset == TICKS_PER_BEAT * (1 + (long)n) &&
                               top[n].end == top[n].onset + TICKS_PER_BEAT);
                    }
                    assert(notes_of(&piece, PLAN_LANE_INNER_VOICE, 2, span, others) == 3);
                    break;
                case PLAN_ACCOMPANIMENT_ARPEGGIATED:
                case PLAN_ACCOMPANIMENT_ALBERTI:
                    /* Six eighths. The chord starts off the meter's cells, so the figure starts again with it: it
                       begins on the lowest note of the voicing, and the first note of the next beat is higher. */
                    assert(count == 6);
                    for (size_t n = 0; n < 6; n++) {
                        assert(top[n].onset == TICKS_PER_BEAT + 16 * (long)n && top[n].end == top[n].onset + 16);
                    }
                    assert(top[0].pitch < top[1].pitch && top[0].pitch < top[2].pitch);
                    break;
                default:
                    /* The motive's four eighths, then its first two, cut at the chord's end. */
                    assert(count == 6);
                    for (size_t n = 0; n < 6; n++) {
                        assert(top[n].onset == TICKS_PER_BEAT + 16 * (long)n && top[n].end == top[n].onset + 16);
                    }
                    break;
                }
            }
            fill_outcome_destroy(&outcome);
            plan_destroy(piece.plan);
        }
        assert(completed > 0);
    }
}

/* A chord of 1 beat and one of 2 after it: each is cut into beats from its own start. */
static void test_accompaniment_short_chord(void) {
    static const Chord chords[2] = {{"I", 1, 1, 48, 76, C_MAJOR_I}, {"V", 2, 2, 43, 74, C_MAJOR_V}};
    size_t completed = 0;
    for (unsigned long seed = 0; seed < SEEDS; seed++) {
        Piece piece = build(chords, 2, true);
        PlanSpan span = {beats(1), beats(3)};
        Note top[CHECK_MAX_NOTES];
        open_voices(&piece, span, PLAN_ACCOMPANIMENT_ALBERTI, 1);
        FillOutcome outcome = accompany(piece.plan, seed);
        if (outcome.status == FILL_STATUS_COMPLETE) {
            completed++;
            /* The first chord's beat holds two eighths of the figure, its first two notes. */
            assert(notes_of(&piece, PLAN_LANE_INNER_VOICE, 1, span, top) == 6);
            assert(check_accompaniment(piece.plan, span, c_major(), PLAN_ACCOMPANIMENT_DEFAULT_LOW,
                                       PLAN_ACCOMPANIMENT_DEFAULT_HIGH) > 0);
            assert(top[0].onset == TICKS_PER_BEAT && top[1].end == 2 * TICKS_PER_BEAT);
            assert(top[2].onset == 2 * TICKS_PER_BEAT);
        }
        fill_outcome_destroy(&outcome);
        plan_destroy(piece.plan);
    }
    assert(completed > 0);
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

/* A voicing over a chord of one beat, then one of two, from beat 2. */
static void test_voicing_one_beat(void) {
    static const Chord chords[2] = {{"I", 1, 1, 48, 76, C_MAJOR_I}, {"V", 2, 2, 43, 74, C_MAJOR_V}};
    size_t completed = 0;
    for (unsigned long seed = 0; seed < SEEDS; seed++) {
        Piece piece = build(chords, 2, true);
        PlanSpan span = {beats(1), beats(3)};
        for (int voice_number = 2; voice_number >= 1; voice_number--) {
            LaneSpanId id =
                add_notes(piece.plan, piece.owner, PLAN_LANE_INNER_VOICE, voice_number, span, 55, PLAN_STATE_CONSTRAINED);
            if (voice_number == 1) {
                PlanConstraint constraint;
                memset(&constraint, 0, sizeof(constraint));
                constraint.kind = PLAN_CONSTRAINT_VOICING;
                assert(plan_lane_span_add_constraint(piece.plan, id, &constraint) == PLAN_OK);
            }
        }
        FillOutcome outcome = voice(piece.plan, seed);
        if (outcome.status == FILL_STATUS_COMPLETE) {
            Note alto[CHECK_MAX_NOTES];
            Note tenor[CHECK_MAX_NOTES];
            completed++;
            assert(notes_of(&piece, PLAN_LANE_INNER_VOICE, 1, span, alto) == 2);
            assert(notes_of(&piece, PLAN_LANE_INNER_VOICE, 2, span, tenor) == 2);
            assert(alto[0].onset == TICKS_PER_BEAT && alto[0].end == 2 * TICKS_PER_BEAT);
            assert(alto[1].onset == 2 * TICKS_PER_BEAT && alto[1].end == 4 * TICKS_PER_BEAT);
            assert(tenor[0].onset == TICKS_PER_BEAT && tenor[0].end == 2 * TICKS_PER_BEAT);
            assert(in_chord(&chords[0], alto[0].pitch) && in_chord(&chords[0], tenor[0].pitch));
            assert(in_chord(&chords[1], alto[1].pitch) && in_chord(&chords[1], tenor[1].pitch));
            assert(alto[0].pitch > tenor[0].pitch && alto[1].pitch > tenor[1].pitch);
        }
        fill_outcome_destroy(&outcome);
        plan_destroy(piece.plan);
    }
    assert(completed > 0);
}

int main(void) {
    test_seven_beats_from_beat_three();
    test_seven_beats_from_beat_two();
    test_strong_beats();
    test_pickup_quarter();
    test_pickup_three_eighths();
    test_pickup_at_start();
    test_pickup_constraint();
    test_pickup_bar_long();
    test_pickup_json();
    test_accompaniment_three_beats();
    test_accompaniment_short_chord();
    test_voicing_one_beat();
    puts("test_melody_beats ok");
    return 0;
}
