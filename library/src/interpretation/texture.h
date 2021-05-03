#ifndef MUSIC_INTERPRETATION_TEXTURE_H
#define MUSIC_INTERPRETATION_TEXTURE_H

#include "interpretation/interpretation.h"

/*
 * Texture (Interpretation): a framework-relative claim naming the
 * texture of a passage under some Framework: how its voices are dense
 * and interact (one line, variants of a line, voices moving together,
 * independent lines).
 *
 * Texture embeds and owns an Interpretation (interpretation/
 * interpretation.h), the same shape Chord, Key, and ScaleDegree have.
 * The underlying fact (which passage this Texture is about) is carried
 * by the embedded Interpretation's own target_id -- the caller sets
 * target_id to the passage's EntityID when building that
 * Interpretation. What a passage is (a Section, a Measure span, a
 * Context) is the caller's choice: this file does not include any other
 * module's header and never resolves target_id, per the L6 cross-cutting
 * rule (docs/modules/l6-notes.md).
 *
 * label is an opaque, non-enumerated string naming what the framework
 * calls this texture (e.g. "monophony", "homorhythm") -- the same
 * opaque-token convention Chord.label uses. The vocabulary belongs to a
 * Framework, not to src/.
 *
 * As with Chord, the Interpretation's framework_reference is REQUIRED:
 * a Texture is specifically the framework-relative claim.
 *
 * Texture has no mutator: a revision is a new Texture construction, not
 * an in-place field change (Law C2).
 */

typedef struct Texture Texture;

typedef enum {
    TEXTURE_OK = 0,
    TEXTURE_ERR_NULL_INTERPRETATION,
    TEXTURE_ERR_NULL_FRAMEWORK_REFERENCE,
    TEXTURE_ERR_NULL_LABEL,
    TEXTURE_ERR_ALLOC
} TextureResult;

/*
 * Takes ownership of interpretation on success (destroyed by
 * texture_destroy, which destroys the embedded Interpretation too).
 * interpretation must be non-NULL and its
 * interpretation_framework_reference() must be non-NULL. label is
 * copied, required, non-empty.
 */
TextureResult texture_create(Interpretation *interpretation, const char *label, Texture **out);
void texture_destroy(Texture *texture);

const Interpretation *texture_interpretation(const Texture *texture);
const char *texture_label(const Texture *texture);

/*
 * Ownership-transferring unwrap: destroys only the Texture shell (its
 * owned label) and hands back the embedded Interpretation with ownership
 * transferred to the caller (the caller must eventually
 * interpretation_destroy it). Use this when a caller needs the
 * Interpretation on its own, e.g. to place it into an AnalysisResult
 * (analysis/analyzer.h). texture must be non-NULL.
 */
Interpretation *texture_take_interpretation(Texture *texture);

#endif
