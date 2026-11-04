#ifndef URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_SPECIES_HELPERS_H
#define URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_SPECIES_HELPERS_H

#include <stdbool.h>
#include <stddef.h>

#include "consonance_rules.h"
#include "counterpoint_interval.h"
#include "pitch/pitch_spelling.h"

/*
 * Checks shared by the species rule files. Internal: not in the manifest and not part of the public API.
 */

/* True when `note` has the letter and accidental of `tonic`, at any octave. */
bool standard_counterpoint_species_is_tonic(const PitchSpelling *note, const PitchSpelling *tonic);

/* True when `note` is a perfect fifth above `tonic`, at any octave. */
bool standard_counterpoint_species_is_sol(const PitchSpelling *note, const PitchSpelling *tonic);

/* 5 for a perfect fifth, 8 for a perfect octave or unison, 0 for any other interval. */
int standard_counterpoint_species_perfect_class(const StandardCounterpointInterval *interval);

/* Whether the ascending-or-descending sixth into attack `j` is allowed although it is not an ascending minor sixth. */
typedef bool (*StandardCounterpointSpeciesLeapException)(const void *context, size_t j);

/*
 * The melodic leap rule over `count` attacks: no interval wider than an octave, none augmented or diminished, no seventh, and a
 * sixth only when it is an ascending minor sixth or `exception` (may be NULL) allows the leap into attack `j`. False when an
 * interval cannot be read.
 */
bool standard_counterpoint_species_check_leaps(const PitchSpelling *const *notes, size_t count,
                                               StandardCounterpointSpeciesLeapException exception, const void *context);

#endif
