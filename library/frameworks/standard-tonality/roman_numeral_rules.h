#ifndef URSATZ_FRAMEWORKS_STANDARD_TONALITY_ROMAN_NUMERAL_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_TONALITY_ROMAN_NUMERAL_RULES_H

#include <stdbool.h>
#include <stddef.h>

#include "chord_symbol_rules.h"
#include "identity/entity_id.h"
#include "interpretation/chord.h"
#include "rules/constraint.h"
#include "rules/rule.h"
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
 * Symbols are UTF-8, as in chord_symbol_rules.h: the degree sign (°) and
 * the empty set sign (∅). Superscript is presentation, so the 7 is plain
 * here. Parsing also takes "o" for the circle and the slashed letter ø
 * for the slashed circle.
 *
 * Absolute chord symbols (chord_symbol_rules.h) name a chord by its root;
 * these name it by its function in a key. Reading a numeral in a key is
 * roman_numeral_analysis_rules.h. common-practice-minimal's own key-
 * relative triad labels are unchanged.
 */

typedef struct {
    int degree;                                  /* 1..7 */
    bool seventh;                                /* a seventh chord rather than a triad */
    StandardTonalityTriadQuality triad;          /* the quality when !seventh */
    StandardTonalitySeventhQuality seventh_quality; /* the quality when seventh */
} StandardTonalityRomanNumeral;

typedef enum {
    STANDARD_TONALITY_ROMAN_NUMERAL_RULE_DEGREES = 0,
    STANDARD_TONALITY_ROMAN_NUMERAL_RULE_CASE,
    STANDARD_TONALITY_ROMAN_NUMERAL_RULE_TRIAD_SYMBOLS,
    STANDARD_TONALITY_ROMAN_NUMERAL_RULE_SEVENTH_SYMBOLS,
    STANDARD_TONALITY_ROMAN_NUMERAL_RULE_SEVENTH_INFERENCE,
    STANDARD_TONALITY_ROMAN_NUMERAL_RULE_COUNT
} StandardTonalityRomanNumeralRule;

const Rule *standard_tonality_roman_numeral_rule(StandardTonalityRomanNumeralRule which);
const Constraint *standard_tonality_roman_numeral_constraint(StandardTonalityRomanNumeralRule which);

/* DEGREES / CASE: "I" ... "VII" or "i" ... "vii" for degree 1..7; NULL
   outside that range. */
const char *standard_tonality_roman_numeral_degree(int degree, bool uppercase);

/* CASE: true for the qualities with a major third above the root (major
   and augmented triads, major-major and major-minor sevenths), false for
   a minor third (minor and diminished triads, minor-minor, half-
   diminished, and fully diminished sevenths). False for an invalid
   quality. */
bool standard_tonality_roman_numeral_triad_uppercase(StandardTonalityTriadQuality quality);
bool standard_tonality_roman_numeral_seventh_uppercase(StandardTonalitySeventhQuality quality);

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

/*
 * Reads a numeral. Uppercase with nothing, "+", or "7" is a major triad,
 * an augmented triad, or (SEVENTH_INFERENCE) a seventh chord; lowercase
 * with nothing, a circle, "7", a slashed circle and 7, or a circle and 7
 * is a minor triad, a diminished triad, a minor-minor seventh, a half-
 * diminished seventh, or a fully diminished seventh. False (output
 * untouched) for NULL, an empty or malformed numeral, mixed case, a mark
 * that does not go with the case (an uppercase numeral with a circle),
 * or trailing text.
 */
bool standard_tonality_roman_numeral_parse(const char *text, StandardTonalityRomanNumeral *out);

/*
 * Builds a real, populated Chord labeled with the numeral, claiming that
 * the thing named by target_id (a Sonority, say) is that chord under
 * standard-tonality-v1. Takes ownership of target_id on success
 * (destroyed with the Chord); on failure the caller still owns it. An
 * invalid numeral is CHORD_ERR_NULL_LABEL; other ChordResult errors as in
 * interpretation/chord.h.
 */
ChordResult standard_tonality_build_roman_numeral_chord(const StandardTonalityRomanNumeral *numeral,
                                                        EntityID *target_id, Chord **out);

#endif
