#ifndef URSATZ_THEORY_KNOWLEDGE_KNOWLEDGE_INTERNAL_H
#define URSATZ_THEORY_KNOWLEDGE_KNOWLEDGE_INTERNAL_H

#include <cJSON.h>
#include <stdbool.h>
#include <stddef.h>

#include "theory/knowledge/instrument.h"
#include "theory/knowledge/knowledge_snapshot.h"
#include "theory/knowledge/vocabulary_entry.h"

/*
 * What the loader files of the knowledge snapshot share: the slot an entry is read into, the snapshot's own layout and the readers
 * every loader uses. Internal to library/theory/knowledge (the layer table lets no other folder include it). A loader file reads its
 * kind's fields into the slot and reports a problem through knowledge_fail.
 */

#define MATERIAL_STEP_MAX 12

typedef struct {
    KnowledgeEntry entry;
    PitchMaterial material;
    bool has_material;
    ChordType chord_type;
    bool has_chord_type;
    Relation relation;
    bool has_relation;
    Instrument instrument;
    bool has_instrument;
    EnsembleTemplate ensemble;
    bool has_ensemble;
    VocabularyEntry vocabulary;
    bool has_vocabulary;
    EnsembleSlot *ensemble_slots; /* the ensemble's slots: the template points into it */
    const char **aliases; /* the entry's aliases, in file order */
    size_t alias_count;
    const char *variant_target;  /* the ids a material or relation names, resolved once every entry is read */
    const char *rotation_target;
    const char *material_target;
    const char *path; /* the file the entry came from: valid only while the snapshot is built */
    char **owned;     /* texts copied for the loaders: key word, family, aliases and link targets */
    size_t owned_count;
    char *strings; /* the id, kind, subtype and name texts the entry and the loaders point into */
} Slot;

struct KnowledgeSnapshot {
    Slot *slots;
    size_t count;
    size_t capacity;
    char hash[17];
};

/* Writes "<path>: <id>: <field>: <problem>" into `message` (when it has room). `id` may be NULL. */
void knowledge_fail(char *message, size_t message_size, const char *path, const char *id, const char *field, const char *problem);

/* The member of an object by name, or NULL. */
const cJSON *knowledge_member(const cJSON *object, const char *name);

/* True for a JSON number with an integer value. */
bool knowledge_is_integer(const cJSON *item);

/* A string member of an entry. Missing gives NULL with *problem "missing" when required (else NULL); a non-string or empty one gives NULL and the problem. */
const cJSON *knowledge_text_field(const cJSON *entry, const char *name, bool required, const char **problem);

/* Copies `text` into the slot (freed with it) and returns the copy, or NULL when out of memory. */
char *knowledge_own(Slot *slot, const char *text);

/* The text of a non-empty string item, or NULL. */
const char *knowledge_string_of(const cJSON *item);

/* True for an integer item from `low` to `high`. */
bool knowledge_in_range(const cJSON *item, int low, int high);

/*
 * knowledge_vocabulary.c: reads the fields of a `vocabulary` entry of this subtype into slot->vocabulary (the caller sets its id and
 * name first). An unknown or missing subtype, a bad code or a missing or bad field of the subtype is refused with the field named.
 */
KnowledgeStatus knowledge_vocabulary_read(Slot *slot, const cJSON *entry, const char *subtype, const char *path, char *message,
                                          size_t message_size);

#endif
