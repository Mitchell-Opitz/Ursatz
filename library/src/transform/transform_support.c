#include "transform/transform_support.h"

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>

#include "core/provenance_reference.h"
#include "core/rational.h"
#include "core/voice_reference.h"
#include "identity/entity_id.h"
#include "pitch/pitch_identity.h"
#include "pitch/pitch_spelling.h"
#include "time/duration.h"
#include "time/musical_time.h"
#include "time/temporal_anchor.h"

/*
 * Shared by the transform files only: every transform builds derived NoteEvents the same way (fresh copies of every field,
 * a derived id "<source id><suffix>", provenance pointing at the source). The copies differ only in the one field a transform
 * changes, which is the helper's argument (pitch, duration or onset); the ornamentation voice copy built its EntityID with its
 * own helper and is otherwise identical.
 */

const char *transform_support_dimension_name(AugmentationDimension dimension) {
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

PitchIdentity *transform_support_clone_pitch_identity(const PitchIdentity *source) {
    size_t size = 0;
    const void *token = pitch_identity_token(source, &size);
    PitchIdentity *clone = NULL;
    if (pitch_identity_create(token, size, &clone) != PITCH_IDENTITY_OK) {
        return NULL;
    }
    return clone;
}

bool transform_support_clone_pitch_spelling_or_null(const PitchSpelling *source, PitchSpelling **out) {
    if (source == NULL) {
        *out = NULL;
        return true;
    }
    return pitch_spelling_create(pitch_spelling_letter(source), pitch_spelling_accidental(source),
                                  pitch_spelling_octave(source), out) == PITCH_SPELLING_OK;
}

bool transform_support_clone_voice_reference_or_null(const VoiceReference *source, VoiceReference **out) {
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

EntityID *transform_support_make_derived_id(const EntityID *source_id, const char *suffix) {
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

ProvenanceReference *transform_support_make_provenance_from(const EntityID *source_id) {
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

/* Builds a new NoteEvent identical to source except for pitch (a newly
   constructed PitchIdentity, ownership transferred in). Returns NULL on
   any allocation failure -- new_pitch is destroyed on that path too. */
NoteEvent *transform_support_clone_note_event_with_new_pitch(const NoteEvent *source, PitchIdentity *new_pitch,
                                                   const char *id_suffix) {
    const MusicalTime *source_position = temporal_anchor_position(note_event_temporal_anchor(source));
    MusicalTime *position = NULL;
    if (musical_time_create(musical_time_numerator(source_position), musical_time_denominator(source_position),
                             &position) != MUSICAL_TIME_OK) {
        pitch_identity_destroy(new_pitch);
        return NULL;
    }
    TemporalAnchor *anchor = NULL;
    if (temporal_anchor_create(position, &anchor) != TEMPORAL_ANCHOR_OK) {
        musical_time_destroy(position);
        pitch_identity_destroy(new_pitch);
        return NULL;
    }

    const Duration *source_duration = note_event_duration(source);
    Duration *duration = NULL;
    if (duration_create(duration_numerator(source_duration), duration_denominator(source_duration), &duration) !=
        DURATION_OK) {
        temporal_anchor_destroy(anchor);
        pitch_identity_destroy(new_pitch);
        return NULL;
    }

    PitchSpelling *spelling = NULL;
    if (!transform_support_clone_pitch_spelling_or_null(note_event_optional_pitch_spelling(source), &spelling)) {
        duration_destroy(duration);
        temporal_anchor_destroy(anchor);
        pitch_identity_destroy(new_pitch);
        return NULL;
    }

    VoiceReference *voice = NULL;
    if (!transform_support_clone_voice_reference_or_null(note_event_optional_voice(source), &voice)) {
        pitch_spelling_destroy(spelling);
        duration_destroy(duration);
        temporal_anchor_destroy(anchor);
        pitch_identity_destroy(new_pitch);
        return NULL;
    }

    EntityID *id = transform_support_make_derived_id(note_event_id(source), id_suffix);
    if (id == NULL) {
        voice_reference_destroy(voice);
        pitch_spelling_destroy(spelling);
        duration_destroy(duration);
        temporal_anchor_destroy(anchor);
        pitch_identity_destroy(new_pitch);
        return NULL;
    }

    ProvenanceReference *provenance = transform_support_make_provenance_from(note_event_id(source));
    if (provenance == NULL) {
        entity_id_destroy(id);
        voice_reference_destroy(voice);
        pitch_spelling_destroy(spelling);
        duration_destroy(duration);
        temporal_anchor_destroy(anchor);
        pitch_identity_destroy(new_pitch);
        return NULL;
    }

    NoteEvent *derived = NULL;
    if (note_event_create(id, anchor, duration, new_pitch, spelling, voice, provenance, &derived) != NOTE_EVENT_OK) {
        return NULL;
    }
    return derived;
}

/* Builds a new NoteEvent identical to source except for duration (a
   fresh Duration at new_duration_numerator/new_duration_denominator).
   Returns NULL on any allocation failure. */
NoteEvent *transform_support_clone_note_event_with_new_duration(const NoteEvent *source, long new_duration_numerator,
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

    PitchIdentity *pitch = transform_support_clone_pitch_identity(note_event_pitch(source));
    if (pitch == NULL) {
        temporal_anchor_destroy(anchor);
        duration_destroy(duration);
        return NULL;
    }

    PitchSpelling *spelling = NULL;
    if (!transform_support_clone_pitch_spelling_or_null(note_event_optional_pitch_spelling(source), &spelling)) {
        pitch_identity_destroy(pitch);
        temporal_anchor_destroy(anchor);
        duration_destroy(duration);
        return NULL;
    }

    VoiceReference *voice = NULL;
    if (!transform_support_clone_voice_reference_or_null(note_event_optional_voice(source), &voice)) {
        pitch_spelling_destroy(spelling);
        pitch_identity_destroy(pitch);
        temporal_anchor_destroy(anchor);
        duration_destroy(duration);
        return NULL;
    }

    EntityID *id = transform_support_make_derived_id(note_event_id(source), id_suffix);
    if (id == NULL) {
        voice_reference_destroy(voice);
        pitch_spelling_destroy(spelling);
        pitch_identity_destroy(pitch);
        temporal_anchor_destroy(anchor);
        duration_destroy(duration);
        return NULL;
    }

    ProvenanceReference *provenance = transform_support_make_provenance_from(note_event_id(source));
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

/* Builds a new NoteEvent identical to source except for temporal_anchor
   (a fresh MusicalTime at new_onset_numerator/new_onset_denominator).
   Returns NULL on any allocation failure. */
NoteEvent *transform_support_clone_note_event_with_new_onset(const NoteEvent *source, long new_onset_numerator,
                                                   unsigned long new_onset_denominator, const char *id_suffix) {
    MusicalTime *position = NULL;
    if (musical_time_create(new_onset_numerator, new_onset_denominator, &position) != MUSICAL_TIME_OK) {
        return NULL;
    }
    TemporalAnchor *anchor = NULL;
    if (temporal_anchor_create(position, &anchor) != TEMPORAL_ANCHOR_OK) {
        musical_time_destroy(position);
        return NULL;
    }

    const Duration *source_duration = note_event_duration(source);
    Duration *duration = NULL;
    if (duration_create(duration_numerator(source_duration), duration_denominator(source_duration), &duration) !=
        DURATION_OK) {
        temporal_anchor_destroy(anchor);
        return NULL;
    }

    PitchIdentity *pitch = transform_support_clone_pitch_identity(note_event_pitch(source));
    if (pitch == NULL) {
        duration_destroy(duration);
        temporal_anchor_destroy(anchor);
        return NULL;
    }

    PitchSpelling *spelling = NULL;
    if (!transform_support_clone_pitch_spelling_or_null(note_event_optional_pitch_spelling(source), &spelling)) {
        pitch_identity_destroy(pitch);
        duration_destroy(duration);
        temporal_anchor_destroy(anchor);
        return NULL;
    }

    VoiceReference *voice = NULL;
    if (!transform_support_clone_voice_reference_or_null(note_event_optional_voice(source), &voice)) {
        pitch_spelling_destroy(spelling);
        pitch_identity_destroy(pitch);
        duration_destroy(duration);
        temporal_anchor_destroy(anchor);
        return NULL;
    }

    EntityID *id = transform_support_make_derived_id(note_event_id(source), id_suffix);
    if (id == NULL) {
        voice_reference_destroy(voice);
        pitch_spelling_destroy(spelling);
        pitch_identity_destroy(pitch);
        duration_destroy(duration);
        temporal_anchor_destroy(anchor);
        return NULL;
    }

    ProvenanceReference *provenance = transform_support_make_provenance_from(note_event_id(source));
    if (provenance == NULL) {
        entity_id_destroy(id);
        voice_reference_destroy(voice);
        pitch_spelling_destroy(spelling);
        pitch_identity_destroy(pitch);
        duration_destroy(duration);
        temporal_anchor_destroy(anchor);
        return NULL;
    }

    NoteEvent *derived = NULL;
    if (note_event_create(id, anchor, duration, pitch, spelling, voice, provenance, &derived) != NOTE_EVENT_OK) {
        return NULL;
    }
    return derived;
}

/* Builds a new NoteEvent identical to source (fresh copies of every
   field) except a new id/provenance. Returns NULL on any allocation
   failure. */
NoteEvent *transform_support_clone_note_event(const NoteEvent *source, const char *id_suffix) {
    const MusicalTime *source_position = temporal_anchor_position(note_event_temporal_anchor(source));
    MusicalTime *position = NULL;
    if (musical_time_create(musical_time_numerator(source_position), musical_time_denominator(source_position),
                             &position) != MUSICAL_TIME_OK) {
        return NULL;
    }
    TemporalAnchor *anchor = NULL;
    if (temporal_anchor_create(position, &anchor) != TEMPORAL_ANCHOR_OK) {
        musical_time_destroy(position);
        return NULL;
    }

    const Duration *source_duration = note_event_duration(source);
    Duration *duration = NULL;
    if (duration_create(duration_numerator(source_duration), duration_denominator(source_duration), &duration) !=
        DURATION_OK) {
        temporal_anchor_destroy(anchor);
        return NULL;
    }

    PitchIdentity *pitch = transform_support_clone_pitch_identity(note_event_pitch(source));
    if (pitch == NULL) {
        duration_destroy(duration);
        temporal_anchor_destroy(anchor);
        return NULL;
    }

    PitchSpelling *spelling = NULL;
    if (!transform_support_clone_pitch_spelling_or_null(note_event_optional_pitch_spelling(source), &spelling)) {
        pitch_identity_destroy(pitch);
        duration_destroy(duration);
        temporal_anchor_destroy(anchor);
        return NULL;
    }

    VoiceReference *voice = NULL;
    if (!transform_support_clone_voice_reference_or_null(note_event_optional_voice(source), &voice)) {
        pitch_spelling_destroy(spelling);
        pitch_identity_destroy(pitch);
        duration_destroy(duration);
        temporal_anchor_destroy(anchor);
        return NULL;
    }

    EntityID *id = transform_support_make_derived_id(note_event_id(source), id_suffix);
    if (id == NULL) {
        voice_reference_destroy(voice);
        pitch_spelling_destroy(spelling);
        pitch_identity_destroy(pitch);
        duration_destroy(duration);
        temporal_anchor_destroy(anchor);
        return NULL;
    }

    ProvenanceReference *provenance = transform_support_make_provenance_from(note_event_id(source));
    if (provenance == NULL) {
        entity_id_destroy(id);
        voice_reference_destroy(voice);
        pitch_spelling_destroy(spelling);
        pitch_identity_destroy(pitch);
        duration_destroy(duration);
        temporal_anchor_destroy(anchor);
        return NULL;
    }

    NoteEvent *derived = NULL;
    if (note_event_create(id, anchor, duration, pitch, spelling, voice, provenance, &derived) != NOTE_EVENT_OK) {
        return NULL;
    }
    return derived;
}

static bool rational_of(long n, unsigned long d, Rational *out) {
    return d <= (unsigned long)LLONG_MAX && rational_make((long long)n, (long long)d, out) == RATIONAL_OK;
}

static bool rational_to(Rational r, long *out_n, unsigned long *out_d) {
    if (r.num < LONG_MIN || r.num > LONG_MAX || (unsigned long long)r.den > ULONG_MAX) {
        return false;
    }
    *out_n = (long)r.num;
    *out_d = (unsigned long)r.den;
    return true;
}

bool transform_support_rational_add(long n1, unsigned long d1, long n2, unsigned long d2, long *out_n,
                                    unsigned long *out_d) {
    Rational a;
    Rational b;
    Rational sum;
    return rational_of(n1, d1, &a) && rational_of(n2, d2, &b) && rational_add(a, b, &sum) == RATIONAL_OK &&
           rational_to(sum, out_n, out_d);
}

bool transform_support_rational_subtract(long n1, unsigned long d1, long n2, unsigned long d2, long *out_n,
                                         unsigned long *out_d) {
    Rational a;
    Rational b;
    Rational difference;
    return rational_of(n1, d1, &a) && rational_of(n2, d2, &b) && rational_sub(a, b, &difference) == RATIONAL_OK &&
           rational_to(difference, out_n, out_d);
}

bool transform_support_rational_scale(long n, unsigned long d, unsigned long scale_n, unsigned long scale_d,
                                      long *out_n, unsigned long *out_d) {
    Rational a;
    Rational scale;
    Rational product;
    return rational_of(n, d, &a) && scale_n <= (unsigned long)LLONG_MAX &&
           rational_of((long)scale_n, scale_d, &scale) && rational_mul(a, scale, &product) == RATIONAL_OK &&
           rational_to(product, out_n, out_d);
}

bool transform_support_rational_compare(long n1, unsigned long d1, long n2, unsigned long d2, int *out) {
    Rational a;
    Rational b;
    if (!rational_of(n1, d1, &a) || !rational_of(n2, d2, &b)) {
        return false;
    }
    *out = rational_cmp(a, b);
    return true;
}
