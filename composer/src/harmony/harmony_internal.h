#ifndef URSATZ_COMPOSER_HARMONY_INTERNAL_H
#define URSATZ_COMPOSER_HARMONY_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>

#include <harmony_chord.h>

#include <ursatz_composer/constraint.h>
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

/* One chord of a progression. A chord starts where the one before it ends. */
typedef struct {
    char roman[PLAN_ROMAN_NUMERAL_MAX];
    PlanHarmonicFunction function;
    HarmonyChord chord;
    size_t cells;
} PhraseChord;

/* A candidate's payload. */
typedef struct {
    PhraseRequest request;
    PhraseChord *chords;
    size_t count;
} PhraseProgression;

HarmonyMode harmony_mode_of(PlanKeyMode mode);

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
