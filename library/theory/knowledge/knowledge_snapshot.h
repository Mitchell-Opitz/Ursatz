#ifndef URSATZ_THEORY_KNOWLEDGE_KNOWLEDGE_SNAPSHOT_H
#define URSATZ_THEORY_KNOWLEDGE_KNOWLEDGE_SNAPSHOT_H

#include <stddef.h>

#include "model/pitch/chord_type.h"
#include "model/pitch/pitch_material.h"
#include "theory/knowledge/form_entry.h"
#include "theory/knowledge/instrument.h"
#include "theory/knowledge/pattern_entry.h"
#include "theory/knowledge/schema_entry.h"
#include "theory/knowledge/relation.h"
#include "theory/knowledge/rule_entry.h"
#include "theory/knowledge/vocabulary_entry.h"
#include "theory/knowledge/voicing_entry.h"

/*
 * The knowledge snapshot (target architecture 5): every entry of knowledge/ as the build embedded it, plus the entries of a user's
 * own files, read once and then never changed. Whoever calls the library builds it and passes it on; the library keeps no copy, so
 * two snapshots built in one process are independent. The snapshot holds each entry's common fields and, for the pitch material,
 * chord type and relation kinds, what the entry says (every field the loader reads is checked, and a bad one refuses the build with
 * the file, the id and the field). The loaded kinds are the pitch material, the chord type, the relation, the instrument (its
 * two subtypes, instrument and ensemble), the vocabulary (its five subtypes), the formal function (kind function, subtype
 * formal-function), the theme type and the voicing. Entries of other kinds are kept by their common fields until their loaders exist.
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

/*
 * The pitch material for a word, matched without regard to case against the id, then the key word, then the name, then an alias: the
 * first of these that matches any material decides. NULL when nothing matches or more than one material matches there.
 */
const PitchMaterial *knowledge_snapshot_material_named(const KnowledgeSnapshot *snapshot, const char *word);

/* The pitch materials in id order: how many, and the one at `index` (NULL out of range). */
size_t knowledge_snapshot_material_count(const KnowledgeSnapshot *snapshot);
const PitchMaterial *knowledge_snapshot_material_at(const KnowledgeSnapshot *snapshot, size_t index);

/* The chord type (kind chord-type) with this id, or NULL when there is none or the entry is another kind. */
const ChordType *knowledge_snapshot_chord_type(const KnowledgeSnapshot *snapshot, const char *id);

/* The chord types in id order: how many, and the one at `index` (NULL out of range). */
size_t knowledge_snapshot_chord_type_count(const KnowledgeSnapshot *snapshot);
const ChordType *knowledge_snapshot_chord_type_at(const KnowledgeSnapshot *snapshot, size_t index);

/* The voicing (kind voicing) with this id, or NULL when there is none or the entry is another kind. */
const VoicingEntry *knowledge_snapshot_voicing(const KnowledgeSnapshot *snapshot, const char *id);

/* The voicings in id order: how many, and the one at `index` (NULL out of range). */
size_t knowledge_snapshot_voicing_count(const KnowledgeSnapshot *snapshot);
const VoicingEntry *knowledge_snapshot_voicing_at(const KnowledgeSnapshot *snapshot, size_t index);

/* The pattern (kind pattern: a rhythm cell or an accompaniment) with this id, or NULL when there is none or the entry is another kind. */
const PatternEntry *knowledge_snapshot_pattern(const KnowledgeSnapshot *snapshot, const char *id);

/* The patterns in id order: how many, and the one at `index` (NULL out of range). */
size_t knowledge_snapshot_pattern_count(const KnowledgeSnapshot *snapshot);
const PatternEntry *knowledge_snapshot_pattern_at(const KnowledgeSnapshot *snapshot, size_t index);

/* The schema (kind schema: a galant schema) with this id, or NULL when there is none or the entry is another kind. */
const SchemaEntry *knowledge_snapshot_schema(const KnowledgeSnapshot *snapshot, const char *id);

/* The schemas in id order: how many, and the one at `index` (NULL out of range). */
size_t knowledge_snapshot_schema_count(const KnowledgeSnapshot *snapshot);
const SchemaEntry *knowledge_snapshot_schema_at(const KnowledgeSnapshot *snapshot, size_t index);

/*
 * The first chord type, in id order and not retired, whose members above the root make `mask_above_root`: bit n is a member n
 * semitones above the root, reduced to 0 to 11 (a ninth at 14 is bit 2). Bit 0 (the root) is ignored in the mask and in the
 * members. NULL when none does.
 */
const ChordType *knowledge_snapshot_chord_type_of(const KnowledgeSnapshot *snapshot, unsigned mask_above_root);

/* The relation (kind relation) with this id, or NULL when there is none or the entry is another kind. */
const Relation *knowledge_snapshot_relation(const KnowledgeSnapshot *snapshot, const char *id);

/* The relation for a word, matched without regard to case against the id, then the name, then an alias, as for a material. */
const Relation *knowledge_snapshot_relation_named(const KnowledgeSnapshot *snapshot, const char *word);

/* The instrument (kind instrument, subtype instrument) with this id, or NULL when there is none or the entry is another kind or subtype. */
const Instrument *knowledge_snapshot_instrument(const KnowledgeSnapshot *snapshot, const char *id);

/* The ensemble template (kind instrument, subtype ensemble) with this id, or NULL when there is none or it is another kind or subtype. */
const EnsembleTemplate *knowledge_snapshot_ensemble(const KnowledgeSnapshot *snapshot, const char *id);

/* The vocabulary entry (kind vocabulary, any of its five subtypes) with this id, or NULL when there is none or the entry is another kind. */
const VocabularyEntry *knowledge_snapshot_vocabulary(const KnowledgeSnapshot *snapshot, const char *id);

/* The rule (kind rule, any of its three subtypes) with this id, or NULL when there is none or the entry is another kind. */
const RuleEntry *knowledge_snapshot_rule(const KnowledgeSnapshot *snapshot, const char *id);

/*
 * The theme type (kind theme-type) whose beginning and ending are the formal functions with these codes ("presentation" and
 * "continuation": the library names of the theme shape table), or NULL when there is none.
 */
const ThemeType *knowledge_snapshot_theme_type_of(const KnowledgeSnapshot *snapshot, const char *beginning_code, const char *ending_code);

#endif
