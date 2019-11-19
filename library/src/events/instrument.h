#ifndef MUSIC_EVENTS_INSTRUMENT_H
#define MUSIC_EVENTS_INSTRUMENT_H

#include <stdbool.h>

#include "identity/entity_id.h"
#include "pitch/pitch_identity.h"

/*
 * Instrument (Entity category): identity, a name, a pitch range, and a
 * transposition. The Registry marks Instrument's property list
 * "Partial (property list is illustrative)" -- range, transposition,
 * clefs, technique, etc. are named as EXAMPLES, not an exhaustive
 * required set.
 *
 * This type builds exactly four of those: id, name, range (lowest/
 * highest PitchIdentity), and transposition. Clefs and technique are
 * DELIBERATELY NOT implemented here -- a deliberate scope choice, not an
 * oversight, consistent with the Registry's own "illustrative" framing.
 * Neither has a settled representation yet (clef notation and technique
 * vocabularies are open questions this pass does not resolve); adding
 * either is a separate, later change, not a gap in this one.
 *
 * range_lowest/range_highest are PitchIdentity (pitch/pitch_identity.h),
 * the same opaque-token type NoteEvent already uses for pitch -- no
 * numeric pitch representation exists at this layer to validate
 * lowest <= highest against, so no such ordering is enforced; that is
 * left to whatever higher-layer code interprets these tokens.
 *
 * transposition_semitones is a plain, uninterpreted integer offset --
 * the same "opaque numeric offset, no theory baked in" treatment
 * pitch/pitch_spelling.h gives its accidental field. No sign convention
 * or TuningSystem tie is enforced here.
 *
 * Instrument does not build on core/entity.h's Entity, mirroring
 * theory/framework.h's Framework and pitch/tuning_system.h's
 * TuningSystem, for the same Entity/Occurrence/Event category-boundary
 * reason.
 */

typedef struct Instrument Instrument;

typedef enum {
    INSTRUMENT_OK = 0,
    INSTRUMENT_ERR_NULL_ID,
    INSTRUMENT_ERR_NULL_NAME,
    INSTRUMENT_ERR_NULL_RANGE_LOWEST,
    INSTRUMENT_ERR_NULL_RANGE_HIGHEST,
    INSTRUMENT_ERR_ALLOC
} InstrumentResult;

/*
 * Takes ownership of id, range_lowest, and range_highest on success
 * (destroyed by instrument_destroy). name is copied, required,
 * non-empty.
 */
InstrumentResult instrument_create(EntityID *id, const char *name, PitchIdentity *range_lowest,
                                    PitchIdentity *range_highest, int transposition_semitones, Instrument **out);
void instrument_destroy(Instrument *instrument);

const EntityID *instrument_id(const Instrument *instrument);
const char *instrument_name(const Instrument *instrument);
const PitchIdentity *instrument_range_lowest(const Instrument *instrument);
const PitchIdentity *instrument_range_highest(const Instrument *instrument);
int instrument_transposition_semitones(const Instrument *instrument);

/* Identity equality: same EntityID, regardless of instance. */
bool instrument_identity_equals(const Instrument *a, const Instrument *b);

#endif