#include "transform/retrograde.h"

#include <stdbool.h>
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

struct Retrograde {
    Transformation *transformation;
    unsigned reversed_dimensions;
};

typedef struct {
    RetrogradeDimension flag;
    const char *name;
} DimensionName;

static const DimensionName kDimensionNames[] = {
    {RETROGRADE_DIMENSION_EVENT_ORDER, "event order"},
    {RETROGRADE_DIMENSION_PITCH, "pitch"},
    {RETROGRADE_DIMENSION_RHYTHM, "rhythm"},
    {RETROGRADE_DIMENSION_ARTICULATION, "articulation"},
};
static const size_t kDimensionCount = sizeof(kDimensionNames) / sizeof(kDimensionNames[0]);

/* Writes a comma-separated listing of the flags in mask (or fallback if none) into a heap-allocated buffer. */
static char *describe_dimensions(unsigned mask, const char *fallback_if_empty) {
    char buffer[128];
    buffer[0] = '\0';
    size_t written = 0;
    bool first = true;

    for (size_t i = 0; i < kDimensionCount; i++) {
        if ((mask & kDimensionNames[i].flag) == 0) {
            continue;
        }
        const char *prefix = first ? "" : ", ";
        int n = snprintf(buffer + written, sizeof(buffer) - written, "%s%s", prefix, kDimensionNames[i].name);
        if (n < 0) {
            break;
        }
        written += (size_t)n;
        first = false;
    }

    if (first) {
        size_t len = strlen(fallback_if_empty);
        char *copy = (char *)malloc(len + 1);
        if (copy != NULL) {
            memcpy(copy, fallback_if_empty, len + 1);
        }
        return copy;
    }

    size_t len = strlen(buffer);
    char *copy = (char *)malloc(len + 1);
    if (copy != NULL) {
        memcpy(copy, buffer, len + 1);
    }
    return copy;
}

RetrogradeResult retrograde_create(EntityID *id, unsigned reversed_dimensions, ProvenanceReference *provenance,
                                    Retrograde **out) {
    if (id == NULL) {
        return RETROGRADE_ERR_NULL_ID;
    }
    if (reversed_dimensions == 0) {
        return RETROGRADE_ERR_NO_DIMENSIONS;
    }
    if ((reversed_dimensions & ~(unsigned)RETROGRADE_DIMENSION_ALL) != 0) {
        return RETROGRADE_ERR_UNKNOWN_DIMENSION_BITS;
    }
    if (provenance == NULL) {
        return RETROGRADE_ERR_NULL_PROVENANCE;
    }

    Retrograde *retrograde = (Retrograde *)malloc(sizeof(Retrograde));
    if (retrograde == NULL) {
        return RETROGRADE_ERR_ALLOC;
    }

    char *modified_dimensions = describe_dimensions(reversed_dimensions, "");
    if (modified_dimensions == NULL) {
        free(retrograde);
        return RETROGRADE_ERR_ALLOC;
    }

    char *preserved_dimensions = describe_dimensions(~reversed_dimensions & (unsigned)RETROGRADE_DIMENSION_ALL,
                                                       "none (all reversed)");
    if (preserved_dimensions == NULL) {
        free(modified_dimensions);
        free(retrograde);
        return RETROGRADE_ERR_ALLOC;
    }

    Transformation *transformation;
    TransformationResult transformation_result =
        transformation_create(id, "Structure", NULL, "Structure", preserved_dimensions, modified_dimensions,
                               "one-to-one, reversal applied over the sequence's ordered material",
                               "reversed dimension(s) explicitly declared by caller, never defaulted", true,
                               provenance, &transformation);
    free(modified_dimensions);
    free(preserved_dimensions);
    if (transformation_result != TRANSFORMATION_OK) {
        free(retrograde);
        return RETROGRADE_ERR_ALLOC;
    }

    retrograde->transformation = transformation;
    retrograde->reversed_dimensions = reversed_dimensions;

    *out = retrograde;
    return RETROGRADE_OK;
}

void retrograde_destroy(Retrograde *retrograde) {
    if (retrograde == NULL) {
        return;
    }
    transformation_destroy(retrograde->transformation);
    free(retrograde);
}

const EntityID *retrograde_id(const Retrograde *retrograde) {
    return transformation_id(retrograde->transformation);
}

const Transformation *retrograde_transformation(const Retrograde *retrograde) {
    return retrograde->transformation;
}

unsigned retrograde_reversed_dimensions(const Retrograde *retrograde) {
    return retrograde->reversed_dimensions;
}

bool retrograde_identity_equals(const Retrograde *a, const Retrograde *b) {
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

static void rational_add(long n1, unsigned long d1, long n2, unsigned long d2, long *out_n, unsigned long *out_d) {
    *out_n = n1 * (long)d2 + n2 * (long)d1;
    *out_d = d1 * d2;
    rational_reduce(out_n, out_d);
}

static void rational_subtract(long n1, unsigned long d1, long n2, unsigned long d2, long *out_n,
                               unsigned long *out_d) {
    *out_n = n1 * (long)d2 - n2 * (long)d1;
    *out_d = d1 * d2;
    rational_reduce(out_n, out_d);
}

static int rational_compare(long n1, unsigned long d1, long n2, unsigned long d2) {
    long lhs = n1 * (long)d2;
    long rhs = n2 * (long)d1;
    if (lhs < rhs) {
        return -1;
    }
    if (lhs > rhs) {
        return 1;
    }
    return 0;
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

/* Builds a new NoteEvent identical to source except for temporal_anchor
   (a fresh MusicalTime at new_onset_numerator/new_onset_denominator).
   Returns NULL on any allocation failure. */
static NoteEvent *clone_note_event_with_new_onset(const NoteEvent *source, long new_onset_numerator,
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

    PitchIdentity *pitch = clone_pitch_identity(note_event_pitch(source));
    if (pitch == NULL) {
        duration_destroy(duration);
        temporal_anchor_destroy(anchor);
        return NULL;
    }

    PitchSpelling *spelling = NULL;
    if (!clone_pitch_spelling_or_null(note_event_optional_pitch_spelling(source), &spelling)) {
        pitch_identity_destroy(pitch);
        duration_destroy(duration);
        temporal_anchor_destroy(anchor);
        return NULL;
    }

    VoiceReference *voice = NULL;
    if (!clone_voice_reference_or_null(note_event_optional_voice(source), &voice)) {
        pitch_spelling_destroy(spelling);
        pitch_identity_destroy(pitch);
        duration_destroy(duration);
        temporal_anchor_destroy(anchor);
        return NULL;
    }

    EntityID *id = make_derived_id(note_event_id(source), id_suffix);
    if (id == NULL) {
        voice_reference_destroy(voice);
        pitch_spelling_destroy(spelling);
        pitch_identity_destroy(pitch);
        duration_destroy(duration);
        temporal_anchor_destroy(anchor);
        return NULL;
    }

    ProvenanceReference *provenance = make_provenance_from(note_event_id(source));
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

RetrogradeApplyResult retrograde_apply(const Retrograde *retrograde, NoteEvent **events, size_t event_count,
                                        TransformResult **out) {
    if (retrograde == NULL) {
        return RETROGRADE_APPLY_ERR_NULL_RETROGRADE;
    }
    if (events == NULL) {
        return RETROGRADE_APPLY_ERR_NULL_EVENTS;
    }
    if (event_count == 0) {
        return RETROGRADE_APPLY_ERR_EMPTY;
    }
    if ((retrograde->reversed_dimensions & ~(unsigned)RETROGRADE_DIMENSION_EVENT_ORDER) != 0) {
        return RETROGRADE_APPLY_ERR_DIMENSION_NOT_IMPLEMENTED;
    }

    long earliest_onset_n = musical_time_numerator(temporal_anchor_position(note_event_temporal_anchor(events[0])));
    unsigned long earliest_onset_d =
        musical_time_denominator(temporal_anchor_position(note_event_temporal_anchor(events[0])));
    long latest_end_n = 0;
    unsigned long latest_end_d = 1;
    bool latest_end_set = false;

    for (size_t i = 0; i < event_count; i++) {
        const MusicalTime *position = temporal_anchor_position(note_event_temporal_anchor(events[i]));
        long onset_n = musical_time_numerator(position);
        unsigned long onset_d = musical_time_denominator(position);
        if (rational_compare(onset_n, onset_d, earliest_onset_n, earliest_onset_d) < 0) {
            earliest_onset_n = onset_n;
            earliest_onset_d = onset_d;
        }

        const Duration *duration = note_event_duration(events[i]);
        long end_n = 0;
        unsigned long end_d = 1;
        rational_add(onset_n, onset_d, duration_numerator(duration), duration_denominator(duration), &end_n, &end_d);
        if (!latest_end_set || rational_compare(end_n, end_d, latest_end_n, latest_end_d) > 0) {
            latest_end_n = end_n;
            latest_end_d = end_d;
            latest_end_set = true;
        }
    }

    NoteEvent **derived = (NoteEvent **)malloc(sizeof(NoteEvent *) * event_count);
    if (derived == NULL) {
        return RETROGRADE_APPLY_ERR_ALLOC;
    }

    long span_n = 0;
    unsigned long span_d = 1;
    rational_add(earliest_onset_n, earliest_onset_d, latest_end_n, latest_end_d, &span_n, &span_d);

    for (size_t i = 0; i < event_count; i++) {
        const MusicalTime *position = temporal_anchor_position(note_event_temporal_anchor(events[i]));
        long onset_n = musical_time_numerator(position);
        unsigned long onset_d = musical_time_denominator(position);
        const Duration *duration = note_event_duration(events[i]);
        long end_n = 0;
        unsigned long end_d = 1;
        rational_add(onset_n, onset_d, duration_numerator(duration), duration_denominator(duration), &end_n, &end_d);

        long new_onset_n = 0;
        unsigned long new_onset_d = 1;
        rational_subtract(span_n, span_d, end_n, end_d, &new_onset_n, &new_onset_d);

        derived[i] = clone_note_event_with_new_onset(events[i], new_onset_n, new_onset_d, "-retrograde");
        if (derived[i] == NULL) {
            for (size_t j = 0; j < i; j++) {
                note_event_destroy(derived[j]);
            }
            free(derived);
            return RETROGRADE_APPLY_ERR_ALLOC;
        }
    }

    TransformResultResult result = transform_result_create(derived, event_count, out);
    if (result != TRANSFORM_RESULT_OK) {
        for (size_t i = 0; i < event_count; i++) {
            note_event_destroy(derived[i]);
        }
        free(derived);
        return RETROGRADE_APPLY_ERR_ALLOC;
    }
    free(derived);
    return RETROGRADE_APPLY_OK;
}