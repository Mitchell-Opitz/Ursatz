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
#include <io/midi_key_signature.h>
#include <analysis/cadence_classification.h>
#include <analysis/chord_identification.h>
#include <analysis/key_estimation.h>
#include <analysis/motif_detection.h>
#include <analysis/motif_window_matching.h>
#include <analysis/form_structure_detection.h>
#include <analysis/period_detection.h>
#include <analysis/phrase_boundary_detection.h>
#include <analysis/sentence_detection.h>
#include <analysis/thematic_return_detection.h>
#include <export/musicxml_export.h>
#include <persistence/persistence.h>
#include <persistence/persistence_interpretation.h>
#include <persistence/persistence_note_events.h>
#include <pitch/pitch_numeric_value.h>
#include <relationship/relationship.h>
#include <semantics/contour.h>
#include <semantics/rhythmic_pattern.h>
#include <semantics/sonority.h>
#include <structure/motif.h>
#include <structure/phrase_occurrence.h>
#include <structure/structure.h>
#include <time/duration.h>
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
static const char *const kIndexTypePhrase = "phrase";
static const char *const kIndexTypeMotif = "motif";
static const char *const kIndexTypePeriod = "period";
static const char *const kIndexTypeSentence = "sentence";
static const char *const kIndexTypeForm = "form";
static const char *const kIndexTypeThematicReturn = "thematic_return";

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

/*
 * ursatz-gui-owned note-target table, in the same per-piece SQLite file
 * as kIndexTableName above. Holds one row per (interpretation_id,
 * note xml:id) pair for "cadence"/"motif" claims, whose Interpretation
 * target_id names a Structure/Occurrence that Ursatz never persists
 * (transient, destroyed once its id is copied into the Interpretation --
 * see classify_and_store_cadence/the MotifDetectionInput call site
 * below). That object's event ids are only ever available while it is
 * alive during analysis, so they are captured and stored here then;
 * GET-time resolution reads this table instead of attempting to reload
 * a Structure/Occurrence that does not exist in persistence. Pieces
 * analyzed before this table existed simply have no rows here for their
 * cadence/motif claims -- not an error, just nothing to highlight.
 */
static const char *const kNoteTargetTableName = "ursatz_gui_interpretation_note_targets";

static char *dup_string(const char *value);

/*
 * Derives the same MusicXML xml:id token export/musicxml_export.c's
 * (static, non-exported) xml_id_from_entity would derive for a NoteEvent
 * EntityID: prefixes with 'n' if raw doesn't start with a letter or
 * underscore, replaces any character outside [A-Za-z0-9_.-] with '_'.
 * Duplicated here (rather than exported from ursatz) because it's a
 * small, pure, deterministic string transform and the two call sites
 * need to agree on the same id for the same NoteEvent -- ursatz-gui
 * cannot reach into that file's internals. Writes into out (must be at
 * least strlen(raw) + 2 bytes).
 */
static void xml_id_from_entity_value(const char *raw, char *out, size_t out_size) {
    size_t pos = 0;
    bool needs_prefix = !((raw[0] >= 'A' && raw[0] <= 'Z') || (raw[0] >= 'a' && raw[0] <= 'z') || raw[0] == '_');
    if (needs_prefix && pos + 1 < out_size) {
        out[pos++] = 'n';
    }
    for (size_t i = 0; raw[i] != '\0' && pos + 1 < out_size; i++) {
        char c = raw[i];
        bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' ||
                  c == '.' || c == '-';
        out[pos++] = ok ? c : '_';
    }
    out[pos] = '\0';
}

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

    snprintf(sql, sizeof(sql),
             "CREATE TABLE IF NOT EXISTS %s ("
             "  interpretation_id TEXT NOT NULL,"
             "  note_xml_id TEXT NOT NULL,"
             "  ord INTEGER NOT NULL"
             ");",
             kNoteTargetTableName);
    if (sqlite3_exec(db, sql, NULL, NULL, NULL) != SQLITE_OK) {
        sqlite3_close(db);
        return NULL;
    }

    return db;
}

/* Records the xml:id (derived via xml_id_from_entity_value) of every
 * event in event_ids under interpretation_id, preserving order.
 * Best-effort, mirroring record_interpretation_id: a failure here does
 * not fail the analysis, only the highlight for this one claim. */
static void record_interpretation_note_targets(sqlite3 *index_db, const char *interpretation_id,
                                                 const EntityID *const *event_ids, size_t event_id_count) {
    if (!index_db || !interpretation_id || event_id_count == 0) {
        return;
    }

    char sql[192];
    snprintf(sql, sizeof(sql), "INSERT INTO %s (interpretation_id, note_xml_id, ord) VALUES (?, ?, ?);",
             kNoteTargetTableName);

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(index_db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return;
    }
    for (size_t i = 0; i < event_id_count; i++) {
        char xml_id[80];
        xml_id_from_entity_value(entity_id_value(event_ids[i]), xml_id, sizeof(xml_id));

        sqlite3_bind_text(stmt, 1, interpretation_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, xml_id, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 3, (int)i);
        sqlite3_step(stmt);
        sqlite3_reset(stmt);
    }
    sqlite3_finalize(stmt);
}

/* Reads the note xml:ids recorded by record_interpretation_note_targets
 * for interpretation_id, in original order. On success, *out_ids is a
 * newly allocated, caller-owned array of *out_count newly allocated
 * strings (may be *out_count == 0, *out_ids == NULL). Returns false only
 * on allocation failure. */
static bool read_interpretation_note_targets(sqlite3 *index_db, const char *interpretation_id, char ***out_ids,
                                               size_t *out_count) {
    *out_ids = NULL;
    *out_count = 0;
    if (!index_db) {
        return true;
    }

    char sql[192];
    snprintf(sql, sizeof(sql), "SELECT note_xml_id FROM %s WHERE interpretation_id = ? ORDER BY ord;",
             kNoteTargetTableName);

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(index_db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return true;
    }
    sqlite3_bind_text(stmt, 1, interpretation_id, -1, SQLITE_TRANSIENT);

    char **ids = NULL;
    size_t count = 0;
    size_t capacity = 0;
    bool ok = true;
    while (ok && sqlite3_step(stmt) == SQLITE_ROW) {
        if (count == capacity) {
            capacity = capacity == 0 ? 4 : capacity * 2;
            char **grown = realloc(ids, sizeof(char *) * capacity);
            if (!grown) {
                ok = false;
                break;
            }
            ids = grown;
        }
        char *value = dup_string((const char *)sqlite3_column_text(stmt, 0));
        if (!value) {
            ok = false;
            break;
        }
        ids[count++] = value;
    }
    sqlite3_finalize(stmt);

    if (!ok) {
        for (size_t i = 0; i < count; i++) {
            free(ids[i]);
        }
        free(ids);
        return false;
    }

    *out_ids = ids;
    *out_count = count;
    return true;
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

static void relationship_summary_destroy_contents(UrsatzGuiRelationshipSummary *row) {
    free(row->relationship_id);
    free(row->source_id);
    free(row->predicate_name);
    free(row->target_id);
}

/* Reads every relationship row recorded by record_relationship. Returns
 * false only on allocation failure; a missing/empty table yields
 * *out_count == 0. Mirrors read_interpretation_index's shape. */
static bool read_relationship_index(sqlite3 *index_db, UrsatzGuiRelationshipSummary **out_rows, size_t *out_count) {
    *out_rows = NULL;
    *out_count = 0;

    char sql[160];
    snprintf(sql, sizeof(sql), "SELECT relationship_id, source_id, predicate_name, target_id FROM %s;",
             kRelationshipTableName);

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(index_db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return true;
    }

    UrsatzGuiRelationshipSummary *rows = NULL;
    size_t count = 0;
    size_t capacity = 0;

    int step;
    while ((step = sqlite3_step(stmt)) == SQLITE_ROW) {
        if (count == capacity) {
            size_t new_capacity = (capacity == 0) ? 8 : (capacity * 2);
            UrsatzGuiRelationshipSummary *grown = realloc(rows, sizeof(UrsatzGuiRelationshipSummary) * new_capacity);
            if (!grown) {
                for (size_t i = 0; i < count; i++) {
                    relationship_summary_destroy_contents(&rows[i]);
                }
                free(rows);
                sqlite3_finalize(stmt);
                return false;
            }
            rows = grown;
            capacity = new_capacity;
        }

        UrsatzGuiRelationshipSummary row = {0};
        row.relationship_id = dup_string((const char *)sqlite3_column_text(stmt, 0));
        row.source_id = dup_string((const char *)sqlite3_column_text(stmt, 1));
        row.predicate_name = dup_string((const char *)sqlite3_column_text(stmt, 2));
        row.target_id = dup_string((const char *)sqlite3_column_text(stmt, 3));
        if (!row.relationship_id || !row.source_id || !row.predicate_name || !row.target_id) {
            relationship_summary_destroy_contents(&row);
            for (size_t i = 0; i < count; i++) {
                relationship_summary_destroy_contents(&rows[i]);
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
            relationship_summary_destroy_contents(&rows[i]);
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

/* Bass pitch class of a SonorityGroup: the pitch class of its lowest
   MIDI note number. Used as a tonic guess when no key signature is
   present -- the closing chord is tried before the opening chord, since
   a piece almost always ends on its tonic but can just as easily open
   on an anacrusis, a pickup, or a non-tonic harmony. Mirrors
   ursatz-analyzer's pipeline/analysis_pipeline.c. */
static int bass_pitch_class(const SonorityGroup *group, PitchNumericValue *const *pitch_values) {
    long lowest_note_number = pitch_numeric_value_position_numerator(pitch_values[group->note_indices[0]]);
    for (size_t i = 1; i < group->note_index_count; i++) {
        long note_number = pitch_numeric_value_position_numerator(pitch_values[group->note_indices[i]]);
        if (note_number < lowest_note_number) {
            lowest_note_number = note_number;
        }
    }
    return pitch_class_of(lowest_note_number);
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

/* Major-key tonic pitch class for each sharps/flats count 0-7, per the
   circle of fifths (index 0 = C, no accidentals). The minor tonic
   pitch class for the same count is always 9 semitones above the
   major tonic (the relative minor), regardless of sharps/flats sign.
   Mirrors ursatz-analyzer's pipeline/key_pipeline.c. */
static const int kSharpsMajorTonic[8] = {0, 7, 2, 9, 4, 11, 6, 1};  /* C G D A E B F# C# */
static const int kFlatsMajorTonic[8] = {0, 5, 10, 3, 8, 1, 6, 11};  /* C F Bb Eb Ab Db Gb Cb */

/* Derives the tonic pitch class a MIDI key signature meta-event names,
   or returns false (out untouched) for an absent or out-of-range one. */
static bool tonic_from_key_signature(MidiKeySignature key_signature, int *out_tonic) {
    if (!key_signature.found || key_signature.sharps_flats < -7 || key_signature.sharps_flats > 7) {
        return false;
    }
    int abs_count = key_signature.sharps_flats < 0 ? -key_signature.sharps_flats : key_signature.sharps_flats;
    int major_tonic = key_signature.sharps_flats >= 0 ? kSharpsMajorTonic[abs_count] : kFlatsMajorTonic[abs_count];
    *out_tonic = key_signature.is_minor ? (major_tonic + 9) % 12 : major_tonic;
    return true;
}

/* Tries one candidate tonic against KeyEstimation and, on a match,
 * persists the winning Interpretation and fills *estimate. Returns
 * true on a match, false otherwise. relative must have room for count
 * entries; it is overwritten on every call regardless of outcome. */
static bool try_tonic_and_store(PersistenceHandle *handle, sqlite3 *index_db, Structure *region,
                                 const Framework *framework, PitchNumericValue *const *pitch_values, size_t count,
                                 int *relative, int tonic, const char *tonic_name, KeyEstimate *estimate) {
    for (size_t i = 0; i < count; i++) {
        long note_number = pitch_numeric_value_position_numerator(pitch_values[i]);
        int pitch_class = pitch_class_of(note_number);
        relative[i] = ((pitch_class - tonic) % 12 + 12) % 12;
    }

    KeyEstimationInput input = {region, framework, tonic_name, relative, count};
    AnalysisResult *result = NULL;
    if (key_estimation_analyze(&input, NULL, NULL, &result) != ANALYZER_OK) {
        return false;
    }

    const Interpretation *interpretation = analysis_result_interpretation_at(result, 0);
    size_t claim_size;
    const char *claim = (const char *)value_content(interpretation_claim(interpretation), &claim_size);

    bool stored = false;
    if (persistence_store_interpretation(handle, interpretation) == PERSISTENCE_INTERPRETATION_OK) {
        estimate->tonic_pitch_class = tonic;
        estimate->is_major = (strstr(claim, "major") != NULL);
        estimate->found = true;
        record_interpretation_id(index_db, entity_id_value(interpretation_id(interpretation)), kIndexTypeKey);
        stored = true;
    }
    analysis_result_destroy(result);
    return stored;
}

/* Estimates one global key for the whole piece, persisting the winning
 * Interpretation (if any) directly. Mirrors ursatz-analyzer's
 * estimate_global_key (pipeline/key_pipeline.c). Precedence:
 *
 *   1. The MIDI key signature meta-event's tonic, if present and
 *      plausible -- no pitch-class scanning at all in the normal case.
 *   2. priority_tonics (priority_tonic_count entries, each 0-11), in
 *      order -- the caller's best guess at the tonic when no key
 *      signature is present (the bass pitch class of the piece's
 *      closing and opening chord).
 *   3. A brute-force scan of all 12 pitch classes in order (C, C#, D,
 *      ...), tried only if every candidate above misses -- this also
 *      serves as a safety net for the rare case where a key signature
 *      is present but doesn't actually match the piece's pitch content
 *      (e.g. a mid-piece modulation).
 */
static KeyEstimate estimate_and_store_global_key(PersistenceHandle *handle, sqlite3 *index_db,
                                                   NoteEvent *const *events, PitchNumericValue *const *pitch_values,
                                                   size_t count, MidiKeySignature key_signature,
                                                   const int *priority_tonics, size_t priority_tonic_count) {
    static const char *const kPitchClassNames[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

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

    int *relative = malloc(sizeof(int) * count);
    if (!relative) {
        structure_destroy(region);
        return estimate;
    }

    int key_signature_tonic;
    if (tonic_from_key_signature(key_signature, &key_signature_tonic)) {
        try_tonic_and_store(handle, index_db, region, framework, pitch_values, count, relative, key_signature_tonic,
                             kPitchClassNames[key_signature_tonic], &estimate);
    }

    for (size_t p = 0; p < priority_tonic_count && !estimate.found; p++) {
        int tonic = priority_tonics[p];
        if (tonic < 0 || tonic > 11) {
            continue;
        }
        try_tonic_and_store(handle, index_db, region, framework, pitch_values, count, relative, tonic,
                             kPitchClassNames[tonic], &estimate);
    }

    for (int tonic = 0; tonic < 12 && !estimate.found; tonic++) {
        try_tonic_and_store(handle, index_db, region, framework, pitch_values, count, relative, tonic,
                             kPitchClassNames[tonic], &estimate);
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
    /* Suffixed with the group's own first event's id so each onset
     * group gets a distinct region id -- a fixed id here would give
     * every cadence classification call the same target, colliding on
     * the derived Interpretation id and silently failing every
     * persistence_store_interpretation call after the first. */
    char region_id_value[192];
    snprintf(region_id_value, sizeof(region_id_value), "ursatz-gui-analyzer-region-group-%s",
             entity_id_value(note_event_id(events[group->note_indices[0]])));

    EntityID *region_id;
    if (entity_id_create(region_id_value, &region_id) != ENTITY_ID_OK) {
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
                const char *cadence_interpretation_id = entity_id_value(interpretation_id(interpretation));
                record_interpretation_id(index_db, cadence_interpretation_id, kIndexTypeCadence);

                size_t region_event_id_count = structure_event_id_count(region);
                const EntityID **region_event_ids = malloc(sizeof(EntityID *) * region_event_id_count);
                if (region_event_ids) {
                    for (size_t k = 0; k < region_event_id_count; k++) {
                        region_event_ids[k] = structure_event_id_at(region, k);
                    }
                    record_interpretation_note_targets(index_db, cadence_interpretation_id, region_event_ids,
                                                        region_event_id_count);
                    free(region_event_ids);
                }
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

/* Persists one AnalysisResult's every Interpretation under the given
 * index type. Shared by the framework-stage analyzers below, each of
 * which (unlike KeyEstimation/ChordIdentification) can legitimately
 * produce more than one Interpretation, or none, per call. */
static void store_all_interpretations(PersistenceHandle *handle, sqlite3 *index_db, const AnalysisResult *result,
                                        const char *type) {
    size_t interpretation_count = analysis_result_interpretation_count(result);
    for (size_t i = 0; i < interpretation_count; i++) {
        const Interpretation *interpretation = analysis_result_interpretation_at(result, i);
        if (persistence_store_interpretation(handle, interpretation) == PERSISTENCE_INTERPRETATION_OK) {
            record_interpretation_id(index_db, entity_id_value(interpretation_id(interpretation)), type);
        }
    }
}

/* Runs Phrase Boundary Detection over one point per onset group (the
 * same onset-grouping already used for chord identification -- SCOPED
 * BRANCH DEFAULT: phrase points are per onset group, not per individual
 * note event, so a phrase's region_event_ids are its member groups'
 * representative events, not every note). has_discontinuity is fixed
 * false for every point (SCOPED BRANCH DEFAULT: this branch wires the
 * cadence signal, real Interpretation output from run_cadence_
 * classification; a contour/rhythm discontinuity signal would require
 * pitch-direction/rest detection this module does not otherwise compute
 * and is left for a later branch) -- boundaries are driven entirely by
 * the real cadence_types signal.
 *
 * On success, persists the boundary and boundary-signal Interpretations
 * under kIndexTypePhrase and returns the built PhraseOccurrence array via
 * out_occurrences/out_occurrence_count (caller frees with
 * phrase_boundary_detection_destroy_occurrences). Returns the points
 * array actually used via out_points (caller frees with free()) so later
 * stages can re-derive which onset group closed each phrase. */
static void run_phrase_boundary_detection(PersistenceHandle *handle, sqlite3 *index_db,
                                            const SonorityGrouping *grouping, NoteEvent *const *events,
                                            char *const *cadence_types, PhraseBoundaryDetectionPoint **out_points,
                                            PhraseOccurrence ***out_occurrences, size_t *out_occurrence_count) {
    *out_points = NULL;
    *out_occurrences = NULL;
    *out_occurrence_count = 0;

    if (grouping->group_count == 0) {
        return;
    }

    PhraseBoundaryDetectionPoint *points = malloc(sizeof(PhraseBoundaryDetectionPoint) * grouping->group_count);
    if (!points) {
        return;
    }
    for (size_t i = 0; i < grouping->group_count; i++) {
        const NoteEvent *representative = events[grouping->groups[i].note_indices[0]];
        bool has_cadence = cadence_types[i] != NULL &&
                            strlen(cadence_types[i]) < PHRASE_BOUNDARY_DETECTION_CADENCE_TYPE_BUFFER_SIZE;
        points[i].event = representative;
        points[i].has_cadence = has_cadence;
        points[i].cadence_type = has_cadence ? cadence_types[i] : "";
        points[i].has_discontinuity = false;
    }

    PhraseBoundaryDetectionInput input = {points, grouping->group_count, 0};

    AnalysisResult *boundary_result = NULL;
    if (phrase_boundary_detection_analyze(&input, NULL, NULL, &boundary_result) == ANALYZER_OK) {
        store_all_interpretations(handle, index_db, boundary_result, kIndexTypePhrase);
        analysis_result_destroy(boundary_result);
    }

    AnalysisResult *signal_result = NULL;
    if (phrase_boundary_detection_signal_analyze(&input, NULL, NULL, &signal_result) == ANALYZER_OK) {
        store_all_interpretations(handle, index_db, signal_result, kIndexTypePhrase);
        analysis_result_destroy(signal_result);
    }

    PhraseOccurrence **occurrences = NULL;
    size_t occurrence_count = 0;
    phrase_boundary_detection_build_occurrences(points, grouping->group_count, 0, &occurrences, &occurrence_count);

    *out_points = points;
    *out_occurrences = occurrences;
    *out_occurrence_count = occurrence_count;
}

/* Finds the onset group index whose representative event (the same one
 * points[] was built from) has the given entity id, or SIZE_MAX if none
 * matches -- used to look back up a PhraseOccurrence's closing cadence
 * type from the group it ends on. */
static size_t group_index_for_event_id(const SonorityGrouping *grouping, NoteEvent *const *events,
                                         const EntityID *event_id) {
    for (size_t i = 0; i < grouping->group_count; i++) {
        const NoteEvent *representative = events[grouping->groups[i].note_indices[0]];
        if (entity_id_equals(note_event_id(representative), event_id)) {
            return i;
        }
    }
    return (size_t)-1;
}

/* The cadence type (borrowed, from cadence_types) that closes phrase
 * occurrence occ, or NULL if its closing event isn't a group
 * representative or has no classified cadence. */
static const char *closing_cadence_type(const PhraseOccurrence *occ, const SonorityGrouping *grouping,
                                          NoteEvent *const *events, char *const *cadence_types) {
    size_t region_count = phrase_occurrence_region_event_id_count(occ);
    if (region_count == 0) {
        return NULL;
    }
    const EntityID *last_id = phrase_occurrence_region_event_id_at(occ, region_count - 1);
    size_t group_index = group_index_for_event_id(grouping, events, last_id);
    if (group_index == (size_t)-1) {
        return NULL;
    }
    return cadence_types[group_index];
}

static size_t event_index_by_id(NoteEvent *const *events, size_t count, const EntityID *id) {
    for (size_t i = 0; i < count; i++) {
        if (entity_id_equals(note_event_id(events[i]), id)) {
            return i;
        }
    }
    return (size_t)-1;
}

/* A Contour/RhythmicPattern/event-id window built (and owned) for one
 * PhraseOccurrence's region events, ready to hand to
 * motif_window_matching_find as a MotifWindow. */
typedef struct {
    Contour *contour;
    RhythmicPattern *rhythm;
    EntityID **event_ids;
    size_t event_id_count;
} OwnedMotifWindow;

static void owned_motif_window_destroy(OwnedMotifWindow *window) {
    if (window->contour) {
        contour_destroy(window->contour);
    }
    if (window->rhythm) {
        rhythmic_pattern_destroy(window->rhythm);
    }
    for (size_t i = 0; i < window->event_id_count; i++) {
        entity_id_destroy(window->event_ids[i]);
    }
    free(window->event_ids);
    memset(window, 0, sizeof(*window));
}

/* Builds a Contour (pitch-direction) + RhythmicPattern (duration)
 * window over occ's own region events, in order -- the same real
 * pitch/duration data this module already reads for key/chord
 * identification, not a placeholder. Requires at least 2 region events
 * (Contour's own minimum); shorter phrases produce no window (false).
 */
static bool build_motif_window(const PhraseOccurrence *occ, NoteEvent *const *events,
                                 PitchNumericValue *const *pitch_values, size_t count, OwnedMotifWindow *out) {
    memset(out, 0, sizeof(*out));
    size_t region_count = phrase_occurrence_region_event_id_count(occ);
    if (region_count < 2) {
        return false;
    }

    size_t *indices = malloc(sizeof(size_t) * region_count);
    if (!indices) {
        return false;
    }
    for (size_t i = 0; i < region_count; i++) {
        indices[i] = event_index_by_id(events, count, phrase_occurrence_region_event_id_at(occ, i));
        if (indices[i] == (size_t)-1) {
            free(indices);
            return false;
        }
    }

    PitchIdentity **pitch_ids = calloc(region_count, sizeof(PitchIdentity *));
    ContourDirection *directions = malloc(sizeof(ContourDirection) * (region_count - 1));
    Duration **durations = calloc(region_count, sizeof(Duration *));
    EntityID **event_ids = calloc(region_count, sizeof(EntityID *));
    bool ok = pitch_ids && directions && durations && event_ids;

    for (size_t i = 0; ok && i < region_count; i++) {
        size_t token_size = 0;
        const void *token = pitch_identity_token(note_event_pitch(events[indices[i]]), &token_size);
        if (pitch_identity_create(token, token_size, &pitch_ids[i]) != PITCH_IDENTITY_OK) {
            ok = false;
            break;
        }
        const Duration *duration = note_event_duration(events[indices[i]]);
        if (duration_create(duration_numerator(duration), duration_denominator(duration), &durations[i]) !=
            DURATION_OK) {
            ok = false;
            break;
        }
        if (entity_id_create(entity_id_value(phrase_occurrence_region_event_id_at(occ, i)), &event_ids[i]) !=
            ENTITY_ID_OK) {
            ok = false;
            break;
        }
    }
    for (size_t i = 0; ok && i + 1 < region_count; i++) {
        long a = pitch_numeric_value_position_numerator(pitch_values[indices[i]]);
        long b = pitch_numeric_value_position_numerator(pitch_values[indices[i + 1]]);
        directions[i] = (b > a) ? CONTOUR_DIRECTION_UP : (b < a) ? CONTOUR_DIRECTION_DOWN : CONTOUR_DIRECTION_SAME;
    }
    free(indices);

    Contour *contour = NULL;
    RhythmicPattern *rhythm = NULL;
    if (ok && contour_create(pitch_ids, region_count, directions, region_count - 1, &contour) != CONTOUR_OK) {
        ok = false;
    }
    if (ok && rhythmic_pattern_create(durations, region_count, &rhythm) != RHYTHMIC_PATTERN_OK) {
        ok = false;
    }

    if (!ok) {
        if (contour) {
            contour_destroy(contour);
        } else {
            for (size_t i = 0; i < region_count && pitch_ids; i++) {
                if (pitch_ids[i]) {
                    pitch_identity_destroy(pitch_ids[i]);
                }
            }
            free(pitch_ids);
            free(directions);
        }
        if (!rhythm) {
            for (size_t i = 0; i < region_count && durations; i++) {
                if (durations[i]) {
                    duration_destroy(durations[i]);
                }
            }
            free(durations);
        }
        for (size_t i = 0; i < region_count && event_ids; i++) {
            if (event_ids[i]) {
                entity_id_destroy(event_ids[i]);
            }
        }
        free(event_ids);
        return false;
    }

    out->contour = contour;
    out->rhythm = rhythm;
    out->event_ids = event_ids;
    out->event_id_count = region_count;
    return true;
}

/* Runs MotifWindowMatching over every phrase occurrence's own window
 * (real Contour/RhythmicPattern equality, not a stub), then
 * MotifDetection over each matched window. SCOPED BRANCH DEFAULT: each
 * matched window is reported as its own single-occurrence Motif (its
 * matched_event_ids double as both the Motif's definition and its sole
 * reported occurrence) -- clustering multiple matching windows under one
 * shared Motif is a real refinement left for a later branch; this still
 * exercises real repeat-detection (motif_window_matching_find) and real
 * MotifDetection packaging, not a no-op. */
static void run_motif_detection(PersistenceHandle *handle, sqlite3 *index_db,
                                  PhraseOccurrence *const *phrase_occurrences, size_t phrase_occurrence_count,
                                  NoteEvent *const *events, PitchNumericValue *const *pitch_values, size_t count) {
    if (phrase_occurrence_count == 0) {
        return;
    }

    OwnedMotifWindow *owned = calloc(phrase_occurrence_count, sizeof(OwnedMotifWindow));
    MotifWindow *windows = malloc(sizeof(MotifWindow) * phrase_occurrence_count);
    if (!owned || !windows) {
        free(owned);
        free(windows);
        return;
    }

    size_t window_count = 0;
    for (size_t i = 0; i < phrase_occurrence_count; i++) {
        if (build_motif_window(phrase_occurrences[i], events, pitch_values, count, &owned[window_count])) {
            windows[window_count].contour = owned[window_count].contour;
            windows[window_count].rhythm = owned[window_count].rhythm;
            windows[window_count].event_ids = (const EntityID *const *)owned[window_count].event_ids;
            windows[window_count].event_id_count = owned[window_count].event_id_count;
            window_count++;
        }
    }

    MotifDetectionOccurrence *matches = NULL;
    size_t match_count = 0;
    if (window_count > 0 &&
        motif_window_matching_find(windows, window_count, &matches, &match_count) == MOTIF_WINDOW_MATCHING_OK) {
        for (size_t m = 0; m < match_count; m++) {
            EntityID *motif_id = NULL;
            if (entity_id_create("ursatz-gui-analyzer-motif", &motif_id) != ENTITY_ID_OK) {
                continue;
            }

            size_t event_id_count = matches[m].matched_event_id_count;
            EntityID **motif_event_ids = calloc(event_id_count, sizeof(EntityID *));
            bool ok = motif_event_ids != NULL;
            for (size_t k = 0; ok && k < event_id_count; k++) {
                if (entity_id_create(entity_id_value(matches[m].matched_event_ids[k]), &motif_event_ids[k]) !=
                    ENTITY_ID_OK) {
                    ok = false;
                }
            }

            Motif *motif = NULL;
            if (ok && motif_create(motif_id, motif_event_ids, event_id_count, &motif) == MOTIF_OK) {
                MotifDetectionInput input = {motif, &matches[m], 1};
                AnalysisResult *result = NULL;
                if (motif_detection_analyze(&input, NULL, NULL, &result) == ANALYZER_OK) {
                    store_all_interpretations(handle, index_db, result, kIndexTypeMotif);
                    if (analysis_result_interpretation_count(result) > 0) {
                        const char *motif_interpretation_id =
                            entity_id_value(interpretation_id(analysis_result_interpretation_at(result, 0)));
                        record_interpretation_note_targets(index_db, motif_interpretation_id,
                                                            matches[m].matched_event_ids,
                                                            matches[m].matched_event_id_count);
                    }
                    analysis_result_destroy(result);
                }
                motif_destroy(motif);
            } else {
                entity_id_destroy(motif_id);
                for (size_t k = 0; k < event_id_count && motif_event_ids; k++) {
                    if (motif_event_ids[k]) {
                        entity_id_destroy(motif_event_ids[k]);
                    }
                }
                free(motif_event_ids);
            }
        }
        motif_window_matching_destroy_occurrences(matches);
    }

    for (size_t i = 0; i < window_count; i++) {
        owned_motif_window_destroy(&owned[i]);
    }
    free(owned);
    free(windows);
}

/* Runs Period Detection over every consecutive PhraseOccurrence pair
 * (using their real closing cadence types from cadence_types) and
 * Sentence Detection over every consecutive pair treated as a
 * statement/repetition candidate. SCOPED BRANCH DEFAULT: Sentence
 * Detection wants MotifOccurrence statement/repetition ids, but
 * run_motif_detection (above) does not retain persisted MotifOccurrence
 * ids across calls in this branch -- consecutive PhraseOccurrences'
 * own ids stand in instead, with their own real Contour/RhythmicPattern
 * windows (rebuilt the same way run_motif_detection builds them) as the
 * statement/continuation windows SentenceContinuationMatching actually
 * compares. A pair whose windows don't qualify produces no Sentence
 * output, per the analyzer's own discipline -- not forced here. */
static void run_period_and_sentence_detection(PersistenceHandle *handle, sqlite3 *index_db,
                                                 PhraseOccurrence *const *phrase_occurrences,
                                                 size_t phrase_occurrence_count, const SonorityGrouping *grouping,
                                                 NoteEvent *const *events, PitchNumericValue *const *pitch_values,
                                                 size_t count, char *const *cadence_types) {
    if (phrase_occurrence_count == 0) {
        return;
    }

    PeriodDetectionEntry *period_entries = malloc(sizeof(PeriodDetectionEntry) * phrase_occurrence_count);
    if (period_entries) {
        for (size_t i = 0; i < phrase_occurrence_count; i++) {
            period_entries[i].phrase_occurrence = phrase_occurrences[i];
            period_entries[i].cadence_type = closing_cadence_type(phrase_occurrences[i], grouping, events, cadence_types);
        }

        PeriodDetectionInput input = {period_entries, phrase_occurrence_count};
        AnalysisResult *result = NULL;
        if (period_detection_analyze(&input, NULL, NULL, &result) == ANALYZER_OK) {
            store_all_interpretations(handle, index_db, result, kIndexTypePeriod);
            analysis_result_destroy(result);
        }

        Relationship **relationships = NULL;
        size_t relationship_count = 0;
        if (period_detection_build_relationships(period_entries, phrase_occurrence_count, &relationships,
                                                   &relationship_count) == PERIOD_DETECTION_REL_OK) {
            record_relationships(index_db, relationships, relationship_count);
            period_detection_destroy_relationships(relationships, relationship_count);
        }
        free(period_entries);
    }

    if (phrase_occurrence_count < 2) {
        return;
    }

    OwnedMotifWindow *owned = calloc(phrase_occurrence_count, sizeof(OwnedMotifWindow));
    MotifWindow *phrase_windows = calloc(phrase_occurrence_count, sizeof(MotifWindow));
    bool *has_window = calloc(phrase_occurrence_count, sizeof(bool));
    SentenceDetectionEntry *sentence_entries = malloc(sizeof(SentenceDetectionEntry) * (phrase_occurrence_count - 1));
    if (!owned || !phrase_windows || !has_window || !sentence_entries) {
        free(owned);
        free(phrase_windows);
        free(has_window);
        free(sentence_entries);
        return;
    }

    for (size_t i = 0; i < phrase_occurrence_count; i++) {
        has_window[i] = build_motif_window(phrase_occurrences[i], events, pitch_values, count, &owned[i]);
        if (has_window[i]) {
            phrase_windows[i].contour = owned[i].contour;
            phrase_windows[i].rhythm = owned[i].rhythm;
            phrase_windows[i].event_ids = (const EntityID *const *)owned[i].event_ids;
            phrase_windows[i].event_id_count = owned[i].event_id_count;
        }
    }

    size_t sentence_entry_count = 0;
    for (size_t i = 0; i + 1 < phrase_occurrence_count; i++) {
        if (!has_window[i] || !has_window[i + 1]) {
            continue;
        }
        SentenceDetectionEntry *entry = &sentence_entries[sentence_entry_count];
        entry->statement_occurrence_id = phrase_occurrence_id(phrase_occurrences[i]);
        entry->repetition_occurrence_id = phrase_occurrence_id(phrase_occurrences[i + 1]);
        entry->statement_window = &phrase_windows[i];
        entry->continuation_candidate = (i + 2 < phrase_occurrence_count && has_window[i + 2])
                                              ? &phrase_windows[i + 2]
                                              : NULL;
        sentence_entry_count++;
    }

    SentenceDetectionInput input = {sentence_entries, sentence_entry_count};
    AnalysisResult *result = NULL;
    if (sentence_detection_analyze(&input, NULL, NULL, &result) == ANALYZER_OK) {
        store_all_interpretations(handle, index_db, result, kIndexTypeSentence);
        analysis_result_destroy(result);
    }

    Relationship **relationships = NULL;
    size_t relationship_count = 0;
    if (sentence_detection_build_relationships(sentence_entries, sentence_entry_count, &relationships,
                                                 &relationship_count) == SENTENCE_DETECTION_REL_OK) {
        record_relationships(index_db, relationships, relationship_count);
        sentence_detection_destroy_relationships(relationships, relationship_count);
    }
    free(sentence_entries);

    for (size_t i = 0; i < phrase_occurrence_count; i++) {
        if (has_window[i]) {
            owned_motif_window_destroy(&owned[i]);
        }
    }
    free(owned);
    free(phrase_windows);
    free(has_window);
}

/* Runs Form Structure Detection over every PhraseOccurrence (grouping
 * them into Sections at structural cadences) and Thematic Return
 * Detection over every PhraseOccurrence's own window. SCOPED BRANCH
 * DEFAULT (same one run_period_and_sentence_detection documents):
 * Thematic Return Detection wants ThemeOccurrence ids/windows, but this
 * branch never constructs a distinct ThemeOccurrence -- each
 * PhraseOccurrence's own id and real Contour/RhythmicPattern window
 * stand in, at phrase granularity rather than theme granularity. Only
 * Interpretations/Relationships are persisted; the Section objects
 * Form Structure Detection can mint (form_structure_detection_build_
 * sections) have no persistence path in this branch (no Structure
 * store exists for them either), so they are not retained. */
static void run_form_and_thematic_return_detection(PersistenceHandle *handle, sqlite3 *index_db,
                                                       PhraseOccurrence *const *phrase_occurrences,
                                                       size_t phrase_occurrence_count, const SonorityGrouping *grouping,
                                                       NoteEvent *const *events, PitchNumericValue *const *pitch_values,
                                                       size_t count, char *const *cadence_types) {
    if (phrase_occurrence_count == 0) {
        return;
    }

    FormStructureDetectionEntry *form_entries = malloc(sizeof(FormStructureDetectionEntry) * phrase_occurrence_count);
    if (form_entries) {
        for (size_t i = 0; i < phrase_occurrence_count; i++) {
            form_entries[i].phrase_occurrence = phrase_occurrences[i];
            form_entries[i].cadence_type = closing_cadence_type(phrase_occurrences[i], grouping, events, cadence_types);
        }

        FormStructureDetectionInput input = {form_entries, phrase_occurrence_count};
        AnalysisResult *result = NULL;
        if (form_structure_detection_analyze(&input, NULL, NULL, &result) == ANALYZER_OK) {
            store_all_interpretations(handle, index_db, result, kIndexTypeForm);
            analysis_result_destroy(result);
        }

        Relationship **relationships = NULL;
        size_t relationship_count = 0;
        if (form_structure_detection_build_relationships(form_entries, phrase_occurrence_count, &relationships,
                                                             &relationship_count) == FORM_STRUCTURE_DETECTION_REL_OK) {
            record_relationships(index_db, relationships, relationship_count);
            form_structure_detection_destroy_relationships(relationships, relationship_count);
        }
        free(form_entries);
    }

    if (phrase_occurrence_count < 2) {
        return;
    }

    OwnedMotifWindow *owned = calloc(phrase_occurrence_count, sizeof(OwnedMotifWindow));
    MotifWindow *windows = calloc(phrase_occurrence_count, sizeof(MotifWindow));
    bool *has_window = calloc(phrase_occurrence_count, sizeof(bool));
    ThematicReturnDetectionEntry *entries = malloc(sizeof(ThematicReturnDetectionEntry) * phrase_occurrence_count);
    if (!owned || !windows || !has_window || !entries) {
        free(owned);
        free(windows);
        free(has_window);
        free(entries);
        return;
    }

    size_t entry_count = 0;
    for (size_t i = 0; i < phrase_occurrence_count; i++) {
        has_window[i] = build_motif_window(phrase_occurrences[i], events, pitch_values, count, &owned[i]);
        if (!has_window[i]) {
            continue;
        }
        windows[i].contour = owned[i].contour;
        windows[i].rhythm = owned[i].rhythm;
        windows[i].event_ids = (const EntityID *const *)owned[i].event_ids;
        windows[i].event_id_count = owned[i].event_id_count;

        entries[entry_count].theme_occurrence_id = phrase_occurrence_id(phrase_occurrences[i]);
        entries[entry_count].window = &windows[i];
        entry_count++;
    }

    ThematicReturnDetectionInput input = {entries, entry_count};
    AnalysisResult *result = NULL;
    if (thematic_return_detection_analyze(&input, NULL, NULL, &result) == ANALYZER_OK) {
        store_all_interpretations(handle, index_db, result, kIndexTypeThematicReturn);
        analysis_result_destroy(result);
    }

    Relationship **relationships = NULL;
    size_t relationship_count = 0;
    if (thematic_return_detection_build_relationships(entries, entry_count, &relationships, &relationship_count) ==
        THEMATIC_RETURN_DETECTION_REL_OK) {
        record_relationships(index_db, relationships, relationship_count);
        thematic_return_detection_destroy_relationships(relationships, relationship_count);
    }
    free(entries);

    for (size_t i = 0; i < phrase_occurrence_count; i++) {
        if (has_window[i]) {
            owned_motif_window_destroy(&owned[i]);
        }
    }
    free(owned);
    free(windows);
    free(has_window);
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

    /* A malformed key signature meta-event doesn't fail an otherwise-valid
       import -- it just means no key signature is reported, same as a
       file that never had one. */
    MidiKeySignature key_signature = {false, 0, false};
    if (midi_key_signature_scan(midi_data, midi_size, &key_signature) != MIDI_KEY_SIGNATURE_OK) {
        key_signature = (MidiKeySignature){false, 0, false};
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

            SonorityGrouping grouping;
            if (segment_by_onset(events, count, &grouping)) {
                int priority_tonics[2];
                size_t priority_tonic_count = 0;
                priority_tonics[priority_tonic_count++] =
                    bass_pitch_class(&grouping.groups[grouping.group_count - 1], pitch_values);
                if (grouping.group_count > 1) {
                    priority_tonics[priority_tonic_count++] = bass_pitch_class(&grouping.groups[0], pitch_values);
                }

                KeyEstimate key = estimate_and_store_global_key(handle, index_db, events, pitch_values, count,
                                                                 key_signature, priority_tonics, priority_tonic_count);

                for (size_t i = 0; i < grouping.group_count; i++) {
                    identify_and_store_chord(handle, index_db, &grouping.groups[i], events, pitch_values, &key);
                }

                char **cadence_types =
                    run_cadence_classification(handle, index_db, &grouping, events, pitch_values, &key);

                PhraseBoundaryDetectionPoint *phrase_points = NULL;
                PhraseOccurrence **phrase_occurrences = NULL;
                size_t phrase_occurrence_count = 0;
                if (cadence_types) {
                    run_phrase_boundary_detection(handle, index_db, &grouping, events, cadence_types, &phrase_points,
                                                   &phrase_occurrences, &phrase_occurrence_count);
                }

                if (phrase_occurrence_count > 0) {
                    run_motif_detection(handle, index_db, phrase_occurrences, phrase_occurrence_count, events,
                                        pitch_values, count);
                    run_period_and_sentence_detection(handle, index_db, phrase_occurrences, phrase_occurrence_count,
                                                       &grouping, events, pitch_values, count, cadence_types);
                    run_form_and_thematic_return_detection(handle, index_db, phrase_occurrences,
                                                            phrase_occurrence_count, &grouping, events, pitch_values,
                                                            count, cadence_types);
                }

                if (phrase_occurrences) {
                    phrase_boundary_detection_destroy_occurrences(phrase_occurrences, phrase_occurrence_count);
                }
                free(phrase_points);

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
    for (size_t i = 0; i < summary->highlighted_note_id_count; i++) {
        free(summary->highlighted_note_ids[i]);
    }
    free(summary->highlighted_note_ids);
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
/* Decodes interpretation's claim into a display label. Key/Chord/
 * Cadence claims are plain null-terminated strings (unchanged from
 * before this branch); Motif/Phrase/Period/Sentence/Form Structure/
 * Thematic Return Detection's claims are structs (see each analyzer's
 * own header) -- value_type() (the type tag every analyzer sets when it
 * builds its claim Value, e.g. "motif-detection-claim") is what
 * distinguishes them, since claim itself is opaque bytes. Falls back to
 * treating the content as a string for any type not recognized here
 * (covers key/chord/cadence without needing to special-case them). */
static char *format_claim_label(const Interpretation *interpretation) {
    const Value *claim = interpretation_claim(interpretation);
    const char *claim_type = value_type(claim);
    size_t claim_size = 0;
    const void *content = value_content(claim, &claim_size);
    char buf[512];

    if (strcmp(claim_type, "motif-detection-claim") == 0 && claim_size == sizeof(MotifDetectionClaim)) {
        const MotifDetectionClaim *c = content;
        snprintf(buf, sizeof(buf), "motif occurrence: %zu matched event(s)", c->matched_event_id_count);
        return dup_string(buf);
    }
    if (claim_type && strcmp(claim_type, "phrase-segmentation-claim") == 0 &&
        claim_size == sizeof(PhraseSegmentationClaim)) {
        const PhraseSegmentationClaim *c = content;
        return dup_string(c->is_boundary ? "phrase boundary" : "not a phrase boundary");
    }
    if (claim_type && strcmp(claim_type, "phrase-boundary-detection-signal-claim") == 0 &&
        claim_size == sizeof(PhraseBoundaryDetectionSignalClaim)) {
        const PhraseBoundaryDetectionSignalClaim *c = content;
        if (c->signal == PHRASE_BOUNDARY_SIGNAL_CADENCE) {
            snprintf(buf, sizeof(buf), "boundary signal: cadence (%s)", c->cadence_type);
        } else {
            snprintf(buf, sizeof(buf), "boundary signal: contour/rhythm discontinuity");
        }
        return dup_string(buf);
    }
    if (claim_type && strcmp(claim_type, "period-detection-claim") == 0 && claim_size == sizeof(PeriodDetectionClaim)) {
        const PeriodDetectionClaim *c = content;
        snprintf(buf, sizeof(buf), "period: %s (%s) -> %s (%s)", c->antecedent_phrase_occurrence_id,
                 c->antecedent_cadence_type, c->consequent_phrase_occurrence_id, c->consequent_cadence_type);
        return dup_string(buf);
    }
    if (claim_type && strcmp(claim_type, "sentence-detection-claim") == 0 &&
        claim_size == sizeof(SentenceDetectionClaim)) {
        const SentenceDetectionClaim *c = content;
        snprintf(buf, sizeof(buf), "sentence: %s -> %s -> %s", c->statement_occurrence_id,
                 c->repetition_occurrence_id, c->continuation_occurrence_id);
        return dup_string(buf);
    }
    if (claim_type && strcmp(claim_type, "form-structure-detection-claim") == 0 &&
        claim_size == sizeof(FormStructureDetectionClaim)) {
        const FormStructureDetectionClaim *c = content;
        snprintf(buf, sizeof(buf), "form: %s -[%s]-> %s", c->earlier_section_id,
                 c->earlier_section_closing_cadence_type, c->later_section_id);
        return dup_string(buf);
    }
    if (claim_type && strcmp(claim_type, "thematic-return-detection-claim") == 0 &&
        claim_size == sizeof(ThematicReturnDetectionClaim)) {
        const ThematicReturnDetectionClaim *c = content;
        snprintf(buf, sizeof(buf), "thematic return: %s resembles %s", c->later_theme_occurrence_id,
                 c->earlier_theme_occurrence_id);
        return dup_string(buf);
    }

    return dup_string((const char *)content);
}

/*
 * Resolves out->highlighted_note_ids/count for interpretation, per type:
 * "phrase" targets a NoteEvent directly (interpretation_target_id IS the
 * NoteEvent id -- see phrase_segmentation.c/phrase_boundary_detection.c
 * in ursatz), so it is derived on the fly with no lookup needed.
 * "cadence"/"motif" targets a Structure/Occurrence that only ever
 * existed at analysis time (see kNoteTargetTableName above); those are
 * looked up by interpretation_id in index_db, which may have no rows
 * (piece analyzed before this table existed, or index_db is NULL) --
 * that is not an error, just an empty highlight set. Every other type
 * (key/chord/period/sentence/form/thematic_return) is left empty this
 * pass. Returns false only on allocation failure.
 */
static bool resolve_highlighted_note_ids(sqlite3 *index_db, const Interpretation *interpretation, const char *type,
                                           const char *interpretation_id, char ***out_ids, size_t *out_count) {
    *out_ids = NULL;
    *out_count = 0;

    if (strcmp(type, kIndexTypePhrase) == 0) {
        const EntityID *target_id = interpretation_target_id(interpretation);
        if (!target_id) {
            return true;
        }
        char **ids = malloc(sizeof(char *));
        if (!ids) {
            return false;
        }
        char xml_id[80];
        xml_id_from_entity_value(entity_id_value(target_id), xml_id, sizeof(xml_id));
        ids[0] = dup_string(xml_id);
        if (!ids[0]) {
            free(ids);
            return false;
        }
        *out_ids = ids;
        *out_count = 1;
        return true;
    }

    if (strcmp(type, kIndexTypeCadence) == 0 || strcmp(type, kIndexTypeMotif) == 0) {
        return read_interpretation_note_targets(index_db, interpretation_id, out_ids, out_count);
    }

    return true;
}

static bool load_interpretation_summary(PersistenceHandle *handle, sqlite3 *index_db, const char *interpretation_id,
                                          const char *type, UrsatzGuiInterpretationSummary *out) {
    Interpretation *interpretation = NULL;
    if (persistence_load_interpretation(handle, interpretation_id, &interpretation) != PERSISTENCE_INTERPRETATION_OK) {
        return false;
    }

    char *label = format_claim_label(interpretation);

    ProvenanceReferenceState provenance_state = provenance_reference_state(interpretation_provenance(interpretation));
    const char *provenance_state_str =
        (provenance_state == PROVENANCE_REFERENCE_PRESENT) ? "present" : "absent";

    char **highlighted_note_ids = NULL;
    size_t highlighted_note_id_count = 0;
    bool highlight_ok =
        resolve_highlighted_note_ids(index_db, interpretation, type, interpretation_id, &highlighted_note_ids,
                                      &highlighted_note_id_count);

    bool ok = false;
    out->interpretation_id = dup_string(interpretation_id);
    out->type = dup_string(type);
    out->label = label;
    out->produced_by_author = dup_string(interpretation_author(interpretation));
    out->produced_by_method = dup_string(interpretation_method(interpretation));
    out->provenance_state = dup_string(provenance_state_str);
    out->highlighted_note_ids = highlighted_note_ids;
    out->highlighted_note_id_count = highlighted_note_id_count;
    if (highlight_ok && out->interpretation_id && out->type && out->label && out->produced_by_author &&
        out->produced_by_method && out->provenance_state) {
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
    UrsatzGuiRelationshipSummary *relationships = NULL;
    size_t relationship_count = 0;
    if (index_db) {
        if (!read_interpretation_index(index_db, &index_ids, &index_types, &index_count) ||
            !read_relationship_index(index_db, &relationships, &relationship_count)) {
            sqlite3_close(index_db);
            free(filename);
            free_ids(index_ids, index_count);
            free_ids(index_types, index_count);
            persistence_close(handle);
            return URSATZ_GUI_ANALYSIS_ERR_ALLOC;
        }
    }

    UrsatzGuiInterpretationSummary *summaries = NULL;
    if (index_count > 0) {
        summaries = calloc(index_count, sizeof(UrsatzGuiInterpretationSummary));
        if (!summaries) {
            if (index_db) {
                sqlite3_close(index_db);
            }
            free_ids(index_ids, index_count);
            free_ids(index_types, index_count);
            persistence_close(handle);
            return URSATZ_GUI_ANALYSIS_ERR_ALLOC;
        }
    }

    size_t summary_count = 0;
    for (size_t i = 0; i < index_count; i++) {
        if (load_interpretation_summary(handle, index_db, index_ids[i], index_types[i], &summaries[summary_count])) {
            summary_count++;
        }
    }
    if (index_db) {
        sqlite3_close(index_db);
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
        for (size_t i = 0; i < relationship_count; i++) {
            relationship_summary_destroy_contents(&relationships[i]);
        }
        free(relationships);
        return URSATZ_GUI_ANALYSIS_ERR_ALLOC;
    }

    detail->id = id_copy;
    detail->filename = filename;
    detail->note_event_count = note_event_count;
    detail->interpretations = summaries;
    detail->interpretation_count = summary_count;
    detail->relationships = relationships;
    detail->relationship_count = relationship_count;

    *out_detail = detail;
    return URSATZ_GUI_ANALYSIS_OK;
}

UrsatzGuiAnalysisResult ursatz_gui_analysis_delete_piece(const char *pieces_dir, const char *id) {
    if (!is_valid_piece_id(id)) {
        return URSATZ_GUI_ANALYSIS_ERR_NOT_FOUND;
    }

    char db_path[320];
    snprintf(db_path, sizeof(db_path), "%s/%s.db", pieces_dir, id);

    if (!db_path_exists(db_path)) {
        return URSATZ_GUI_ANALYSIS_ERR_NOT_FOUND;
    }

    if (remove(db_path) != 0) {
        return URSATZ_GUI_ANALYSIS_ERR_PERSISTENCE_FAILED;
    }

    return URSATZ_GUI_ANALYSIS_OK;
}

UrsatzGuiAnalysisResult ursatz_gui_analysis_rename_piece(const char *pieces_dir, const char *id,
                                                          const char *new_filename) {
    if (!is_valid_piece_id(id)) {
        return URSATZ_GUI_ANALYSIS_ERR_NOT_FOUND;
    }

    char db_path[320];
    snprintf(db_path, sizeof(db_path), "%s/%s.db", pieces_dir, id);

    if (!db_path_exists(db_path)) {
        return URSATZ_GUI_ANALYSIS_ERR_NOT_FOUND;
    }

    sqlite3 *index_db = open_interpretation_index(db_path);
    if (!index_db) {
        return URSATZ_GUI_ANALYSIS_ERR_PERSISTENCE_FAILED;
    }
    record_original_filename(index_db, new_filename);
    sqlite3_close(index_db);

    return URSATZ_GUI_ANALYSIS_OK;
}

UrsatzGuiAnalysisResult ursatz_gui_analysis_get_piece_musicxml(const char *pieces_dir, const char *id,
                                                                char **out_xml, bool *out_cosmetic_meter,
                                                                MusicXmlExportResult *out_export_error) {
    *out_export_error = MUSICXML_EXPORT_OK;

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

    char *xml = NULL;
    MusicXmlExportResult result = musicxml_export_write(handle, &xml);
    persistence_close(handle);

    if (result != MUSICXML_EXPORT_OK) {
        fprintf(stderr, "ursatz_gui_analysis_get_piece_musicxml: musicxml_export_write failed for piece '%s' "
                         "with MusicXmlExportResult=%d\n",
                id, (int)result);
        *out_export_error = result;
        return URSATZ_GUI_ANALYSIS_ERR_PERSISTENCE_FAILED;
    }

    *out_xml = xml;
    *out_cosmetic_meter = strstr(xml, "ursatz:synthetic-meter") != NULL;
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
    for (size_t i = 0; i < detail->relationship_count; i++) {
        relationship_summary_destroy_contents(&detail->relationships[i]);
    }
    free(detail->relationships);
    free(detail->id);
    free(detail->filename);
    free(detail);
}
