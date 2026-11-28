#ifndef URSATZ_THEORY_HARMONY_CHORD_VALUE_H
#define URSATZ_THEORY_HARMONY_CHORD_VALUE_H

#include <stdbool.h>
#include <stddef.h>

/*
 * Hindemith's value of a sonority (R-764; The Craft of Musical Composition, Book I chapter III sections 2 to 12 and chapter IV section 3).
 * Series 2 orders the twelve interval classes from the most to the least consonant and names the tone that is the root of each; the
 * chord groups Ia to VI, ranked 1 to 12, order the chords by their tension. The two tables below are the identity of the `interval-class`
 * and `chord-group` vocabulary entries of `knowledge/vocabulary/hindemith.json`; a test pins each table to its entries both ways.
 */

#define CHORD_VALUE_RANKS 12

/* Words of the tables as the vocabulary schemas have them, in order. */
enum { CHORD_VALUE_ROOT_NONE = 0, CHORD_VALUE_ROOT_LOWER = 1, CHORD_VALUE_ROOT_UPPER = 2 };                 /* of an interval */
enum { CHORD_VALUE_TRITONES_NONE = 0, CHORD_VALUE_TRITONES_ONE, CHORD_VALUE_TRITONES_SEVERAL, CHORD_VALUE_TRITONES_ANY };
enum { CHORD_VALUE_SHARP_NONE = 0, CHORD_VALUE_SHARP_MILD, CHORD_VALUE_SHARP_SHARP, CHORD_VALUE_SHARP_ANY };
enum { CHORD_VALUE_GROUP_ROOT_BASS = 0, CHORD_VALUE_GROUP_ROOT_ABOVE, CHORD_VALUE_GROUP_ROOT_NONE, CHORD_VALUE_GROUP_ROOT_ANY };

typedef struct {
    const char *code; /* "fifth" */
    int semitones;    /* 0 to 12 */
    int rank;         /* 1 (octave) to 12 (tritone) */
    int root;         /* CHORD_VALUE_ROOT_*: which tone of the interval is its root */
} ChordValueInterval;

typedef struct {
    const char *code; /* "ia", "iib1" */
    int group;        /* 1 to 6 */
    int rank;         /* 1 (Ia) to 12 (VI) */
    int tritones;     /* CHORD_VALUE_TRITONES_* */
    int sharp;        /* CHORD_VALUE_SHARP_*: mild is a major second or minor seventh, sharp a minor second or major seventh */
    int root;         /* CHORD_VALUE_GROUP_ROOT_* */
} ChordValueGroup;

extern const ChordValueInterval CHORD_VALUE_SERIES_TWO[CHORD_VALUE_RANKS];
extern const ChordValueGroup CHORD_VALUE_GROUPS[CHORD_VALUE_RANKS];

typedef struct {
    const char *group; /* the code of the chord group, as in CHORD_VALUE_GROUPS */
    int rank;          /* 1 to 12 */
    int root_pitch_class; /* 0 to 11, or -1 for a chord without a root (groups V and VI) */
    bool root_in_bass;
} ChordValue;

/*
 * The chord group of a sonority. `pitches` are MIDI numbers, the lowest the bass; a pitch class sounding twice counts once, by its lowest
 * pitch. Intervals are taken as sounding, from the lower tone to the upper, reduced into the octave. False for fewer than three pitch
 * classes (or NULL arguments), `out` untouched.
 */
bool chord_value_classify(const int *pitches, size_t count, ChordValue *out);

#endif
