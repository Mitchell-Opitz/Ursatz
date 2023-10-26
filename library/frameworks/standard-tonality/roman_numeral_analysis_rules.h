#ifndef URSATZ_FRAMEWORKS_STANDARD_TONALITY_ROMAN_NUMERAL_ANALYSIS_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_TONALITY_ROMAN_NUMERAL_ANALYSIS_RULES_H

#include <stdbool.h>
#include <stddef.h>

#include "pitch/pitch_spelling.h"
#include "roman_numeral_rules.h"
#include "rules/constraint.h"
#include "rules/rule.h"

/*
 * Roman numeral analysis rules for standard-tonality-v1: reading the
 * Roman numeral of a chord in a key. The steps:
 *
 *   KEY      identify the key and state it at the start: a capital note
 *            name alone is a major key, "mi" after it a minor key
 *            (B-flat minor is "B♭mi"); a lowercase letter is also a
 *            minor key ("b♭")
 *   ROOT     find the root of the chord (the chord may be inverted, so
 *            the root is not necessarily the bass) and its scale degree:
 *            that is the numeral
 *   QUALITY  write the numeral upper- or lowercase, with the symbols,
 *            for the chord's quality; each degree of a key has the same
 *            quality in every key of the kind, so the qualities can be
 *            memorized (triad_in_key_rules.h, seventh_in_key_rules.h)
 *   RAISED_LEADING_TONE
 *            in minor a V or viio (and the augmented III+) implies the
 *            raised leading tone: the minor v and the subtonic VII
 *            become V and vii°. A VII7 on the unraised subtonic is a
 *            dominant seventh chord
 *   DOMINANT_SEVENTH
 *            dominant seventh chords are only on V7 (or VII7 in minor);
 *            any other uppercase numeral with a 7 is a major seventh
 *   INVARIANCE
 *            voicing (octaves, doubling, an omitted fifth), inversion
 *            (the bass note), and embellishing tones do not change the
 *            numeral; a seventh chord with its fifth omitted is still the
 *            seventh chord (V7 without the fifth is V7). Identification here is of the chord tones: the
 *            notes passed in are the harmony's, embellishing tones
 *            already set aside by the caller, in any order
 *
 * A chord is named only when its root is a degree of the key (major,
 * natural minor, or the raised leading tone of minor) and its quality is
 * one the key's tables give that degree. Chords from outside the key
 * (borrowed, secondary, chromatic roots) and chords outside the five
 * seventh qualities get no numeral. Inversion figures are Figured Bass's.
 */

typedef enum { STANDARD_TONALITY_ROMAN_KEY_MAJOR = 0, STANDARD_TONALITY_ROMAN_KEY_MINOR } StandardTonalityRomanKeyMode;

typedef enum {
    STANDARD_TONALITY_ROMAN_NUMERAL_ANALYSIS_RULE_KEY = 0,
    STANDARD_TONALITY_ROMAN_NUMERAL_ANALYSIS_RULE_ROOT,
    STANDARD_TONALITY_ROMAN_NUMERAL_ANALYSIS_RULE_QUALITY,
    STANDARD_TONALITY_ROMAN_NUMERAL_ANALYSIS_RULE_RAISED_LEADING_TONE,
    STANDARD_TONALITY_ROMAN_NUMERAL_ANALYSIS_RULE_DOMINANT_SEVENTH,
    STANDARD_TONALITY_ROMAN_NUMERAL_ANALYSIS_RULE_INVARIANCE,
    STANDARD_TONALITY_ROMAN_NUMERAL_ANALYSIS_RULE_COUNT
} StandardTonalityRomanNumeralAnalysisRule;

const Rule *standard_tonality_roman_numeral_analysis_rule(StandardTonalityRomanNumeralAnalysisRule which);
const Constraint *standard_tonality_roman_numeral_analysis_constraint(StandardTonalityRomanNumeralAnalysisRule which);

/* KEY: writes the key label into buffer: the note name, with "mi" after
   it for minor ("C", "B♭mi"). False for an invalid mode or accidental
   (outside -2..+2), a NULL buffer, or a buffer too small. */
bool standard_tonality_roman_key_label(PitchLetter letter, int accidental, StandardTonalityRomanKeyMode mode,
                                       char *buffer, size_t size);

/* KEY: reads a key label: a capital letter is major, a capital letter
   with "mi" or a lowercase letter minor; accidentals as in chord
   symbols (♭ ♯ 𝄫 𝄪, or b and #). False (outputs untouched) for NULL, an
   empty or malformed label, a quality mark other than "mi", "mi" after a
   lowercase letter, or trailing text. */
bool standard_tonality_parse_roman_key_label(const char *label, PitchLetter *letter, int *accidental,
                                             StandardTonalityRomanKeyMode *mode);

/* DOMINANT_SEVENTH: true for the degrees a dominant seventh chord is
   found on: 5 in major, 5 and 7 in minor. False otherwise. */
bool standard_tonality_roman_dominant_seventh_degree(StandardTonalityRomanKeyMode mode, int degree);

/* RAISED_LEADING_TONE: true when the numeral, in a minor key, implies the
   raised leading tone: V and V7 (major-minor on 5), vii° and vii°7 (7),
   and III+ (3). False in major and for any other numeral. */
bool standard_tonality_roman_numeral_implies_raised_leading_tone(StandardTonalityRomanKeyMode mode,
                                                                 const StandardTonalityRomanNumeral *numeral);

/*
 * ROOT: the scale degree of a note name in the key of `tonic` (major, or
 * natural minor). For minor, the raised leading tone is also found: it
 * is degree 7 with *raised_leading_tone true. False (outputs untouched)
 * for NULL, a note that is not a degree of the key, or a key with no
 * scale (its key signature is imaginary). raised_leading_tone may be
 * NULL when the caller does not need it.
 */
bool standard_tonality_roman_root_degree(StandardTonalityRomanKeyMode mode, const PitchSpelling *tonic,
                                         PitchLetter root_letter, int root_accidental, int *degree,
                                         bool *raised_leading_tone);

/*
 * KEY / ROOT / QUALITY / INVARIANCE: the Roman numeral of the chord the
 * notes make in the key of `tonic`. Four distinct note names are tried as
 * a seventh chord, three as a triad (standard_tonality_identify_seventh,
 * standard_tonality_identify_triad: any order, any octaves, doublings
 * allowed); three that are a root, third, and seventh with the fifth
 * omitted are a seventh chord read from the third and seventh (a minor
 * third with a minor seventh is the key's minor-minor or half-diminished,
 * whichever the key gives that degree). False (out untouched) for NULL, notes that are not a
 * triad or one of the five seventh chords, a root that is not a degree of
 * the key, a quality the key does not give that degree, or a seventh
 * chord whose numeral would read back as the other seventh quality
 * (SEVENTH_INFERENCE: a major-major seventh on V, say).
 */
/*
 * QUALITY: true when the key gives the numeral's degree and quality: in major the key's table; in minor
 * the natural table or, below the seventh, the one the raised leading tone gives, and for a root on the
 * raised leading tone (raised_leading_tone) only the harmonic minor's seventh-degree quality. The same
 * test standard_tonality_identify_roman_numeral applies, for callers that already know the root's degree
 * (the pitch-class adapter, analysis/). False for an invalid mode or numeral.
 */
bool standard_tonality_roman_numeral_fits_key(StandardTonalityRomanKeyMode mode, int degree, bool raised_leading_tone,
                                              const StandardTonalityRomanNumeral *numeral);

bool standard_tonality_identify_roman_numeral(StandardTonalityRomanKeyMode mode, const PitchSpelling *tonic,
                                              const PitchSpelling *const *notes, size_t count,
                                              StandardTonalityRomanNumeral *out);

#endif
