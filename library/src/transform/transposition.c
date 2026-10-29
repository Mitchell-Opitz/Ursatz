#include "transform/transposition.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/string_copy.h"
#include "core/voice_reference.h"
#include "pitch/pitch_identity.h"
#include "pitch/pitch_spelling.h"
#include "time/duration.h"
#include "time/musical_time.h"
#include "time/temporal_anchor.h"
#include "transform/transform_support.h"

struct Transposition {
    Transformation *transformation;
    long semitones;
    char *spelling_behavior;
    bool has_tuning_behavior;
    bool has_range_policy;
    bool has_instrument_context;
};

TranspositionResult transposition_create(EntityID *id, long semitones, const char *spelling_behavior,
                                          bool has_tuning_behavior, bool has_range_policy,
                                          bool has_instrument_context, ProvenanceReference *provenance,
                                          Transposition **out) {
    if (id == NULL) {
        return TRANSPOSITION_ERR_NULL_ID;
    }
    if (spelling_behavior == NULL || spelling_behavior[0] == '\0') {
        return TRANSPOSITION_ERR_NULL_SPELLING_BEHAVIOR;
    }
    if (has_tuning_behavior) {
        return TRANSPOSITION_ERR_TUNING_BEHAVIOR_NOT_SUPPORTED;
    }
    if (has_range_policy) {
        return TRANSPOSITION_ERR_RANGE_POLICY_NOT_SUPPORTED;
    }
    if (has_instrument_context) {
        return TRANSPOSITION_ERR_INSTRUMENT_CONTEXT_NOT_SUPPORTED;
    }
    if (provenance == NULL) {
        return TRANSPOSITION_ERR_NULL_PROVENANCE;
    }

    Transposition *transposition = (Transposition *)malloc(sizeof(Transposition));
    if (transposition == NULL) {
        return TRANSPOSITION_ERR_ALLOC;
    }

    transposition->spelling_behavior = ursatz_string_copy(spelling_behavior);
    if (transposition->spelling_behavior == NULL) {
        free(transposition);
        return TRANSPOSITION_ERR_ALLOC;
    }

    Value *parameters;
    if (value_create("semitones", &semitones, sizeof(semitones), &parameters) != VALUE_OK) {
        free(transposition->spelling_behavior);
        free(transposition);
        return TRANSPOSITION_ERR_ALLOC;
    }

    Transformation *transformation;
    TransformationResult transformation_result = transformation_create(
        id, "NoteEvent", parameters, "NoteEvent", "rhythm, event order, articulation", "pitch",
        "one-to-one, per-event pitch displacement",
        "spelling behavior applied per configured policy; tuning/range/instrument checks not yet implemented "
        "(Deliverable D)",
        true, provenance, &transformation);
    if (transformation_result != TRANSFORMATION_OK) {
        value_destroy(parameters);
        free(transposition->spelling_behavior);
        free(transposition);
        return TRANSPOSITION_ERR_ALLOC;
    }

    transposition->transformation = transformation;
    transposition->semitones = semitones;
    transposition->has_tuning_behavior = has_tuning_behavior;
    transposition->has_range_policy = has_range_policy;
    transposition->has_instrument_context = has_instrument_context;

    *out = transposition;
    return TRANSPOSITION_OK;
}

void transposition_destroy(Transposition *transposition) {
    if (transposition == NULL) {
        return;
    }
    transformation_destroy(transposition->transformation);
    free(transposition->spelling_behavior);
    free(transposition);
}

const EntityID *transposition_id(const Transposition *transposition) {
    return transformation_id(transposition->transformation);
}

const Transformation *transposition_transformation(const Transposition *transposition) {
    return transposition->transformation;
}

long transposition_semitones(const Transposition *transposition) {
    return transposition->semitones;
}

const char *transposition_spelling_behavior(const Transposition *transposition) {
    return transposition->spelling_behavior;
}

bool transposition_has_tuning_behavior(const Transposition *transposition) {
    return transposition->has_tuning_behavior;
}

bool transposition_has_range_policy(const Transposition *transposition) {
    return transposition->has_range_policy;
}

bool transposition_has_instrument_context(const Transposition *transposition) {
    return transposition->has_instrument_context;
}

bool transposition_identity_equals(const Transposition *a, const Transposition *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    return transformation_identity_equals(a->transformation, b->transformation);
}

TranspositionApplyResult transposition_apply(const Transposition *transposition, const NoteEvent *event,
                                              const PitchNumericValue *event_pitch_value, TransformResult **out) {
    if (transposition == NULL) {
        return TRANSPOSITION_APPLY_ERR_NULL_TRANSPOSITION;
    }
    if (event == NULL) {
        return TRANSPOSITION_APPLY_ERR_NULL_EVENT;
    }
    if (event_pitch_value == NULL) {
        return TRANSPOSITION_APPLY_ERR_NULL_PITCH_VALUE;
    }

    PitchNumericValue *shifted = NULL;
    if (pitch_numeric_value_add_interval(event_pitch_value, transposition->semitones, 1, &shifted) !=
        PITCH_NUMERIC_VALUE_OP_OK) {
        return TRANSPOSITION_APPLY_ERR_ALLOC;
    }

    PitchIdentity *new_pitch = NULL;
    PitchNumericValueToIdentityResult identity_result = pitch_numeric_value_to_identity(shifted, &new_pitch);
    pitch_numeric_value_destroy(shifted);
    if (identity_result != PITCH_NUMERIC_VALUE_TO_IDENTITY_OK) {
        return TRANSPOSITION_APPLY_ERR_ALLOC;
    }

    NoteEvent *derived = transform_support_clone_note_event_with_new_pitch(event, new_pitch, "-transposed");
    if (derived == NULL) {
        return TRANSPOSITION_APPLY_ERR_ALLOC;
    }

    NoteEvent *events[1];
    events[0] = derived;
    TransformResultResult result = transform_result_create(events, 1, out);
    if (result != TRANSFORM_RESULT_OK) {
        note_event_destroy(derived);
        return TRANSPOSITION_APPLY_ERR_ALLOC;
    }
    return TRANSPOSITION_APPLY_OK;
}