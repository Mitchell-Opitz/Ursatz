#ifndef URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_TONICIZATION_FREQUENCY_H
#define URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_TONICIZATION_FREQUENCY_H

#include <stdbool.h>

#include "harmony_chord.h"

/*
 * How often a chord is tonicized, for standard-harmony-v1 (Tonicization): the frequency with
 * which a chord is tonicized is related to the frequency with which it generally appears. A
 * value of the target, not a rule:
 *
 *   COMMON      the dominant and the strong predominants: V, IV, and ii (minor: V and iv)
 *   FREQUENT    vi, which is less common than ii, IV, and V but frequently tonicized (minor:
 *               VI, and III, the relative major, which the chapter does not name)
 *   UNCOMMON    iii and, in minor, VII, which are uncommon harmonies and not tonicized often
 */

typedef enum {
    HARMONY_TONICIZATION_COMMON = 0,
    HARMONY_TONICIZATION_FREQUENT,
    HARMONY_TONICIZATION_UNCOMMON,
    HARMONY_TONICIZATION_FREQUENCY_COUNT
} HarmonyTonicizationFrequency;

/* True for the three values above; false for any other value. */
bool harmony_tonicization_frequency_valid(HarmonyTonicizationFrequency frequency);

/* "common" "frequent" "uncommon"; NULL for an invalid frequency. */
const char *harmony_tonicization_frequency_name(HarmonyTonicizationFrequency frequency);

/* How often the target (a scale degree in the mode that can be tonicized, harmony_applied.h)
   is tonicized, or HARMONY_TONICIZATION_FREQUENCY_COUNT for a degree that cannot be and for an
   invalid mode. */
HarmonyTonicizationFrequency harmony_tonicization_frequency(HarmonyMode mode, int target);

#endif
