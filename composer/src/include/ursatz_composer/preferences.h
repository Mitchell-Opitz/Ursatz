#ifndef URSATZ_COMPOSER_PREFERENCES_H
#define URSATZ_COMPOSER_PREFERENCES_H

#include <stdbool.h>
#include <stddef.h>

#include <cJSON.h>
#include <ursatz_composer/grow.h>
#include <ursatz_composer/melody_fill.h>
#include <ursatz_composer/motive_random.h>

/*
 * The composer's settings (R-411): the numbers that shape what it makes, held in one document so that a listening
 * session can change one and compose again without a code change or a rebuild. Every member of the document is
 * optional and its default is the value the Composer used before the document existed, so with no preferences every
 * output is byte-identical to what it was (R-413). docs/preferences.md lists every key, its default and its limit.
 *
 * ComposerPreferences holds the melody group, the settings of the melody stage (MelodyFillParams): the range, the
 * non-chord tones, the density, where the climax falls and how it is rated, the search (attempts and keep), and the
 * weight each group of soft rules counts in a melody's score. The motif group is the settings of the segments motive
 * generator (MotiveRandomPreferences): the bars, pickup, shapes, chord plans, features, leap, rung moves, decorations and
 * durations it draws from. The development group is the fidelity minimum table of the theme's development
 * (GrowFidelityTable).
 */
typedef struct {
    MelodyFillParams melody;
    MotiveRandomPreferences motif;
    GrowFidelityTable development;
} ComposerPreferences;

/*
 * The limits of the settings (docs/preferences.md), which the reader enforces and the API's vocabulary shows a client:
 * weights and weighted-list entries 0 to WEIGHT_MAX; climax_position and climax_slack 0 to SCALE_MAX; ratios n/d with
 * d up to RATIO_DENOMINATOR_MAX (the fidelity minimums up to FIDELITY_DENOMINATOR_MAX); attempts 1 to ATTEMPTS_MAX (keep
 * is 1 to MELODY_KEEP_MAX and at most attempts); range.low RANGE_LOW_MIN to RANGE_LOW_MAX, range.high at least
 * range.low + RANGE_SPAN_MIN and at most RANGE_HIGH_MAX; density 0 to DENSITY_MAX; climax_rise CLIMAX_RISE_MIN to
 * CLIMAX_RISE_MAX; pickup_one_in 0 to PICKUP_ONE_IN_MAX; accented_one_in 0 to ACCENTED_ONE_IN_MAX; leaps_per_motif 0 to
 * LEAPS_PER_MOTIF_MAX; leap.low and leap.high LEAP_MIN to LEAP_MAX.
 */
#define COMPOSER_PREFERENCES_WEIGHT_MAX 20
#define COMPOSER_PREFERENCES_SCALE_MAX 1000
#define COMPOSER_PREFERENCES_RATIO_DENOMINATOR_MAX 16
#define COMPOSER_PREFERENCES_ATTEMPTS_MAX 256
#define COMPOSER_PREFERENCES_RANGE_LOW_MIN 36
#define COMPOSER_PREFERENCES_RANGE_LOW_MAX 96
#define COMPOSER_PREFERENCES_RANGE_HIGH_MAX 108
#define COMPOSER_PREFERENCES_RANGE_SPAN_MIN 12
#define COMPOSER_PREFERENCES_DENSITY_MAX 4
#define COMPOSER_PREFERENCES_CLIMAX_RISE_MIN 1
#define COMPOSER_PREFERENCES_CLIMAX_RISE_MAX 12
#define COMPOSER_PREFERENCES_PICKUP_ONE_IN_MAX 64
#define COMPOSER_PREFERENCES_ACCENTED_ONE_IN_MAX 64
#define COMPOSER_PREFERENCES_LEAPS_PER_MOTIF_MAX 11
#define COMPOSER_PREFERENCES_LEAP_MIN 3
#define COMPOSER_PREFERENCES_LEAP_MAX 12
#define COMPOSER_PREFERENCES_FIDELITY_DENOMINATOR_MAX 1000

#define COMPOSER_PREFERENCES_PATH_MAX 128
#define COMPOSER_PREFERENCES_MESSAGE_MAX 160

/* Writes the defaults, every setting the Composer used before preferences existed, into *out. */
void composer_preferences_default(ComposerPreferences *out);

/*
 * Reads the JSON object into *out. *out starts from the defaults and every member present is applied over them. A member
 * that is unknown, of the wrong type or outside its limit makes it return false, leaving *out as the defaults
 * with the members read so far, with path set to where it is, relative to the object (for example "/melody/keep"), and
 * message naming the limit. path and message may be NULL when the details are not wanted. Members of the document that
 * hold a null are refused like any other wrong type.
 */
bool composer_preferences_read(const cJSON *object, ComposerPreferences *out, char *path, size_t path_size, char *message,
                               size_t message_size);

/*
 * The full document for the preferences, every member present, in the shape docs/preferences.md describes: ratios as
 * "n/d" text and nct_types in the order passing, neighbor, appoggiatura, suspension, anticipation. The caller deletes
 * it with cJSON_Delete. NULL when memory runs out.
 */
cJSON *composer_preferences_write(const ComposerPreferences *preferences);

/* True when every setting is the default: the preferences change nothing. */
bool composer_preferences_is_default(const ComposerPreferences *preferences);

#endif
