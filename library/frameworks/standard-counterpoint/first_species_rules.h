#ifndef URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_FIRST_SPECIES_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_FIRST_SPECIES_RULES_H

#include <stdbool.h>

#include "counterpoint_rules.h"

/*
 * Checking a whole first-species (note against note) exercise: the rules
 * every species shares (counterpoint_rules.h) plus the first-species rules
 * stacked on top of them, which are stricter. The input and the result are
 * those of standard_counterpoint_check, and so is the meaning of the
 * tonic: when it is NULL the groups that need it (the cantus firmus, the
 * counterpoint line, and the beginning and ending) are skipped.
 *
 *   CANTUS_FIRMUS       cantus_firmus_rules.h, on the cantus firmus part
 *   MELODIC             melodic_rules.h, on both parts
 *   HARMONIC            harmonic_rules.h, on the two parts together
 *   TENDENCY            tendency_rules.h, on both parts (soft only)
 *   COUNTERPOINT_LINE   first_species_line_rules.h, on the counterpoint
 *   HARMONY             first_species_harmony_rules.h
 *   ENDS                first_species_ends_rules.h
 *   INDEPENDENCE        first_species_independence_rules.h
 *   MOTION              first_species_motion_rules.h (soft only)
 *
 * The first four groups have the same bit positions as in
 * StandardCounterpointGroup, so those bits match the common check. The
 * result sets a bit (1u << group) in hard_violations for each group with a
 * hard rule broken, and in soft_violations for each group with a soft rule
 * broken. An exercise is valid when no hard rule is broken. The number of
 * counterpoint notes against each cantus firmus note is one for first
 * species (counterpoint_species_notes_per_cantus_note in
 * counterpoint_species.h), which equal-length lines already
 * give.
 */

typedef enum {
    STANDARD_COUNTERPOINT_FIRST_SPECIES_GROUP_CANTUS_FIRMUS = STANDARD_COUNTERPOINT_GROUP_CANTUS_FIRMUS,
    STANDARD_COUNTERPOINT_FIRST_SPECIES_GROUP_MELODIC = STANDARD_COUNTERPOINT_GROUP_MELODIC,
    STANDARD_COUNTERPOINT_FIRST_SPECIES_GROUP_HARMONIC = STANDARD_COUNTERPOINT_GROUP_HARMONIC,
    STANDARD_COUNTERPOINT_FIRST_SPECIES_GROUP_TENDENCY = STANDARD_COUNTERPOINT_GROUP_TENDENCY,
    STANDARD_COUNTERPOINT_FIRST_SPECIES_GROUP_COUNTERPOINT_LINE,
    STANDARD_COUNTERPOINT_FIRST_SPECIES_GROUP_HARMONY,
    STANDARD_COUNTERPOINT_FIRST_SPECIES_GROUP_ENDS,
    STANDARD_COUNTERPOINT_FIRST_SPECIES_GROUP_INDEPENDENCE,
    STANDARD_COUNTERPOINT_FIRST_SPECIES_GROUP_MOTION,
    STANDARD_COUNTERPOINT_FIRST_SPECIES_GROUP_COUNT
} StandardCounterpointFirstSpeciesGroup;

/* "cantus-firmus" "melodic" "harmonic" "tendency" "counterpoint-line"
   "harmony" "ends" "independence" "motion"; NULL for an invalid group. */
const char *standard_counterpoint_first_species_group_name(StandardCounterpointFirstSpeciesGroup group);

/* Runs the groups over the exercise. False (*out untouched) for invalid
   input, as for standard_counterpoint_check. */
bool standard_counterpoint_first_species_check(const StandardCounterpointInput *input,
                                               StandardCounterpointResult *out);

/* True when the input is valid and breaks no hard rule. */
bool standard_counterpoint_first_species_valid(const StandardCounterpointInput *input);

#endif
