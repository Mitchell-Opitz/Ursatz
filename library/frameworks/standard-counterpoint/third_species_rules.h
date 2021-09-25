#ifndef URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_THIRD_SPECIES_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_THIRD_SPECIES_RULES_H

#include <stdbool.h>

#include "counterpoint_rules.h"
#include "pitch/pitch_spelling.h"
#include "third_species_exercise.h"

/*
 * Checking a whole third-species (four notes against one) exercise against
 * the rule groups of standard-counterpoint-v1:
 *
 *   CANTUS_FIRMUS       cantus_firmus_rules.h and melodic_rules.h (with the
 *                       default limits), on the cantus firmus. The
 *                       cantus firmus is the same as in first species. The
 *                       cantus firmus rules need the tonic and are skipped
 *                       without it
 *   SPACING             the common harmonic rules that concern how far apart
 *                       the parts are and whether they cross
 *                       (harmonic_rules.h: MAX_GAP, MOSTLY_WITHIN_OCTAVE,
 *                       NO_CROSSING, CLIMAX_COORDINATION), run on every note
 *                       the counterpoint sounds against the cantus firmus
 *                       note it sounds with. The other common harmonic
 *                       rules compare one pair of notes with the next and
 *                       do not fit four notes to a bar; their third-species
 *                       forms are in the strong-beat rules. Keeping the
 *                       counterpoint's climax apart from the cantus firmus
 *                       climax is the CLIMAX_COORDINATION rule
 *   STRONG_BEATS        third_species_strong_rules.h
 *   OTHER_BEATS         third_species_beat_rules.h
 *   ENDS                third_species_ends_rules.h (needs the tonic)
 *   COUNTERPOINT_LINE   third_species_line_rules.h (needs the tonic)
 *
 * The result sets a bit (1u << group) in hard_violations for each group with
 * a hard rule broken, and in soft_violations for each group with a soft rule
 * broken. An exercise is valid when no hard rule is broken. When the tonic
 * is NULL the groups that need it are skipped.
 */

typedef enum {
    STANDARD_COUNTERPOINT_THIRD_SPECIES_GROUP_CANTUS_FIRMUS = 0,
    STANDARD_COUNTERPOINT_THIRD_SPECIES_GROUP_SPACING,
    STANDARD_COUNTERPOINT_THIRD_SPECIES_GROUP_STRONG_BEATS,
    STANDARD_COUNTERPOINT_THIRD_SPECIES_GROUP_OTHER_BEATS,
    STANDARD_COUNTERPOINT_THIRD_SPECIES_GROUP_ENDS,
    STANDARD_COUNTERPOINT_THIRD_SPECIES_GROUP_COUNTERPOINT_LINE,
    STANDARD_COUNTERPOINT_THIRD_SPECIES_GROUP_COUNT
} StandardCounterpointThirdSpeciesGroup;

/* "cantus-firmus" "spacing" "strong-beats" "other-beats" "ends"
   "counterpoint-line"; NULL for an invalid group. */
const char *standard_counterpoint_third_species_group_name(StandardCounterpointThirdSpeciesGroup group);

/* The exercise and what the caller knows about it: the exercise (which also
   says which part is the cantus firmus), and the tonic of the mode, or NULL
   when it is not known. */
typedef struct {
    const StandardCounterpointThirdSpeciesExercise *exercise;
    const PitchSpelling *tonic;
} StandardCounterpointThirdSpeciesInput;

/* Runs the groups over the exercise. False (*out untouched) for invalid
   input: NULL, or an invalid exercise. */
bool standard_counterpoint_third_species_check(const StandardCounterpointThirdSpeciesInput *input,
                                                StandardCounterpointResult *out);

/* True when the input is valid and breaks no hard rule. */
bool standard_counterpoint_third_species_valid(const StandardCounterpointThirdSpeciesInput *input);

#endif
