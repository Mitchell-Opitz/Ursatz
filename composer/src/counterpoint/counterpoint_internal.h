#ifndef URSATZ_COMPOSER_COUNTERPOINT_INTERNAL_H
#define URSATZ_COMPOSER_COUNTERPOINT_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>

#include <pitch/pitch_spelling.h>

#include <ursatz_composer/counterpoint.h>

#include "texture/texture_internal.h"

/* The most notes of the given line: standard-counterpoint's most bars. */
#define CP_MAX_NOTES 64
/* The most slots of a counter-melody: four to a bar. */
#define CP_MAX_SLOTS (CP_MAX_NOTES * 4)
#define CP_PAYLOAD_MAGIC 0x43505031UL

/* How a slot of the counter-melody sounds; the values are standard-counterpoint's StandardCounterpointSlotKind. */
#define CP_SLOT_ATTACK 0
#define CP_SLOT_TIE 1
#define CP_SLOT_REST 2

/* What the operator reads from the plan: the given line, the key and the pitches the counter-melody may take. */
typedef struct {
    LaneSpanId target;
    NodeId owner;
    PlanSpan span;
    AnalysisKey key;
    int species;
    bool above;
    size_t count;            /* the notes of the given line */
    Rational onset[CP_MAX_NOTES]; /* positions from the start of the target, in whole notes */
    Rational duration[CP_MAX_NOTES];
    int line[CP_MAX_NOTES];  /* MIDI pitch of each note of the given line */
    Rational length;
    Rational bar;            /* the length of a bar; 0 in species 1 */
    size_t per_bar;          /* the slots to a bar in species 2 to 4; 1 in species 1, a slot for each note */
    int low;                 /* the MIDI range of the counter-melody */
    int high;
    PitchSpelling *spell[128]; /* the spelling of each MIDI pitch in the key's scale, else NULL */
    PitchSpelling *tonic;
    /* Whether each pitch makes a consonance with each note of the given line (false when the parts would cross). */
    bool consonant[CP_MAX_NOTES][128];
} CounterpointContext;

/* A candidate's payload: what each slot sounds, and its pitch. Species 1 has a slot for each note. */
typedef struct {
    unsigned long magic;
    size_t slots;
    unsigned char kind[CP_MAX_SLOTS];
    int pitch[CP_MAX_SLOTS];
} CounterpointCandidate;

/* Reads a target; false when it is not one the operator applies to, and then *why is the reason when it is not NULL. */
bool counterpoint_read(const PiecePlan *plan, LaneSpanId target, CounterpointContext *out, const char **why);
/* The same for a target that is written already (Realized or Given): its counter-melody is read from the span. */
bool counterpoint_read_written(const PiecePlan *plan, LaneSpanId target, CounterpointContext *out,
                               CounterpointCandidate *line, const char **why);
void counterpoint_context_free(CounterpointContext *context);

/* How the slots of the counter-melody are laid out in the given line's notes: the note a slot sounds with. */
size_t counterpoint_slot_note(const CounterpointContext *context, size_t slot);

/* Judges the first notes of the given line's counter-melody, as a candidate holds them. */
typedef struct {
    const char *hard; /* NULL when no hard rule is broken, else which */
    unsigned held;    /* the soft groups with no violation, of total */
    unsigned total;
    /* With final true: the groups asked, from 1 (0 is the given line's own, which is not judged), and the groups with a
       hard or a soft rule broken, bit g for group g. */
    unsigned groups;
    unsigned hard_groups;
    unsigned soft_groups;
    const char *(*group_name)(unsigned group);
} CounterpointVerdict;

/*
 * Asks standard-counterpoint whether the counter-melody holds against the first notes of the given line. slots is
 * notes * per_bar. With final true the line is the whole counter-melody and every rule is asked; else the line is a
 * prefix whose last bar stands for a whole note and the rules that only a finished line can meet (the ending, the
 * final step) are not. A prefix too short for the species to judge passes. False when the library failed.
 */
bool counterpoint_eval(const CounterpointContext *context, size_t notes, const unsigned char *kind, const int *pitch,
                       bool final, CounterpointVerdict *verdict);

/*
 * Proposes up to wanted complete counter-melodies, cheapest first, as CounterpointCandidate payloads in out, which the
 * caller frees; *count is how many. None when no line holds. Ties in cost are ordered by rng. False only on allocation
 * failure.
 */
bool counterpoint_search(const CounterpointContext *context, FillRng *rng, size_t wanted, size_t beam,
                         CounterpointCandidate **out, size_t *count);

#endif
