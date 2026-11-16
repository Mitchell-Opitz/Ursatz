#ifndef URSATZ_FRAMEWORKS_STANDARD_TONALITY_ROMAN_NUMERAL_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_TONALITY_ROMAN_NUMERAL_RULES_H

#include <stdbool.h>
#include <stddef.h>

#include "identity/entity_id.h"
#include "interpretation/chord.h"
#include "seventh_rules.h"
#include "triad_rules.h"

/*
 * Roman numeral rules for standard-tonality-v1. A Roman numeral labels a
 * chord in relation to the key of the piece, so the same numeral means
 * the same thing in every key:
 *
 *   DEGREES        the numeral is the scale degree of the chord's root:
 *                  I II III IV V VI VII for 1 to 7
 *   CASE           the quality of the third sets the case: uppercase for a
 *                  major third above the root (major, augmented),
 *                  lowercase for a minor third (minor, diminished,
 *                  half-diminished)
 *   TRIAD_SYMBOLS  a diminished triad adds a circle to a lowercase
 *                  numeral (the degree sign: vii°), an augmented triad a
 *                  plus sign to an uppercase one (V+); major and minor
 *                  add nothing (V, ii)
 *   SEVENTH_SYMBOLS
 *                  a seventh chord adds a 7 (V7, ii7, IV7); a half-
 *                  diminished seventh chord a slashed circle before it
 *                  (ii∅7), a fully diminished one a circle (vii°7)
 *   SEVENTH_INFERENCE
 *                  the major-major and major-minor seventh chords share
 *                  the label "uppercase numeral with a 7"; context tells
 *                  them apart. A dominant seventh chord is only found on
 *                  V7 (or VII7 in minor), so those two are read as
 *                  dominant sevenths and any other uppercase numeral
 *                  with a 7 as a major seventh chord
 *
 * Symbols are UTF-8: the degree sign (°) and
 * the empty set sign (∅). Superscript is presentation, so the 7 is plain
 * here. Parsing also takes "o" for the circle and the slashed letter ø
 * for the slashed circle.
 *
 * Absolute chord symbols name a chord by its root; these name it by its
 * function in a key. Reading a numeral in a key is
 * roman_numeral_analysis_rules.h. common-practice-minimal's own key-
 * relative triad labels are unchanged.
 */

typedef struct {
    int degree;                                  /* 1..7 */
    bool seventh;                                /* a seventh chord rather than a triad */
    StandardTonalityTriadQuality triad;          /* the quality when !seventh */
    StandardTonalitySeventhQuality seventh_quality; /* the quality when seventh */
} StandardTonalityRomanNumeral;

/* DEGREES / CASE: "I" ... "VII" or "i" ... "vii" for degree 1..7; NULL
   outside that range. */
const char *standard_tonality_roman_numeral_degree(int degree, bool uppercase);

/* TRIAD_SYMBOLS: "" for major and minor, "°" (the degree sign)
   diminished, "+" augmented. NULL for an invalid quality. */
const char *standard_tonality_roman_numeral_triad_symbol(StandardTonalityTriadQuality quality);

/* SEVENTH_SYMBOLS: "7" for major-major, major-minor, and minor-minor, the
   empty set sign and 7 for half-diminished, the degree sign and 7 for
   fully diminished. NULL for an invalid quality. */
const char *standard_tonality_roman_numeral_seventh_symbol(StandardTonalitySeventhQuality quality);

/* SEVENTH_INFERENCE: the seventh chord an uppercase numeral with a 7 on
   the degree stands for: major-minor (dominant) on 5 and 7, major-major
   on the others. False (output untouched) for a degree outside 1..7 or a
   NULL output. */
bool standard_tonality_roman_numeral_inferred_seventh(int degree, StandardTonalitySeventhQuality *quality);

/* The numeral for the degree and quality. False (output untouched) for a
   degree outside 1..7, an invalid quality, or NULL output. */
bool standard_tonality_roman_numeral_triad(int degree, StandardTonalityTriadQuality quality,
                                           StandardTonalityRomanNumeral *out);
bool standard_tonality_roman_numeral_seventh_chord(int degree, StandardTonalitySeventhQuality quality,
                                                   StandardTonalityRomanNumeral *out);

/* True when reading the formatted numeral back gives the same chord:
   false for a major-major seventh on 5 or 7 and a major-minor seventh on
   any other degree, which share a label with the other quality
   (SEVENTH_INFERENCE), and for an invalid numeral. */
bool standard_tonality_roman_numeral_is_unambiguous(const StandardTonalityRomanNumeral *numeral);

/*
 * DEGREES / CASE / TRIAD_SYMBOLS / SEVENTH_SYMBOLS: writes the numeral
 * into buffer (size bytes, NUL-terminated): V, ii, viio (with the degree
 * sign), V+, V7, ii7, ii with the empty set sign and 7, vii with the
 * degree sign and 7. False for an invalid numeral, a NULL argument, or a
 * buffer too small.
 */
bool standard_tonality_roman_numeral_format(const StandardTonalityRomanNumeral *numeral, char *buffer, size_t size);

#endif
