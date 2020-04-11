#include "persistence/persistence.h"

#include <stdlib.h>

struct PersistenceHandle {
    sqlite3 *db;
};

/*
 * Full schema for this module's scope (NoteEvent + Uncertainty +
 * Interpretation + Chord/Key), created up front so later store/load
 * commits only add functions, not tables. See docs/modules/persistence.md.
 */
static const char *PERSISTENCE_SCHEMA =
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

PersistenceResult persistence_open(const char *path, PersistenceHandle **out) {
    if (path == NULL || out == NULL) {
        return PERSISTENCE_ERR_NULL_ARG;
    }

    sqlite3 *db = NULL;
    int rc = sqlite3_open_v2(path, &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
    if (rc != SQLITE_OK) {
        if (db != NULL) {
            sqlite3_close(db);
        }
        return PERSISTENCE_ERR_OPEN_FAILED;
    }

    /*
     * Without a busy timeout, a second handle opened against the same
     * .db file while another handle holds a lock (e.g. two concurrent
     * requests on the same piece) gets SQLITE_BUSY immediately instead
     * of waiting. 5s gives concurrent opens/reads room to succeed
     * without a request hanging too long. This does not cover sustained
     * writer-vs-writer contention, which still serializes under
     * SQLite's own locking and can still time out under heavy write
     * load -- a separate concern from this fix.
     */
    sqlite3_busy_timeout(db, 5000);

    char *errmsg = NULL;
    rc = sqlite3_exec(db, PERSISTENCE_SCHEMA, NULL, NULL, &errmsg);
    if (rc != SQLITE_OK) {
        sqlite3_free(errmsg);
        sqlite3_close(db);
        return PERSISTENCE_ERR_SCHEMA_FAILED;
    }

    PersistenceHandle *handle = malloc(sizeof(PersistenceHandle));
    if (handle == NULL) {
        sqlite3_close(db);
        return PERSISTENCE_ERR_ALLOC;
    }

    handle->db = db;
    *out = handle;
    return PERSISTENCE_OK;
}

void persistence_close(PersistenceHandle *handle) {
    if (handle == NULL) {
        return;
    }
    sqlite3_close(handle->db);
    free(handle);
}

sqlite3 *persistence_db(PersistenceHandle *handle) {
    return handle->db;
}
