#ifndef MUSIC_SEMANTICS_SCALE_H
#define MUSIC_SEMANTICS_SCALE_H

#include <stddef.h>

#include "identity/entity_id.h"
#include "pitch/pitch_class.h"

/*
 * Scale (Value): an ordered collection of PitchClass (pitch/pitch_class.h)
 * under a TuningSystem (pitch/tuning_system.h).
 *
 * Fields:
 *   - tuning_system_id: EntityID reference to the TuningSystem this
 *     Scale's pitch classes are drawn under, referenced not embedded --
 *     the same reference-only pattern Sonority uses for voice_ids
 *     (semantics/sonority.h). This header does not include
 *     pitch/tuning_system.h; only its EntityID is carried here.
 *   - pitch_classes: owned, ordered array of PitchClass, count > 0. A
 *     Scale is a Value, not an Entity -- it carries no EntityID of its
 *     own (same as Sonority, which also has no sonority_id()).
 *
 * Registry non-normative, reference concept only (docs/modules/semantics.md):
 * this is minimal scaffolding, not a claim that scale theory (modes,
 * degree names, etc.) is settled here. Scale has no FrameworkReference --
 * an ordered PitchClass collection under a TuningSystem does not require
 * an interpretive claim to exist; contrast ScaleDegree
 * (interpretation/scale_degree.h), which does.
 *
 * Scale has no mutator: a revision is a new Scale construction, not an
 * in-place field change (Law C2).
 */

typedef struct Scale Scale;

typedef enum {
    SCALE_OK = 0,
    SCALE_ERR_NULL_TUNING_SYSTEM_ID,
    SCALE_ERR_NULL_PITCH_CLASSES,
    SCALE_ERR_NULL_PITCH_CLASS,
    SCALE_ERR_ALLOC
} ScaleResult;

/*
 * Takes ownership of tuning_system_id and of pitch_classes (and each
 * PitchClass it points to) on success -- all destroyed by
 * scale_destroy. pitch_classes must be non-NULL with
 * pitch_class_count > 0; no entry may be NULL.
 */
ScaleResult scale_create(EntityID *tuning_system_id, PitchClass **pitch_classes, size_t pitch_class_count,
                          Scale **out);
void scale_destroy(Scale *scale);

const EntityID *scale_tuning_system_id(const Scale *scale);
size_t scale_pitch_class_count(const Scale *scale);
const PitchClass *scale_pitch_class_at(const Scale *scale, size_t index);

#endif