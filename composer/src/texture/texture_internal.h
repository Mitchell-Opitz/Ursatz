#ifndef URSATZ_COMPOSER_TEXTURE_INTERNAL_H
#define URSATZ_COMPOSER_TEXTURE_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>

#include <ursatz_composer/accompaniment.h>

#include "melody/melody_internal.h"

/* Time inside the operator is ticks from the start of the target, MELODY_TICKS to a whole note. */
#define ACCOMP_VOICES 3
#define ACCOMP_MAX_NOTES 256
#define ACCOMP_MAX_LINE 256
#define ACCOMP_MAX_BEATS 64
#define ACCOMP_PAYLOAD_MAGIC 0x41434331UL
/* An eighth note, the grid of the arpeggiated and Alberti figures. */
#define ACCOMP_EIGHTH (MELODY_TICKS / 8)

/* One sounding note of the bass or the melody, clipped to the target. */
typedef struct {
    long onset;
    long end;
    int pitch;
} AccompLine;

/* What the operator reads from the plan: the target, the chords, the bass and the melody it is written under. */
typedef struct {
    LaneSpanId target;
    NodeId owner;
    PlanSpan span;
    PlanKey key;
    PlanMeter meter;
    PlanAccompanimentPattern pattern; /* as asked: ANY or one */
    int texture;                      /* a StandardTexture, or -1 */
    int low;
    int high;
    bool compound;
    long length;
    long beat;  /* the pattern's beat: a quarter, or the dotted quarter of a compound meter */
    long bar;
    long phase; /* ticks from the bar line at or before the target's start to its start */
    size_t chord_count;
    long chord_start[HARMONY_MAX_CHORDS + 1]; /* the last entry is the length */
    int chord_pcs[HARMONY_MAX_CHORDS][4];     /* root, third, fifth and seventh, as pitch classes */
    size_t chord_pc_count[HARMONY_MAX_CHORDS];
    unsigned chord_mask[HARMONY_MAX_CHORDS];
    AccompLine bass[ACCOMP_MAX_LINE];
    size_t bass_count;
    AccompLine melody[ACCOMP_MAX_LINE];
    size_t melody_count;
    LaneSpanId voice_span[ACCOMP_VOICES]; /* the span of each voice that is there to be written, else none */
} AccompContext;

typedef struct {
    long onset;
    long duration;
    int pitch;
} AccompNote;

/* A candidate's payload: the pattern, the notes of each voice (index 0 is voice 1, the top), and how it was voiced. */
typedef struct {
    unsigned long magic;
    PlanAccompanimentPattern pattern;
    size_t voices; /* how many voices the pattern writes */
    size_t note_count[ACCOMP_VOICES];
    AccompNote notes[ACCOMP_VOICES][ACCOMP_MAX_NOTES];
    size_t beat_count;
    bool voiced[ACCOMP_MAX_BEATS];
    /* How each beat was voiced, low to high: three notes, or two where the window leaves no room for three. */
    size_t voicing_size[ACCOMP_MAX_BEATS];
    int voicing[ACCOMP_MAX_BEATS][ACCOMP_VOICES];
} AccompCandidate;

/* Reads a target; false when it is not one the operator applies to. */
bool accomp_read(const PiecePlan *plan, LaneSpanId target, AccompContext *out);

/* The pitch classes of a chord in the key, root first (3 or 4 of them). */
bool accomp_chord_tones(const PlanKey *key, const PhraseChord *chord, int pcs[4], size_t *count);
/* The notes of a lane over a span that sound inside it, ticks from origin; false if any span there is open. */
bool accomp_read_line(const PiecePlan *plan, PlanLaneKind lane, PlanSpan span, long origin, long length, AccompLine *out,
                      size_t *count);
/* The span of an inner voice (2 or 3) that is to be written with a target: none, or an open one over the same time. */
bool accomp_read_voice(const PiecePlan *plan, int voice, PlanSpan span, LaneSpanId *out);

/* The index of the chord sounding at a tick of the target. */
size_t accomp_chord_at(const AccompContext *context, long tick);

/* Whether the pattern can be written in the meter, and under the texture asked for when the pattern is ANY. */
bool accomp_pattern_allowed(const AccompContext *context, PlanAccompanimentPattern pattern);
/* How many voices the pattern writes: three for a block and an oom-pah, one for the lines. */
size_t accomp_pattern_voices(PlanAccompanimentPattern pattern);
const char *accomp_pattern_name(PlanAccompanimentPattern pattern);

/* The distance in semitones between two voicings, voice by voice from the top. */
long accomp_voicing_distance(const int *a, size_t a_size, const int *b, size_t b_size);

/* Builds one accompaniment in the pattern, choosing only with rng. False when a beat cannot be voiced. */
bool accomp_build(const AccompContext *context, PlanAccompanimentPattern pattern, FillRng *rng, AccompCandidate *out);

/*
 * Whether standard-texture's analysis hears the melody, the bass and the candidate, as three tracks (the melody the
 * first, the bass the second, the accompaniment the third, its voices the voices of the track), as the texture the
 * target asks for or its category in every measure of the span. *heard is meaningful on true; false when the analysis
 * could not be run.
 */
bool accomp_hear(const AccompContext *context, const AccompCandidate *candidate, bool *heard);

/* Judges a candidate against the plan: hard rules, and the soft ones held of those that apply. */
typedef struct {
    const char *hard; /* NULL when no hard rule is broken, else which */
    unsigned held;
    unsigned total;
} AccompVerdict;

void accomp_judge(const AccompContext *context, const AccompCandidate *candidate, AccompVerdict *verdict);

#endif
