#ifndef URSATZ_FRAMEWORKS_STANDARD_TONALITY_ROMAN_NUMERAL_ANALYSIS_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_TONALITY_ROMAN_NUMERAL_ANALYSIS_RULES_H

#include <stdbool.h>
#include <stddef.h>

#include "model/pitch/pitch_spelling.h"
#include "roman_numeral_rules.h"

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
 *            seventh chord (V7 without the fifth is V7)
 *
 * A chord is named only when its root is a degree of the key (major,
 * natural minor, or the raised leading tone of minor) and its quality is
 * one the key's tables give that degree. Chords from outside the key
 * (borrowed, secondary, chromatic roots) and chords outside the five
 * seventh qualities get no numeral. Inversion figures are Figured Bass's.
 */

typedef enum { STANDARD_TONALITY_ROMAN_KEY_MAJOR = 0, STANDARD_TONALITY_ROMAN_KEY_MINOR } StandardTonalityRomanKeyMode;

/*
 * QUALITY: true when the key gives the numeral's degree and quality: in major the key's table; in minor
 * the natural table or, below the seventh, the one the raised leading tone gives, and for a root on the
 * raised leading tone (raised_leading_tone) only the harmonic minor's seventh-degree quality. The same
 * test the pitch-class adapter (analysis/) applies once it knows the root's degree. False for an invalid mode or
 * numeral.
 */
bool standard_tonality_roman_numeral_fits_key(StandardTonalityRomanKeyMode mode, int degree, bool raised_leading_tone,
                                              const StandardTonalityRomanNumeral *numeral);

#endif
