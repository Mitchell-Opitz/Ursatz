#ifndef MUSIC_PITCH_PITCH_NUMERIC_VALUE_H
#define MUSIC_PITCH_PITCH_NUMERIC_VALUE_H

#include <stdbool.h>

#include "pitch/pitch_identity.h"
#include "pitch/tuning_system.h"

/*
 * PitchNumericValue: an explicit numeric position of a pitch, measured
 * as an exact rational (position_numerator/position_denominator,
 * reduced to lowest terms, numerator may be negative) along whatever
 * linear ordering a TuningSystem defines. Immutable, never represented
 * as float/double (Law 19).
 *
 * This is NOT PitchIdentity (pitch/pitch_identity.h), which stays
 * exactly as it is elsewhere in this codebase: an opaque, tuning-
 * agnostic token with no numeric semantics, per Domain Spec §7.
 * PitchNumericValue is an additional, optional resolution of a pitch
 * under one specific TuningSystem -- never a replacement for
 * PitchIdentity, and never implying every PitchIdentity has one.
 *
 * This is also NOT PitchRealization (pitch/pitch_realization.h), which
 * measures a MULTIPLICATIVE frequency ratio against a reference
 * frequency and explicitly refuses cents/log-based conversion because
 * a step like a 12-TET semitone (2^(1/12)) is irrational -- unsupportable
 * under Law 19. Transposition (add an interval) and Inversion (reflect
 * about an axis) need ADDITIVE arithmetic that stays exact rational for
 * ANY tuning system, not just ratio-based ones: this is the type that
 * provides it. A "position" here makes no claim about frequency; it is
 * purely an ordinal/linear coordinate the caller defines the meaning of
 * relative to its TuningSystem.
 *
 * Construction MUST supply a TuningSystem: a numeric value is only
 * meaningful relative to the tuning system that defines it, so no
 * "tuning-agnostic" numeric constructor exists (unlike PitchIdentity,
 * where such a constructor is required). Like PitchRealization,
 * PitchNumericValue does NOT take ownership of the TuningSystem passed
 * to pitch_numeric_value_create: it copies the tuning system's identity
 * and name internally, so a single TuningSystem can be shared as the
 * reference for many PitchNumericValues without any ownership/aliasing
 * hazard. The caller retains ownership of the TuningSystem it passed in
 * and must destroy it itself.
 *
 * Binary operations (add_interval, distance, reflect_about_axis) that
 * take two PitchNumericValues require both to share the same
 * TuningSystem (by identity) -- combining positions measured under
 * different tuning systems is not a meaningful operation and is
 * rejected rather than silently computed.
 *
 * Conversion to/from PitchIdentity is intentionally one-directional.
 * pitch_numeric_value_to_identity packages a PitchNumericValue into a
 * new, opaque PitchIdentity -- used to give a Transformation's derived
 * NoteEvent a pitch. There is deliberately no general
 * "PitchIdentity -> PitchNumericValue" decoder: PitchIdentity's token is
 * opaque by design (pitch/pitch_identity.h), and a generic decoder would
 * silently assume every PitchIdentity was produced by this module,
 * which defeats the point of that opacity. A caller holding an arbitrary
 * input PitchIdentity that needs numeric treatment (e.g. Transposition's
 * input pitch) must supply the corresponding PitchNumericValue itself,
 * by whatever theory-level knowledge it has -- this module does not
 * pretend to derive that knowledge mechanically.
 */

typedef struct PitchNumericValue PitchNumericValue;

typedef enum {
    PITCH_NUMERIC_VALUE_OK = 0,
    PITCH_NUMERIC_VALUE_ERR_NULL_TUNING_SYSTEM,
    PITCH_NUMERIC_VALUE_ERR_NON_POSITIVE_DENOMINATOR,
    PITCH_NUMERIC_VALUE_ERR_ALLOC
} PitchNumericValueResult;

/* Reduces position_numerator/position_denominator to lowest terms at construction.
   position_denominator must be positive; position_numerator may be negative (a
   position "below" whatever zero point the caller's TuningSystem usage defines). */
PitchNumericValueResult pitch_numeric_value_create(const TuningSystem *tuning_system, long position_numerator,
                                                    unsigned long position_denominator, PitchNumericValue **out);
void pitch_numeric_value_destroy(PitchNumericValue *value);

const TuningSystem *pitch_numeric_value_tuning_system(const PitchNumericValue *value);
long pitch_numeric_value_position_numerator(const PitchNumericValue *value);
unsigned long pitch_numeric_value_position_denominator(const PitchNumericValue *value);

/* Value equality: same TuningSystem (by identity) AND same reduced position. */
bool pitch_numeric_value_equals(const PitchNumericValue *a, const PitchNumericValue *b);

typedef enum {
    PITCH_NUMERIC_VALUE_OP_OK = 0,
    PITCH_NUMERIC_VALUE_OP_ERR_NULL_VALUE,
    PITCH_NUMERIC_VALUE_OP_ERR_NON_POSITIVE_DENOMINATOR,
    PITCH_NUMERIC_VALUE_OP_ERR_TUNING_SYSTEM_MISMATCH,
    PITCH_NUMERIC_VALUE_OP_ERR_ALLOC
} PitchNumericValueOpResult;

/*
 * *out = base + (interval_numerator/interval_denominator), exact
 * rational addition, reduced to lowest terms, under base's TuningSystem.
 * interval_denominator must be positive; interval_numerator may be
 * negative (a downward interval). This is Transposition's core
 * operation.
 */
PitchNumericValueOpResult pitch_numeric_value_add_interval(const PitchNumericValue *base, long interval_numerator,
                                                             unsigned long interval_denominator,
                                                             PitchNumericValue **out);

/*
 * *out_numerator/*out_denominator = a - b, exact rational, reduced.
 * Requires a and b to share the same TuningSystem (by identity).
 */
PitchNumericValueOpResult pitch_numeric_value_distance(const PitchNumericValue *a, const PitchNumericValue *b,
                                                         long *out_numerator, unsigned long *out_denominator);

/*
 * *out = reflection of value about axis: 2*axis - value, exact
 * rational, reduced, under value's TuningSystem. Requires value and
 * axis to share the same TuningSystem (by identity). This is
 * Inversion's core operation.
 */
PitchNumericValueOpResult pitch_numeric_value_reflect_about_axis(const PitchNumericValue *value,
                                                                   const PitchNumericValue *axis,
                                                                   PitchNumericValue **out);

typedef enum {
    PITCH_NUMERIC_VALUE_TO_IDENTITY_OK = 0,
    PITCH_NUMERIC_VALUE_TO_IDENTITY_ERR_NULL_VALUE,
    PITCH_NUMERIC_VALUE_TO_IDENTITY_ERR_ALLOC
} PitchNumericValueToIdentityResult;

/*
 * Packages value's TuningSystem id and reduced position into a new,
 * opaque PitchIdentity (pitch/pitch_identity.h) the caller owns
 * (destroyed by pitch_identity_destroy). One-directional -- see file
 * header; there is no corresponding "from identity" decoder.
 */
PitchNumericValueToIdentityResult pitch_numeric_value_to_identity(const PitchNumericValue *value,
                                                                    PitchIdentity **out);

#endif