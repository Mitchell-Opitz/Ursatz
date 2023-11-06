#ifndef MUSIC_INTERPRETATION_CHORD_H
#define MUSIC_INTERPRETATION_CHORD_H

#include <stdbool.h>
#include <stddef.h>

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

/*
 * ChordStructure (optional): the framework-neutral shape a Chord's producer knew when it built the
 * claim, so a consumer in another framework can read it without parsing the label. It is the same
 * fact the label names, in numbers: the root's pitch class (0 = C ... 11 = B), the quality as the
 * root-relative interval set in semitones, ascending, starting at 0 (a major triad {0,4,7}, a
 * dominant seventh {0,4,7,10}), and the bass's pitch class. Any of the three may be unknown: a
 * pitch class of -1, or an interval_count of 0. A Chord built without a structure reports none.
 */
#define CHORD_STRUCTURE_MAX_INTERVALS 8

typedef struct {
    int root_pitch_class;
    int bass_pitch_class;
    size_t interval_count;
    int root_relative_intervals[CHORD_STRUCTURE_MAX_INTERVALS];
} ChordStructure;

typedef enum {
    CHORD_OK = 0,
    CHORD_ERR_NULL_INTERPRETATION,
    CHORD_ERR_NULL_FRAMEWORK_REFERENCE,
    CHORD_ERR_NULL_LABEL,
    CHORD_ERR_ALLOC,
    CHORD_ERR_BAD_STRUCTURE
} ChordResult;

/*
 * Takes ownership of interpretation on success (destroyed by
 * chord_destroy, which destroys the embedded Interpretation too).
 * interpretation must be non-NULL and its
 * interpretation_framework_reference() must be non-NULL. label is
 * copied, required, non-empty.
 */
ChordResult chord_create(Interpretation *interpretation, const char *label, Chord **out);

/*
 * As chord_create, and the Chord also carries *structure (copied). ChordResult CHORD_ERR_BAD_STRUCTURE
 * for an interval_count above CHORD_STRUCTURE_MAX_INTERVALS or a pitch class outside -1..11.
 */
ChordResult chord_create_with_structure(Interpretation *interpretation, const char *label,
                                         const ChordStructure *structure, Chord **out);
void chord_destroy(Chord *chord);

const Interpretation *chord_interpretation(const Chord *chord);
const char *chord_label(const Chord *chord);

/* Copies the Chord's structure into *out and returns true; false (out untouched) when it has none. */
bool chord_structure(const Chord *chord, ChordStructure *out);

/*
 * Ownership-transferring unwrap: destroys only the Chord shell (its
 * owned label) and hands back the embedded Interpretation with
 * ownership transferred to the caller (the caller must eventually
 * interpretation_destroy it). Use this when a caller needs the
 * Interpretation on its own -- e.g. to place it into an AnalysisResult
 * (analysis/analyzer.h), which holds bare Interpretation objects, not
 * Chord wrappers -- without losing everything chord_create validated.
 * chord must be non-NULL.
 */
Interpretation *chord_take_interpretation(Chord *chord);

#endif