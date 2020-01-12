#include "persistence/persistence_chord.h"

#include <stdio.h>

#include "identity/entity_id.h"
#include "persistence/persistence_interpretation.h"

PersistenceChordResult persistence_store_chord(PersistenceHandle *handle, const Chord *chord) {
    if (handle == NULL || chord == NULL) {
        return PERSISTENCE_CHORD_ERR_NULL_ARG;
    }

    const Interpretation *interpretation = chord_interpretation(chord);
    if (persistence_store_interpretation(handle, interpretation) != PERSISTENCE_INTERPRETATION_OK) {
        return PERSISTENCE_CHORD_ERR_DB;
    }

    const char *id_value = entity_id_value(interpretation_id(interpretation));

    sqlite3 *db = persistence_db(handle);
    const char *sql = "INSERT INTO chords (id, interpretation_id, label) VALUES (?, ?, ?)";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return PERSISTENCE_CHORD_ERR_DB;
    }

    sqlite3_bind_text(stmt, 1, id_value, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, id_value, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, chord_label(chord), -1, SQLITE_TRANSIENT);

    PersistenceChordResult result = PERSISTENCE_CHORD_OK;
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        result = PERSISTENCE_CHORD_ERR_DB;
    }

    sqlite3_finalize(stmt);
    return result;
}

PersistenceChordResult persistence_load_chord(PersistenceHandle *handle, const char *chord_id, Chord **out_chord) {
    if (handle == NULL || chord_id == NULL || out_chord == NULL) {
        return PERSISTENCE_CHORD_ERR_NULL_ARG;
    }

    sqlite3 *db = persistence_db(handle);
    const char *sql = "SELECT label FROM chords WHERE id = ?";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return PERSISTENCE_CHORD_ERR_DB;
    }
    sqlite3_bind_text(stmt, 1, chord_id, -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return PERSISTENCE_CHORD_ERR_DB;
    }
    const unsigned char *label_text = sqlite3_column_text(stmt, 0);
    char label[512];
    snprintf(label, sizeof(label), "%s", (const char *)label_text);
    sqlite3_finalize(stmt);

    Interpretation *interpretation = NULL;
    if (persistence_load_interpretation(handle, chord_id, &interpretation) != PERSISTENCE_INTERPRETATION_OK) {
        return PERSISTENCE_CHORD_ERR_DB;
    }

    Chord *chord = NULL;
    if (chord_create(interpretation, label, &chord) != CHORD_OK) {
        interpretation_destroy(interpretation);
        return PERSISTENCE_CHORD_ERR_DB;
    }

    *out_chord = chord;
    return PERSISTENCE_CHORD_OK;
}
