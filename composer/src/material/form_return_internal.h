#ifndef URSATZ_COMPOSER_FORM_RETURN_INTERNAL_H
#define URSATZ_COMPOSER_FORM_RETURN_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>

#include <plan/plan_constraint.h>
#include <ursatz_composer/fill_loop.h>
#include <ursatz_composer/fill_rng.h>
#include <ursatz_composer/form_return.h>
#include <plan/plan_lanes.h>
#include <plan/plan.h>
#include <plan/plan_state.h>

#define RETURN_PAYLOAD_MAGIC 0x52544E31UL
/* The owner of a node or span that belongs to the section itself. */
#define RETURN_SECTION ((size_t)-1)

/* One node of the subtree of a section, below the section. A node's parent comes before it. */
typedef struct {
    PlanNodeKind kind;
    size_t parent; /* an index into the copy's nodes, or RETURN_SECTION */
    PlanSpan span;
    char *name;
    PlanState state;
    PlanConstraint *constraints;
    size_t constraint_count;
} ReturnNode;

/* One written lane span under the section: harmony, bass, melody or inner voice. */
typedef struct {
    PlanLaneKind lane;
    int voice;
    size_t owner; /* an index into the copy's nodes, or RETURN_SECTION */
    PlanSpan span;
    PlanState state;
    PlanHarmony harmony; /* the chord, in the harmony lane */
    PlanNote *notes;     /* the notes, in the others */
    size_t note_count;
    PlanConstraint *constraints;
    size_t constraint_count;
} ReturnSpan;

/* A section's subtree and lane spans, copied out of the plan. */
typedef struct {
    ReturnNode *nodes;
    size_t node_count;
    size_t node_capacity;
    ReturnSpan *spans;
    size_t span_count;
    size_t span_capacity;
} ReturnCopy;

/* What a candidate of form-return carries: the returned section as it is, and the target as it is to become. */
typedef struct {
    unsigned long magic;
    SectionReturnVariation variation;
    PlanSpan span;       /* the target section's span */
    SpelledKey key;         /* the key at its start */
    PlanMeter meter;
    Rational from_start; /* where the returned section starts */
    int transpose;       /* the key difference, in semitones, the copy is moved by */
    int octave;          /* REREGISTER: 12 or -12, else 0 */
    int range_low;       /* the melody range a re-registered melody keeps to */
    int range_high;
    bool reharmonize;    /* FIGURATE: the figure sits over a reharmonized copy (the FIGURE constraint says so; always, with mode_change) */
    bool mode_change;    /* FIGURATE: the target is the source's tonic in the other mode (R-499): the copy is mapped degree by degree */
    const PitchMaterial *to_material; /* the target's material; read when mode_change */
    PlanAccompanimentPattern setting; /* SETTING: the pattern of the new accompaniment (R-518) */
    ReturnCopy source;   /* the returned section, as read */
    ReturnCopy result;   /* the target's, after the variation */
} ReturnPayload;

/* Reads the subtree and written lane spans of section into out. False on allocation failure. */
bool return_copy_read(const Plan *plan, NodeId section, ReturnCopy *out);
bool return_copy_clone(const ReturnCopy *from, ReturnCopy *out);
void return_copy_destroy(ReturnCopy *copy);
/* Moves the copy from a section starting at from to one starting at to, its pitches by transpose semitones. */
bool return_copy_move(ReturnCopy *copy, Rational from, Rational to, int transpose);
/*
 * Maps the copy, already moved, from the other mode of home's tonic into `to` (R-499): into minor every melody and inner
 * voice pitch 4 or 9 semitones over the tonic is lowered a semitone; into major every pitch 3 or 8 over it is raised a
 * semitone and 10 is raised to 11 (the leading tone is kept). The KEY constraints of the nodes and spans take the mode.
 */
bool return_copy_change_mode(ReturnCopy *copy, SpelledKey home, const PitchMaterial *to);
/* The pitch moved from the other mode of the tonic (a pitch class) into `to`, as return_copy_change_mode maps it. */
int return_map_pitch(int tonic, const PitchMaterial *to, int pitch);
/* The pitch a source pitch (after the move) has in the result: mapped by the mode change of the payload, else as it is. */
int return_mode_pitch(const ReturnPayload *payload, int pitch);
/* Whether the notes of two spans are the same (count, pitches, onsets and durations): the second's pitches are the first's moved `shift`
   semitones, then mapped as return_mode_pitch maps them when map_or_null is a payload (NULL: not mapped). */
bool return_same_notes(const ReturnSpan *a, const ReturnSpan *b, int shift, const ReturnPayload *map_or_null);
/* Adds the copy to the plan under section: its nodes, then its lane spans. */
/*
 * Writes the copy into the plan under `section`. operation says what the written notes are made from (R-601): with
 * PLAN_NOTE_FROM_RETURN each note gets source = its id in the copy, a fresh id and operation RETURN (a return inside its own plan);
 * with PLAN_NOTE_FROM_NONE its id, source and operation are cleared (a copy from another plan has no source here, so the plan
 * gives it a fresh id).
 */
PlanResult return_copy_write(Plan *plan, NodeId section, const ReturnCopy *copy, PlanNoteOperation operation);

void return_payload_destroy(void *payload);

/*
 * The key difference in semitones, from -5 to 6, between a section of one plan and a section of another (or the same)
 * plan, whose key lanes must have the same shape and mode; false when they differ.
 */
bool return_key_shift(const Plan *source_plan, PlanSpan source, const Plan *target_plan, PlanSpan target,
                      int *shift);
/*
 * Whether the key lanes of the two sections have the same shape, every target span is in the other mode from the
 * source's at the same place, all target spans are in one mode, and the tonic moves by one step throughout (R-524);
 * *shift is that step in semitones, from -5 to 6, and *to_material the target's material.
 */
bool return_mode_shift_by(const Plan *source_plan, PlanSpan source, const Plan *target_plan, PlanSpan target,
                          int *shift, const PitchMaterial **to_material);
/*
 * return_mode_shift_by with the shift required to be 0: every target span is on the same tonic as the source's at the
 * same place in the other mode (R-499). A copy across modes of a figurate return is moved by 0.
 */
bool return_mode_shift(const Plan *source_plan, PlanSpan source, const Plan *target_plan, PlanSpan target,
                       const PitchMaterial **to_material);
/* The return of a section that form_return_copies holds (the returned section, the variation); false otherwise. */
bool return_target(const Plan *plan, NodeId section, ReturnRef *ret);
/* The meter lane span that holds the span whole; false when none does. */
bool return_meter_over(const Plan *plan, PlanSpan span, PlanMeter *out);
/* Whether the section is not Constrained and its harmony, bass and melody are all written. */
bool return_source_ready(const Plan *plan, NodeId section);

/* What a variation may change in a copy that is otherwise the returned section moved onto the target. */
typedef struct {
    unsigned skip_notes; /* bits (1u << lane) of the lanes whose notes are not compared */
    unsigned shifted;    /* bits of the lanes whose pitches are shift semitones from the returned section's */
    int shift;
    unsigned ignored;    /* bits of the lanes whose spans are left out of the comparison, on both sides */
} ReturnAllow;

/* Whether payload->result is payload->source moved onto the target, but for what allow lets differ. */
bool return_result_matches(const ReturnPayload *payload, const ReturnAllow *allow);

/*
 * The variations (form_vary.c). return_vary makes payload->result the variation of the moved copy it holds, drawing
 * from rng; false when the variation cannot be made (no note to ornament, no octave inside the range).
 * return_vary_check judges a candidate of a variation other than EXACT.
 */
bool return_vary(ReturnPayload *payload, FillRng *rng);
void return_vary_check(const ReturnPayload *payload, FillCheck *out);
/* Whether two candidates of ORNAMENT ornament the melody the same way. */
bool return_same_melody(const ReturnPayload *a, const ReturnPayload *b);

#endif
