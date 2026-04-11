#ifndef URSATZ_TESTS_PERSISTENCE_STRUCTURE_TEST_UTIL_H
#define URSATZ_TESTS_PERSISTENCE_STRUCTURE_TEST_UTIL_H

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/occurrence.h"
#include "core/provenance_reference.h"
#include "core/value.h"
#include "identity/entity_id.h"
#include "persistence/persistence.h"
#include "relationship/relationship.h"
#include "structure/cadence.h"
#include "structure/motif.h"
#include "structure/motif_occurrence.h"
#include "structure/phrase_occurrence.h"

/*
 * Helpers for the relationship and structure repository tests: shell constructors from plain values, equality
 * checks, row counts, and a trigger that makes one chosen insert fail so a store can be seen to fail partway.
 */

static inline PersistenceHandle *su_open(const char *path) {
    remove(path);
    PersistenceHandle *handle = NULL;
    assert(persistence_open(path, &handle) == PERSISTENCE_OK);
    return handle;
}

static inline EntityID *su_id(const char *value) {
    EntityID *id = NULL;
    assert(entity_id_create(value, &id) == ENTITY_ID_OK);
    return id;
}

/* A Value whose content is size raw bytes (zero bytes and all). */
static inline Value *su_value(const char *type, const void *content, size_t size) {
    Value *value = NULL;
    assert(value_create(type, content, size, &value) == VALUE_OK);
    return value;
}

/* A newly allocated array of n new EntityIDs. */
static inline EntityID **su_ids(const char *const *values, size_t n) {
    EntityID **ids = (EntityID **)calloc(n, sizeof(EntityID *));
    assert(ids != NULL);
    for (size_t i = 0; i < n; i++) {
        ids[i] = su_id(values[i]);
    }
    return ids;
}

static inline ProvenanceReference *su_provenance(const char *id_or_null) {
    ProvenanceReference *reference = NULL;
    if (id_or_null != NULL) {
        assert(provenance_reference_create_present(su_id(id_or_null), &reference) == PROVENANCE_REFERENCE_OK);
    } else {
        assert(provenance_reference_create_absent(&reference) == PROVENANCE_REFERENCE_OK);
    }
    return reference;
}

static inline void su_assert_same_id(const EntityID *a, const EntityID *b) {
    assert((a == NULL) == (b == NULL));
    if (a != NULL) {
        assert(entity_id_equals(a, b));
    }
}

static inline void su_assert_same_value(const Value *a, const Value *b) {
    assert((a == NULL) == (b == NULL));
    if (a != NULL) {
        assert(value_equals(a, b));
    }
}

/* The value bytes with an embedded zero and a high byte, to prove the content is a blob and not text. */
static const unsigned char SU_BLOB[] = {0x00, 0x01, 0xFF, 0x00, 0x7F, 0x80};

/* Rows in a table. */
static inline long su_count(PersistenceHandle *handle, const char *table) {
    char sql[96];
    snprintf(sql, sizeof(sql), "SELECT COUNT(*) FROM %s", table);
    sqlite3_stmt *stmt = NULL;
    assert(sqlite3_prepare_v2(persistence_db(handle), sql, -1, &stmt, NULL) == SQLITE_OK);
    assert(sqlite3_step(stmt) == SQLITE_ROW);
    long count = (long)sqlite3_column_int64(stmt, 0);
    sqlite3_finalize(stmt);
    return count;
}

/* Makes every insert into table with column = poison fail (RAISE(ABORT)), so a store fails when it reaches that row. */
static inline void su_poison(PersistenceHandle *handle, const char *table, const char *column, const char *poison) {
    char sql[256];
    snprintf(sql, sizeof(sql),
             "CREATE TRIGGER poison_%s BEFORE INSERT ON %s WHEN NEW.%s = '%s' BEGIN SELECT RAISE(ABORT, 'poison'); END;",
             table, table, column, poison);
    assert(sqlite3_exec(persistence_db(handle), sql, NULL, NULL, NULL) == SQLITE_OK);
}

/* --- relationships --- */

/* full: every optional field set and provenance present; otherwise every optional field NULL and provenance absent. */
static inline Relationship *su_relationship(const char *id, const char *source, const char *predicate,
                                            const char *target, bool full) {
    Relationship *relationship = NULL;
    assert(relationship_create(su_id(id), su_id(source), predicate, su_id(target),
                               full ? su_value("scope", SU_BLOB, sizeof(SU_BLOB)) : NULL,
                               full ? su_id("context-1") : NULL,
                               full ? su_value("qualifiers", "q=1", 3) : NULL, full ? su_id("evidence-1") : NULL,
                               full ? su_value("uncertainty", "3/4", 3) : NULL,
                               su_provenance(full ? "provenance-1" : NULL), &relationship) == RELATIONSHIP_OK);
    return relationship;
}

static inline void su_assert_same_relationship(const Relationship *a, const Relationship *b) {
    su_assert_same_id(relationship_id(a), relationship_id(b));
    su_assert_same_id(relationship_source_id(a), relationship_source_id(b));
    assert(strcmp(relationship_predicate_name(a), relationship_predicate_name(b)) == 0);
    su_assert_same_id(relationship_target_id(a), relationship_target_id(b));
    su_assert_same_value(relationship_scope(a), relationship_scope(b));
    su_assert_same_id(relationship_context_id(a), relationship_context_id(b));
    su_assert_same_value(relationship_qualifiers(a), relationship_qualifiers(b));
    su_assert_same_id(relationship_evidence_id(a), relationship_evidence_id(b));
    su_assert_same_value(relationship_uncertainty(a), relationship_uncertainty(b));
    assert(provenance_reference_equals(relationship_provenance(a), relationship_provenance(b)));
}

/* --- structure --- */

static inline Motif *su_motif(const char *id, const char *const *events, size_t n) {
    Motif *motif = NULL;
    assert(motif_create(su_id(id), su_ids(events, n), n, &motif) == MOTIF_OK);
    return motif;
}

static inline void su_assert_same_motif(const Motif *a, const Motif *b) {
    su_assert_same_id(motif_id(a), motif_id(b));
    assert(motif_event_id_count(a) == motif_event_id_count(b));
    for (size_t i = 0; i < motif_event_id_count(a); i++) {
        su_assert_same_id(motif_event_id_at(a, i), motif_event_id_at(b, i));
    }
}

static inline MotifOccurrence *su_motif_occurrence(const char *id, const char *motif, const char *const *events,
                                                   size_t n) {
    Occurrence *occurrence = NULL;
    assert(occurrence_create(su_id(id), su_id(motif), &occurrence) == OCCURRENCE_OK);
    MotifOccurrence *result = NULL;
    assert(motif_occurrence_create(occurrence, su_ids(events, n), n, &result) == MOTIF_OCCURRENCE_OK);
    return result;
}

static inline void su_assert_same_motif_occurrence(const MotifOccurrence *a, const MotifOccurrence *b) {
    su_assert_same_id(motif_occurrence_id(a), motif_occurrence_id(b));
    su_assert_same_id(motif_occurrence_motif_id(a), motif_occurrence_motif_id(b));
    assert(motif_occurrence_region_event_id_count(a) == motif_occurrence_region_event_id_count(b));
    for (size_t i = 0; i < motif_occurrence_region_event_id_count(a); i++) {
        su_assert_same_id(motif_occurrence_region_event_id_at(a, i), motif_occurrence_region_event_id_at(b, i));
    }
}

static inline Cadence *su_cadence(const char *id, const char *const *events, size_t n, bool with_context) {
    Cadence *cadence = NULL;
    assert(cadence_create(su_id(id), su_ids(events, n), n,
                          with_context ? su_value("harmonic-context", SU_BLOB, sizeof(SU_BLOB)) : NULL,
                          &cadence) == CADENCE_OK);
    return cadence;
}

static inline void su_assert_same_cadence(const Cadence *a, const Cadence *b) {
    su_assert_same_id(cadence_id(a), cadence_id(b));
    assert(cadence_event_id_count(a) == cadence_event_id_count(b));
    for (size_t i = 0; i < cadence_event_id_count(a); i++) {
        su_assert_same_id(cadence_event_id_at(a, i), cadence_event_id_at(b, i));
    }
    su_assert_same_value(cadence_optional_harmonic_context(a), cadence_optional_harmonic_context(b));
}

static inline PhraseOccurrence *su_phrase_occurrence(const char *id, const char *phrase, const char *const *events,
                                                     size_t n) {
    Occurrence *occurrence = NULL;
    assert(occurrence_create(su_id(id), su_id(phrase), &occurrence) == OCCURRENCE_OK);
    PhraseOccurrence *result = NULL;
    assert(phrase_occurrence_create(occurrence, su_ids(events, n), n, &result) == PHRASE_OCCURRENCE_OK);
    return result;
}

static inline void su_assert_same_phrase_occurrence(const PhraseOccurrence *a, const PhraseOccurrence *b) {
    su_assert_same_id(phrase_occurrence_id(a), phrase_occurrence_id(b));
    su_assert_same_id(phrase_occurrence_phrase_id(a), phrase_occurrence_phrase_id(b));
    assert(phrase_occurrence_region_event_id_count(a) == phrase_occurrence_region_event_id_count(b));
    for (size_t i = 0; i < phrase_occurrence_region_event_id_count(a); i++) {
        su_assert_same_id(phrase_occurrence_region_event_id_at(a, i), phrase_occurrence_region_event_id_at(b, i));
    }
}

#endif
