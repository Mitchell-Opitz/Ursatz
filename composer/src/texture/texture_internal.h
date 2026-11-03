#ifndef URSATZ_COMPOSER_TEXTURE_INTERNAL_H
#define URSATZ_COMPOSER_TEXTURE_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>

#include <ursatz_composer/accompaniment.h>

#include "melody/melody_internal.h"

/* Time inside the operator is a Rational position in whole notes from the start of the target (util/position.h). */
#define ACCOMP_VOICES 3
#define ACCOMP_MAX_NOTES 256
#define ACCOMP_MAX_LINE 256
#define ACCOMP_MAX_BEATS 64
#define ACCOMP_PAYLOAD_MAGIC 0x41434331UL
/* An eighth note, the step of the arpeggiated and Alberti figures. */
#define ACCOMP_EIGHTH ((Rational){1, 8})

/* One sounding note of the bass or the melody, clipped to the target. */
typedef struct {
    Rational onset;
    Rational end;
    int pitch;
} AccompLine;

/* The most notes of an accompaniment motive. */
#define ACCOMP_MAX_MOTIVE 64

/* One note of an accompaniment motive: positions from the start of the motive. */
typedef struct {
    Rational onset;
    Rational duration;
    int pitch;
} AccompMotiveNote;

/* What the operator reads from the plan: the target, the chords, the bass and the melody it is written under. */
typedef struct {
    LaneSpanId target;
    NodeId owner;
    PlanSpan span;
    AnalysisKey key;
    PlanMeter meter;
    PlanAccompanimentPattern pattern; /* as asked: ANY or one */
    int texture;                      /* a StandardTexture, or -1 */
    int low;
    int high;
    bool compound;
    Rational length;
    Rational beat;  /* the pattern's beat: the meter's beat, or the dotted quarter of a compound meter */
    Rational cell;  /* a cell of the meter (the places in a bar a chord may start) */
    Rational bar;
    Rational phase; /* from the bar line at or before the target's start to its start */
    size_t chord_count;
    Rational chord_start[HARMONY_MAX_CHORDS + 1]; /* the last entry is the length */
    int chord_pcs[HARMONY_MAX_CHORDS][4];     /* root, third, fifth and seventh, as pitch classes */
    size_t chord_pc_count[HARMONY_MAX_CHORDS];
    unsigned chord_mask[HARMONY_MAX_CHORDS];
    AccompLine bass[ACCOMP_MAX_LINE];
    size_t bass_count;
    AccompLine melody[ACCOMP_MAX_LINE];
    size_t melody_count;
    LaneSpanId voice_span[ACCOMP_VOICES]; /* the span of each voice that is there to be written, else none */
    /* The motive of the pattern MOTIVE; empty (motive_count 0) for the other patterns. */
    AccompMotiveNote motive[ACCOMP_MAX_MOTIVE];
    size_t motive_count;
    Rational motive_length;  /* where the motive's last note ends */
    Rational liquidate_from; /* from the target's start: the motive is liquidated from here (the last bar of the
                                section); at or past length when the target does not end the section */
} AccompContext;

typedef struct {
    Rational onset;
    Rational duration;
    int pitch;
} AccompNote;

/* A candidate's payload: the pattern, the notes of each voice (index 0 is voice 1, the top), and how it was voiced. */
typedef struct {
    unsigned long magic;
    PlanAccompanimentPattern pattern;
    size_t voices; /* how many voices the pattern writes */
    size_t note_count[ACCOMP_VOICES];
    AccompNote notes[ACCOMP_VOICES][ACCOMP_MAX_NOTES];
    /*
     * The beats of the pattern: each chord is cut into beats from its start, the last of a chord cut at its end, so a
     * chord of any length in beats has whole beats and, when the chord is shorter than a beat or not a whole number of
     * them, a short one. Notes and voicings are indexed by these.
     */
    size_t beat_count;
    Rational beat_start[ACCOMP_MAX_BEATS];
    Rational beat_end[ACCOMP_MAX_BEATS];
    bool voiced[ACCOMP_MAX_BEATS];
    /* How each beat was voiced, low to high: three notes, or two where the window leaves no room for three. */
    size_t voicing_size[ACCOMP_MAX_BEATS];
    int voicing[ACCOMP_MAX_BEATS][ACCOMP_VOICES];
} AccompCandidate;

/* Reads a target; false when it is not one the operator applies to. */
bool accomp_read(const PiecePlan *plan, LaneSpanId target, AccompContext *out);

/* The pitch classes of a chord in the key, root first (3 or 4 of them). */
bool accomp_chord_tones(const AnalysisKey *key, const PhraseChord *chord, int pcs[4], size_t *count);
/* The notes of a lane and voice over a span that sound inside it, from origin; false if any span there is open. */
bool accomp_read_line(const PiecePlan *plan, PlanLaneKind lane, int voice, PlanSpan span, Rational origin,
                      Rational length, AccompLine *out, size_t *count);
/* The span of an inner voice (2 or 3) that is to be written with a target: none, or an open one over the same time. */
bool accomp_read_voice(const PiecePlan *plan, int voice, PlanSpan span, LaneSpanId *out);

/* Sets every position of a context to 0, so a context that is not read still holds valid Rationals. */
void accomp_context_zero(AccompContext *context);

/* The index of the chord sounding at a position of the target. */
size_t accomp_chord_at(const AccompContext *context, Rational position);

/* Whether the pattern can be written in the meter, and under the texture asked for when the pattern is ANY. */
bool accomp_pattern_allowed(const AccompContext *context, PlanAccompanimentPattern pattern);
/* How many voices the pattern writes: three for a block and an oom-pah, one for the lines and the motive. */
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
