#ifndef URSATZ_THEORY_KNOWLEDGE_PATTERN_ENTRY_H
#define URSATZ_THEORY_KNOWLEDGE_PATTERN_ENTRY_H

#include <stdbool.h>
#include <stddef.h>

/*
 * K11 pattern (target architecture 5.2, R-745): a rhythm cell, one bar of a meter with the jobs it serves, or an accompaniment
 * pattern, the figure written for each unit of a bar. A rhythm is a note-list text (`x` sounds, `r` rests, then a colon and a
 * duration); the loader checks its tokens and that the durations fill one bar, the Composer's note-list reader reads it. An
 * accompaniment is read in steps on a beat or an eighth grid: `rest`, `chord` (every voice of the chord), `member` (one member of the
 * voicing, 0 to 7) or `bass_octave` (the bass note an octave up); `by_meter` replaces the steps for a meter that has its own. A
 * snapshot owns it; the strings live as long as the snapshot.
 */

#define PATTERN_STEP_MAX 16
#define PATTERN_METER_MAX 4

/* The jobs a rhythm cell serves, as bits of PatternRhythm.jobs. */
#define PATTERN_JOB_RUNNING 1u
#define PATTERN_JOB_LONG 2u
#define PATTERN_JOB_SYNCOPATED 4u
#define PATTERN_JOB_CADENCE 8u

typedef enum {
    PATTERN_STEP_REST,
    PATTERN_STEP_CHORD,
    PATTERN_STEP_MEMBER,
    PATTERN_STEP_BASS_OCTAVE
} PatternStepKind;

typedef struct {
    const char *meter; /* "2/4", "3/4", "4/4" or "6/8" */
    const char *rhythm;
    unsigned jobs;
} PatternRhythm;

typedef struct {
    PatternStepKind kind;
    int member; /* the voicing member of a PATTERN_STEP_MEMBER, 0 to 7 */
} PatternStep;

typedef struct {
    bool beat_grid; /* a step is a beat; else an eighth */
    bool compound;  /* false: the pattern is not written in a compound meter */
    bool planing;   /* true: each later chord keeps the first chord's voicing shape (R-763); absent is false */
    size_t step_count;
    PatternStep steps[PATTERN_STEP_MAX];
    size_t meter_count;
    struct {
        char meter[8];
        size_t step_count;
        PatternStep steps[PATTERN_STEP_MAX];
    } meters[PATTERN_METER_MAX];
} PatternAccompaniment;

typedef struct {
    const char *id;
    const char *name;
    bool is_rhythm; /* rhythm cell (the `rhythm` member) or accompaniment (the `accompaniment` member) */
    PatternRhythm rhythm;
    PatternAccompaniment accompaniment;
} PatternEntry;

#endif
