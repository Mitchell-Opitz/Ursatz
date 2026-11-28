#ifndef URSATZ_COMPOSER_REQUEST_CHORDS_H
#define URSATZ_COMPOSER_REQUEST_CHORDS_H

#include <stdbool.h>
#include <stddef.h>

#include <cJSON.h>
#include <ursatz_composer/composer_api.h>
#include <plan/plan_constraint.h>
#include <ursatz_composer/given.h>
#include <plan/plan_lanes.h>
#include <ursatz_composer/harmony_chords.h>
#include <theory/tonality/triad_rules.h>

/* The most bars a list of chords reaches. */
#define REQUEST_CHORDS_BARS_MAX 64

/*
 * Reads the chords a request gives (the theme request's "chords", a section's `harmony.chords`, R-610): a list of {"bar", "text"}, at
 * most one a bar, the text in the own-harmony syntax relative to the bar ("I V@3/2"), bars first_bar to first_bar + bars - 1 of `what` (0 for a theme, 1 for a section) ("the theme", "the
 * section"). The chords are appended to chords[] and numerals[] (a chord's numeral points into numerals) from *chord_count, up to
 * capacity, each lasting to the next chord of its bar or the end of the bar. A refusal names the entry as base/<i> in error with the
 * message the theme request has always given; a NULL or null list is no chords. An altered chord (bVI, bII) is a borrowed one (R-729): it
 * needs `tones` holding the snapshot and names a triad of the nearest mode that has it; with NULL `tones` it is refused as it always was. URSATZ_OK, or URSATZ_ERR_REQUEST.
 */
UrsatzStatus request_read_chords(const ComposerChords *tones, const cJSON *list, const char *base, const char *what, long first_bar, const PlanMeter *meter,
                                   const SpelledKey *key, long bars, GivenChord *chords, char numerals[][PLAN_ROMAN_NUMERAL_MAX], size_t capacity,
                                   size_t *chord_count, UrsatzError *error);

/*
 * The text a chord is written as in the chord lists above (R-765): the chord whose root is `alteration` semitones from degree `degree` of the
 * major scale of the key's tonic (3 and -1 is the flat third) and whose triad has `quality`, as the first of these texts that
 * request_read_chords reads in `key` as exactly that chord: the upper-case numeral (its alteration against the key's own degrees), the
 * numeral that names the quality (iv, vio, bVI), then `<UPPER>:<mode word>` when `mode` (a seven-step material) holds the chord (R-762).
 * False, `out` untouched, for a chord none of them reads (a chord the key's own modes do not hold, or an unusable argument).
 */
bool composer_chord_text_of(const ComposerChords *chords, const SpelledKey *key, int degree, int alteration, StandardTonalityTriadQuality quality,
                            const PitchMaterial *mode, char out[PLAN_ROMAN_NUMERAL_MAX]);

/*
 * The root (semitones above the tonic) and the quality of the triad a chord numeral of a plan (the text request_read_chords gives) names in the key.
 * False for a numeral the harmony reader does not read, or a seventh, color or applied chord.
 */
bool composer_given_chord_triad(const ComposerChords *chords, const SpelledKey *key, const char *numeral, int *root, StandardTonalityTriadQuality *quality);

#endif
