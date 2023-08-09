#ifndef URSATZ_COMPOSER_HARMONY_INTERNAL_H
#define URSATZ_COMPOSER_HARMONY_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>

#include <analysis_input.h>
#include <harmony_chord.h>
#include <semantics/beat_strength.h>
#include <semantics/cadence_type.h>

#include <ursatz_composer/constraint.h>
#include <ursatz_composer/harmony_embellish.h>
#include <ursatz_composer/harmony_phrase.h>
#include <ursatz_composer/lanes.h>

/* What a harmony span asks for, read from the plan. */
typedef struct {
    LaneSpanId target;
    NodeId owner;
    PlanSpan span;
    PlanKeyMode mode;
    PlanCadenceType cadence;
    PlanMeter meter;
    PlanTime bar_length;
    size_t bars;
    size_t cells_per_bar; /* the places in a bar where a chord may start */
    PlanTime cell_length;
    size_t cells;         /* bars * cells_per_bar */
} PhraseRequest;

/* The meter under a phrase: its bars and the places in a bar where a chord may start. */
typedef struct {
    PlanMeter meter;
    PlanTime bar_length;
    size_t bars;
    size_t cells_per_bar;
    PlanTime cell_length;
    size_t cells; /* bars * cells_per_bar */
} PhraseMetre;

/* One chord of a progression. A chord starts where the one before it ends. */
typedef struct {
    char roman[PLAN_ROMAN_NUMERAL_MAX];
    PlanHarmonicFunction function;
    HarmonyChord chord;          /* the chord, when it is not applied */
    bool applied;
    HarmonyAppliedChord applied_chord; /* the chord, when it is applied */
    size_t cells;
} PhraseChord;

/* A candidate's payload. */
typedef struct {
    PhraseRequest request;
    PhraseChord *chords;
    size_t count;
} PhraseProgression;

#define HARMONY_MAX_CHORDS 64

/* A phrase of chord spans in the plan, as the embellishment operator reads it. */
typedef struct {
    NodeId owner;
    PlanSpan span;
    PlanKeyMode mode;
    PlanCadenceType cadence; /* the cadence constraint on the last chord */
    PhraseMetre metre;
    size_t count;
    LaneSpanId ids[HARMONY_MAX_CHORDS];
    PhraseChord chords[HARMONY_MAX_CHORDS];
    size_t target; /* the index of the marked chord */
} PhraseContext;

/* A phrase as the recognizer is shown it. */
typedef struct {
    const PhraseChord *chords;
    size_t count;
    PlanKeyMode mode;
    PlanCadenceType requested;
    size_t cells_per_bar;
    PlanTime cell_length;
} HeardPhrase;

HarmonyMode harmony_mode_of(PlanKeyMode mode);

/* Reads the chords of the harmony lane over span, which must tile it exactly. False if it cannot be read. */
bool harmony_read_lane(const PiecePlan *plan, PlanSpan span, PlanKeyMode mode, PhraseChord *chords, size_t *count,
                       PhraseMetre *metre);

/* What the recognizer hears at the end of the phrase, and whether it arrives on the last chord. */
bool harmony_hear_phrase(const HeardPhrase *phrase, CadenceType *heard, bool *at_end);

/* How many strong beats a bar of the meter has, which is where a chord may start. False for a meter not supported. */
bool harmony_rhythm_cells_per_bar(const PlanMeter *meter, size_t *cells);
/* The strength of the cell with this index in its bar: the first is strong, any other medium. */
BeatStrength harmony_rhythm_cell_strength(size_t cell_in_bar);

/* A constraint of this kind on the span, else on its owner. */
bool harmony_find_constraint(const PiecePlan *plan, LaneSpanId id, NodeId owner, PlanConstraintKind kind,
                             PlanConstraint *out);
bool harmony_span_has_constraint(const PiecePlan *plan, LaneSpanId id, PlanConstraintKind kind);
/* The mode of the key lane over the span, else of a key constraint on the span or its owner. */
bool harmony_find_mode(const PiecePlan *plan, LaneSpanId id, NodeId owner, PlanSpan span, PlanKeyMode *out);
/* The meter under span, which must lie in one meter lane span, start on a barline and be whole bars long. */
bool harmony_read_metre(const PiecePlan *plan, PlanSpan span, PhraseMetre *out);
/* The span of cells [first_cell, first_cell + cells) counted from start. */
PlanResult harmony_cells_span(PlanTime start, PlanTime cell_length, size_t first_cell, size_t cells, PlanSpan *out);

/* Fills out from the plan. False when the span is not one this operator applies to. */
bool harmony_phrase_read_request(const PiecePlan *plan, LaneSpanId target, PhraseRequest *out);

/* The span of cells [first_cell, first_cell + cells) of the request. */
PlanResult harmony_request_cells_span(const PhraseRequest *request, size_t first_cell, size_t cells, PlanSpan *out);

/*
 * Proposes progressions for the request into out, using only rng for choices.
 * True on success, including with no candidates when the phrase is too short.
 */
bool harmony_phrase_propose(const PhraseRequest *request, FillRng *rng, FillCandidateList *out);

void harmony_progression_destroy(void *payload);

#endif
