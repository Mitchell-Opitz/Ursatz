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
/* The smallest leap, in semitones, that a step the other way is asked to follow. */
#define MELODY_RECOVER_FROM 5

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

/* The pitch classes of the key's scale at chord (the seventh degree raised when the chord has the raised one). */
unsigned melody_scale_mask(const MelodyContext *context, size_t chord);

/*
 * Whether note i of a melody is allowed: a chord tone of every chord it sounds through, or a non-chord tone of a
 * figure params->nct_types allows (passing, neighbor, appoggiatura, suspension, anticipation), approached and left
 * as NonChordTone defines the figure, and never on a strong beat but as an appoggiatura or a suspension. It reads
 * the pitches of notes i - 1 and i + 1.
 */
bool melody_figure_ok(const MelodyContext *context, const MelodyFillParams *params, const MelodyNote *notes,
                      size_t count, size_t i);

/*
 * Gives the notes between from and to, which are non-chord-tone candidates, pitches with the pitches of notes from
 * and to fixed: chord tones, or the non-chord tones of figures that hold. ideal[j] is the pitch the note j would
 * take if it could, and nothing above cap is used. False when no assignment holds.
 */
bool melody_fill_between(const MelodyContext *context, const MelodyFillParams *params, MelodyNote *notes,
                         size_t count, size_t from, size_t to, const int *ideal, int cap, FillRng *rng);

/*
 * A note that must be a chord tone: the first and the last, any on a strong beat, any as long as a cell, and any
 * held into the next chord. The others, when non-chord tones are allowed, are filled in between these.
 */
bool melody_is_anchor(const MelodyContext *context, const MelodyFillParams *params, const MelodyCandidate *melody,
                      size_t i);

/*
 * The shape of the phrase after the motive, planned once the motive's notes are placed: the note that is the
 * climax and its pitch, and the highest pitch any other note after the motive may take.
 */
typedef struct {
    bool made;
    bool in_motive; /* the climax is in the motive; no note after it is as high */
    size_t climax;  /* the index of the climax note, when it is after the motive */
    int climax_pitch;
    int cap;        /* the highest pitch of any other note after the motive */
    int motive_top; /* the highest pitch of the motive's notes placed so far */
} MelodyContour;

/* Plans the contour at note first, the first note after the motive. False when no climax can be placed. */
bool melody_contour_plan(const MelodyContext *context, const MelodyFillParams *params, const MelodyCandidate *melody,
                         size_t first, FillRng *rng, MelodyContour *out);
/* The pitch the arch would give anchor i, whose anchor before it is `before`. */
int melody_contour_ideal(const MelodyContext *context, const MelodyContour *contour, const MelodyCandidate *melody,
                         size_t i, size_t before);
/* Whether exactly one note is the highest. */
bool melody_contour_unique(const MelodyCandidate *melody);
/*
 * How many of the contour's soft rules hold: one climax, at about the asked place, leaps of a fourth or more
 * recovered by a step the other way, and not many repeated notes. *held of *total.
 */
void melody_contour_score(const MelodyCandidate *melody, unsigned *held, unsigned *total);

/*
 * Builds one melody for the context, choosing only with rng. False when the motive cannot be placed (it does not
 * fit the phrase or the range, or a strong beat has no chord tone near enough to its contour).
 */
bool melody_build(const MelodyContext *context, const MelodyFillParams *params, FillRng *rng, MelodyCandidate *out);

#endif
