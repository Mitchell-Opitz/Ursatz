#ifndef MUSIC_INTERPRETATION_KEY_H
#define MUSIC_INTERPRETATION_KEY_H

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

typedef enum {
    KEY_OK = 0,
    KEY_ERR_NULL_INTERPRETATION,
    KEY_ERR_NULL_FRAMEWORK_REFERENCE,
    KEY_ERR_NULL_LABEL,
    KEY_ERR_ALLOC
} KeyResult;

/*
 * Takes ownership of interpretation on success (destroyed by
 * key_destroy, which destroys the embedded Interpretation too).
 * interpretation must be non-NULL and its
 * interpretation_framework_reference() must be non-NULL. label is
 * copied, required, non-empty.
 */
KeyResult key_create(Interpretation *interpretation, const char *label, Key **out);
void key_destroy(Key *key);

const Interpretation *key_interpretation(const Key *key);
const char *key_label(const Key *key);

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