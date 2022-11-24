#ifndef URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_RELATED_KEY_H
#define URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_RELATED_KEY_H

#include <stdbool.h>

#include "harmony_chord.h"

/*
 * Closely related keys for standard-harmony-v1 (Extended Tonicization and Modulation to Closely
 * Related Keys). A modulation, a longer-term change of tonic, may go anywhere, but some are more
 * common because of the number of chords the two keys share. Keys whose signatures are within one
 * accidental of the home key's (C major and G major) are closely related. They are the home
 * key's neighbors on the circle of fifths one step either way and the relative major or minor of
 * each, which makes them the keys of the diatonic major and minor triads of the home key:
 *
 *   major   ii, iii, IV, V, vi         the keys of Dm, Em, F, G, Am in C major
 *   minor   III, iv, v, VI, VII        the keys of Eb, Fm, Gm, Ab, Bb in C minor
 *
 * The home key is not in its own list. In minor, v is the minor key on the fifth degree, unlike
 * the major V that is a tonicization target (harmony_applied.h). This module works from the home
 * key, in scale degrees, and so does not depend on the pitch spelling of keys
 * (standard-tonality has the circle of fifths and the relative keys by name).
 *
 * A key is the diatonic chord whose root is its tonic, given as that chord's scale degree in the
 * home key and whether the key is major or minor. Of the closely related keys, I to V and I to vi
 * in major, and i to III and i to v in minor, are the most common modulations.
 */

typedef struct {
    int degree;
    HarmonyMode mode;
} HarmonyRelatedKey;

typedef enum {
    HARMONY_KEY_RELATION_COMMON = 0,
    HARMONY_KEY_RELATION_OTHER,
    HARMONY_KEY_RELATION_COUNT
} HarmonyKeyRelationRank;

/* The Roman numeral of the triad on the degree in the mode (natural minor), upper case for a
   major triad, lower case for a minor one, "o" after a diminished one: major I ii iii IV V vi
   viio; minor i iio III iv v VI VII. NULL for an invalid mode or degree. */
const char *harmony_triad_numeral(HarmonyMode mode, int degree);

/* How many closely related keys the home key has (5), or 0 for an invalid mode. */
int harmony_related_key_count(HarmonyMode home);

/* The index-th closely related key (0 to 4) of the home key, in scale order. False (*out
   untouched) for an invalid mode or index, and for NULL. */
bool harmony_related_key_at(HarmonyMode home, int index, HarmonyRelatedKey *out);

/* The closely related key whose tonic is the diatonic chord on the degree. False (*out
   untouched) when the degree is the tonic or the chord is a diminished triad (there are no
   diminished keys), for an invalid mode or degree, and for NULL. */
bool harmony_related_key_of(HarmonyMode home, int degree, HarmonyRelatedKey *out);

/* True when the key is one of the home key's closely related keys. */
bool harmony_key_is_closely_related(HarmonyMode home, const HarmonyRelatedKey *key);

/* The key's tonic as semitones above the home key's tonic (0 to 11): the semitone of its scale
   degree. -1 for an invalid key. */
int harmony_related_key_semitone(HarmonyMode home, const HarmonyRelatedKey *key);

/* "common" "other"; NULL for an invalid rank. */
const char *harmony_key_relation_rank_name(HarmonyKeyRelationRank rank);

/* True for the two ranks above; false for any other value. */
bool harmony_key_relation_rank_valid(HarmonyKeyRelationRank rank);

/* How common a modulation to the key is: COMMON for V and vi in major and III and v in minor,
   OTHER for the rest of the closely related keys, and HARMONY_KEY_RELATION_COUNT for a key
   that is not closely related. */
HarmonyKeyRelationRank harmony_key_relation_rank(HarmonyMode home, const HarmonyRelatedKey *key);

#endif
