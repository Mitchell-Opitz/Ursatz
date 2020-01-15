#include "corpus/persistence_corpus.h"

#include <stdio.h>
#include <stdlib.h>

#include "identity/entity_id.h"

static const char *CORPUS_SCHEMA =
    "CREATE TABLE IF NOT EXISTS corpora ("
    "  id TEXT PRIMARY KEY,"
    "  name TEXT NOT NULL"
    ");";

static PersistenceCorpusResult ensure_schema(PersistenceHandle *handle) {
    sqlite3 *db = persistence_db(handle);
    char *errmsg = NULL;
    int rc = sqlite3_exec(db, CORPUS_SCHEMA, NULL, NULL, &errmsg);
    if (rc != SQLITE_OK) {
        sqlite3_free(errmsg);
        return PERSISTENCE_CORPUS_ERR_DB;
    }
    return PERSISTENCE_CORPUS_OK;
}

PersistenceCorpusResult persistence_store_corpus(PersistenceHandle *handle, const Corpus *corpus) {
    if (handle == NULL || corpus == NULL) {
        return PERSISTENCE_CORPUS_ERR_NULL_ARG;
    }

    PersistenceCorpusResult schema_result = ensure_schema(handle);
    if (schema_result != PERSISTENCE_CORPUS_OK) {
        return schema_result;
    }

    sqlite3 *db = persistence_db(handle);
    const char *sql = "INSERT INTO corpora (id, name) VALUES (?, ?)";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return PERSISTENCE_CORPUS_ERR_DB;
    }

    sqlite3_bind_text(stmt, 1, entity_id_value(corpus_id(corpus)), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, corpus_name(corpus), -1, SQLITE_TRANSIENT);

    PersistenceCorpusResult result = PERSISTENCE_CORPUS_OK;
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        result = PERSISTENCE_CORPUS_ERR_DB;
    }

    sqlite3_finalize(stmt);
    return result;
}

PersistenceCorpusResult persistence_load_corpus(PersistenceHandle *handle, const char *corpus_id_value,
                                                 Corpus **out_corpus) {
    if (handle == NULL || corpus_id_value == NULL || out_corpus == NULL) {
        return PERSISTENCE_CORPUS_ERR_NULL_ARG;
    }

    PersistenceCorpusResult schema_result = ensure_schema(handle);
    if (schema_result != PERSISTENCE_CORPUS_OK) {
        return schema_result;
    }

    sqlite3 *db = persistence_db(handle);
    const char *sql = "SELECT name FROM corpora WHERE id = ?";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return PERSISTENCE_CORPUS_ERR_DB;
    }
    sqlite3_bind_text(stmt, 1, corpus_id_value, -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return PERSISTENCE_CORPUS_ERR_DB;
    }
    const unsigned char *name_text = sqlite3_column_text(stmt, 0);
    char name[512];
    snprintf(name, sizeof(name), "%s", (const char *)name_text);
    sqlite3_finalize(stmt);

    EntityID *id = NULL;
    if (entity_id_create(corpus_id_value, &id) != ENTITY_ID_OK) {
        return PERSISTENCE_CORPUS_ERR_ALLOC;
    }

    Corpus *corpus = NULL;
    if (corpus_create(id, name, &corpus) != CORPUS_OK) {
        entity_id_destroy(id);
        return PERSISTENCE_CORPUS_ERR_ALLOC;
    }

    *out_corpus = corpus;
    return PERSISTENCE_CORPUS_OK;
}

PersistenceCorpusResult persistence_list_corpora(PersistenceHandle *handle, Corpus ***out_corpora,
                                                  size_t *out_count) {
    if (handle == NULL || out_corpora == NULL || out_count == NULL) {
        return PERSISTENCE_CORPUS_ERR_NULL_ARG;
    }

    PersistenceCorpusResult schema_result = ensure_schema(handle);
    if (schema_result != PERSISTENCE_CORPUS_OK) {
        return schema_result;
    }

    sqlite3 *db = persistence_db(handle);
    const char *sql = "SELECT id FROM corpora";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return PERSISTENCE_CORPUS_ERR_DB;
    }

    Corpus **corpora = NULL;
    size_t count = 0;
    int step_rc;
    while ((step_rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        const unsigned char *id_text = sqlite3_column_text(stmt, 0);
        Corpus *loaded = NULL;
        if (persistence_load_corpus(handle, (const char *)id_text, &loaded) != PERSISTENCE_CORPUS_OK) {
            for (size_t i = 0; i < count; i++) {
                corpus_destroy(corpora[i]);
            }
            free(corpora);
            sqlite3_finalize(stmt);
            return PERSISTENCE_CORPUS_ERR_DB;
        }

        Corpus **grown = (Corpus **)realloc(corpora, sizeof(Corpus *) * (count + 1));
        if (grown == NULL) {
            corpus_destroy(loaded);
            for (size_t i = 0; i < count; i++) {
                corpus_destroy(corpora[i]);
            }
            free(corpora);
            sqlite3_finalize(stmt);
            return PERSISTENCE_CORPUS_ERR_ALLOC;
        }
        corpora = grown;
        corpora[count] = loaded;
        count++;
    }
    sqlite3_finalize(stmt);

    if (step_rc != SQLITE_DONE) {
        for (size_t i = 0; i < count; i++) {
            corpus_destroy(corpora[i]);
        }
        free(corpora);
        return PERSISTENCE_CORPUS_ERR_DB;
    }

    *out_corpora = corpora;
    *out_count = count;
    return PERSISTENCE_CORPUS_OK;
}
