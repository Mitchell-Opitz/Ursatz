#include "pipeline/chord_pipeline.h"

#include <stdlib.h>
#include <string.h>

#include "analysis/chord_identification.h"
#include "framework.h"
#include "identity/entity_id.h"
#include "pipeline/pitch_class_util.h"
#include "semantics/sonority.h"
#include "time/musical_time.h"
#include "time/temporal_anchor.h"
#include "time/time_span.h"

/* Semitone offset from tonic -> diatonic scale degree (1-7), or -1 if
   the offset is not diatonic in that mode. */
static const int MAJOR_DEGREE_BY_OFFSET[12] = {1, -1, 2, -1, 3, 4, -1, 5, -1, 6, -1, 7};
static const int MINOR_DEGREE_BY_OFFSET[12] = {1, -1, 2, 3, -1, 4, -1, 5, 6, -1, 7, -1};

static Sonority *build_sonority(const SonorityGroup *group, NoteEvent *const *events) {
    const NoteEvent *first = events[group->note_indices[0]];
    const MusicalTime *onset = temporal_anchor_position(note_event_temporal_anchor(first));

    MusicalTime *start_copy;
    if (musical_time_create(musical_time_numerator(onset), musical_time_denominator(onset), &start_copy) !=
        MUSICAL_TIME_OK) {
        return NULL;
    }
    MusicalTime *end_copy;
    if (musical_time_create(musical_time_numerator(onset), musical_time_denominator(onset), &end_copy) !=
        MUSICAL_TIME_OK) {
        musical_time_destroy(start_copy);
        return NULL;
    }
    TimeSpan *window;
    if (time_span_create(start_copy, end_copy, TIME_SPAN_BOUNDARY_LEFT_CLOSED, &window) != TIME_SPAN_OK) {
        musical_time_destroy(start_copy);
        musical_time_destroy(end_copy);
        return NULL;
    }
    musical_time_destroy(start_copy);
    musical_time_destroy(end_copy);

    EntityID **event_ids = malloc(sizeof(EntityID *) * group->note_index_count);
    if (!event_ids) {
        time_span_destroy(window);
        return NULL;
    }
    for (size_t i = 0; i < group->note_index_count; i++) {
        const NoteEvent *note = events[group->note_indices[i]];
        if (entity_id_create(entity_id_value(note_event_id(note)), &event_ids[i]) != ENTITY_ID_OK) {
            for (size_t k = 0; k < i; k++) {
                entity_id_destroy(event_ids[k]);
            }
            free(event_ids);
            time_span_destroy(window);
            return NULL;
        }
    }

    Sonority *sonority;
    if (sonority_create(window, event_ids, group->note_index_count, NULL, 0, SONORITY_EVENT_DOMAIN_SOUNDING,
                         &sonority) != SONORITY_OK) {
        for (size_t i = 0; i < group->note_index_count; i++) {
            entity_id_destroy(event_ids[i]);
        }
        free(event_ids);
        time_span_destroy(window);
        return NULL;
    }
    free(event_ids);
    return sonority;
}

ChordEstimate identify_chord_for_group(const SonorityGroup *group, NoteEvent *const *events,
                                        PitchNumericValue *const *pitch_values, const KeyEstimate *key) {
    ChordEstimate estimate = {false, ""};
    if (!key || !key->found) {
        return estimate;
    }

    const int *degree_table = key->is_major ? MAJOR_DEGREE_BY_OFFSET : MINOR_DEGREE_BY_OFFSET;

    int *degrees = malloc(sizeof(int) * group->note_index_count);
    if (!degrees) {
        return estimate;
    }
    size_t degree_count = 0;
    for (size_t i = 0; i < group->note_index_count; i++) {
        long note_number = pitch_numeric_value_position_numerator(pitch_values[group->note_indices[i]]);
        int pitch_class = pitch_class_of(note_number);
        int offset = ((pitch_class - key->tonic_pitch_class) % 12 + 12) % 12;
        int degree = degree_table[offset];
        if (degree != -1) {
            degrees[degree_count++] = degree;
        }
    }

    if (degree_count == 0) {
        free(degrees);
        return estimate;
    }

    Sonority *sonority = build_sonority(group, events);
    if (!sonority) {
        free(degrees);
        return estimate;
    }

    const Framework *framework = common_practice_minimal_framework();
    if (framework) {
        ChordIdentificationInput input = {sonority, framework, degrees, degree_count};
        AnalysisResult *result = NULL;
        if (chord_identification_analyze(&input, NULL, NULL, &result) == ANALYZER_OK) {
            const Interpretation *interpretation = analysis_result_interpretation_at(result, 0);
            size_t claim_size;
            const char *claim = (const char *)value_content(interpretation_claim(interpretation), &claim_size);
            strncpy(estimate.label, claim, sizeof(estimate.label) - 1);
            estimate.label[sizeof(estimate.label) - 1] = '\0';
            estimate.found = true;
            analysis_result_destroy(result);
        }
    }

    sonority_destroy(sonority);
    free(degrees);
    return estimate;
}
