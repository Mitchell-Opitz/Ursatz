#ifndef URSATZ_THEORY_KNOWLEDGE_KNOWLEDGE_SNAPSHOT_H
#define URSATZ_THEORY_KNOWLEDGE_KNOWLEDGE_SNAPSHOT_H

#include <stddef.h>

#include "model/pitch/pitch_material.h"

/*
 * The knowledge snapshot (target architecture 5): every entry of knowledge/ as the build embedded it, plus the entries of a user's
 * own files, read once and then never changed. Whoever calls the library builds it and passes it on; the library keeps no copy, so
 * two snapshots built in one process are independent. The snapshot holds each entry's common fields and, for the pitch material
 * kind, its steps. Entries of other kinds are kept by their common fields until their loaders exist.
 */

typedef struct KnowledgeSnapshot KnowledgeSnapshot;

typedef enum {
    KNOWLEDGE_STATUS_REFERENCE,
    KNOWLEDGE_STATUS_ACTIVE,
    KNOWLEDGE_STATUS_RETIRED
} KnowledgeEntryStatus;

typedef struct {
    const char *id;
    const char *kind;
    const char *subtype; /* NULL when the entry has none */
    const char *name;
    KnowledgeEntryStatus status;
    int version;
    char hash[17]; /* 16 lowercase hex digits: FNV-1a 64 of the entry's compact JSON text */
} KnowledgeEntry;

/* A user's own knowledge file: its name (for messages) and its JSON text. Its entry ids all start with "user.". */
typedef struct {
    const char *path;
    const char *text;
} KnowledgeUserFile;

typedef enum {
    KNOWLEDGE_OK,
    KNOWLEDGE_ERR_NULL_ARGUMENT,
    KNOWLEDGE_ERR_PARSE,     /* a file is not JSON */
    KNOWLEDGE_ERR_FIELD,     /* an entry's field is missing, of the wrong type or out of range */
    KNOWLEDGE_ERR_DUPLICATE, /* an id appears twice */
    KNOWLEDGE_ERR_NAMESPACE, /* a repository id starts with "user.", or a user id does not */
    KNOWLEDGE_ERR_ALLOC
} KnowledgeStatus;

/*
 * Reads the embedded files and then each user file (user_files may be NULL when user_file_count is 0). On failure *out is left
 * untouched and `message` (when not NULL, with message_size above 0) says "<path>: <id>: <field>: <problem>"; on success it is
 * empty.
 */
KnowledgeStatus knowledge_snapshot_build(const KnowledgeUserFile *user_files, size_t user_file_count, KnowledgeSnapshot **out,
                                         char *message, size_t message_size);

void knowledge_snapshot_destroy(KnowledgeSnapshot *snapshot);

/* 16 lowercase hex digits: FNV-1a 64 over the lines "<id>=<entry hash>\n" in id order. Equal snapshots have equal hashes. */
const char *knowledge_snapshot_hash(const KnowledgeSnapshot *snapshot);

size_t knowledge_snapshot_entry_count(const KnowledgeSnapshot *snapshot);

/* The entry at `index` in id order, or NULL out of range. */
const KnowledgeEntry *knowledge_snapshot_entry_at(const KnowledgeSnapshot *snapshot, size_t index);

/* The entry with this id, or NULL. */
const KnowledgeEntry *knowledge_snapshot_entry(const KnowledgeSnapshot *snapshot, const char *id);

/* The pitch material (kind material, subtype unordered) with this id, or NULL when there is none or the entry is another kind. */
const PitchMaterial *knowledge_snapshot_material(const KnowledgeSnapshot *snapshot, const char *id);

#endif
