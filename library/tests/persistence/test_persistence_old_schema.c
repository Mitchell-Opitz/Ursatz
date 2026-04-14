#include "structure_test_util.h"

#include <sqlite3.h>

#include "persistence/persistence_cadence.h"
#include "persistence/persistence_motif.h"
#include "persistence/persistence_relationship.h"

/*
 * A database file made before the relationship and structure tables existed opens, gains the new tables without
 * touching the old ones, and stores a Relationship. The schema below is the one persistence.c created before this
 * change, verbatim.
 */

static const char *const OLD_SCHEMA =
"CREATE TABLE IF NOT EXISTS note_events ("
    "  id TEXT PRIMARY KEY,"
    "  sequence_order INTEGER NOT NULL,"
    "  anchor_numerator INTEGER NOT NULL,"
    "  anchor_denominator INTEGER NOT NULL,"
    "  duration_numerator INTEGER NOT NULL,"
    "  duration_denominator INTEGER NOT NULL,"
    "  pitch_token BLOB NOT NULL,"
    "  pitch_token_size INTEGER NOT NULL,"
    "  pitch_spelling_letter INTEGER,"
    "  pitch_spelling_accidental INTEGER,"
    "  pitch_spelling_octave INTEGER,"
    "  voice_reference_id TEXT,"
    "  provenance_state INTEGER NOT NULL,"
    "  provenance_id TEXT"
    ");"
    "CREATE TABLE IF NOT EXISTS uncertainties ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  kind INTEGER NOT NULL,"
    "  confidence_numerator INTEGER,"
    "  confidence_denominator INTEGER"
    ");"
    "CREATE TABLE IF NOT EXISTS uncertainty_candidates ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  uncertainty_id INTEGER NOT NULL REFERENCES uncertainties(id),"
    "  label TEXT NOT NULL,"
    "  weight_numerator INTEGER NOT NULL,"
    "  weight_denominator INTEGER NOT NULL"
    ");"
    "CREATE TABLE IF NOT EXISTS interpretations ("
    "  id TEXT PRIMARY KEY,"
    "  target_id TEXT NOT NULL,"
    "  claim_value_type TEXT NOT NULL,"
    "  claim_value_content BLOB NOT NULL,"
    "  claim_value_content_size INTEGER NOT NULL,"
    "  context_id TEXT,"
    "  evidence_id TEXT,"
    "  framework_reference TEXT,"
    "  uncertainty_id INTEGER NOT NULL REFERENCES uncertainties(id),"
    "  author TEXT NOT NULL,"
    "  method TEXT NOT NULL,"
    "  status INTEGER NOT NULL,"
    "  provenance_state INTEGER NOT NULL,"
    "  provenance_id TEXT"
    ");"
    "CREATE TABLE IF NOT EXISTS chords ("
    "  id TEXT PRIMARY KEY,"
    "  interpretation_id TEXT NOT NULL REFERENCES interpretations(id),"
    "  label TEXT NOT NULL"
    ");"
    "CREATE TABLE IF NOT EXISTS keys ("
    "  id TEXT PRIMARY KEY,"
    "  interpretation_id TEXT NOT NULL REFERENCES interpretations(id),"
    "  label TEXT NOT NULL"
    ");";

static long table_count(sqlite3 *db, const char *name) {
    sqlite3_stmt *stmt = NULL;
    assert(sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM sqlite_master WHERE type = 'table' AND name = ?", -1, &stmt,
                              NULL) == SQLITE_OK);
    sqlite3_bind_text(stmt, 1, name, -1, SQLITE_TRANSIENT);
    assert(sqlite3_step(stmt) == SQLITE_ROW);
    long count = (long)sqlite3_column_int64(stmt, 0);
    sqlite3_finalize(stmt);
    return count;
}

int main(void) {
    const char *path = "test_persistence_old_schema.sqlite";
    remove(path);

    /* a database as the old library left it, with a row in an old table */
    sqlite3 *raw = NULL;
    assert(sqlite3_open(path, &raw) == SQLITE_OK);
    assert(sqlite3_exec(raw, OLD_SCHEMA, NULL, NULL, NULL) == SQLITE_OK);
    assert(sqlite3_exec(raw,
                        "INSERT INTO uncertainties (kind) VALUES (0);"
                        "INSERT INTO note_events (id, sequence_order, anchor_numerator, anchor_denominator, "
                        "duration_numerator, duration_denominator, pitch_token, pitch_token_size, provenance_state) "
                        "VALUES ('old-note', 0, 0, 1, 1, 4, x'3c', 1, 0);",
                        NULL, NULL, NULL) == SQLITE_OK);
    assert(table_count(raw, "relationships") == 0 && table_count(raw, "motifs") == 0);
    sqlite3_close(raw);

    PersistenceHandle *handle = NULL;
    assert(persistence_open(path, &handle) == PERSISTENCE_OK);
    assert(table_count(persistence_db(handle), "relationships") == 1);
    assert(table_count(persistence_db(handle), "motifs") == 1);
    assert(table_count(persistence_db(handle), "cadences") == 1);
    assert(su_count(handle, "note_events") == 1); /* the old rows are untouched */
    assert(su_count(handle, "uncertainties") == 1);

    Relationship *original = su_relationship("rel-1", "old-note", "begins", "ghost", true);
    assert(persistence_store_relationship(handle, original) == PERSISTENCE_RELATIONSHIP_OK);
    const char *events[] = {"old-note"};
    Motif *motif = su_motif("motif-1", events, 1);
    assert(persistence_store_motif(handle, motif) == PERSISTENCE_MOTIF_OK);
    persistence_close(handle);

    /* reopening a database that already has the new tables is also fine */
    assert(persistence_open(path, &handle) == PERSISTENCE_OK);
    Relationship *loaded = NULL;
    assert(persistence_load_relationship(handle, "rel-1", &loaded) == PERSISTENCE_RELATIONSHIP_OK);
    su_assert_same_relationship(original, loaded);
    assert(su_count(handle, "note_events") == 1);

    relationship_destroy(loaded);
    relationship_destroy(original);
    motif_destroy(motif);
    persistence_close(handle);
    remove(path);
    printf("test_persistence_old_schema: all tests passed\n");
    return 0;
}
