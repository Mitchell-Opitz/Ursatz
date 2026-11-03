#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <standard-texture/texture_rules.h>

#include <ursatz_composer/accompaniment.h>
#include <ursatz_composer/constraint.h>
#include <ursatz_composer/fill_loop.h>
#include <ursatz_composer/motive.h>
#include <ursatz_composer/plan_json.h>
#include <ursatz_composer/plan_state.h>

#include "accompaniment_check.h"

/*
 * The accompaniment operator on realized plans: the fixed 8-bar sentence in C major 4/4, and plans built here in 3/4
 * and 6/8, a pattern at a time. Each result is held to the rules in accompaniment_check.h, which are worked out from
 * the plan and not from the operator, and to the shape of its pattern.
 */

#ifndef COMPOSER_FIXTURE_DIR
#error "COMPOSER_FIXTURE_DIR is not defined"
#endif

#define SEEDS 8

typedef struct {
    PiecePlan *plan;
    NodeId owner;
    PlanSpan span;
    PlanMeter meter;
    long beat;
    long bar;
} Piece;

static Rational fraction(long num, unsigned long den) {
    Rational time;
    assert(rational_make(num, den, &time) == RATIONAL_OK);
    return time;
}

static AnalysisKey c_major(void) {
    AnalysisKey key = {NULL, PITCH_LETTER_C, 0, KEY_STRUCTURE_MODE_MAJOR};
    return key;
}

static void measure(Piece *piece) {
    piece->owner = plan_root(piece->plan);
    piece->span = plan_node_span(piece->plan, piece->owner);
    LaneSpanId meter;
    assert(plan_lane_find(piece->plan, PLAN_LANE_METER, 0, piece->span, &meter, 1) == 1);
    piece->meter = plan_lane_span_payload(piece->plan, meter)->meter;
    bool compound = piece->meter.beat_unit == 8 && piece->meter.beats_per_bar % 3 == 0;
    piece->beat = compound ? 48 : CHECK_TICKS / piece->meter.beat_unit;
    piece->bar = piece->meter.beats_per_bar * CHECK_TICKS / piece->meter.beat_unit;
}

static Piece fixture_piece(void) {
    Piece piece;
    PlanJsonError error;
    memset(&piece, 0, sizeof(piece));
    assert(plan_read_json_file(COMPOSER_FIXTURE_DIR "/sentence_c_major.json", &piece.plan, &error) == PLAN_OK);
    measure(&piece);
    return piece;
}

static const char *const NUMERALS[8] = {"I", "IV", "V", "I", "I", "ii", "V", "I"};
static const int BASS[8] = {48, 41, 43, 48, 48, 50, 43, 48};

/*
 * Eight bars in the meter, a chord a bar, a bass note a bar, and a melody: of one note a beat that wanders over
 * 72 to 76, or, steady, of one held note a bar, so the room between the bass and the melody never changes.
 */
/* The accompaniment motive of the tests, C4:e G4:e E4:e G4:e, as the node "acc" in a bar after the eight. */
static void add_motive_node(PiecePlan *plan, NodeId form) {
    Motive motive;
    NodeId section;
    NodeId phrase;
    NodeId idea;
    PlanSpan material = {fraction(8, 1), fraction(1, 1)};
    memset(&motive, 0, sizeof(motive));
    motive.meter.beats_per_bar = 4;
    motive.meter.beat_unit = 4;
    motive.key = c_major();
    motive.key_source = MOTIVE_KEY_GIVEN;
    motive.role = MOTIVE_ROLE_ACCOMPANIMENT;
    static const int PITCHES[4] = {60, 67, 64, 67};
    for (size_t n = 0; n < 4; n++) {
        motive.notes[n].midi = PITCHES[n];
        motive.notes[n].spelled = false;
        motive.notes[n].offset = fraction((long)n, 8);
        motive.notes[n].duration = fraction(1, 8);
    }
    motive.note_count = 4;
    motive.length = fraction(1, 2);
    assert(plan_add_node(plan, form, PLAN_NODE_SECTION, material, "material", &section) == PLAN_OK);
    assert(plan_add_node(plan, section, PLAN_NODE_PHRASE, material, "material", &phrase) == PLAN_OK);
    assert(plan_add_node(plan, phrase, PLAN_NODE_IDEA, material, "material", &idea) == PLAN_OK);
    assert(motive_attach(plan, idea, material.start, &motive, "acc", NULL, NULL) == PLAN_OK);
    assert(plan_node_set_state(plan, section, PLAN_STATE_REALIZED) == PLAN_OK);
    assert(plan_node_set_state(plan, phrase, PLAN_STATE_REALIZED) == PLAN_OK);
    assert(plan_node_set_state(plan, idea, PLAN_STATE_REALIZED) == PLAN_OK);
}

static Piece built_piece_with(int beats, int unit, bool steady, long extra_bars) {
    Piece piece;
    Rational bar;
    PlanLanePayload payload;
    LaneSpanId id;
    NodeId form;
    memset(&piece, 0, sizeof(piece));
    piece.plan = plan_create();
    assert(rational_make(beats, (long long)unit, &bar) == RATIONAL_OK);
    PlanSpan all = {fraction(0, 1), fraction(bar.num * (8 + extra_bars), bar.den)};
    assert(plan_add_node(piece.plan, PLAN_NODE_NONE, PLAN_NODE_FORM, all, "form", &form) == PLAN_OK);
    memset(&payload, 0, sizeof(payload));
    payload.meter.beats_per_bar = beats;
    payload.meter.beat_unit = unit;
    assert(plan_add_lane_span(piece.plan, form, PLAN_LANE_METER, 0, all, &payload, &id) == PLAN_OK);
    assert(plan_lane_span_set_state(piece.plan, id, PLAN_STATE_GIVEN) == PLAN_OK);
    memset(&payload, 0, sizeof(payload));
    payload.key = c_major();
    assert(plan_add_lane_span(piece.plan, form, PLAN_LANE_KEY, 0, all, &payload, &id) == PLAN_OK);
    assert(plan_lane_span_set_state(piece.plan, id, PLAN_STATE_GIVEN) == PLAN_OK);

    bool compound = unit == 8 && beats % 3 == 0;
    long beat_num = compound ? 3 : 1;
    unsigned long beat_den = compound ? 8 : (unsigned long)unit;
    long beats_in_bar = compound ? beats / 3 : beats;
    static const int MELODY[4] = {72, 74, 76, 74};
    for (size_t b = 0; b < 8; b++) {
        PlanSpan span = {fraction(bar.num * (long)b, bar.den), bar};
        PlanNote notes[8];
        memset(&payload, 0, sizeof(payload));
        strcpy(payload.harmony.roman_numeral, NUMERALS[b]);
        payload.harmony.has_function = false;
        assert(plan_add_lane_span(piece.plan, form, PLAN_LANE_HARMONY, 0, span, &payload, &id) == PLAN_OK);
        assert(plan_lane_span_set_state(piece.plan, id, PLAN_STATE_REALIZED) == PLAN_OK);

        memset(&payload, 0, sizeof(payload));
        notes[0].midi = BASS[b];
        notes[0].spelled = false;
        notes[0].offset = fraction(0, 1);
        notes[0].duration = bar;
        payload.notes.notes = notes;
        payload.notes.note_count = 1;
        assert(plan_add_lane_span(piece.plan, form, PLAN_LANE_BASS, 0, span, &payload, &id) == PLAN_OK);
        assert(plan_lane_span_set_state(piece.plan, id, PLAN_STATE_REALIZED) == PLAN_OK);

        size_t count = steady ? 1 : (size_t)beats_in_bar;
        for (size_t n = 0; n < count; n++) {
            notes[n].midi = steady ? 72 : MELODY[(b + n) % 4];
            notes[n].spelled = false;
            notes[n].offset = steady ? fraction(0, 1) : fraction(beat_num * (long)n, beat_den);
            notes[n].duration = steady ? bar : fraction(beat_num, beat_den);
        }
        payload.notes.note_count = count;
        assert(plan_add_lane_span(piece.plan, form, PLAN_LANE_MELODY, 0, span, &payload, &id) == PLAN_OK);
        assert(plan_lane_span_set_state(piece.plan, id, PLAN_STATE_REALIZED) == PLAN_OK);
    }
    if (extra_bars > 0) {
        add_motive_node(piece.plan, form);
    }
    assert(plan_node_set_state(piece.plan, form, PLAN_STATE_GIVEN) == PLAN_OK);
    measure(&piece);
    return piece;
}

static Piece built_piece(int beats, int unit, bool steady) {
    return built_piece_with(beats, unit, steady, 0);
}

/* The open inner voice spans of an accompaniment over the whole piece: voice 1 with the constraint, then more. */
static LaneSpanId open_voices(Piece *piece, PlanAccompaniment accompaniment, int voices) {
    LaneSpanId first = PLAN_LANE_SPAN_NONE;
    for (int voice = 1; voice <= voices; voice++) {
        PlanLanePayload payload;
        PlanNote note;
        LaneSpanId id;
        memset(&payload, 0, sizeof(payload));
        note.midi = 55;
        note.spelled = false;
        note.offset = fraction(0, 1);
        note.duration = piece->span.length;
        payload.notes.notes = &note;
        payload.notes.note_count = 1;
        assert(plan_add_lane_span(piece->plan, piece->owner, PLAN_LANE_INNER_VOICE, voice, piece->span, &payload, &id) ==
               PLAN_OK);
        if (voice == 1) {
            PlanConstraint constraint;
            memset(&constraint, 0, sizeof(constraint));
            constraint.kind = PLAN_CONSTRAINT_ACCOMPANIMENT;
            constraint.params.accompaniment = accompaniment;
            assert(plan_lane_span_add_constraint(piece->plan, id, &constraint) == PLAN_OK);
            first = id;
        }
    }
    return first;
}

static PlanAccompaniment ask(PlanAccompanimentPattern pattern, int texture) {
    PlanAccompaniment accompaniment;
    memset(&accompaniment, 0, sizeof(accompaniment));
    accompaniment.pattern = pattern;
    accompaniment.texture = texture;
    return accompaniment;
}

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

static size_t voice_notes(const Piece *piece, int voice, CheckNote *out) {
    return check_notes(piece->plan, PLAN_LANE_INNER_VOICE, voice, piece->span, out);
}

/* Whether the beat with this index in its bar is one an oom-pah rests on. */
static bool rests(const Piece *piece, long onset) {
    long beats_in_bar = piece->bar / piece->beat;
    long index = (onset % piece->bar) / piece->beat;
    return beats_in_bar % 2 == 0 ? index % 2 == 0 : index == 0;
}

static void check_shape(const Piece *piece, PlanAccompanimentPattern pattern, bool steady) {
    CheckNote top[CHECK_MAX_NOTES];
    CheckNote others[CHECK_MAX_NOTES];
    long length = check_ticks(piece->span.length);
    size_t count = voice_notes(piece, 1, top);
    switch (pattern) {
    case PLAN_ACCOMPANIMENT_BLOCK:
        /* A chord on every beat, held to the next. */
        assert(count == (size_t)(length / piece->beat));
        for (size_t n = 0; n < count; n++) {
            assert(top[n].onset == (long)n * piece->beat && top[n].end == top[n].onset + piece->beat);
        }
        for (int voice = 2; voice <= 3; voice++) {
            size_t other = voice_notes(piece, voice, others);
            assert(other <= count && other > 0);
            for (size_t n = 0; n < other; n++) {
                assert(others[n].onset % piece->beat == 0 && others[n].end - others[n].onset == piece->beat);
            }
        }
        break;
    case PLAN_ACCOMPANIMENT_OOM_PAH: {
        size_t expected = 0;
        for (long at = 0; at < length; at += piece->beat) {
            expected += !rests(piece, at);
        }
        assert(count == expected);
        for (size_t n = 0; n < count; n++) {
            assert(!rests(piece, top[n].onset) && top[n].onset % piece->beat == 0 && top[n].end - top[n].onset == piece->beat);
        }
        break;
    }
    case PLAN_ACCOMPANIMENT_ARPEGGIATED:
    case PLAN_ACCOMPANIMENT_ALBERTI:
        /* One note an eighth, in voice 1 alone. */
        assert(count == (size_t)(length / (CHECK_TICKS / 8)));
        for (size_t n = 0; n < count; n++) {
            assert(top[n].onset == (long)n * (CHECK_TICKS / 8) && top[n].end == top[n].onset + CHECK_TICKS / 8);
        }
        for (int voice = 2; voice <= 3; voice++) {
            assert(voice_notes(piece, voice, others) == 0);
        }
        /* With the room between bass and melody unchanging, a figure keeps to one voicing: its shape shows. */
        for (long bar = 0; steady && bar < length / piece->bar; bar++) {
            size_t per_bar = (size_t)(piece->bar / (CHECK_TICKS / 8));
            for (size_t g = 0; g + 4 <= per_bar; g += 4) {
                const CheckNote *figure = &top[(size_t)bar * per_bar + g];
                if (pattern == PLAN_ACCOMPANIMENT_ARPEGGIATED) {
                    /* Rising then falling: low, middle, high, middle. */
                    assert(figure[0].pitch < figure[1].pitch && figure[1].pitch < figure[2].pitch);
                    assert(figure[3].pitch == figure[1].pitch);
                } else {
                    /* Low, high, middle, high. */
                    assert(figure[0].pitch < figure[2].pitch && figure[2].pitch < figure[1].pitch);
                    assert(figure[3].pitch == figure[1].pitch);
                }
            }
        }
        break;
    case PLAN_ACCOMPANIMENT_MOTIVE:
    case PLAN_ACCOMPANIMENT_ANY:
        break;
    }
}

static void run_pattern(const char *name, Piece (*make)(void), AnalysisKey key, PlanAccompanimentPattern pattern,
                        bool steady) {
    for (unsigned long seed = 1; seed <= SEEDS; seed++) {
        Piece piece = make();
        int voices = pattern == PLAN_ACCOMPANIMENT_ARPEGGIATED || pattern == PLAN_ACCOMPANIMENT_ALBERTI ? 1 : 3;
        open_voices(&piece, ask(pattern, -1), voices);
        FillOutcome outcome = accompany(piece.plan, seed);
        if (outcome.status != FILL_STATUS_COMPLETE) {
            fprintf(stderr, "%s, pattern %d, seed %lu: not complete (%zu stuck, reason %d)\n", name, (int)pattern, seed,
                    outcome.stuck_count, outcome.stuck_count ? (int)outcome.stuck[0].reason : -1);
        }
        assert(outcome.status == FILL_STATUS_COMPLETE && outcome.steps == 1);
        assert(check_accompaniment(piece.plan, piece.span, key, PLAN_ACCOMPANIMENT_DEFAULT_LOW,
                                   PLAN_ACCOMPANIMENT_DEFAULT_HIGH) > 0);
        check_shape(&piece, pattern, steady);

        /* The same seed gives the same notes. */
        Piece again = make();
        open_voices(&again, ask(pattern, -1), voices);
        FillOutcome second = accompany(again.plan, seed);
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

static Piece fixture_4_4(void) {
    return fixture_piece();
}
static Piece wandering_3_4(void) {
    return built_piece(3, 4, false);
}
static Piece wandering_6_8(void) {
    return built_piece(6, 8, false);
}
static Piece steady_4_4(void) {
    return built_piece(4, 4, true);
}
static Piece steady_3_4(void) {
    return built_piece(3, 4, true);
}

static void test_patterns(void) {
    static const PlanAccompanimentPattern PATTERNS[] = {PLAN_ACCOMPANIMENT_BLOCK, PLAN_ACCOMPANIMENT_ARPEGGIATED,
                                                        PLAN_ACCOMPANIMENT_ALBERTI, PLAN_ACCOMPANIMENT_OOM_PAH,
                                                        PLAN_ACCOMPANIMENT_ANY};
    for (size_t p = 0; p < sizeof(PATTERNS) / sizeof(PATTERNS[0]); p++) {
        run_pattern("C major 4/4 fixture", fixture_4_4, c_major(), PATTERNS[p], false);
        run_pattern("C major 3/4", wandering_3_4, c_major(), PATTERNS[p], false);
        run_pattern("steady C major 4/4", steady_4_4, c_major(), PATTERNS[p], true);
        run_pattern("steady C major 3/4", steady_3_4, c_major(), PATTERNS[p], true);
        /* Alberti is not written in a compound meter; ANY skips it there. */
        if (PATTERNS[p] != PLAN_ACCOMPANIMENT_ALBERTI) {
            run_pattern("C major 6/8", wandering_6_8, c_major(), PATTERNS[p], false);
        }
    }
}

static void test_alberti_refused_in_compound_meter(void) {
    Piece piece = wandering_6_8();
    LaneSpanId target = open_voices(&piece, ask(PLAN_ACCOMPANIMENT_ALBERTI, -1), 1);
    FillOutcome outcome = accompany(piece.plan, 1);
    assert(outcome.status == FILL_STATUS_STUCK && outcome.stuck_count == 1);
    assert(outcome.stuck[0].target.kind == FILL_TARGET_LANE_SPAN && outcome.stuck[0].target.id == target);
    assert(outcome.stuck[0].reason == FILL_STUCK_NO_OPERATOR);
    assert(plan_lane_span_state(piece.plan, target) == PLAN_STATE_CONSTRAINED);
    fill_outcome_destroy(&outcome);
    plan_destroy(piece.plan);
}

/* The voices a pattern does not use are written empty when their spans are open. */
static void test_unused_voices_are_written_empty(void) {
    Piece piece = steady_4_4();
    open_voices(&piece, ask(PLAN_ACCOMPANIMENT_ARPEGGIATED, -1), 3);
    FillOutcome outcome = accompany(piece.plan, 3);
    assert(outcome.status == FILL_STATUS_COMPLETE);
    for (int voice = 2; voice <= 3; voice++) {
        LaneSpanId id;
        assert(plan_lane_find(piece.plan, PLAN_LANE_INNER_VOICE, voice, piece.span, &id, 1) == 1);
        assert(plan_lane_span_state(piece.plan, id) == PLAN_STATE_REALIZED);
        assert(plan_lane_span_payload(piece.plan, id)->notes.note_count == 0);
    }
    /* An empty voice survives the plan file. */
    char *json = plan_to_json(piece.plan, NULL);
    PiecePlan *read = NULL;
    PlanJsonError error;
    assert(json && plan_from_json(json, &read, &error) == PLAN_OK);
    char *again = plan_to_json(read, NULL);
    assert(again && strcmp(json, again) == 0);
    free(again);
    free(json);
    plan_destroy(read);
    fill_outcome_destroy(&outcome);
    plan_destroy(piece.plan);
}

/* A texture asked for narrows the patterns ANY chooses among. */
static void test_texture_chooses_patterns(void) {
    for (unsigned long seed = 1; seed <= SEEDS; seed++) {
        Piece piece = steady_4_4();
        open_voices(&piece, ask(PLAN_ACCOMPANIMENT_ANY, STANDARD_TEXTURE_HOMORHYTHM), 3);
        FillOutcome outcome = accompany(piece.plan, seed);
        assert(outcome.status == FILL_STATUS_COMPLETE);
        check_shape(&piece, PLAN_ACCOMPANIMENT_BLOCK, true);
        assert(check_accompaniment(piece.plan, piece.span, c_major(), PLAN_ACCOMPANIMENT_DEFAULT_LOW,
                                   PLAN_ACCOMPANIMENT_DEFAULT_HIGH) > 0);
        fill_outcome_destroy(&outcome);
        plan_destroy(piece.plan);
    }
    /* A texture that is not homophonic has no accompaniment: the operator does not apply. */
    Piece piece = steady_4_4();
    LaneSpanId target = open_voices(&piece, ask(PLAN_ACCOMPANIMENT_ANY, STANDARD_TEXTURE_POLYPHONY), 3);
    FillOutcome outcome = accompany(piece.plan, 1);
    assert(outcome.status == FILL_STATUS_STUCK && outcome.stuck[0].reason == FILL_STUCK_NO_OPERATOR);
    assert(plan_lane_span_state(piece.plan, target) == PLAN_STATE_CONSTRAINED);
    fill_outcome_destroy(&outcome);
    plan_destroy(piece.plan);
}

/* The range the constraint names is the one kept to. */
static void test_range_is_kept(void) {
    for (unsigned long seed = 1; seed <= SEEDS; seed++) {
        Piece piece = fixture_4_4();
        PlanAccompaniment accompaniment = ask(PLAN_ACCOMPANIMENT_BLOCK, -1);
        accompaniment.low = 52;
        accompaniment.high = 62;
        open_voices(&piece, accompaniment, 3);
        FillOutcome outcome = accompany(piece.plan, seed);
        assert(outcome.status == FILL_STATUS_COMPLETE);
        assert(check_accompaniment(piece.plan, piece.span, c_major(), 52, 62) > 0);
        fill_outcome_destroy(&outcome);
        plan_destroy(piece.plan);
    }
}

/* A target whose melody is still open is not one the operator applies to. */
static void test_open_melody_is_not_accompanied(void) {
    Piece piece = steady_4_4();
    LaneSpanId melody[16];
    size_t found = plan_lane_find(piece.plan, PLAN_LANE_MELODY, 0, piece.span, melody, 16);
    assert(found == 8);
    assert(plan_lane_span_set_state(piece.plan, melody[3], PLAN_STATE_CONSTRAINED) == PLAN_OK);
    open_voices(&piece, ask(PLAN_ACCOMPANIMENT_BLOCK, -1), 3);
    FillOutcome outcome = accompany(piece.plan, 1);
    assert(outcome.status == FILL_STATUS_STUCK);
    fill_outcome_destroy(&outcome);
    plan_destroy(piece.plan);
}

static void test_constraint(void) {
    Piece piece = steady_4_4();
    LaneSpanId target = open_voices(&piece, ask(PLAN_ACCOMPANIMENT_ALBERTI, STANDARD_TEXTURE_MELODY_AND_ACCOMPANIMENT), 1);
    PlanConstraint constraint;
    memset(&constraint, 0, sizeof(constraint));
    constraint.kind = PLAN_CONSTRAINT_ACCOMPANIMENT;

    /* What the constraint accepts, and refuses. */
    constraint.params.accompaniment = ask(PLAN_ACCOMPANIMENT_OOM_PAH, -1);
    constraint.params.accompaniment.low = 40;
    constraint.params.accompaniment.high = 72;
    assert(plan_lane_span_add_constraint(piece.plan, target, &constraint) == PLAN_OK);
    constraint.params.accompaniment = ask((PlanAccompanimentPattern)9, -1);
    assert(plan_lane_span_add_constraint(piece.plan, target, &constraint) == PLAN_ERR_BAD_PAYLOAD);
    constraint.params.accompaniment = ask(PLAN_ACCOMPANIMENT_BLOCK, (int)STANDARD_TEXTURE_COUNT);
    assert(plan_lane_span_add_constraint(piece.plan, target, &constraint) == PLAN_ERR_BAD_PAYLOAD);
    constraint.params.accompaniment = ask(PLAN_ACCOMPANIMENT_BLOCK, -2);
    assert(plan_lane_span_add_constraint(piece.plan, target, &constraint) == PLAN_ERR_BAD_PAYLOAD);
    constraint.params.accompaniment = ask(PLAN_ACCOMPANIMENT_BLOCK, -1);
    constraint.params.accompaniment.low = 60;
    constraint.params.accompaniment.high = 60;
    assert(plan_lane_span_add_constraint(piece.plan, target, &constraint) == PLAN_ERR_BAD_PAYLOAD);
    constraint.params.accompaniment.low = 128;
    constraint.params.accompaniment.high = 0;
    assert(plan_lane_span_add_constraint(piece.plan, target, &constraint) == PLAN_ERR_BAD_PAYLOAD);
    assert(plan_lane_span_constraint_count(piece.plan, target) == 2);

    /* The plan file names the pattern and the texture, and gives back what was written. */
    char *json = plan_to_json(piece.plan, NULL);
    assert(json && strstr(json, "\"kind\": \"accompaniment\"") && strstr(json, "\"pattern\": \"alberti\"") &&
           strstr(json, "\"texture\": \"melody and accompaniment\"") && strstr(json, "\"pattern\": \"oom-pah\""));
    PiecePlan *read = NULL;
    PlanJsonError error;
    assert(plan_from_json(json, &read, &error) == PLAN_OK);
    LaneSpanId read_target;
    assert(plan_lane_find(read, PLAN_LANE_INNER_VOICE, 1, piece.span, &read_target, 1) == 1);
    assert(plan_lane_span_constraint_count(read, read_target) == 2);
    const PlanAccompaniment *first = &plan_lane_span_constraint_at(read, read_target, 0)->params.accompaniment;
    const PlanAccompaniment *second = &plan_lane_span_constraint_at(read, read_target, 1)->params.accompaniment;
    assert(plan_lane_span_constraint_at(read, read_target, 0)->kind == PLAN_CONSTRAINT_ACCOMPANIMENT);
    assert(first->pattern == PLAN_ACCOMPANIMENT_ALBERTI && first->texture == STANDARD_TEXTURE_MELODY_AND_ACCOMPANIMENT &&
           first->low == 0 && first->high == 0);
    assert(second->pattern == PLAN_ACCOMPANIMENT_OOM_PAH && second->texture == -1 && second->low == 40 &&
           second->high == 72);
    char *again = plan_to_json(read, NULL);
    assert(again && strcmp(json, again) == 0);
    free(again);

    /* A plan file with a pattern or a texture that does not exist is refused. */
    char *bad = strdup(json);
    char *at = strstr(bad, "\"alberti\"");
    assert(at);
    memcpy(at, "\"albertz\"", 9);
    PiecePlan *refused = NULL;
    assert(plan_from_json(bad, &refused, &error) == PLAN_ERR_FORMAT && !refused);
    free(bad);
    bad = strdup(json);
    at = strstr(bad, "\"melody and accompaniment\"");
    assert(at);
    memcpy(at + 1, "melody and accompanimenx", 24);
    assert(plan_from_json(bad, &refused, &error) == PLAN_ERR_FORMAT && !refused);
    free(bad);
    free(json);
    plan_destroy(read);
    plan_destroy(piece.plan);
}

/*
 * The motive pattern: C4:e G4:e E4:e G4:e (half a bar) in C major 4/4 over I IV V I I ii V I, a bass note a bar
 * (48 41 43 48 48 50 43 48) and a held melody note of 72 a bar, with the motive in a ninth bar after the piece.
 */
static Piece motive_piece(void) {
    Piece piece = built_piece_with(4, 4, true, 1);
    piece.span.length = fraction(8, 1);
    return piece;
}

static PlanAccompaniment ask_motive(const char *name) {
    PlanAccompaniment accompaniment = ask(PLAN_ACCOMPANIMENT_MOTIVE, -1);
    strcpy(accompaniment.motive_node, name);
    return accompaniment;
}

static void test_motive_pattern(void) {
    /* Per bar: the pitches of the eighths, the chord tones nearest the motive's contour between bass and melody. */
    static const int BAR[4][8] = {{60, 67, 64, 67, 60, 67, 64, 67},
                                  {60, 65, 60, 65, 60, 65, 60, 65},
                                  {59, 67, 62, 67, 59, 67, 62, 67},
                                  {60, 67, 64, 67, 60, 67, 64, 67}};
    const long eighth = CHECK_TICKS / 8;
    for (unsigned long seed = 1; seed <= SEEDS; seed++) {
        Piece piece = motive_piece();
        CheckNote top[CHECK_MAX_NOTES];
        CheckNote others[CHECK_MAX_NOTES];
        open_voices(&piece, ask_motive("acc"), 3);
        FillOutcome outcome = accompany(piece.plan, seed);
        assert(outcome.status == FILL_STATUS_COMPLETE && outcome.steps == 1);
        assert(check_accompaniment(piece.plan, piece.span, c_major(), PLAN_ACCOMPANIMENT_DEFAULT_LOW,
                                   PLAN_ACCOMPANIMENT_DEFAULT_HIGH) > 0);
        size_t count = voice_notes(&piece, 1, top);
        /* Seven bars of eight eighths, then the liquidated bar: two eighths, the second held to the bar's end. */
        assert(count == 7 * 8 + 2);
        for (size_t bar = 0; bar < 4; bar++) {
            for (size_t n = 0; n < 8; n++) {
                const CheckNote *note = &top[bar * 8 + n];
                assert(note->onset == (long)(bar * 8 + n) * eighth && note->end == note->onset + eighth);
                assert(note->pitch == BAR[bar][n]);
            }
        }
        assert(top[56].onset == 7 * piece.bar && top[56].pitch == 60 && top[56].end == top[56].onset + eighth);
        assert(top[57].onset == 7 * piece.bar + eighth && top[57].pitch == 67 && top[57].end == 8 * piece.bar);
        /* The motive is voice 1 alone; the others are written empty. */
        assert(voice_notes(&piece, 2, others) == 0 && voice_notes(&piece, 3, others) == 0);
        fill_outcome_destroy(&outcome);
        plan_destroy(piece.plan);
    }
}

/* The pattern needs a motive: a missing node, or a node with no Given melody, leaves the target open; ANY never picks it. */
static void test_motive_pattern_needs_its_motive(void) {
    Piece piece = motive_piece();
    LaneSpanId target = open_voices(&piece, ask_motive("other"), 1);
    FillOutcome outcome = accompany(piece.plan, 1);
    assert(outcome.status == FILL_STATUS_STUCK && outcome.stuck[0].reason == FILL_STUCK_NO_OPERATOR);
    assert(plan_lane_span_state(piece.plan, target) == PLAN_STATE_CONSTRAINED);
    fill_outcome_destroy(&outcome);
    plan_destroy(piece.plan);

    /* ANY with the motive in the plan chooses a pattern of the four and not the motive. */
    for (unsigned long seed = 1; seed <= SEEDS; seed++) {
        piece = motive_piece();
        CheckNote top[CHECK_MAX_NOTES];
        open_voices(&piece, ask(PLAN_ACCOMPANIMENT_ANY, -1), 3);
        outcome = accompany(piece.plan, seed);
        assert(outcome.status == FILL_STATUS_COMPLETE);
        size_t count = voice_notes(&piece, 1, top);
        for (size_t n = 0; n < count; n++) {
            assert(top[n].end - top[n].onset == piece.beat || top[n].end - top[n].onset == CHECK_TICKS / 8);
        }
        fill_outcome_destroy(&outcome);
        plan_destroy(piece.plan);
    }
}

int main(void) {
    test_motive_pattern();
    test_motive_pattern_needs_its_motive();
    test_constraint();
    test_patterns();
    test_alberti_refused_in_compound_meter();
    test_unused_voices_are_written_empty();
    test_texture_chooses_patterns();
    test_range_is_kept();
    test_open_melody_is_not_accompanied();
    printf("test_accompaniment passed\n");
    return 0;
}
