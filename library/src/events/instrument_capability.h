#ifndef MUSIC_EVENTS_INSTRUMENT_CAPABILITY_H
#define MUSIC_EVENTS_INSTRUMENT_CAPABILITY_H

#include <stdbool.h>

#include "events/instrument.h"
#include "pitch/pitch_identity.h"

/*
 * InstrumentCapability: range and transposition, separable from any
 * particular Instrument's identity. A Value, not an Entity -- two
 * InstrumentCapability values with identical range/transposition are
 * equal regardless of which Instrument (or none) produced them.
 *
 * The only constructor derives one from an existing Instrument
 * (instrument_capability_create_from_instrument): this is how the type
 * "depends on Instrument" per the Registry, while staying separable from
 * it -- the derived InstrumentCapability copies range/transposition out
 * and holds no reference back to the source Instrument's id or any
 * other Instrument field. Destroying the source Instrument afterward
 * does not affect an already-derived InstrumentCapability.
 *
 * technique is DELIBERATELY NOT represented here, for the same reason
 * events/instrument.h omits it: the Registry's Instrument property list
 * is illustrative, not exhaustive, and no settled technique
 * representation exists yet. This is a deliberate scope choice, not an
 * oversight.
 */

typedef struct InstrumentCapability InstrumentCapability;

typedef enum {
    INSTRUMENT_CAPABILITY_OK = 0,
    INSTRUMENT_CAPABILITY_ERR_NULL_INSTRUMENT,
    INSTRUMENT_CAPABILITY_ERR_ALLOC
} InstrumentCapabilityResult;

InstrumentCapabilityResult instrument_capability_create_from_instrument(const Instrument *instrument,
                                                                         InstrumentCapability **out);
void instrument_capability_destroy(InstrumentCapability *capability);

const PitchIdentity *instrument_capability_range_lowest(const InstrumentCapability *capability);
const PitchIdentity *instrument_capability_range_highest(const InstrumentCapability *capability);
int instrument_capability_transposition_semitones(const InstrumentCapability *capability);

/* Value equality: same range (lowest and highest) and same transposition. */
bool instrument_capability_equals(const InstrumentCapability *a, const InstrumentCapability *b);

#endif