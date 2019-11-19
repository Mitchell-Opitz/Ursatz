#ifndef MUSIC_PITCH_PITCH_REALIZATION_H
#define MUSIC_PITCH_PITCH_REALIZATION_H

#include <stdbool.h>

#include "pitch/tuning_system.h"

/*
 * PitchRealization: a sounding/physical pitch, measured as an exact
 * rational ratio (numerator/denominator, reduced to lowest terms)
 * against a TuningSystem's reference. Immutable, never represented as
 * float/double (Law 19).
 *
 * Frequency is derivable via pitch_realization_frequency, given a
 * reference frequency the caller supplies as its own exact rational
 * (e.g. 440/1 for A440) -- no default reference is hard-coded here,
 * matching this platform's "no single theory baked in" principle.
 * ratio * reference stays exact rational arithmetic throughout.
 *
 * "Cents" is NOT supported here. Converting a ratio to cents is a
 * log2-based operation and is irrational in the general case (e.g. one
 * semitone in 12-TET is 2^(1/12), which has no exact rational value) --
 * supporting it would require float/double, which Law 19 prohibits.
 * General cents support is deferred (Deliverable D, an exact-rational
 * or fixed-point cents model would need to be designed first), mirroring
 * the deferral pattern PitchIdentity/PitchSpelling already use.
 *
 * PitchRealization does NOT take ownership of the TuningSystem passed to
 * pitch_realization_create: it copies the tuning system's identity and
 * name internally (mirroring identity/version.h's Version, which copies
 * its EntityID for the same reason) so a single TuningSystem can be
 * shared as the reference for many PitchRealizations without any
 * ownership/aliasing hazard. The caller retains ownership of the
 * TuningSystem it passed in and must destroy it itself.
 */

typedef struct PitchRealization PitchRealization;

typedef enum {
    PITCH_REALIZATION_OK = 0,
    PITCH_REALIZATION_ERR_NULL_TUNING_SYSTEM,
    PITCH_REALIZATION_ERR_NON_POSITIVE_NUMERATOR,
    PITCH_REALIZATION_ERR_NON_POSITIVE_DENOMINATOR,
    PITCH_REALIZATION_ERR_ALLOC
} PitchRealizationResult;

/* Reduces ratio_numerator/ratio_denominator to lowest terms at construction. Both must be positive: a
   frequency ratio of zero or negative is not physically meaningful. */
PitchRealizationResult pitch_realization_create(const TuningSystem *tuning_system, long ratio_numerator,
                                                 unsigned long ratio_denominator, PitchRealization **out);
void pitch_realization_destroy(PitchRealization *pitch_realization);

const TuningSystem *pitch_realization_tuning_system(const PitchRealization *pitch_realization);
long pitch_realization_ratio_numerator(const PitchRealization *pitch_realization);
unsigned long pitch_realization_ratio_denominator(const PitchRealization *pitch_realization);

/*
 * Exact rational frequency = ratio * reference, reduced to lowest terms.
 * reference_numerator/reference_denominator is the caller-supplied
 * reference frequency (e.g. 440/1); reference_denominator must be
 * positive.
 */
typedef enum {
    PITCH_REALIZATION_FREQUENCY_OK = 0,
    PITCH_REALIZATION_FREQUENCY_ERR_NON_POSITIVE_REFERENCE_DENOMINATOR
} PitchRealizationFrequencyResult;

PitchRealizationFrequencyResult pitch_realization_frequency(const PitchRealization *pitch_realization,
                                                              long reference_numerator,
                                                              unsigned long reference_denominator,
                                                              long *out_numerator, unsigned long *out_denominator);

/* Value equality: same TuningSystem (by identity) AND same reduced ratio. */
bool pitch_realization_equals(const PitchRealization *a, const PitchRealization *b);

#endif