#ifndef URSATZ_COMPOSER_REQUEST_CHORDS_H
#define URSATZ_COMPOSER_REQUEST_CHORDS_H

#include <stddef.h>

#include <cJSON.h>
#include <ursatz_composer/composer_api.h>
#include <ursatz_composer/constraint.h>
#include <ursatz_composer/given.h>
#include <ursatz_composer/lanes.h>

/* The most bars a list of chords reaches. */
#define REQUEST_CHORDS_BARS_MAX 64

/*
 * Reads the chords a request gives (the theme request's "chords", a section's `harmony.chords`, R-610): a list of {"bar", "text"}, at
 * most one a bar, the text in the own-harmony syntax relative to the bar ("I V@3/2"), bars first_bar to first_bar + bars - 1 of `what` (0 for a theme, 1 for a section) ("the theme", "the
 * section"). The chords are appended to chords[] and numerals[] (a chord's numeral points into numerals) from *chord_count, up to
 * capacity, each lasting to the next chord of its bar or the end of the bar. A refusal names the entry as base/<i> in error with the
 * message the theme request has always given; a NULL or null list is no chords. COMPOSER_OK, or COMPOSER_ERR_REQUEST.
 */
ComposerStatus request_read_chords(const cJSON *list, const char *base, const char *what, long first_bar, const PlanMeter *meter,
                                   const SpelledKey *key, long bars, GivenChord *chords, char numerals[][PLAN_ROMAN_NUMERAL_MAX], size_t capacity,
                                   size_t *chord_count, ComposerError *error);

#endif
