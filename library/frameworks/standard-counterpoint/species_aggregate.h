#ifndef URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_SPECIES_AGGREGATE_H
#define URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_SPECIES_AGGREGATE_H

#include <stdbool.h>
#include <stddef.h>

#include "counterpoint_line.h"
#include "counterpoint_rules.h"
#include "counterpoint_slots.h"

/*
 * Helpers shared by the species aggregate checks (first_species_rules.h and
 * those of the later species). Each species decides its own groups and runs
 * its own rule modules; these are the parts that are the same for all of
 * them.
 */

/* The most slots a species exercise has: the fifth species, eight to a bar for
   at most 64 bars. */
#define STANDARD_COUNTERPOINT_SPECIES_MAX_SLOTS 512

/* Records the outcome of one group in a result: sets bit (1u << group) in
   hard_violations when `hard` is not zero, and in soft_violations when `soft`
   is not zero. */
void standard_counterpoint_species_record(StandardCounterpointResult *result, unsigned group, unsigned hard,
                                          unsigned soft);

/*
 * The spacing group of a species with more than one counterpoint note to a
 * bar: the common harmonic rules that concern how far apart the parts are and
 * whether they cross (harmonic_rules.h: MAX_GAP, MOSTLY_WITHIN_OCTAVE,
 * NO_CROSSING, CLIMAX_COORDINATION), run on every note the counterpoint sounds
 * (a tie counts, a rest does not) against the cantus firmus note of its bar.
 * The other common harmonic rules compare one pair of notes with the next and
 * do not fit these species.
 *
 * `slots_per_bar` is the number of slots to a bar of the species. Sets *hard
 * and *soft to the masks of the rules that fail (a bit for each rule), and
 * returns false, leaving them untouched, when the lines are invalid, the
 * counterpoint does not have `slots_per_bar` slots for each cantus firmus
 * note, or it has more than STANDARD_COUNTERPOINT_SPECIES_MAX_SLOTS.
 */
bool standard_counterpoint_species_spacing_check(const StandardCounterpointLine *cantus,
                                                 const StandardCounterpointSlotLine *counterpoint,
                                                 size_t slots_per_bar, StandardCounterpointCantusPart cantus_part,
                                                 unsigned *hard, unsigned *soft);

#endif
