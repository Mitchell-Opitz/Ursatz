#ifndef URSATZ_THEORY_KNOWLEDGE_KNOWLEDGE_INTERNAL_H
#define URSATZ_THEORY_KNOWLEDGE_KNOWLEDGE_INTERNAL_H

#include <cJSON.h>
#include <stdbool.h>
#include <stddef.h>

#include "theory/knowledge/instrument.h"
#include "theory/knowledge/form_entry.h"
#include "theory/knowledge/knowledge_snapshot.h"
#include "theory/knowledge/pattern_entry.h"
#include "theory/knowledge/schema_entry.h"
#include "theory/knowledge/rule_entry.h"
#include "theory/knowledge/voicing_entry.h"
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
    RuleEntry rule;
    bool has_rule;
    FormalFunction function;
    bool has_function;
    const char *harmony_targets[FORMAL_HARMONY_MAX]; /* the ids a formal function names, resolved once every entry is read */
    const char *against_target;
    VoicingEntry voicing;
    bool has_voicing;
    PatternEntry pattern;
    bool has_pattern;
    SchemaEntry schema;
    bool has_schema;
    const char *fit_targets[CHORD_TYPE_MEMBER_MAX]; /* the ids a voicing fits, resolved once every entry is read */
    const char *cadence_material_target;            /* the id a modal cadence kind names, resolved likewise */
    ThemeType theme_type;
    bool has_theme_type;
    const char *beginning_target;
    const char *ending_target;
    const char *idea_targets[THEME_TYPE_IDEA_MAX];
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
    cJSON *json;   /* the entry as loaded (a copy of its object), for knowledge_snapshot_entry_json */
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

/* True for lowercase letter words joined by single `separator` characters: "minor" (any separator), "harmonic minor" (' '). */
bool knowledge_lowercase_words(const char *text, char separator);

/* The slot of the entry with this id in a built snapshot (sorted by id), or NULL. */
const Slot *knowledge_find_slot(const KnowledgeSnapshot *snapshot, const char *id);

/* Reads the non-empty string content.<name> into *out (owned by the slot), or refuses naming the field. */
KnowledgeStatus knowledge_read_text(Slot *slot, const cJSON *content, const char *name, const char **out, const char *path,
                                    char *message, size_t message_size);

/* The role named by `text` ("third", "added-sixth"), or false. */
bool knowledge_chord_role(const char *text, ChordMemberRole *role);

/*
 * A chord numeral without alteration: "I" to "VII" (major) or "i" to "vii" (minor), an "o" after it for diminished. Gives its degree
 * 1 to 7 (the case and the "o" are not kept). False for an alteration, mixed case, a numeral past VII or any other text.
 */
bool knowledge_read_numeral(const char *text, int *degree);

/* The index of `text` among `count` words, or -1 (also for NULL). */
int knowledge_word_index(const char *text, const char *const *words, size_t count);

/* Reads content.<name> as one of `words` into *index, or refuses naming the field with `problem` ("missing" when absent). */
KnowledgeStatus knowledge_read_word(Slot *slot, const cJSON *content, const char *name, const char *const *words, size_t count,
                                    const char *problem, int *index, const char *path, char *message, size_t message_size);

/*
 * knowledge_form.c: reads the fields of a `function` entry of subtype formal-function, or of a `theme-type` entry, into the slot
 * (the caller sets the id and name first), keeping the ids they name; knowledge_form_link points them at the entries, once every
 * entry is read and sorted, or refuses naming the field. Both do nothing for a slot of another kind.
 */
KnowledgeStatus knowledge_form_read_function(Slot *slot, const cJSON *entry, const char *subtype, const char *path, char *message,
                                             size_t message_size);
KnowledgeStatus knowledge_form_read_theme_type(Slot *slot, const cJSON *entry, const char *path, char *message,
                                               size_t message_size);
KnowledgeStatus knowledge_form_link(const KnowledgeSnapshot *snapshot, Slot *slot, char *message, size_t message_size);

/*
 * knowledge_voicing.c: reads the fields of a `voicing` entry into slot->voicing (the caller sets its id and name first), keeping the
 * ids it fits; knowledge_voicing_link points them at the chord types, once every entry is read and sorted, or refuses naming the
 * field. The link does nothing for a slot of another kind.
 */
KnowledgeStatus knowledge_voicing_read(Slot *slot, const cJSON *entry, const char *path, char *message, size_t message_size);
KnowledgeStatus knowledge_voicing_link(const KnowledgeSnapshot *snapshot, Slot *slot, char *message, size_t message_size);

/*
 * knowledge_pattern.c: reads the fields of a `pattern` entry of this subtype (rhythm or accompaniment) into slot->pattern (the caller
 * sets its id and name first). A bad or missing field, or a rhythm that does not fill one bar of its meter, is refused with the field
 * named. The snapshot accessors for the kind are defined there too.
 */
KnowledgeStatus knowledge_pattern_read(Slot *slot, const cJSON *entry, const char *subtype, const char *path, char *message,
                                       size_t message_size);

/*
 * knowledge_schema.c: reads the fields of a `schema` entry of this subtype (galant or progression) into slot->schema (the caller sets its id and name
 * first). A missing or unknown subtype, a stage count outside 1 to 8, a degree outside 1 to 7 or an unknown strength, function or
 * alteration is refused with the field named. The snapshot accessors for the kind are defined there too.
 */
KnowledgeStatus knowledge_schema_read(Slot *slot, const cJSON *entry, const char *subtype, const char *path, char *message,
                                      size_t message_size);
/* Once every entry is read and sorted: a progression's `realizes` must be a progression-type vocabulary entry and each of its materials
   a pitch material, or the build is refused naming the field and the entry. Does nothing for a slot of another kind or subtype. */
KnowledgeStatus knowledge_schema_link(const KnowledgeSnapshot *snapshot, Slot *slot, char *message, size_t message_size);

/*
 * knowledge_vocabulary.c: reads the fields of a `vocabulary` entry of this subtype into slot->vocabulary (the caller sets its id and
 * name first). An unknown or missing subtype, a bad code or a missing or bad field of the subtype is refused with the field named.
 */
KnowledgeStatus knowledge_vocabulary_read(Slot *slot, const cJSON *entry, const char *subtype, const char *path, char *message,
                                          size_t message_size);

/*
 * knowledge_rule.c: reads the fields of a `rule` entry of this subtype into slot->rule (the caller sets its id and name first). An
 * unknown or missing subtype, a bad code, scope, message, function, weight or parameter, or a message placeholder outside the list
 * is refused with the field named.
 */
KnowledgeStatus knowledge_rule_read(Slot *slot, const cJSON *entry, const char *subtype, const char *path, char *message,
                                    size_t message_size);

#endif
