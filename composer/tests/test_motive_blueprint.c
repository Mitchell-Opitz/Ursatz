#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/motive.h>
#include <ursatz_composer/motive_blueprint.h>
#include <ursatz_composer/motive_tones.h>

#include "support/test_knowledge.h"

/*
 * The events of a blueprint and the blueprint motive (R-746): where composer_event_pitch places each non-chord tone between
 * its neighbours, and a motive drawn from a rhythm, chords, an anchor and an event. Everything is in C major, 4/4.
 */

#define C4 60
#define D4 62
#define E4 64
#define F4 65
#define G4 67
#define A4 69
#define C5 72
#define D5 74
#define E5 76

static SpelledKey c_major(void) {
    return test_key_of((TestKeySpec){PITCH_LETTER_C, 0, HARMONY_MODE_MAJOR});
}

static Rational fraction(long num, unsigned long den) {
    Rational time;
    assert(rational_make(num, den, &time) == RATIONAL_OK);
    return time;
}

static int event(PlanEventType type, int previous, int next) {
    SpelledKey key = c_major();
    return composer_event_pitch(test_composer_chords(), &key, type, previous, next);
}

static void test_event_pitches(void) {
    /* The cases of the plan: appoggiatura before E4, suspension after D5, passing C4 to E4, neighbor of G4, anticipation of C5. */
    assert(event(PLAN_EVENT_APPOGGIATURA, C4, E4) == F4);
    assert(event(PLAN_EVENT_SUSPENSION, D5, C5) == D5);
    assert(event(PLAN_EVENT_PASSING, C4, E4) == D4);
    assert(event(PLAN_EVENT_NEIGHBOR, G4, G4) == A4);
    assert(event(PLAN_EVENT_ANTICIPATION, G4, C5) == C5);
    /* The other branches of the rule. */
    assert(event(PLAN_EVENT_APPOGGIATURA, D4, E4) == D4);  /* approached from a step below: below the next note */
    assert(event(PLAN_EVENT_RETARDATION, 71, C5) == 71);
    assert(event(PLAN_EVENT_PASSING, E4, C4) == D4);
    assert(event(PLAN_EVENT_PASSING, C4, G4) == E4);       /* a fifth apart: the scale tone nearest the middle, the lower */
    assert(event(PLAN_EVENT_NEIGHBOR, G4, A4 + 2) == F4);  /* the next note above: the neighbor below */
    assert(event(PLAN_EVENT_ESCAPE, E4, C4) == F4);        /* left by a leap down: the step goes up */
    assert(event(PLAN_EVENT_ESCAPE, E4, A4) == D4);
    /* A missing neighbour takes the other's pitch; both missing is none. */
    assert(event(PLAN_EVENT_APPOGGIATURA, -1, E4) == F4);
    assert(event(PLAN_EVENT_SUSPENSION, -1, D5) == D5);
    assert(event(PLAN_EVENT_NEIGHBOR, G4, -1) == A4);
    assert(event(PLAN_EVENT_PASSING, -1, -1) == -1);
}

/* The request of the plan's case: I then V7 under "x:q. x:e x:e x:e x:q | x:e x:e x:q x:h", range 63 to 79. */
static BlueprintMotiveRequest request_of(const char *rhythm, const char *harmony) {
    BlueprintMotiveRequest request;
    MotiveOptions options;
    Motive notes;
    MotiveError error;
    memset(&request, 0, sizeof(request));
    memset(&options, 0, sizeof(options));
    options.knowledge = test_knowledge();
    options.has_meter = true;
    options.meter = (PlanMeter){4, 4, 0};
    request.key = c_major();
    options.has_key = true;
    options.key = request.key;
    request.meter = options.meter;
    assert(motive_from_rhythm_text(rhythm, &options, &notes, &error) == MOTIVE_OK);
    request.note_count = notes.note_count;
    for (size_t i = 0; i < notes.note_count; i++) {
        request.onsets[i] = notes.notes[i].onset;
        request.durations[i] = notes.notes[i].duration;
    }
    assert(motive_harmony_parse_in(test_knowledge(), harmony, &request.meter, request.chords, &request.chord_count, &error) ==
           MOTIVE_OK);
    request.range_low = 63;
    request.range_high = 79;
    return request;
}

static BlueprintMotiveRequest plan_case(void) {
    BlueprintMotiveRequest request = request_of("x:q. x:e x:e x:e x:q | x:e x:e x:q x:h", "I V7@4");
    request.anchors[request.anchor_count++] = (PlanAnchor){fraction(0, 1), D5};
    request.events[request.event_count++] = (PlanEvent){fraction(0, 1), PLAN_EVENT_APPOGGIATURA};
    return request;
}

static Motive draw(const BlueprintMotiveRequest *request, unsigned long seed) {
    FillRng rng;
    Motive motive;
    MotiveError error;
    fill_rng_seed(&rng, seed);
    memset(&error, 0, sizeof(error));
    MotiveResult result = composer_blueprint_motive(test_composer_chords(), request, &rng, &motive, &error);
    if (result != MOTIVE_OK) {
        fprintf(stderr, "refused: %s (%s)\n", error.message, error.location);
    }
    assert(result == MOTIVE_OK);
    return motive;
}

/* The label motive_tones gives note i of the motive, from its overrides and harmony. */
static const char *label_of(const Motive *motive, size_t i) {
    static char label[PLAN_TONE_LABEL_MAX];
    PlanConstraint tones;
    assert(motive_tones_constraint(motive, &tones));
    assert(tones.params.motive_tones.label_count == motive->note_count);
    snprintf(label, sizeof(label), "%s", tones.params.motive_tones.labels[i]);
    return label;
}

static void test_blueprint_motive(void) {
    BlueprintMotiveRequest request = plan_case();
    for (unsigned long seed = 1; seed <= 10; seed++) {
        Motive motive = draw(&request, seed);
        size_t leaps = 0;
        assert(motive.note_count == 9);
        assert(motive.form == MOTIVE_SKETCH_FORM_FULL);
        assert(motive.notes[0].pitch == D5);
        assert(strcmp(label_of(&motive, 0), "appoggiatura") == 0);
        assert(motive.notes[1].pitch == C5);
        assert(strcmp(label_of(&motive, 1), "chord") == 0);
        for (size_t i = 0; i < motive.note_count; i++) {
            assert(motive.notes[i].pitch >= 63 && motive.notes[i].pitch <= 79);
            assert(rational_cmp(motive.notes[i].onset, request.onsets[i]) == 0);
            assert(rational_cmp(motive.notes[i].duration, request.durations[i]) == 0);
            if (i > 0) {
                int move = motive.notes[i].pitch - motive.notes[i - 1].pitch;
                assert(move != 0);
                leaps += move >= 5 || move <= -5;
            }
        }
        assert(leaps <= 1);
        /* The motive's own harmony is the chords: I, then V7 in the second bar. */
        assert(motive.chord_count == 2 && motive.chords[1].degree == 5 && motive.chords[1].seventh);
        /* The same seed draws the same motive. */
        Motive again = draw(&request, seed);
        for (size_t i = 0; i < motive.note_count; i++) {
            assert(again.notes[i].pitch == motive.notes[i].pitch);
        }
    }
}

/* Without anchors the first note is the chord tone nearest the middle of the range (71): C5, then a step where a chord tone is one. */
static void test_first_note(void) {
    BlueprintMotiveRequest request = request_of("x:q x:q x:h", "I");
    for (unsigned long seed = 1; seed <= 10; seed++) {
        Motive motive = draw(&request, seed);
        assert(motive.notes[0].pitch == C5);
        assert(motive.notes[1].pitch == E5 || motive.notes[1].pitch == G4);
        assert(motive.notes[2].pitch != motive.notes[1].pitch);
    }
}

/* An event without an anchor is placed after its neighbours: a passing tone between the chord tones around it. */
static void test_event_between(void) {
    BlueprintMotiveRequest request = request_of("x:q x:q x:q x:q", "I");
    request.anchors[request.anchor_count++] = (PlanAnchor){fraction(0, 1), E4 + 12};
    request.anchors[request.anchor_count++] = (PlanAnchor){fraction(1, 2), C5};
    request.events[request.event_count++] = (PlanEvent){fraction(1, 4), PLAN_EVENT_PASSING};
    Motive motive = draw(&request, 3);
    assert(motive.notes[0].pitch == E5 && motive.notes[1].pitch == D5 && motive.notes[2].pitch == C5);
    assert(strcmp(label_of(&motive, 1), "passing") == 0);
}

/* The refusals: an anchor where no note starts, no chords, a range with no chord tone. */
static void test_refusals(void) {
    FillRng rng;
    Motive motive;
    MotiveError error;
    BlueprintMotiveRequest request = request_of("x:h x:h", "I");
    request.anchors[request.anchor_count++] = (PlanAnchor){fraction(1, 4), C5};
    fill_rng_seed(&rng, 1);
    assert(composer_blueprint_motive(test_composer_chords(), &request, &rng, &motive, &error) == MOTIVE_ERR_VALUE);
    assert(strcmp(error.message, "anchor at 1/4: no note starts there") == 0);
    request = request_of("x:h x:h", "I");
    request.chord_count = 0;
    assert(composer_blueprint_motive(test_composer_chords(), &request, &rng, &motive, &error) == MOTIVE_ERR_VALUE);
    request = request_of("x:h x:h", "I");
    request.range_low = 61;
    request.range_high = 63;
    assert(composer_blueprint_motive(test_composer_chords(), &request, &rng, &motive, &error) == MOTIVE_ERR_VALUE);
    assert(strcmp(error.message, "note 0 at 0: no chord tone of I in 61 to 63") == 0);
    assert(composer_blueprint_motive(test_composer_chords(), NULL, &rng, &motive, &error) == MOTIVE_ERR_NULL);
}

int main(void) {
    test_event_pitches();
    test_blueprint_motive();
    test_first_note();
    test_event_between();
    test_refusals();
    printf("test_motive_blueprint: ok\n");
    return 0;
}
