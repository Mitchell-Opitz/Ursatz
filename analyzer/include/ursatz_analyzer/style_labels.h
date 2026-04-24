#ifndef URSATZ_ANALYZER_STYLE_LABELS_H
#define URSATZ_ANALYZER_STYLE_LABELS_H

#include <stdbool.h>
#include <stddef.h>

/*
 * The labels a style profile counts. The Analyzer labels a corpus's analysis documents with these functions and
 * the Composer labels its own material with the same ones, so a count and a candidate always meet under the same
 * name.
 */

typedef enum {
    STYLE_DIMENSION_HARMONY = 0,
    STYLE_DIMENSION_RHYTHM,
    STYLE_DIMENSION_MELODY,
    STYLE_DIMENSION_FORM
} StyleDimension;

#define STYLE_DIMENSION_COUNT 4

/* Category names, each a StyleStatistic category string. */
#define STYLE_CATEGORY_CHORD_NUMERAL "chord-numeral"
#define STYLE_CATEGORY_CHORD_PROGRESSION_2 "chord-progression-2"
#define STYLE_CATEGORY_CHORD_PROGRESSION_3 "chord-progression-3"
#define STYLE_CATEGORY_CADENCE_TYPE "cadence-type"
#define STYLE_CATEGORY_HARMONIC_RHYTHM "harmonic-rhythm"
#define STYLE_CATEGORY_DURATION "duration"
#define STYLE_CATEGORY_METER "meter"
#define STYLE_CATEGORY_SYNCOPATION "syncopation-per-phrase"
#define STYLE_CATEGORY_MELODIC_INTERVAL "melodic-interval"
#define STYLE_CATEGORY_CONTOUR "contour"
#define STYLE_CATEGORY_PHRASE_RANGE "phrase-range"
#define STYLE_CATEGORY_THEME_FORM "theme-form"
#define STYLE_CATEGORY_PHRASE_LENGTH "phrase-length"
#define STYLE_CATEGORY_SECTION_LETTERS "section-letters"
#define STYLE_CATEGORY_MODE "mode"

/* "harmony", "rhythm", "melody" or "form"; NULL for a value out of range. */
const char *style_dimension_name(StyleDimension dimension);

/* The dimension named name; false for any other text. */
bool style_dimension_from_name(const char *name, StyleDimension *out);

/* The dimension a category belongs to; false for a category this module does not define. */
bool style_category_dimension(const char *category, StyleDimension *out);

/*
 * A Roman numeral in root position: the inversion figure is dropped and a seventh chord keeps a 7, so "V6/5",
 * "V4/3" and "V4/2" give "V7", "I6/4" gives "I" and an applied chord keeps its target ("V6/5/V" gives "V7/V").
 * Accidentals and quality marks before the figure are kept. NULL when text is not a numeral. Caller frees.
 */
char *style_label_numeral(const char *numeral);

/* Numerals joined with '-' ("ii-V7-I"), each as given. NULL for count 0 or on allocation failure. Caller frees. */
char *style_label_progression(const char *const *numerals, size_t count);

/*
 * The directed semitone step between two melody notes: "+2", "-5", "0"; a step of an octave and a step beyond
 * (13 or more) read "+13+" and "-13+". Writes at most size bytes, always terminated.
 */
void style_label_interval(int semitones, char *buffer, size_t size);

/* "up", "down" or "same" for the sign of a semitone step. */
const char *style_label_contour(int semitones);

/* A duration in whole notes as a reduced fraction: "1/8", "3/16", "1/1". A zero denominator reads "0/1". */
void style_label_duration(unsigned long numerator, unsigned long denominator, char *buffer, size_t size);

#endif
