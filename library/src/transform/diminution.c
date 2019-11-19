#include "transform/diminution.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/provenance_reference.h"
#include "core/voice_reference.h"
#include "identity/entity_id.h"
#include "pitch/pitch_identity.h"
#include "pitch/pitch_spelling.h"
#include "time/duration.h"
#include "time/musical_time.h"
#include "time/temporal_anchor.h"

struct Diminution {
    Transformation *transformation;
    AugmentationDimension dimension;
    unsigned long scale_numerator;
    unsigned long scale_denominator;
};

static const char *dimension_name(AugmentationDimension dimension) {
    switch (dimension) {
        case AUGMENTATION_DIMENSION_DURATIONS:
            return "durations";
        case AUGMENTATION_DIMENSION_INTER_ONSET_INTERVALS:
            return "inter-onset intervals";
        case AUGMENTATION_DIMENSION_STRUCTURAL_TIMING:
            return "structural timing";
        case AUGMENTATION_DIMENSION_NOTATION_VALUES:
            return "notation values";
        case AUGMENTATION_DIMENSION_PERFORMANCE_TIMING:
            return "performance timing";
        default:
            return NULL;
    }
}

DiminutionResult diminution_create(EntityID *id, AugmentationDimension dimension, unsigned long scale_numerator,
                                    unsigned long scale_denominator, ProvenanceReference *provenance,
                                    Diminution **out) {
    if (id == NULL) {
        return DIMINUTION_ERR_NULL_ID;
    }
    const char *dimension_label = dimension_name(dimension);
    if (dimension_label == NULL) {
        return DIMINUTION_ERR_UNKNOWN_DIMENSION;
    }
    if (scale_numerator == 0 || scale_denominator == 0) {
        return DIMINUTION_ERR_NON_POSITIVE_DENOMINATOR;
    }
    if (scale_numerator >= scale_denominator) {
        return DIMINUTION_ERR_FACTOR_NOT_LESS_THAN_ONE;
    }
    if (provenance == NULL) {
        return DIMINUTION_ERR_NULL_PROVENANCE;
    }

    Diminution *diminution = (Diminution *)malloc(sizeof(Diminution));
    if (diminution == NULL) {
        return DIMINUTION_ERR_ALLOC;
    }

    Transformation *transformation;
    TransformationResult transformation_result = transformation_create(
        id, "NoteEvent", NULL, "NoteEvent", "pitch, event order, articulation", dimension_label,
        "one-to-one, scales the selected timing dimension by the given rational factor",
        "scale factor requires 0 < numerator < denominator (net shrinking); dimension explicitly declared, never "
        "defaulted to performance timing",
        true, provenance, &transformation);
    if (transformation_result != TRANSFORMATION_OK) {
        free(diminution);
        return DIMINUTION_ERR_ALLOC;
    }

    diminution->transformation = transformation;
    diminution->dimension = dimension;
    diminution->scale_numerator = scale_numerator;
    diminution->scale_denominator = scale_denominator;

    *out = diminution;
    return DIMINUTION_OK;
}

void diminution_destroy(Diminution *diminution) {
    if (diminution == NULL) {
        return;
    }
    transformation_destroy(diminution->transformation);
    free(diminution);
}

const EntityID *diminution_id(const Diminution *diminution) {
    return transformation_id(diminution->transformation);
}

const Transformation *diminution_transformation(const Diminution *diminution) {
    return diminution->transformation;
}

AugmentationDimension diminution_dimension(const Diminution *diminution) {
    return diminution->dimension;
}

unsigned long diminution_scale_numerator(const Diminution *diminution) {
    return diminution->scale_numerator;
}

unsigned long diminution_scale_denominator(const Diminution *diminution) {
    return diminution->scale_denominator;
}

bool diminution_identity_equals(const Diminution *a, const Diminution *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    return transformation_identity_equals(a->transformation, b->transformation);
}

static unsigned long gcd_ul(unsigned long a, unsigned long b) {
    while (b != 0) {
        unsigned long t = b;
        b = a % b;
        a = t;
    }
    return a;
}

static void rational_reduce(long *numerator, unsigned long *denominator) {
    unsigned long abs_numerator = *numerator < 0 ? (unsigned long)(-*numerator) : (unsigned long)*numerator;
    unsigned long divisor = gcd_ul(abs_numerator, *denominator);
    if (divisor == 0) {
        divisor = 1;
    }
    *numerator = *numerator / (long)divisor;
    *denominator = *denominator / divisor;
}

static PitchIdentity *clone_pitch_identity(const PitchIdentity *source) {
    size_t size = 0;
    const void *token = pitch_identity_token(source, &size);
    PitchIdentity *clone = NULL;
    if (pitch_identity_create(token, size, &clone) != PITCH_IDENTITY_OK) {
        return NULL;
    }
    return clone;
}

static bool clone_pitch_spelling_or_null(const PitchSpelling *source, PitchSpelling **out) {
    if (source == NULL) {
        *out = NULL;
        return true;
    }
    return pitch_spelling_create(pitch_spelling_letter(source), pitch_spelling_accidental(source),
                                  pitch_spelling_octave(source), out) == PITCH_SPELLING_OK;
}

static bool clone_voice_reference_or_null(const VoiceReference *source, VoiceReference **out) {
    if (source == NULL) {
        *out = NULL;
        return true;
    }
    EntityID *target_copy = NULL;
    if (entity_id_create(entity_id_value(voice_reference_target_id(source)), &target_copy) != ENTITY_ID_OK) {
        return false;
    }
    if (voice_reference_create(target_copy, out) != VOICE_REFERENCE_OK) {
        entity_id_destroy(target_copy);
        return false;
    }
    return true;
}

static EntityID *make_derived_id(const EntityID *source_id, const char *suffix) {
    const char *source_value = entity_id_value(source_id);
    size_t len = strlen(source_value) + strlen(suffix) + 1;
    char *buffer = (char *)malloc(len);
    if (buffer == NULL) {
        return NULL;
    }
    snprintf(buffer, len, "%s%s", source_value, suffix);
    EntityID *derived = NULL;
    entity_id_create(buffer, &derived);
    free(buffer);
    return derived;
}

static ProvenanceReference *make_provenance_from(const EntityID *source_id) {
    EntityID *target_copy = NULL;
    if (entity_id_create(entity_id_value(source_id), &target_copy) != ENTITY_ID_OK) {
        return NULL;
    }
    ProvenanceReference *provenance = NULL;
    if (provenance_reference_create_present(target_copy, &provenance) != PROVENANCE_REFERENCE_OK) {
        entity_id_destroy(target_copy);
        return NULL;
    }
    return provenance;
}

/* Builds a new NoteEvent identical to source except for duration (a
   fresh Duration at new_duration_numerator/new_duration_denominator).
   Returns NULL on any allocation failure. */
static NoteEvent *clone_note_event_with_new_duration(const NoteEvent *source, long new_duration_numerator,
                                                      unsigned long new_duration_denominator,
                                                      const char *id_suffix) {
    Duration *duration = NULL;
    if (duration_create(new_duration_numerator, new_duration_denominator, &duration) != DURATION_OK) {
        return NULL;
    }

    const MusicalTime *source_position = temporal_anchor_position(note_event_temporal_anchor(source));
    MusicalTime *position = NULL;
    if (musical_time_create(musical_time_numerator(source_position), musical_time_denominator(source_position),
                             &position) != MUSICAL_TIME_OK) {
        duration_destroy(duration);
        return NULL;
    }
    TemporalAnchor *anchor = NULL;
    if (temporal_anchor_create(position, &anchor) != TEMPORAL_ANCHOR_OK) {
        musical_time_destroy(position);
        duration_destroy(duration);
        return NULL;
    }

    PitchIdentity *pitch = clone_pitch_identity(note_event_pitch(source));
    if (pitch == NULL) {
        temporal_anchor_destroy(anchor);
        duration_destroy(duration);
        return NULL;
    }

    PitchSpelling *spelling = NULL;
    if (!clone_pitch_spelling_or_null(note_event_optional_pitch_spelling(source), &spelling)) {
        pitch_identity_destroy(pitch);
        temporal_anchor_destroy(anchor);
        duration_destroy(duration);
        return NULL;
    }

    VoiceReference *voice = NULL;
    if (!clone_voice_reference_or_null(note_event_optional_voice(source), &voice)) {
        pitch_spelling_destroy(spelling);
        pitch_identity_destroy(pitch);
        temporal_anchor_destroy(anchor);
        duration_destroy(duration);
        return NULL;
    }

    EntityID *id = make_derived_id(note_event_id(source), id_suffix);
    if (id == NULL) {
        voice_reference_destroy(voice);
        pitch_spelling_destroy(spelling);
        pitch_identity_destroy(pitch);
        temporal_anchor_destroy(anchor);
        duration_destroy(duration);
        return NULL;
    }

    ProvenanceReference *provenance = make_provenance_from(note_event_id(source));
    if (provenance == NULL) {
        entity_id_destroy(id);
        voice_reference_destroy(voice);
        pitch_spelling_destroy(spelling);
        pitch_identity_destroy(pitch);
        temporal_anchor_destroy(anchor);
        duration_destroy(duration);
        return NULL;
    }

    NoteEvent *derived = NULL;
    if (note_event_create(id, anchor, duration, pitch, spelling, voice, provenance, &derived) != NOTE_EVENT_OK) {
        return NULL;
    }
    return derived;
}

DiminutionApplyResult diminution_apply(const Diminution *diminution, const NoteEvent *event, TransformResult **out) {
    if (diminution == NULL) {
        return DIMINUTION_APPLY_ERR_NULL_DIMINUTION;
    }
    if (event == NULL) {
        return DIMINUTION_APPLY_ERR_NULL_EVENT;
    }
    if (diminution->dimension != AUGMENTATION_DIMENSION_DURATIONS) {
        return DIMINUTION_APPLY_ERR_DIMENSION_NOT_IMPLEMENTED;
    }

    const Duration *source_duration = note_event_duration(event);
    long new_numerator = duration_numerator(source_duration) * (long)diminution->scale_numerator;
    unsigned long new_denominator = duration_denominator(source_duration) * diminution->scale_denominator;
    rational_reduce(&new_numerator, &new_denominator);

    NoteEvent *derived = clone_note_event_with_new_duration(event, new_numerator, new_denominator, "-diminished");
    if (derived == NULL) {
        return DIMINUTION_APPLY_ERR_ALLOC;
    }

    NoteEvent *events[1];
    events[0] = derived;
    TransformResultResult result = transform_result_create(events, 1, out);
    if (result != TRANSFORM_RESULT_OK) {
        note_event_destroy(derived);
        return DIMINUTION_APPLY_ERR_ALLOC;
    }
    return DIMINUTION_APPLY_OK;
}