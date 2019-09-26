#ifndef MUSIC_INTERPRETATION_CHORD_H
#define MUSIC_INTERPRETATION_CHORD_H

#include "interpretation/interpretation.h"

/*
 * Chord (Interpretation): a framework-relative claim naming what a
 * Sonority (semantics/sonority.h) is called under some Framework.
 *
 * Chord embeds and owns an Interpretation (interpretation/interpretation.h)
 * -- the same relationship structure/motif_occurrence.h's MotifOccurrence
 * has to core/occurrence.h's Occurrence: the next type built directly on
 * top of a base type composes it rather than duplicating its fields.
 * The underlying fact (which Sonority this Chord is about) is carried by
 * the embedded Interpretation's own target_id -- the caller sets
 * target_id to the Sonority's EntityID when building that Interpretation.
 * This file does not include semantics/sonority.h and never resolves
 * target_id to an actual Sonority: per the L6 cross-cutting rule
 * (docs/modules/l6-notes.md), music-interpretation never imports
 * music-semantics, even sideways for this reference.
 *
 * label is an opaque, non-enumerated string naming what the framework
 * calls this Chord (e.g. "V7", "Cmaj7#11") -- the same opaque-token
 * convention already used for Articulation.kind, Dynamics.marking,
 * Instrument's illustrative properties, and SourceVariant's descriptor.
 * No concrete theory framework exists yet to own real chord-name
 * vocabulary; hard-coding one now would prejudge which framework's
 * vocabulary is authoritative.
 *
 * Unlike the base Interpretation type, where framework_reference is
 * optional (most existing Interpretation producers are framework-
 * neutral), a Chord's framework_reference is REQUIRED: chord_create
 * rejects an interpretation whose framework_reference is NULL, since a
 * Chord is specifically the framework-relative claim the Registry
 * defines it as.
 *
 * Chord has no mutator: a revision is a new Chord construction, not an
 * in-place field change (Law C2).
 */

typedef struct Chord Chord;

typedef enum {
    CHORD_OK = 0,
    CHORD_ERR_NULL_INTERPRETATION,
    CHORD_ERR_NULL_FRAMEWORK_REFERENCE,
    CHORD_ERR_NULL_LABEL,
    CHORD_ERR_ALLOC
} ChordResult;

/*
 * Takes ownership of interpretation on success (destroyed by
 * chord_destroy, which destroys the embedded Interpretation too).
 * interpretation must be non-NULL and its
 * interpretation_framework_reference() must be non-NULL. label is
 * copied, required, non-empty.
 */
ChordResult chord_create(Interpretation *interpretation, const char *label, Chord **out);
void chord_destroy(Chord *chord);

const Interpretation *chord_interpretation(const Chord *chord);
const char *chord_label(const Chord *chord);

#endif