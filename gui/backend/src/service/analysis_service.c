#include "analysis_service.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

#include <sqlite3.h>

#include <core/entity.h>
#include <core/provenance_reference.h>
#include <core/value.h>
#include <events/note_event.h>
#include <framework.h>
#include <identity/entity_id.h>
#include <identity/schema_version.h>
#include <interpretation/interpretation.h>
#include <io/midi_importer.h>
#include <analysis/cadence_classification.h>
#include <analysis/chord_identification.h>
#include <analysis/key_estimation.h>
#include <persistence/persistence.h>
#include <persistence/persistence_interpretation.h>
#include <persistence/persistence_note_events.h>
#include <pitch/pitch_numeric_value.h>
#include <relationship/relationship.h>
#include <semantics/sonority.h>
#include <structure/structure.h>
#include <time/musical_time.h>
#include <time/temporal_anchor.h>
#include <time/time_span.h>

/*
 * ursatz-gui-owned interpretation index: a small table, in the same
 * per-piece SQLite file Ursatz's PersistenceHandle manages, that records
 * which Interpretation ids this module produced and their kind ("key" or
 * "chord"). Ursatz's persistence layer has no way to enumerate the
 * Interpretations stored for a piece (only load-by-id), so this index is
 * what lets the detail route (GET /api/analyzer/pieces/<id>) know which
 * ids to load. This is ursatz-gui's own schema, opened via a separate
 * sqlite3 connection to db_path -- entirely outside Ursatz's schema/
 * contract, never touching PersistenceHandle's internals.
 */
static const char *const kIndexTableName = "ursatz_gui_interpretations";
static const char *const kIndexTypeKey = "key";
static const char *const kIndexTypeChord = "chord";
static const char *const kIndexTypeCadence = "cadence";

/*
 * ursatz-gui-owned piece metadata table, in the same per-piece SQLite
 * file as kIndexTableName above. Currently holds only the original
 * upload filename; a single row keyed by a fixed id.
 */
static const char *const kMetaTableName = "ursatz_gui_piece_meta";

/*
 * ursatz-gui-owned relationship table, in the same per-piece SQLite file
 * as kIndexTableName/kMetaTableName above. Ursatz's persistence layer
 * (persistence/persistence.h) has no store/load path for Relationship
 * (relationship/relationship.h) at all -- unlike Interpretation, there is
 * no persistence_store_relationship anywhere in Ursatz to delegate to.
 * Rather than block Period/Sentence/Form/Thematic Return Detection's
 * Relationship-shaped output on a real Ursatz RelationshipRepository
 * (Registry: Not Started), this mirrors the same stopgap pattern
 * kIndexTableName already uses for Ursatz's other persistence gap
 * (enumerating stored Interpretations): a small ursatz-gui-owned table
 * recording each Relationship's (source, predicate, target) tuple by its
 * own id, opened via the same separate sqlite3 connection to db_path.
 */
static const char *const kRelationshipTableName = "ursatz_gui_relationships";

static char *dup_string(const char *value);

/* Opens (creating if absent) the ursatz-gui interpretation index table in
 * the SQLite file at db_path. Returns NULL on failure. */
static sqlite3 *open_interpretation_index(const char *db_path) {
    sqlite3 *db = NULL;
    if (sqlite3_open(db_path, &db) != SQLITE_OK) {
        if (db) {
            sqlite3_close(db);
        }
        return NULL;
    }

    char sql[256];
    snprintf(sql, sizeof(sql),
             "CREATE TABLE IF NOT EXISTS %s ("
             "  interpretation_id TEXT PRIMARY KEY,"
             "  interpretation_type TEXT NOT NULL"
             ");",
             kIndexTableName);
    if (sqlite3_exec(db, sql, NULL, NULL, NULL) != SQLITE_OK) {
        sqlite3_close(db);
        return NULL;
    }

    snprintf(sql, sizeof(sql),
             "CREATE TABLE IF NOT EXISTS %s ("
             "  id INTEGER PRIMARY KEY CHECK (id = 0),"
             "  original_filename TEXT NOT NULL"
             ");",
             kMetaTableName);
    if (sqlite3_exec(db, sql, NULL, NULL, NULL) != SQLITE_OK) {
        sqlite3_close(db);
        return NULL;
    }

    snprintf(sql, sizeof(sql),
             "CREATE TABLE IF NOT EXISTS %s ("
             "  relationship_id TEXT PRIMARY KEY,"
             "  source_id TEXT NOT NULL,"
             "  predicate_name TEXT NOT NULL,"
             "  target_id TEXT NOT NULL"
             ");",
             kRelationshipTableName);
    if (sqlite3_exec(db, sql, NULL, NULL, NULL) != SQLITE_OK) {
        sqlite3_close(db);
        return NULL;
    }

    return db;
}

/* Records original_filename ("" if NULL) for this piece. Best-effort,
 * mirroring record_interpretation_id: a failure here does not fail the
 * analysis, only the display of the filename later. */
static void record_original_filename(sqlite3 *index_db, const char *original_filename) {
    if (!index_db) {
        return;
    }

    char sql[160];
    snprintf(sql, sizeof(sql), "INSERT OR REPLACE INTO %s (id, original_filename) VALUES (0, ?);", kMetaTableName);

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(index_db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return;
    }
    sqlite3_bind_text(stmt, 1, original_filename ? original_filename : "", -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

/* Reads the original filename recorded by record_original_filename, or
 * "" if none was recorded. Returns a newly allocated, caller-owned
 * string, or NULL only on allocation failure. */
static char *read_original_filename(sqlite3 *index_db) {
    if (!index_db) {
        return dup_string("");
    }

    char sql[128];
    snprintf(sql, sizeof(sql), "SELECT original_filename FROM %s WHERE id = 0;", kMetaTableName);

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(index_db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return dup_string("");
    }

    char *filename = NULL;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        filename = dup_string((const char *)sqlite3_column_text(stmt, 0));
    }
    sqlite3_finalize(stmt);

    return filename ? filename : dup_string("");
}

/* Records that interpretation_id (of the given type) was produced for
 * this piece. Best-effort: a failure here does not fail the analysis --
 * the Interpretation is already durably stored by Ursatz's own
 * persistence; only the index entry (used for later listing) is lost. */
static void record_interpretation_id(sqlite3 *index_db, const char *interpretation_id, const char *type) {
    if (!index_db) {
        return;
    }

    char sql[256];
    snprintf(sql, sizeof(sql), "INSERT OR REPLACE INTO %s (interpretation_id, interpretation_type) VALUES (?, ?);",
             kIndexTableName);

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(index_db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return;
    }
    sqlite3_bind_text(stmt, 1, interpretation_id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, type, -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

/* Records one Relationship's (source, predicate, target) tuple by its own
 * id. Best-effort, mirroring record_interpretation_id: this is the only
 * durable storage a Relationship built by this module gets (Ursatz's
 * PersistenceHandle has no Relationship store), so a failure here loses
 * the Relationship entirely -- but never fails the overall analysis. */
static void record_relationship(sqlite3 *index_db, const Relationship *relationship) {
    if (!index_db || !relationship) {
        return;
    }

    char sql[256];
    snprintf(sql, sizeof(sql),
             "INSERT OR REPLACE INTO %s (relationship_id, source_id, predicate_name, target_id) "
             "VALUES (?, ?, ?, ?);",
             kRelationshipTableName);

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(index_db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return;
    }
    sqlite3_bind_text(stmt, 1, entity_id_value(relationship_id(relationship)), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, entity_id_value(relationship_source_id(relationship)), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, relationship_predicate_name(relationship), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, entity_id_value(relationship_target_id(relationship)), -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

/* Records every Relationship in relationships[0..count). Convenience
 * wrapper around record_relationship for the analyzers that produce
 * several Relationships per call (Sentence/Form Structure Detection). */
static void record_relationships(sqlite3 *index_db, Relationship *const *relationships, size_t count) {
    for (size_t i = 0; i < count; i++) {
        record_relationship(index_db, relationships[i]);
    }
}

typedef struct {
    char *relationship_id;
    char *source_id;
    char *predicate_name;
    char *target_id;
} UrsatzGuiRelationshipRow;

static void relationship_row_destroy_contents(UrsatzGuiRelationshipRow *row) {
    free(row->relationship_id);
    free(row->source_id);
    free(row->predicate_name);
    free(row->target_id);
}

/* Reads every relationship row recorded by record_relationship. Returns
 * false only on allocation failure; a missing/empty table yields
 * *out_count == 0. Mirrors read_interpretation_index's shape. */
static bool read_relationship_index(sqlite3 *index_db, UrsatzGuiRelationshipRow **out_rows, size_t *out_count) {
    *out_rows = NULL;
    *out_count = 0;

    char sql[160];
    snprintf(sql, sizeof(sql), "SELECT relationship_id, source_id, predicate_name, target_id FROM %s;",
             kRelationshipTableName);

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(index_db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return true;
    }

    UrsatzGuiRelationshipRow *rows = NULL;
    size_t count = 0;
    size_t capacity = 0;

    int step;
    while ((step = sqlite3_step(stmt)) == SQLITE_ROW) {
        if (count == capacity) {
            size_t new_capacity = (capacity == 0) ? 8 : (capacity * 2);
            UrsatzGuiRelationshipRow *grown = realloc(rows, sizeof(UrsatzGuiRelationshipRow) * new_capacity);
            if (!grown) {
                for (size_t i = 0; i < count; i++) {
                    relationship_row_destroy_contents(&rows[i]);
                }
                free(rows);
                sqlite3_finalize(stmt);
                return false;
            }
            rows = grown;
            capacity = new_capacity;
        }

        UrsatzGuiRelationshipRow row = {0};
        row.relationship_id = dup_string((const char *)sqlite3_column_text(stmt, 0));
        row.source_id = dup_string((const char *)sqlite3_column_text(stmt, 1));
        row.predicate_name = dup_string((const char *)sqlite3_column_text(stmt, 2));
        row.target_id = dup_string((const char *)sqlite3_column_text(stmt, 3));
        if (!row.relationship_id || !row.source_id || !row.predicate_name || !row.target_id) {
            relationship_row_destroy_contents(&row);
            for (size_t i = 0; i < count; i++) {
                relationship_row_destroy_contents(&rows[i]);
            }
            free(rows);
            sqlite3_finalize(stmt);
            return false;
        }
        rows[count++] = row;
    }
    sqlite3_finalize(stmt);

    if (step != SQLITE_DONE) {
        for (size_t i = 0; i < count; i++) {
            relationship_row_destroy_contents(&rows[i]);
        }
        free(rows);
        return true;
    }

    *out_rows = rows;
    *out_count = count;
    return true;
}

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
static KeyEstimate estimate_and_store_global_key(PersistenceHandle *handle, sqlite3 *index_db,
                                                   NoteEvent *const *events, PitchNumericValue *const *pitch_values,
                                                   size_t count) {
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
                record_interpretation_id(index_db, entity_id_value(interpretation_id(interpretation)),
                                          kIndexTypeKey);
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
static void identify_and_store_chord(PersistenceHandle *handle, sqlite3 *index_db, const SonorityGroup *group,
                                       NoteEvent *const *events, PitchNumericValue *const *pitch_values,
                                       const KeyEstimate *key) {
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
            if (persistence_store_interpretation(handle, interpretation) == PERSISTENCE_INTERPRETATION_OK) {
                record_interpretation_id(index_db, entity_id_value(interpretation_id(interpretation)),
                                          kIndexTypeChord);
            }
            analysis_result_destroy(result);
        }
    }

    sonority_destroy(sonority);
    free(degrees);
}

/* Diatonic scale degree (1-7) of the lowest-pitched note in group, or -1
 * if none is diatonic in key. SCOPED BRANCH DEFAULT (not Registry-
 * settled, same "flagged default" treatment the analyzer headers
 * themselves use for their own caller-supplied dimensions): Cadence
 * Classification needs a single root scale degree per chord, but
 * ChordIdentification's degrees array (as already computed by
 * identify_and_store_chord) has no notion of "the root" -- this treats
 * the lowest sounding note's degree as the chord's root, a common
 * bass-note heuristic, not a harmonic-inversion analysis. */
static int group_root_degree(const SonorityGroup *group, PitchNumericValue *const *pitch_values,
                              const KeyEstimate *key) {
    if (!key->found) {
        return -1;
    }
    const int *degree_table = key->is_major ? kMajorDegreeByOffset : kMinorDegreeByOffset;

    long best_note = 0;
    int best_degree = -1;
    bool have_best = false;
    for (size_t i = 0; i < group->note_index_count; i++) {
        long note_number = pitch_numeric_value_position_numerator(pitch_values[group->note_indices[i]]);
        int pitch_class = pitch_class_of(note_number);
        int offset = ((pitch_class - key->tonic_pitch_class) % 12 + 12) % 12;
        int degree = degree_table[offset];
        if (degree == -1) {
            continue;
        }
        if (!have_best || note_number < best_note) {
            best_note = note_number;
            best_degree = degree;
            have_best = true;
        }
    }
    return best_degree;
}

/* Builds a Structure region spanning exactly group's own events, for use
 * as CadenceClassificationInput's region (the "arriving" chord). Mirrors
 * build_whole_piece_region's construction but for one onset group. */
static Structure *build_group_region(const SonorityGroup *group, NoteEvent *const *events) {
    EntityID *region_id;
    if (entity_id_create("ursatz-gui-analyzer-region-group", &region_id) != ENTITY_ID_OK) {
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

    EntityID **event_ids = malloc(sizeof(EntityID *) * group->note_index_count);
    if (!event_ids) {
        entity_destroy(entity);
        return NULL;
    }
    for (size_t i = 0; i < group->note_index_count; i++) {
        const NoteEvent *note = events[group->note_indices[i]];
        if (entity_id_create(entity_id_value(note_event_id(note)), &event_ids[i]) != ENTITY_ID_OK) {
            for (size_t k = 0; k < i; k++) {
                entity_id_destroy(event_ids[k]);
            }
            free(event_ids);
            entity_destroy(entity);
            return NULL;
        }
    }

    Structure *region;
    if (structure_create(entity, event_ids, group->note_index_count, &region) != STRUCTURE_OK) {
        for (size_t i = 0; i < group->note_index_count; i++) {
            entity_id_destroy(event_ids[i]);
        }
        free(event_ids);
        entity_destroy(entity);
        return NULL;
    }
    free(event_ids);
    return region;
}

/* Classifies the cadence (if any) at the harmonic motion from the
 * previous onset group's root degree into this group's root degree, and
 * persists the winning Interpretation. Returns a newly allocated copy of
 * the winning candidate label ("unclassified" included -- a legitimate
 * value per CadenceClassification's own contract), or NULL when no
 * classification could be attempted (no previous group, or either root
 * degree not diatonic in the estimated key). */
static char *classify_and_store_cadence(PersistenceHandle *handle, sqlite3 *index_db, const SonorityGroup *group,
                                          NoteEvent *const *events, int from_degree, int to_degree) {
    if (from_degree < 1 || to_degree < 1) {
        return NULL;
    }

    Structure *region = build_group_region(group, events);
    if (!region) {
        return NULL;
    }

    const Framework *framework = common_practice_minimal_framework();
    char *label = NULL;
    if (framework) {
        CadenceClassificationInput input = {region, framework, from_degree, to_degree,
                                              false, false, false, false, VOICE_LEADING_BASS_MOTION_SAME};
        AnalysisResult *result = NULL;
        if (cadence_classification_analyze(&input, NULL, NULL, &result) == ANALYZER_OK) {
            const Interpretation *interpretation = analysis_result_interpretation_at(result, 0);
            size_t claim_size = 0;
            const char *claim = (const char *)value_content(interpretation_claim(interpretation), &claim_size);
            if (persistence_store_interpretation(handle, interpretation) == PERSISTENCE_INTERPRETATION_OK) {
                label = dup_string(claim);
                record_interpretation_id(index_db, entity_id_value(interpretation_id(interpretation)),
                                          kIndexTypeCadence);
            }
            analysis_result_destroy(result);
        }
    }

    structure_destroy(region);
    return label;
}

/* Runs Cadence Classification over every adjacent pair of onset groups
 * (group i-1's root degree "arriving into" group i), persisting each
 * winning Interpretation. Returns a newly allocated array of
 * grouping->group_count entries (caller frees each non-NULL entry, then
 * the array): entry i is the cadence label that CLOSES group i (NULL if
 * none was classified for that arrival), used as the phrase-boundary
 * cadence signal for the next stage. */
static char **run_cadence_classification(PersistenceHandle *handle, sqlite3 *index_db,
                                           const SonorityGrouping *grouping, NoteEvent *const *events,
                                           PitchNumericValue *const *pitch_values, const KeyEstimate *key) {
    if (grouping->group_count == 0) {
        return NULL;
    }
    char **cadence_types = calloc(grouping->group_count, sizeof(char *));
    if (!cadence_types) {
        return NULL;
    }

    int previous_degree = group_root_degree(&grouping->groups[0], pitch_values, key);
    for (size_t i = 1; i < grouping->group_count; i++) {
        int current_degree = group_root_degree(&grouping->groups[i], pitch_values, key);
        cadence_types[i] =
            classify_and_store_cadence(handle, index_db, &grouping->groups[i], events, previous_degree, current_degree);
        previous_degree = current_degree;
    }

    return cadence_types;
}

static void free_cadence_types(char **cadence_types, size_t count) {
    if (!cadence_types) {
        return;
    }
    for (size_t i = 0; i < count; i++) {
        free(cadence_types[i]);
    }
    free(cadence_types);
}

static bool ensure_dir_exists(const char *path) {
#ifdef _WIN32
    if (GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES) {
        return true;
    }
    return _mkdir(path) == 0;
#else
    struct stat st;
    if (stat(path, &st) == 0) {
        return true;
    }
    return mkdir(path, 0755) == 0;
#endif
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
                                                              size_t midi_size, const char *original_filename,
                                                              char **out_id) {
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
            sqlite3 *index_db = open_interpretation_index(db_path);
            record_original_filename(index_db, original_filename);

            KeyEstimate key = estimate_and_store_global_key(handle, index_db, events, pitch_values, count);

            SonorityGrouping grouping;
            if (segment_by_onset(events, count, &grouping)) {
                for (size_t i = 0; i < grouping.group_count; i++) {
                    identify_and_store_chord(handle, index_db, &grouping.groups[i], events, pitch_values, &key);
                }

                char **cadence_types =
                    run_cadence_classification(handle, index_db, &grouping, events, pitch_values, &key);

                free_cadence_types(cadence_types, grouping.group_count);
                sonority_grouping_destroy(&grouping);
            }

            if (index_db) {
                sqlite3_close(index_db);
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

static void free_ids(char **ids, size_t count) {
    for (size_t i = 0; i < count; i++) {
        free(ids[i]);
    }
    free(ids);
}

/* Appends a newly allocated copy of a "<id>.db" filename's id part to
 * *ids/*count (growing *capacity as needed with realloc). Returns
 * false only on allocation failure. */
static bool append_db_id(char ***ids, size_t *count, size_t *capacity, const char *filename) {
    size_t len = strlen(filename);
    if (len <= 3 || strcmp(filename + len - 3, ".db") != 0) {
        return true;
    }

    if (*count == *capacity) {
        size_t new_capacity = (*capacity == 0) ? 8 : (*capacity * 2);
        char **grown = realloc(*ids, sizeof(char *) * new_capacity);
        if (!grown) {
            return false;
        }
        *ids = grown;
        *capacity = new_capacity;
    }

    size_t id_len = len - 3;
    char *id = malloc(id_len + 1);
    if (!id) {
        return false;
    }
    memcpy(id, filename, id_len);
    id[id_len] = '\0';

    (*ids)[*count] = id;
    (*count)++;
    return true;
}

UrsatzGuiAnalysisResult ursatz_gui_analysis_list(const char *pieces_dir, UrsatzGuiPieceSummary **out_pieces,
                                                  size_t *out_count) {
    *out_pieces = NULL;
    *out_count = 0;

    char **ids = NULL;
    size_t count = 0;
    size_t capacity = 0;

#ifdef _WIN32
    char pattern[MAX_PATH];
    snprintf(pattern, sizeof(pattern), "%s\\*.db", pieces_dir);

    WIN32_FIND_DATAA find_data;
    HANDLE find_handle = FindFirstFileA(pattern, &find_data);
    if (find_handle == INVALID_HANDLE_VALUE) {
        return URSATZ_GUI_ANALYSIS_OK;
    }
    do {
        if (!append_db_id(&ids, &count, &capacity, find_data.cFileName)) {
            free_ids(ids, count);
            FindClose(find_handle);
            return URSATZ_GUI_ANALYSIS_ERR_ALLOC;
        }
    } while (FindNextFileA(find_handle, &find_data) != 0);
    FindClose(find_handle);
#else
    DIR *dir = opendir(pieces_dir);
    if (!dir) {
        return URSATZ_GUI_ANALYSIS_OK;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (!append_db_id(&ids, &count, &capacity, entry->d_name)) {
            free_ids(ids, count);
            closedir(dir);
            return URSATZ_GUI_ANALYSIS_ERR_ALLOC;
        }
    }
    closedir(dir);
#endif

    UrsatzGuiPieceSummary *pieces = NULL;
    if (count > 0) {
        pieces = calloc(count, sizeof(UrsatzGuiPieceSummary));
        if (!pieces) {
            free_ids(ids, count);
            return URSATZ_GUI_ANALYSIS_ERR_ALLOC;
        }
    }

    for (size_t i = 0; i < count; i++) {
        char db_path[320];
        snprintf(db_path, sizeof(db_path), "%s/%s.db", pieces_dir, ids[i]);
        sqlite3 *index_db = open_interpretation_index(db_path);
        char *filename = read_original_filename(index_db);
        if (index_db) {
            sqlite3_close(index_db);
        }
        if (!filename) {
            for (size_t k = 0; k < i; k++) {
                free(pieces[k].id);
                free(pieces[k].filename);
            }
            free(pieces);
            for (size_t k = i; k < count; k++) {
                free(ids[k]);
            }
            free(ids);
            return URSATZ_GUI_ANALYSIS_ERR_ALLOC;
        }
        pieces[i].id = ids[i];
        pieces[i].filename = filename;
    }
    free(ids);

    *out_pieces = pieces;
    *out_count = count;
    return URSATZ_GUI_ANALYSIS_OK;
}

static bool db_path_exists(const char *db_path) {
#ifdef _WIN32
    return GetFileAttributesA(db_path) != INVALID_FILE_ATTRIBUTES;
#else
    struct stat st;
    return stat(db_path, &st) == 0;
#endif
}

static char *dup_string(const char *value) {
    if (!value) {
        return NULL;
    }
    size_t len = strlen(value) + 1;
    char *copy = malloc(len);
    if (copy) {
        memcpy(copy, value, len);
    }
    return copy;
}

/* Reads every (interpretation_id, interpretation_type) row recorded by
 * record_interpretation_id at analysis time. Returns false only on
 * allocation failure; a missing/empty index yields *out_count == 0. */
static bool read_interpretation_index(sqlite3 *index_db, char ***out_ids, char ***out_types, size_t *out_count) {
    *out_ids = NULL;
    *out_types = NULL;
    *out_count = 0;

    char sql[128];
    snprintf(sql, sizeof(sql), "SELECT interpretation_id, interpretation_type FROM %s;", kIndexTableName);

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(index_db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return true;
    }

    char **ids = NULL;
    char **types = NULL;
    size_t count = 0;
    size_t capacity = 0;

    int step;
    while ((step = sqlite3_step(stmt)) == SQLITE_ROW) {
        if (count == capacity) {
            size_t new_capacity = (capacity == 0) ? 8 : (capacity * 2);
            char **grown_ids = realloc(ids, sizeof(char *) * new_capacity);
            char **grown_types = realloc(types, sizeof(char *) * new_capacity);
            if (!grown_ids || !grown_types) {
                free(grown_ids ? grown_ids : ids);
                free(grown_types ? grown_types : types);
                free_ids(ids, count);
                free_ids(types, count);
                sqlite3_finalize(stmt);
                return false;
            }
            ids = grown_ids;
            types = grown_types;
            capacity = new_capacity;
        }

        char *id_copy = dup_string((const char *)sqlite3_column_text(stmt, 0));
        char *type_copy = dup_string((const char *)sqlite3_column_text(stmt, 1));
        if (!id_copy || !type_copy) {
            free(id_copy);
            free(type_copy);
            free_ids(ids, count);
            free_ids(types, count);
            sqlite3_finalize(stmt);
            return false;
        }
        ids[count] = id_copy;
        types[count] = type_copy;
        count++;
    }
    sqlite3_finalize(stmt);

    if (step != SQLITE_DONE) {
        free_ids(ids, count);
        free_ids(types, count);
        return true;
    }

    *out_ids = ids;
    *out_types = types;
    *out_count = count;
    return true;
}

static void interpretation_summary_destroy_contents(UrsatzGuiInterpretationSummary *summary) {
    free(summary->interpretation_id);
    free(summary->type);
    free(summary->label);
    free(summary->produced_by_author);
    free(summary->produced_by_method);
    free(summary->provenance_state);
}

/* Loads the Chord or Key at interpretation_id (per type) and fills out
 * with its label and produced_by author/method. Returns false if the
 * row could not be loaded (skipped by the caller, not fatal to the
 * overall piece detail). */
/* Loads the raw Interpretation at interpretation_id and fills out with
 * its claim (as the display label -- both key_vocabulary.c and
 * triad_identification.c build their claim Value as a null-terminated
 * label string, e.g. "C major", "V7") and its author/method as
 * produced_by. This module stores Interpretations via
 * persistence_store_interpretation directly (see
 * estimate_and_store_global_key/identify_and_store_chord above), not
 * via persistence_store_chord/persistence_store_key, so loading goes
 * through the matching persistence_load_interpretation rather than
 * persistence_load_chord/persistence_load_key (whose backing tables
 * this module never populates). */
static bool load_interpretation_summary(PersistenceHandle *handle, const char *interpretation_id, const char *type,
                                          UrsatzGuiInterpretationSummary *out) {
    Interpretation *interpretation = NULL;
    if (persistence_load_interpretation(handle, interpretation_id, &interpretation) != PERSISTENCE_INTERPRETATION_OK) {
        return false;
    }

    size_t claim_size = 0;
    const char *label = (const char *)value_content(interpretation_claim(interpretation), &claim_size);

    ProvenanceReferenceState provenance_state = provenance_reference_state(interpretation_provenance(interpretation));
    const char *provenance_state_str =
        (provenance_state == PROVENANCE_REFERENCE_PRESENT) ? "present" : "absent";

    bool ok = false;
    out->interpretation_id = dup_string(interpretation_id);
    out->type = dup_string(type);
    out->label = dup_string(label);
    out->produced_by_author = dup_string(interpretation_author(interpretation));
    out->produced_by_method = dup_string(interpretation_method(interpretation));
    out->provenance_state = dup_string(provenance_state_str);
    if (out->interpretation_id && out->type && out->label && out->produced_by_author && out->produced_by_method &&
        out->provenance_state) {
        ok = true;
    } else {
        interpretation_summary_destroy_contents(out);
    }

    interpretation_destroy(interpretation);
    return ok;
}

/* id is generated server-side (generate_piece_id) for stored pieces, but
 * this function's id comes from an HTTP path segment -- reject anything
 * that is not a plain filename component before it reaches db_path, so
 * a crafted id (e.g. containing "/" or "..") can't escape pieces_dir. */
static bool is_valid_piece_id(const char *id) {
    if (!id || id[0] == '\0') {
        return false;
    }
    for (const char *c = id; *c; c++) {
        if (!(isalnum((unsigned char)*c) || *c == '-' || *c == '_')) {
            return false;
        }
    }
    return true;
}

UrsatzGuiAnalysisResult ursatz_gui_analysis_get_piece(const char *pieces_dir, const char *id,
                                                        UrsatzGuiPieceDetail **out_detail) {
    if (!is_valid_piece_id(id)) {
        return URSATZ_GUI_ANALYSIS_ERR_NOT_FOUND;
    }

    char db_path[320];
    snprintf(db_path, sizeof(db_path), "%s/%s.db", pieces_dir, id);

    if (!db_path_exists(db_path)) {
        return URSATZ_GUI_ANALYSIS_ERR_NOT_FOUND;
    }

    PersistenceHandle *handle = NULL;
    if (persistence_open(db_path, &handle) != PERSISTENCE_OK) {
        return URSATZ_GUI_ANALYSIS_ERR_PERSISTENCE_FAILED;
    }

    NoteEvent **events = NULL;
    size_t note_event_count = 0;
    if (persistence_load_note_events(handle, &events, &note_event_count) != PERSISTENCE_NOTE_EVENTS_OK) {
        persistence_close(handle);
        return URSATZ_GUI_ANALYSIS_ERR_PERSISTENCE_FAILED;
    }
    for (size_t i = 0; i < note_event_count; i++) {
        note_event_destroy(events[i]);
    }
    free(events);

    sqlite3 *index_db = open_interpretation_index(db_path);
    char *filename = read_original_filename(index_db);
    if (!filename) {
        if (index_db) {
            sqlite3_close(index_db);
        }
        persistence_close(handle);
        return URSATZ_GUI_ANALYSIS_ERR_ALLOC;
    }

    char **index_ids = NULL;
    char **index_types = NULL;
    size_t index_count = 0;
    if (index_db) {
        if (!read_interpretation_index(index_db, &index_ids, &index_types, &index_count)) {
            sqlite3_close(index_db);
            free(filename);
            persistence_close(handle);
            return URSATZ_GUI_ANALYSIS_ERR_ALLOC;
        }
        sqlite3_close(index_db);
    }

    UrsatzGuiInterpretationSummary *summaries = NULL;
    if (index_count > 0) {
        summaries = calloc(index_count, sizeof(UrsatzGuiInterpretationSummary));
        if (!summaries) {
            free_ids(index_ids, index_count);
            free_ids(index_types, index_count);
            persistence_close(handle);
            return URSATZ_GUI_ANALYSIS_ERR_ALLOC;
        }
    }

    size_t summary_count = 0;
    for (size_t i = 0; i < index_count; i++) {
        if (load_interpretation_summary(handle, index_ids[i], index_types[i], &summaries[summary_count])) {
            summary_count++;
        }
    }
    free_ids(index_ids, index_count);
    free_ids(index_types, index_count);
    persistence_close(handle);

    UrsatzGuiPieceDetail *detail = malloc(sizeof(UrsatzGuiPieceDetail));
    char *id_copy = dup_string(id);
    if (!detail || !id_copy) {
        free(detail);
        free(id_copy);
        free(filename);
        for (size_t i = 0; i < summary_count; i++) {
            interpretation_summary_destroy_contents(&summaries[i]);
        }
        free(summaries);
        return URSATZ_GUI_ANALYSIS_ERR_ALLOC;
    }

    detail->id = id_copy;
    detail->filename = filename;
    detail->note_event_count = note_event_count;
    detail->interpretations = summaries;
    detail->interpretation_count = summary_count;

    *out_detail = detail;
    return URSATZ_GUI_ANALYSIS_OK;
}

void ursatz_gui_piece_detail_destroy(UrsatzGuiPieceDetail *detail) {
    if (!detail) {
        return;
    }
    for (size_t i = 0; i < detail->interpretation_count; i++) {
        interpretation_summary_destroy_contents(&detail->interpretations[i]);
    }
    free(detail->interpretations);
    free(detail->id);
    free(detail->filename);
    free(detail);
}
