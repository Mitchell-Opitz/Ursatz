#ifndef MUSIC_INTERPRETATION_KEY_H
#define MUSIC_INTERPRETATION_KEY_H

#include <stdbool.h>

#include "interpretation/interpretation.h"

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
 * KeyStructure (optional): the tonic and mode a Key's producer knew, in numbers, so a consumer in another
 * framework can read them without parsing the label. tonic_pitch_class is 0 = C ... 11 = B. The mode is
 * the Key's quality as the producer's vocabulary has it: major, or minor (the natural-minor key; the raised
 * sixth and seventh of minor are a Roman numeral convention, not a different key). A Key built without a
 * structure reports none.
 */
typedef enum { KEY_STRUCTURE_MODE_MAJOR = 0, KEY_STRUCTURE_MODE_MINOR } KeyStructureMode;

typedef struct {
    int tonic_pitch_class;
    KeyStructureMode mode;
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
   0..11 or an invalid mode. */
KeyResult key_create_with_structure(Interpretation *interpretation, const char *label, const KeyStructure *structure,
                                     Key **out);
void key_destroy(Key *key);

const Interpretation *key_interpretation(const Key *key);
const char *key_label(const Key *key);

/* Copies the Key's structure into *out and returns true; false (out untouched) when it has none. */
bool key_structure(const Key *key, KeyStructure *out);

/*
 * Ownership-transferring unwrap: destroys only the Key shell (its
 * owned label) and hands back the embedded Interpretation with
 * ownership transferred to the caller (the caller must eventually
 * interpretation_destroy it). Use this when a caller needs the
 * Interpretation on its own -- e.g. to place it into an AnalysisResult
 * (analysis/analyzer.h), which holds bare Interpretation objects, not
 * Key wrappers -- without losing everything key_create validated.
 * key must be non-NULL.
 */
Interpretation *key_take_interpretation(Key *key);

#endif