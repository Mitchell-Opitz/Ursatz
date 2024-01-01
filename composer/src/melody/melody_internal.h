#ifndef URSATZ_COMPOSER_MELODY_INTERNAL_H
#define URSATZ_COMPOSER_MELODY_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>

#include <ursatz_composer/melody_fill.h>
#include <ursatz_composer/motive.h>

#include "harmony/harmony_internal.h"

/*
 * Inside the melody operator, time is a whole number of ticks from the start of the phrase, MELODY_TICKS to a
 * whole note. A time the plan holds that is not a whole number of ticks is not one the operator places on.
 */
#define MELODY_TICKS 128L
#define MELODY_MAX_NOTES 256
#define MELODY_MAX_BASS 256
#define MELODY_PAYLOAD_MAGIC 0x4D454C31UL

/* One note of the bass lane: when it sounds, its pitch, and the chord it sounds under. */
typedef struct {
    long onset;
    long end;
    int pitch;
    size_t chord;
} MelodyBass;

/* What the operator reads from the plan: the harmony and bass under the phrase, and the motive to place. */
typedef struct {
    LaneSpanId target;
    NodeId owner;
    PlanSpan span;
    PlanKey key;
    PhraseMetre metre;
    long length;
    long cell;
    size_t count;
    PhraseChord chords[HARMONY_MAX_CHORDS];
    long chord_start[HARMONY_MAX_CHORDS + 1]; /* the last entry is the phrase's length */
    unsigned chord_mask[HARMONY_MAX_CHORDS];  /* the pitch classes of each chord, bit pc */
    unsigned chord_seventh[HARMONY_MAX_CHORDS]; /* the pitch class of its seventh, or 0 */
    MelodyBass bass[MELODY_MAX_BASS];
    size_t bass_count;
    bool has_cadence;
    PlanCadenceType cadence;
    bool motive_given; /* the motive is the original, not a development */
    PlanMotiveTransform transform;
    NodeId motive_node;
    size_t motive_count;
    int motive_pitch[MOTIVE_MAX_NOTES];
    long motive_onset[MOTIVE_MAX_NOTES];
    long motive_duration[MOTIVE_MAX_NOTES];
    long motive_length;
} MelodyContext;

/* One note of a candidate. motive_index is the motive note it carries, or -1 for a note that fills the phrase. */
typedef struct {
    long onset;
    long duration;
    int pitch;
    int motive_index;
    bool strong; /* it starts on a cell */
    size_t chord; /* the chord sounding at its onset */
} MelodyNote;

/* A candidate's payload. */
typedef struct {
    unsigned long magic;
    MelodyContext context;
    MelodyFillParams params;
    MelodyNote notes[MELODY_MAX_NOTES];
    size_t count;
} MelodyCandidate;

/* A time in the plan as ticks from the start of the plan; false when it is not a whole number of them. */
bool melody_ticks(PlanTime time, long *out);
/* Ticks as a time in the plan. */
PlanResult melody_time(long ticks, PlanTime *out);

/* The pitch classes (bit pc of mask) of a chord in the key, and of its seventh (0 when it has none). */
bool melody_chord_mask(const PlanKey *key, const PhraseChord *chord, unsigned *mask, unsigned *seventh);
/* The index of the chord sounding at a tick of the phrase. */
size_t melody_chord_at(const MelodyContext *context, long tick);

/*
 * Reads the harmony, bass and motive under a Constrained melody span: the chord spans must tile the span exactly,
 * none of them open, the bass spans must tile it, and a motive_ref constraint must name a motive with a melody.
 * False when the span is not one the melody operator applies to.
 */
bool melody_context_read(const PiecePlan *plan, LaneSpanId target, MelodyContext *out);

/*
 * The pitch classes a note must be one of: those the chords it sounds through share, so that a note that is
 * held into the next chord is a chord tone of it too.
 */
unsigned melody_need_mask(const MelodyContext *context, const MelodyFillParams *params, const MelodyNote *note);

/*
 * Builds one melody for the context, choosing only with rng. False when the motive cannot be placed (it does not
 * fit the phrase or the range, or a strong beat has no chord tone near enough to its contour).
 */
bool melody_build(const MelodyContext *context, const MelodyFillParams *params, FillRng *rng, MelodyCandidate *out);

#endif
