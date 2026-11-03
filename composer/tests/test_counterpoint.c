#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <standard-counterpoint/counterpoint_rules.h>
#include <standard-counterpoint/counterpoint_slots.h>
#include <standard-counterpoint/first_species_rules.h>
#include <standard-counterpoint/fourth_species_rules.h>
#include <pitch/pitch_spelling.h>
#include <standard-counterpoint/second_species_rules.h>
#include <standard-counterpoint/third_species_rules.h>

#include <ursatz_composer/constraint.h>
#include <ursatz_composer/counterpoint.h>
#include <ursatz_composer/fill_loop.h>
#include <ursatz_composer/plan_state.h>

#include "accompaniment_check.h"

/*
 * The counterpoint operator on two cantus firmi, Fux's in D minor and one in C major, in species 1 to 4, the counter-melody
 * above (the cantus the bass) and below (the cantus the melody), seeds 1 to 10. Every result is read back from the plan's
 * lanes and held to standard-counterpoint's checks, called here on the spellings this file makes from the pitches, not
 * through the operator's own checker.
 */

#define SEEDS 10
#define MAX_BARS 16

typedef struct {
    const char *name;
    AnalysisKey key;
    size_t bars;
    int above[MAX_BARS]; /* the cantus as a bass */
    int below[MAX_BARS]; /* the cantus as a melody */
} Cantus;

static const Cantus CANTI[] = {
    {"Fux, D Dorian",
     {NULL, PITCH_LETTER_D, 0, KEY_STRUCTURE_MODE_MINOR},
     11,
     {62, 65, 64, 62, 67, 65, 69, 67, 65, 64, 62},
     {62, 65, 64, 62, 67, 65, 69, 67, 65, 64, 62}},
    {"C major",
     {NULL, PITCH_LETTER_C, 0, KEY_STRUCTURE_MODE_MAJOR},
     11,
     {60, 64, 62, 65, 64, 67, 69, 67, 64, 62, 60},
     {60, 64, 62, 65, 64, 67, 69, 67, 64, 62, 60}},
};

static Rational fraction(long num, unsigned long den) {
    Rational time;
    assert(rational_make(num, den, &time) == RATIONAL_OK);
    return time;
}

typedef struct {
    PiecePlan *plan;
    LaneSpanId target;
    PlanSpan span;
} Piece;

/* When set, the given line's first bar is two half notes, not a whole note. */
static bool split_first;

static Piece build(const Cantus *cantus, int species, bool above) {
    Piece piece;
    PlanSpan all = {fraction(0, 1), fraction((long)cantus->bars, 1)};
    PlanLanePayload payload;
    NodeId owner;
    LaneSpanId id;
    memset(&piece, 0, sizeof(piece));
    piece.plan = plan_create();
    piece.span = all;
    assert(plan_add_node(piece.plan, PLAN_NODE_NONE, PLAN_NODE_FORM, all, "form", &owner) == PLAN_OK);
    memset(&payload, 0, sizeof(payload));
    payload.meter.beats_per_bar = 4;
    payload.meter.beat_unit = 4;
    assert(plan_add_lane_span(piece.plan, owner, PLAN_LANE_METER, 0, all, &payload, &id) == PLAN_OK);
    assert(plan_lane_span_set_state(piece.plan, id, PLAN_STATE_GIVEN) == PLAN_OK);
    memset(&payload, 0, sizeof(payload));
    payload.key = cantus->key;
    assert(plan_add_lane_span(piece.plan, owner, PLAN_LANE_KEY, 0, all, &payload, &id) == PLAN_OK);
    assert(plan_lane_span_set_state(piece.plan, id, PLAN_STATE_GIVEN) == PLAN_OK);
    for (size_t b = 0; b < cantus->bars; b++) {
        PlanSpan span = {fraction((long)b, 1), fraction(1, 1)};
        int pitch = above ? cantus->above[b] : cantus->below[b];
        PlanNote note[2] = {plan_note(pitch, fraction(0, 1), fraction(1, 1)), plan_note(pitch, fraction(1, 2), fraction(1, 2))};
        memset(&payload, 0, sizeof(payload));
        payload.notes.notes = note;
        payload.notes.note_count = 1;
        if (b == 0 && split_first) {
            note[0].duration = fraction(1, 2);
            payload.notes.note_count = 2;
        }
        assert(plan_add_lane_span(piece.plan, owner, above ? PLAN_LANE_BASS : PLAN_LANE_MELODY, 0, span, &payload, &id) ==
               PLAN_OK);
        assert(plan_lane_span_set_state(piece.plan, id, PLAN_STATE_GIVEN) == PLAN_OK);
    }
    assert(plan_node_set_state(piece.plan, owner, PLAN_STATE_GIVEN) == PLAN_OK);
    {
        PlanNote note = plan_note(60, fraction(0, 1), all.length);
        PlanConstraint constraint;
        memset(&payload, 0, sizeof(payload));
        payload.notes.notes = &note;
        payload.notes.note_count = 1;
        assert(plan_add_lane_span(piece.plan, owner, PLAN_LANE_INNER_VOICE, 1, all, &payload, &id) == PLAN_OK);
        memset(&constraint, 0, sizeof(constraint));
        constraint.kind = PLAN_CONSTRAINT_COUNTERPOINT;
        constraint.params.counterpoint.species = species;
        constraint.params.counterpoint.above = above;
        assert(plan_lane_span_add_constraint(piece.plan, id, &constraint) == PLAN_OK);
        piece.target = id;
    }
    return piece;
}

static FillOutcome write(PiecePlan *plan, unsigned long seed) {
    FillOperatorRegistry *registry = fill_registry_create();
    FillConfig config;
    FillOutcome outcome;
    memset(&config, 0, sizeof(config));
    config.seed = seed;
    config.checker = counterpoint_checker;
    /* A narrower beam than the default keeps the run short; the lines still hold. */
    static const CounterpointParams params = {4, 48};
    assert(registry && counterpoint_register(registry, &params) == FILL_OK);
    assert(fill_run(plan, registry, &config, &outcome, NULL) == FILL_OK);
    fill_registry_destroy(registry);
    return outcome;
}

/* ------------------------------------------------- the library's check */

/* The spelling of a pitch of the two keys: naturals, the raised seventh and the B flat of D minor. */
static PitchSpelling *spell(int pitch) {
    static const PitchLetter LETTER[12] = {PITCH_LETTER_C, PITCH_LETTER_C, PITCH_LETTER_D, PITCH_LETTER_E,
                                           PITCH_LETTER_E, PITCH_LETTER_F, PITCH_LETTER_F, PITCH_LETTER_G,
                                           PITCH_LETTER_A, PITCH_LETTER_A, PITCH_LETTER_B, PITCH_LETTER_B};
    static const int ACCIDENTAL[12] = {0, 1, 0, -1, 0, 0, 1, 0, -1, 0, -1, 0};
    PitchSpelling *out = NULL;
    assert(pitch_spelling_create(LETTER[pitch % 12], ACCIDENTAL[pitch % 12], pitch / 12 - 1, &out) == PITCH_SPELLING_OK);
    return out;
}

/* What sounds in a slot of the counter-melody: an attack, a tie or a rest, and its pitch. */
static size_t read_slots(const Piece *piece, size_t per_bar, StandardCounterpointSlot *slots, PitchSpelling **owned) {
    CheckNote notes[CHECK_MAX_NOTES];
    size_t count = check_notes(piece->plan, PLAN_LANE_INNER_VOICE, 1, piece->span, notes);
    long bars = check_ticks(piece->span.length);
    size_t total = (size_t)(bars / CHECK_TICKS) * per_bar;
    long slot_ticks = CHECK_TICKS / (long)per_bar;
    for (size_t s = 0; s < total; s++) {
        long at = (long)s * slot_ticks;
        slots[s].kind = STANDARD_COUNTERPOINT_SLOT_REST;
        slots[s].pitch = NULL;
        for (size_t n = 0; n < count; n++) {
            if (notes[n].onset <= at && at < notes[n].end) {
                slots[s].kind = notes[n].onset == at ? STANDARD_COUNTERPOINT_SLOT_ATTACK : STANDARD_COUNTERPOINT_SLOT_TIE;
                owned[s] = spell(notes[n].pitch);
                slots[s].pitch = owned[s];
            }
        }
    }
    return total;
}

static void check_result(const Piece *piece, const Cantus *cantus, int species, bool above, const char *label) {
    PitchSpelling *cantus_spelled[MAX_BARS];
    PitchSpelling *owned[MAX_BARS * 4] = {NULL};
    StandardCounterpointSlot slots[MAX_BARS * 4];
    PitchSpelling *tonic = spell(analysis_key_tonic_pitch_class(&cantus->key) + 60);
    StandardCounterpointLine line = {(const PitchSpelling *const *)cantus_spelled, cantus->bars};
    StandardCounterpointCantusPart part = above ? STANDARD_COUNTERPOINT_CANTUS_IN_LOWER : STANDARD_COUNTERPOINT_CANTUS_IN_UPPER;
    StandardCounterpointResult result;
    bool ok;
    size_t per_bar = species == 1 ? 1 : species == 3 ? 4 : 2;
    size_t total = read_slots(piece, per_bar, slots, owned);
    /* The cantus is read from the plan as the given line, not taken from the table. */
    {
        CheckNote notes[CHECK_MAX_NOTES];
        size_t count = check_notes(piece->plan, above ? PLAN_LANE_BASS : PLAN_LANE_MELODY, 0, piece->span, notes);
        assert(count == cantus->bars);
        for (size_t i = 0; i < count; i++) {
            cantus_spelled[i] = spell(notes[i].pitch);
        }
    }
    assert(total == cantus->bars * per_bar);
    if (species == 1) {
        const PitchSpelling *counter[MAX_BARS];
        StandardCounterpointLine counter_line = {counter, cantus->bars};
        StandardCounterpointPair pair = {above ? &counter_line : &line, above ? &line : &counter_line};
        StandardCounterpointInput input = {&pair, part, tonic, NULL};
        for (size_t i = 0; i < total; i++) {
            assert(slots[i].kind == STANDARD_COUNTERPOINT_SLOT_ATTACK);
            counter[i] = slots[i].pitch;
        }
        ok = standard_counterpoint_check(&input, &result);
        assert(ok && result.hard_violations == 0);
        ok = standard_counterpoint_first_species_check(&input, &result);
        assert(ok && result.hard_violations == 0);
    } else {
        StandardCounterpointSlotLine counter = {slots, total};
        if (species == 2) {
            StandardCounterpointSecondSpeciesExercise exercise = {&line, &counter, part};
            StandardCounterpointSecondSpeciesInput input = {&exercise, tonic};
            ok = standard_counterpoint_second_species_check(&input, &result);
        } else if (species == 3) {
            StandardCounterpointThirdSpeciesExercise exercise = {&line, &counter, part};
            StandardCounterpointThirdSpeciesInput input = {&exercise, tonic};
            ok = standard_counterpoint_third_species_check(&input, &result);
        } else {
            StandardCounterpointFourthSpeciesExercise exercise = {&line, &counter, part};
            StandardCounterpointFourthSpeciesInput input = {&exercise, tonic};
            ok = standard_counterpoint_fourth_species_check(&input, &result);
        }
        if (!ok || result.hard_violations) {
            fprintf(stderr, "%s: species %d %s: ok %d hard %x\n", label, species, above ? "above" : "below", ok,
                    result.hard_violations);
        }
        assert(ok && result.hard_violations == 0);
    }
    for (size_t i = 0; i < cantus->bars; i++) {
        pitch_spelling_destroy(cantus_spelled[i]);
    }
    for (size_t i = 0; i < total; i++) {
        pitch_spelling_destroy(owned[i]);
    }
    pitch_spelling_destroy(tonic);
}

/* The notes of the counter-melody, for comparing two runs. */
static size_t notes_of(const Piece *piece, CheckNote *notes) {
    return check_notes(piece->plan, PLAN_LANE_INNER_VOICE, 1, piece->span, notes);
}

/* A line of two half notes to a bar is not a cantus firmus: species 2 is not applicable to it, and the reason says so. */
static void check_not_applicable(void) {
    Piece piece;
    PlanConstraint constraint;
    const char *why;
    split_first = true;
    piece = build(&CANTI[0], 2, true);
    split_first = false;
    why = counterpoint_why_not(piece.plan, piece.target);
    assert(why && strstr(why, "cantus firmus"));
    /* Species 1 takes any rhythm. */
    memset(&constraint, 0, sizeof(constraint));
    constraint.kind = PLAN_CONSTRAINT_COUNTERPOINT;
    constraint.params.counterpoint.species = 1;
    constraint.params.counterpoint.above = true;
    assert(plan_lane_span_remove_constraint(piece.plan, piece.target, 0) == PLAN_OK);
    assert(plan_lane_span_add_constraint(piece.plan, piece.target, &constraint) == PLAN_OK);
    assert(counterpoint_why_not(piece.plan, piece.target) == NULL);
    plan_destroy(piece.plan);
}

/* Species 1 to 4 are written by their own ctest entry: the argument is the species, none means all of them, na the
   check that a line that is not a cantus firmus is refused. */
int main(int argc, char **argv) {
    int only = argc > 1 ? atoi(argv[1]) : 0;
    if (argc > 1 && strcmp(argv[1], "na") == 0) {
        check_not_applicable();
        puts("ok");
        return 0;
    }
    for (size_t c = 0; c < sizeof(CANTI) / sizeof(CANTI[0]); c++) {
        for (int species = 1; species <= 4; species++) {
            for (int above = 0; above < 2; above++) {
                if (only && only != species) {
                    continue;
                }
                for (unsigned long seed = 1; seed <= SEEDS; seed++) {
                    Piece piece = build(&CANTI[c], species, above != 0);
                    FillOutcome outcome = write(piece.plan, seed);
                    assert(outcome.status == FILL_STATUS_COMPLETE);
                    check_result(&piece, &CANTI[c], species, above != 0, CANTI[c].name);
                    /* The same seed writes the same line. */
                    if (seed == 1) {
                        Piece again = build(&CANTI[c], species, above != 0);
                        FillOutcome repeat = write(again.plan, seed);
                        CheckNote first[CHECK_MAX_NOTES];
                        CheckNote second[CHECK_MAX_NOTES];
                        size_t count = notes_of(&piece, first);
                        assert(repeat.status == FILL_STATUS_COMPLETE);
                        assert(count == notes_of(&again, second));
                        for (size_t n = 0; n < count; n++) {
                            assert(first[n].onset == second[n].onset && first[n].end == second[n].end &&
                                   first[n].pitch == second[n].pitch);
                        }
                        fill_outcome_destroy(&repeat);
                        plan_destroy(again.plan);
                    }
                    fill_outcome_destroy(&outcome);
                    plan_destroy(piece.plan);
                }
            }
        }
    }
    puts("ok");
    return 0;
}
