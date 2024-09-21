#ifndef MUSIC_SEMANTICS_COUNTERPOINT_SPECIES_H
#define MUSIC_SEMANTICS_COUNTERPOINT_SPECIES_H

#include <stdbool.h>

/*
 * CounterpointSpecies (Value): one of the five species of strict
 * counterpoint (Fux, Gradus ad Parnassum), the graded exercises that
 * introduce one or two new problems at each stage. A plain enum: it names
 * a stage and carries no EntityID, no rules, and no framework claim.
 *
 *   FIRST    note against note: one counterpoint note for each note of
 *            the cantus firmus
 *   SECOND   two notes against one
 *   THIRD    four notes against one
 *   FOURTH   syncopation (suspensions)
 *   FIFTH    florid counterpoint, mixing the other four
 *
 * The species are ordered from the simplest to the most elaborate. What
 * each species requires of a line is a convention of a Framework
 * (frameworks/standard-counterpoint), not of this type.
 */

typedef enum {
    COUNTERPOINT_SPECIES_FIRST = 0,
    COUNTERPOINT_SPECIES_SECOND,
    COUNTERPOINT_SPECIES_THIRD,
    COUNTERPOINT_SPECIES_FOURTH,
    COUNTERPOINT_SPECIES_FIFTH,
    COUNTERPOINT_SPECIES_COUNT
} CounterpointSpecies;

/* True for the five species above; false for any other value. */
bool counterpoint_species_valid(CounterpointSpecies species);

/* "first" "second" "third" "fourth" "fifth"; NULL for an invalid species. */
const char *counterpoint_species_name(CounterpointSpecies species);

/* The number of counterpoint notes against each cantus firmus note in the
   species: 1, 2, 4 for the first three, and 2 for the fourth (a
   syncopated half note against a whole). 0 for the fifth, which mixes
   ratios, and for an invalid species. */
int counterpoint_species_notes_per_cantus_note(CounterpointSpecies species);

/* True when `a` is an earlier (simpler) species than `b`. False when either
   is invalid, or for equal species. */
bool counterpoint_species_precedes(CounterpointSpecies a, CounterpointSpecies b);

#endif
