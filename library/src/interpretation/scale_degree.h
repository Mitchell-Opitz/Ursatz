#ifndef MUSIC_INTERPRETATION_SCALE_DEGREE_H
#define MUSIC_INTERPRETATION_SCALE_DEGREE_H

#include "interpretation/interpretation.h"
#include "pitch/pitch_identity.h"

/*
 * ScaleDegree (Interpretation): a framework-relative claim naming what
 * a specific pitch within a Scale (semantics/scale.h) is called under
 * some Framework -- e.g. what a Roman-numeral framework would
 * eventually populate here, but this layer does not know or care what
 * framework is used.
 *
 * ScaleDegree embeds and owns an Interpretation
 * (interpretation/interpretation.h) -- the same relationship
 * structure/motif_occurrence.h's MotifOccurrence has to
 * core/occurrence.h's Occurrence. The underlying Scale this
 * ScaleDegree is about is carried by the embedded Interpretation's own
 * target_id -- the caller sets target_id to the Scale's EntityID when
 * building that Interpretation. This file does not include
 * semantics/scale.h and never resolves target_id to an actual Scale:
 * per the L6 cross-cutting rule (docs/modules/l6-notes.md),
 * music-interpretation never imports music-semantics, even sideways
 * for this reference.
 *
 * pitch is the specific PitchIdentity (pitch/pitch_identity.h) within
 * that Scale this ScaleDegree names -- owned by value here (not
 * referenced by EntityID), since PitchIdentity is an opaque token, not
 * an Entity with its own identity to reference.
 *
 * label is an opaque, non-enumerated string naming the degree under
 * that framework (e.g. "V", "flat-6") -- the same opaque-token
 * convention already used for Articulation.kind, Dynamics.marking,
 * Instrument's illustrative properties, and SourceVariant's
 * descriptor. No concrete theory framework exists yet to own real
 * degree-name vocabulary; hard-coding one now would prejudge which
 * framework's vocabulary is authoritative.
 *
 * Unlike the base Interpretation type, where framework_reference is
 * optional, a ScaleDegree's framework_reference is REQUIRED:
 * scale_degree_create rejects an interpretation whose
 * framework_reference is NULL, since a ScaleDegree is specifically the
 * framework-relative claim the Registry defines it as.
 *
 * ScaleDegree has no mutator: a revision is a new ScaleDegree
 * construction, not an in-place field change (Law C2).
 */

typedef struct ScaleDegree ScaleDegree;

typedef enum {
    SCALE_DEGREE_OK = 0,
    SCALE_DEGREE_ERR_NULL_INTERPRETATION,
    SCALE_DEGREE_ERR_NULL_FRAMEWORK_REFERENCE,
    SCALE_DEGREE_ERR_NULL_PITCH,
    SCALE_DEGREE_ERR_NULL_LABEL,
    SCALE_DEGREE_ERR_ALLOC
} ScaleDegreeResult;

/*
 * Takes ownership of interpretation and of pitch on success (both
 * destroyed by scale_degree_destroy, which destroys the embedded
 * Interpretation too). interpretation must be non-NULL and its
 * interpretation_framework_reference() must be non-NULL. pitch must be
 * non-NULL. label is copied, required, non-empty.
 */
ScaleDegreeResult scale_degree_create(Interpretation *interpretation, PitchIdentity *pitch, const char *label,
                                       ScaleDegree **out);
void scale_degree_destroy(ScaleDegree *scale_degree);

const Interpretation *scale_degree_interpretation(const ScaleDegree *scale_degree);
const PitchIdentity *scale_degree_pitch(const ScaleDegree *scale_degree);
const char *scale_degree_label(const ScaleDegree *scale_degree);

#endif