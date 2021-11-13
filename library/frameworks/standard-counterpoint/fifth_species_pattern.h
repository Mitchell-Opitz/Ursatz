#ifndef URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_FIFTH_SPECIES_PATTERN_H
#define URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_FIFTH_SPECIES_PATTERN_H

#include <stdbool.h>
#include <stddef.h>

#include "fifth_species_exercise.h"

/*
 * Note values and bar patterns for fifth species. Fifth species mixes the
 * note values of the species before it, so the rules ask what a note's value
 * is and what shape the bar around it has. The framework has no rhythm model
 * (the slot grid is the only time it knows), so a note value is read from the
 * number of eighths a note lasts.
 *
 * Note values:
 *
 *   EIGHTH        1 eighth
 *   QUARTER       2 eighths
 *   HALF          4 eighths
 *   DOTTED_HALF   6 eighths
 *   WHOLE         8 eighths
 *
 * A bar pattern says how the notes that start in a bar divide it. A note that
 * is tied across the bar line counts only for the part of it in the bar where
 * it starts, and a bar that begins with a tie is SYNCOPATED whatever follows:
 *
 *   WHOLE           one whole note
 *   HALVES          two half notes
 *   HALF_QUARTERS   a half note and two quarter notes
 *   QUARTERS_HALF   two quarter notes and a half note
 *   QUARTERS        four quarter notes
 *   EIGHTHS         the bar has a pair of eighth notes
 *   SYNCOPATED      the strong beat is a tie from the bar before
 *   REST_START      the first bar, with a rest before its first note
 *   OTHER           any other division of the bar
 */

typedef enum {
    STANDARD_COUNTERPOINT_NOTE_VALUE_EIGHTH = 0,
    STANDARD_COUNTERPOINT_NOTE_VALUE_QUARTER,
    STANDARD_COUNTERPOINT_NOTE_VALUE_HALF,
    STANDARD_COUNTERPOINT_NOTE_VALUE_DOTTED_HALF,
    STANDARD_COUNTERPOINT_NOTE_VALUE_WHOLE,
    STANDARD_COUNTERPOINT_NOTE_VALUE_COUNT
} StandardCounterpointNoteValue;

typedef enum {
    STANDARD_COUNTERPOINT_BAR_PATTERN_WHOLE = 0,
    STANDARD_COUNTERPOINT_BAR_PATTERN_HALVES,
    STANDARD_COUNTERPOINT_BAR_PATTERN_HALF_QUARTERS,
    STANDARD_COUNTERPOINT_BAR_PATTERN_QUARTERS_HALF,
    STANDARD_COUNTERPOINT_BAR_PATTERN_QUARTERS,
    STANDARD_COUNTERPOINT_BAR_PATTERN_EIGHTHS,
    STANDARD_COUNTERPOINT_BAR_PATTERN_SYNCOPATED,
    STANDARD_COUNTERPOINT_BAR_PATTERN_REST_START,
    STANDARD_COUNTERPOINT_BAR_PATTERN_OTHER,
    STANDARD_COUNTERPOINT_BAR_PATTERN_COUNT
} StandardCounterpointBarPattern;

/* "eighth" "quarter" "half" "dotted-half" "whole"; NULL for an invalid value. */
const char *standard_counterpoint_note_value_name(StandardCounterpointNoteValue value);

/* The number of eighths of a value; 0 for an invalid value. */
size_t standard_counterpoint_note_value_eighths(StandardCounterpointNoteValue value);

/* The value of a note of `eighths` eighths. False (*out untouched) when no
   value lasts that long. */
bool standard_counterpoint_note_value_from_eighths(size_t eighths, StandardCounterpointNoteValue *out);

/* "whole" "halves" "half-quarters" "quarters-half" "quarters" "eighths"
   "syncopated" "rest-start" "other"; NULL for an invalid pattern. */
const char *standard_counterpoint_bar_pattern_name(StandardCounterpointBarPattern pattern);

/* The pattern of a bar. False (*out untouched) for an invalid exercise or a
   bar out of range. */
bool standard_counterpoint_fifth_species_bar_pattern(const StandardCounterpointFifthSpeciesExercise *exercise, size_t bar,
                                                     StandardCounterpointBarPattern *out);

/* How many notes of each value the counterpoint has, a tied note counted once
   at its full length. False (counts untouched) for an invalid exercise. */
bool standard_counterpoint_fifth_species_value_counts(const StandardCounterpointFifthSpeciesExercise *exercise,
                                                      size_t counts[STANDARD_COUNTERPOINT_NOTE_VALUE_COUNT]);

#endif
