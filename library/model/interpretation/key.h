#ifndef MUSIC_INTERPRETATION_KEY_H
#define MUSIC_INTERPRETATION_KEY_H

#include <stdbool.h>

#include "model/interpretation/interpretation.h"
#include "model/pitch/pitch_material.h"

/*
 * Key (Interpretation): a framework-relative claim naming what a
 * Context (context/context.h) is called under some Framework.
 *
 * Key embeds and owns an Interpretation (interpretation/interpretation.h)
 * -- the same relationship structure/motif_occurrence.h's MotifOccurrence
 * has to core/occurrence.h's Occurrence. The underlying fact (which
 * Context this Key is about) is carried by the embedded Interpretation's
 * own target_id -- the caller sets target_id to the Context's EntityID
 * when building that Interpretation. This file does not include
 * context/context.h and never resolves target_id to an actual Context:
 * per the L6 cross-cutting rule (docs/modules/l6-notes.md),
 * music-interpretation never imports music-context, even sideways for
 * this reference.
 *
 * label is an opaque, non-enumerated string naming what the framework
 * calls this Key (e.g. "D major", "F# aeolian") -- the same
 * opaque-token convention already used for Articulation.kind,
 * Dynamics.marking, Instrument's illustrative properties, and
 * SourceVariant's descriptor. No concrete theory framework exists yet
 * to own real key-name vocabulary; hard-coding one now would prejudge
 * which framework's vocabulary is authoritative.
 *
 * Unlike the base Interpretation type, where framework_reference is
 * optional, a Key's framework_reference is REQUIRED: key_create
 * rejects an interpretation whose framework_reference is NULL, since a
 * Key is specifically the framework-relative claim the Registry
 * defines it as.
 *
 * Key has no mutator: a revision is a new Key construction, not an
 * in-place field change (Law C2).
 */

typedef struct Key Key;

/*
 * KeyStructure (optional): the tonic and material a Key's producer knew, in numbers, so a consumer in another
 * framework can read them without parsing the label. tonic_pitch_class is 0 = C ... 11 = B. The material is the pitch
 * material of the key (a pointer into the knowledge snapshot the key was made from; the snapshot outlives every key made from
 * it). A consumer that needs the key's quality reads it from the material (pitch_material_third). A Key built without a structure
 * reports none.
 */
typedef struct {
    int tonic_pitch_class;
    const PitchMaterial *material;
} KeyStructure;

typedef enum {
    KEY_OK = 0,
    KEY_ERR_NULL_INTERPRETATION,
    KEY_ERR_NULL_FRAMEWORK_REFERENCE,
    KEY_ERR_NULL_LABEL,
    KEY_ERR_ALLOC,
    KEY_ERR_BAD_STRUCTURE
} KeyResult;

/*
 * Takes ownership of interpretation on success (destroyed by
 * key_destroy, which destroys the embedded Interpretation too).
 * interpretation must be non-NULL and its
 * interpretation_framework_reference() must be non-NULL. label is
 * copied, required, non-empty.
 */
KeyResult key_create(Interpretation *interpretation, const char *label, Key **out);

/* As key_create, and the Key also carries *structure (copied). KEY_ERR_BAD_STRUCTURE for a tonic outside
   0..11 or a NULL material. */
KeyResult key_create_with_structure(Interpretation *interpretation, const char *label, const KeyStructure *structure,
                                     Key **out);
void key_destroy(Key *key);

/* Copies the Key's structure into *out and returns true; false (out untouched) when it has none. */
bool key_structure(const Key *key, KeyStructure *out);

#endif