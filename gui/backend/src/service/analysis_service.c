#include "analysis_service.h"

#include <dirent.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include <core/entity.h>
#include <core/provenance_reference.h>
#include <core/value.h>
#include <events/note_event.h>
#include <framework.h>
#include <identity/entity_id.h>
#include <identity/schema_version.h>
#include <interpretation/interpretation.h>
#include <io/midi_importer.h>
#include <analysis/chord_identification.h>
#include <analysis/key_estimation.h>
#include <persistence/persistence.h>
#include <persistence/persistence_interpretation.h>
#include <persistence/persistence_note_events.h>
#include <pitch/pitch_numeric_value.h>
#include <semantics/sonority.h>
#include <structure/structure.h>
#include <time/musical_time.h>
#include <time/temporal_anchor.h>
#include <time/time_span.h>

/* Semitone offset from tonic -> diatonic scale degree (1-7), or -1 if
 * the offset is not diatonic in that mode. Ported from ursatz-analyzer's
 * pipeline/chord_pipeline.c (reference only, not linked against). */
static const int kMajorDegreeByOffset[12] = {1, -1, 2, -1, 3, 4, -1, 5, -1, 6, -1, 7};
static const int kMinorDegreeByOffset[12] = {1, -1, 2, 3, -1, 4, -1, 5, 6, -1, 7, -1};

/* JUDGMENT CALL (same one ursatz-analyzer's segmentation.c documents):
 * Ursatz provides no logic for grouping NoteEvents into Sonority-sized
 * chunks. Notes are grouped by exact onset-time equality, matching the
 * common case of a stacked chord. */
typedef struct {
    size_t *note_indices;
    size_t note_index_count;
} SonorityGroup;

typedef struct {
    SonorityGroup *groups;
    size_t group_count;
} SonorityGrouping;

static bool onset_equals(const NoteEvent *a, const NoteEvent *b) {
    const MusicalTime *ta = temporal_anchor_position(note_event_temporal_anchor(a));
    const MusicalTime *tb = temporal_anchor_position(note_event_temporal_anchor(b));
    long long lhs = (long long)musical_time_numerator(ta) * (long long)musical_time_denominator(tb);
    long long rhs = (long long)musical_time_numerator(tb) * (long long)musical_time_denominator(ta);
    return lhs == rhs;
}

static bool segment_by_onset(NoteEvent *const *events, size_t count, SonorityGrouping *out) {
    out->groups = NULL;
    out->group_count = 0;
    if (count == 0) {
        return true;
    }

    SonorityGroup *groups = malloc(sizeof(SonorityGroup) * count);
    if (!groups) {
        return false;
    }
    size_t group_count = 0;

    size_t i = 0;
    while (i < count) {
        size_t j = i + 1;
        while (j < count && onset_equals(events[i], events[j])) {
            j++;
        }
        size_t group_size = j - i;

        size_t *indices = malloc(sizeof(size_t) * group_size);
        if (!indices) {
            for (size_t k = 0; k < group_count; k++) {
                free(groups[k].note_indices);
            }
            free(groups);
            return false;
        }
        for (size_t k = 0; k < group_size; k++) {
            indices[k] = i + k;
        }

        groups[group_count].note_indices = indices;
        groups[group_count].note_index_count = group_size;
        group_count++;
        i = j;
    }

    out->groups = groups;
    out->group_count = group_count;
    return true;
}

static void sonority_grouping_destroy(SonorityGrouping *grouping) {
    for (size_t i = 0; i < grouping->group_count; i++) {
        free(grouping->groups[i].note_indices);
    }
    free(grouping->groups);
    grouping->groups = NULL;
    grouping->group_count = 0;
}

static int pitch_class_of(long midi_note_number) {
    long remainder = midi_note_number % 12;
    if (remainder < 0) {
        remainder += 12;
    }
    return (int)remainder;
}

typedef struct {
    bool found;
    int tonic_pitch_class;
    bool is_major;
} KeyEstimate;

static Structure *build_whole_piece_region(NoteEvent *const *events, size_t count) {
    EntityID *region_id;
    if (entity_id_create("ursatz-gui-analyzer-region-whole-piece", &region_id) != ENTITY_ID_OK) {
        return NULL;
    }
    SchemaVersion *schema_version;
    if (schema_version_create(1, &schema_version) != SCHEMA_VERSION_OK) {
        entity_id_destroy(region_id);
        return NULL;
    }
    ProvenanceReference *provenance;
    if (provenance_reference_create_absent(&provenance) != PROVENANCE_REFERENCE_OK) {
        schema_version_destroy(schema_version);
        entity_id_destroy(region_id);
        return NULL;
    }
    Entity *entity;
    if (entity_create(region_id, "Structure", schema_version, NULL, provenance, &entity) != ENTITY_OK) {
        provenance_reference_destroy(provenance);
        schema_version_destroy(schema_version);
        entity_id_destroy(region_id);
        return NULL;
    }

    EntityID **event_ids = malloc(sizeof(EntityID *) * count);
    if (!event_ids) {
        entity_destroy(entity);
        return NULL;
    }
    for (size_t i = 0; i < count; i++) {
        if (entity_id_create(entity_id_value(note_event_id(events[i])), &event_ids[i]) != ENTITY_ID_OK) {
            for (size_t k = 0; k < i; k++) {
                entity_id_destroy(event_ids[k]);
            }
            free(event_ids);
            entity_destroy(entity);
            return NULL;
        }
    }

    Structure *region;
    if (structure_create(entity, event_ids, count, &region) != STRUCTURE_OK) {
        for (size_t i = 0; i < count; i++) {
            entity_id_destroy(event_ids[i]);
        }
        free(event_ids);
        entity_destroy(entity);
        return NULL;
    }
    free(event_ids);
    return region;
}

/* Estimates one global key for the whole piece, persisting the winning
 * Interpretation (if any) directly. Mirrors ursatz-analyzer's
 * estimate_global_key (pipeline/key_pipeline.c). */
static KeyEstimate estimate_and_store_global_key(PersistenceHandle *handle, NoteEvent *const *events,
                                                   PitchNumericValue *const *pitch_values, size_t count) {
    KeyEstimate estimate = {false, 0, false};
    if (count == 0) {
        return estimate;
    }

    Structure *region = build_whole_piece_region(events, count);
    if (!region) {
        return estimate;
    }

    const Framework *framework = common_practice_minimal_framework();
    if (!framework) {
        structure_destroy(region);
        return estimate;
    }

    static const char *const kPitchClassNames[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

    int *relative = malloc(sizeof(int) * count);
    if (!relative) {
        structure_destroy(region);
        return estimate;
    }

    for (int tonic = 0; tonic < 12 && !estimate.found; tonic++) {
        for (size_t i = 0; i < count; i++) {
            long note_number = pitch_numeric_value_position_numerator(pitch_values[i]);
            int pitch_class = pitch_class_of(note_number);
            relative[i] = ((pitch_class - tonic) % 12 + 12) % 12;
        }

        KeyEstimationInput input = {region, framework, kPitchClassNames[tonic], relative, count};
        AnalysisResult *result = NULL;
        if (key_estimation_analyze(&input, NULL, NULL, &result) == ANALYZER_OK) {
            const Interpretation *interpretation = analysis_result_interpretation_at(result, 0);
            size_t claim_size;
            const char *claim = (const char *)value_content(interpretation_claim(interpretation), &claim_size);

            if (persistence_store_interpretation(handle, interpretation) == PERSISTENCE_INTERPRETATION_OK) {
                estimate.tonic_pitch_class = tonic;
                estimate.is_major = (strstr(claim, "major") != NULL);
                estimate.found = true;
            }
            analysis_result_destroy(result);
        }
    }

    free(relative);
    structure_destroy(region);
    return estimate;
}

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

/* Identifies and stores the chord Interpretation (if any) for one
 * onset-grouped SonorityGroup. Mirrors ursatz-analyzer's
 * identify_chord_for_group (pipeline/chord_pipeline.c). */
static void identify_and_store_chord(PersistenceHandle *handle, const SonorityGroup *group, NoteEvent *const *events,
                                       PitchNumericValue *const *pitch_values, const KeyEstimate *key) {
    if (!key->found) {
        return;
    }

    const int *degree_table = key->is_major ? kMajorDegreeByOffset : kMinorDegreeByOffset;

    int *degrees = malloc(sizeof(int) * group->note_index_count);
    if (!degrees) {
        return;
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
        return;
    }

    Sonority *sonority = build_sonority(group, events);
    if (!sonority) {
        free(degrees);
        return;
    }

    const Framework *framework = common_practice_minimal_framework();
    if (framework) {
        ChordIdentificationInput input = {sonority, framework, degrees, degree_count};
        AnalysisResult *result = NULL;
        if (chord_identification_analyze(&input, NULL, NULL, &result) == ANALYZER_OK) {
            const Interpretation *interpretation = analysis_result_interpretation_at(result, 0);
            persistence_store_interpretation(handle, interpretation);
            analysis_result_destroy(result);
        }
    }

    sonority_destroy(sonority);
    free(degrees);
}

static bool ensure_dir_exists(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0) {
        return true;
    }
    return mkdir(path, 0755) == 0;
}

static char *generate_piece_id(void) {
    char *id = malloc(64);
    if (!id) {
        return NULL;
    }
    snprintf(id, 64, "piece-%ld-%d", (long)time(NULL), rand());
    return id;
}

UrsatzGuiAnalysisResult ursatz_gui_analysis_import_and_save(const char *pieces_dir, const unsigned char *midi_data,
                                                              size_t midi_size, char **out_id) {
    NoteEvent **events = NULL;
    PitchNumericValue **pitch_values = NULL;
    size_t count = 0;
    if (midi_importer_import(midi_data, midi_size, &events, &pitch_values, &count) != MIDI_IMPORT_OK) {
        return URSATZ_GUI_ANALYSIS_ERR_IMPORT_FAILED;
    }

    if (!ensure_dir_exists(pieces_dir)) {
        for (size_t i = 0; i < count; i++) {
            note_event_destroy(events[i]);
            pitch_numeric_value_destroy(pitch_values[i]);
        }
        free(events);
        free(pitch_values);
        return URSATZ_GUI_ANALYSIS_ERR_PERSISTENCE_FAILED;
    }

    char *id = generate_piece_id();
    if (!id) {
        for (size_t i = 0; i < count; i++) {
            note_event_destroy(events[i]);
            pitch_numeric_value_destroy(pitch_values[i]);
        }
        free(events);
        free(pitch_values);
        return URSATZ_GUI_ANALYSIS_ERR_ALLOC;
    }

    char db_path[320];
    snprintf(db_path, sizeof(db_path), "%s/%s.db", pieces_dir, id);

    PersistenceHandle *handle = NULL;
    UrsatzGuiAnalysisResult result = URSATZ_GUI_ANALYSIS_OK;
    if (persistence_open(db_path, &handle) != PERSISTENCE_OK) {
        result = URSATZ_GUI_ANALYSIS_ERR_PERSISTENCE_FAILED;
    } else {
        if (persistence_store_note_events(handle, events, count) != PERSISTENCE_NOTE_EVENTS_OK) {
            result = URSATZ_GUI_ANALYSIS_ERR_PERSISTENCE_FAILED;
        } else {
            KeyEstimate key = estimate_and_store_global_key(handle, events, pitch_values, count);

            SonorityGrouping grouping;
            if (segment_by_onset(events, count, &grouping)) {
                for (size_t i = 0; i < grouping.group_count; i++) {
                    identify_and_store_chord(handle, &grouping.groups[i], events, pitch_values, &key);
                }
                sonority_grouping_destroy(&grouping);
            }
        }
        persistence_close(handle);
    }

    for (size_t i = 0; i < count; i++) {
        note_event_destroy(events[i]);
        pitch_numeric_value_destroy(pitch_values[i]);
    }
    free(events);
    free(pitch_values);

    if (result != URSATZ_GUI_ANALYSIS_OK) {
        free(id);
        return result;
    }

    *out_id = id;
    return URSATZ_GUI_ANALYSIS_OK;
}

UrsatzGuiAnalysisResult ursatz_gui_analysis_list(const char *pieces_dir, char ***out_ids, size_t *out_count) {
    *out_ids = NULL;
    *out_count = 0;

    DIR *dir = opendir(pieces_dir);
    if (!dir) {
        return URSATZ_GUI_ANALYSIS_OK;
    }

    size_t count = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        size_t len = strlen(entry->d_name);
        if (len > 3 && strcmp(entry->d_name + len - 3, ".db") == 0) {
            count++;
        }
    }
    rewinddir(dir);

    if (count == 0) {
        closedir(dir);
        return URSATZ_GUI_ANALYSIS_OK;
    }

    char **ids = malloc(sizeof(char *) * count);
    if (!ids) {
        closedir(dir);
        return URSATZ_GUI_ANALYSIS_ERR_ALLOC;
    }

    size_t i = 0;
    while ((entry = readdir(dir)) != NULL && i < count) {
        size_t len = strlen(entry->d_name);
        if (len > 3 && strcmp(entry->d_name + len - 3, ".db") == 0) {
            size_t id_len = len - 3;
            ids[i] = malloc(id_len + 1);
            if (!ids[i]) {
                for (size_t k = 0; k < i; k++) {
                    free(ids[k]);
                }
                free(ids);
                closedir(dir);
                return URSATZ_GUI_ANALYSIS_ERR_ALLOC;
            }
            memcpy(ids[i], entry->d_name, id_len);
            ids[i][id_len] = '\0';
            i++;
        }
    }
    closedir(dir);

    *out_ids = ids;
    *out_count = count;
    return URSATZ_GUI_ANALYSIS_OK;
}
