#ifndef URSATZ_COMPOSER_PREFERENCES_H
#define URSATZ_COMPOSER_PREFERENCES_H

#include <stdbool.h>
#include <stddef.h>

#include <cJSON.h>
#include <ursatz_composer/melody_fill.h>

/*
 * The composer's settings (R-411): the numbers that shape what it makes, held in one document so that a listening
 * session can change one and compose again without a code change or a rebuild. Every member of the document is
 * optional and its default is the value the Composer used before the document existed, so with no preferences every
 * output is byte-identical to what it was (R-413). docs/preferences.md lists every key, its default and its limit.
 *
 * ComposerPreferences holds the melody group, the settings of the melody stage (MelodyFillParams): the range, the
 * non-chord tones, the density, where the climax falls and how it is rated, the search (attempts and keep), and the
 * weight each group of soft rules counts in a melody's score. The motif and development groups follow.
 */
typedef struct {
    MelodyFillParams melody;
} ComposerPreferences;

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
