#include <string.h>

#include "theory/knowledge/knowledge_internal.h"

/* K5 schema: a galant schema (content.function, bass_defining, melody_in_tenths, stages). */

static const char *const FUNCTIONS[] = {"opening-gambit", "continuation", "pre-cadential", "cadence", "post-cadential"};
static const char *const STRENGTHS[] = {"strong", "weak"};
static const char *const ALTERATIONS[] = {"natural", "lowered", "either", "raised"};
static const int ALTERATION_VALUES[] = {0, -1, 2, 1};

/* Reads a {"degree", "alteration"} object, or gives why it is not one. */
static const char *read_degree(const cJSON *item, SchemaDegree *out) {
    const cJSON *degree = knowledge_member(item, "degree");
    if (!cJSON_IsObject(item) || !knowledge_in_range(degree, 1, 7)) {
        return "a degree is not {\"degree\": 1 to 7, \"alteration\": ...}";
    }
    int alteration = knowledge_word_index(knowledge_string_of(knowledge_member(item, "alteration")), ALTERATIONS, 4);
    if (alteration < 0) {
        return "an alteration is not natural, lowered, either or raised";
    }
    out->degree = degree->valueint;
    out->alteration = ALTERATION_VALUES[alteration];
    return NULL;
}

static KnowledgeStatus read_galant(Slot *slot, const cJSON *content, const char *path, char *message, size_t message_size) {
    const char *id = slot->entry.id;
    SchemaEntry *schema = &slot->schema;
    int function = knowledge_word_index(knowledge_string_of(knowledge_member(content, "function")), FUNCTIONS, 5);
    if (function < 0) {
        knowledge_fail(message, message_size, path, id, "content.function",
                       "not opening-gambit, continuation, pre-cadential, cadence or post-cadential");
        return KNOWLEDGE_ERR_FIELD;
    }
    schema->function = FUNCTIONS[function];
    const cJSON *flags[2] = {knowledge_member(content, "bass_defining"), knowledge_member(content, "melody_in_tenths")};
    if (!cJSON_IsBool(flags[0]) || !cJSON_IsBool(flags[1])) {
        knowledge_fail(message, message_size, path, id, "content.bass_defining", "bass_defining and melody_in_tenths are not both booleans");
        return KNOWLEDGE_ERR_FIELD;
    }
    schema->bass_defining = cJSON_IsTrue(flags[0]);
    schema->melody_in_tenths = cJSON_IsTrue(flags[1]);
    const cJSON *stages = knowledge_member(content, "stages");
    int count = cJSON_IsArray(stages) ? cJSON_GetArraySize(stages) : -1;
    if (count < 1 || count > SCHEMA_GALANT_STAGES_MAX) {
        knowledge_fail(message, message_size, path, id, "content.stages", "not 1 to 8 stages");
        return KNOWLEDGE_ERR_FIELD;
    }
    const cJSON *item;
    cJSON_ArrayForEach(item, stages) {
        SchemaGalantStage *stage = &schema->galant[schema->stage_count++];
        int strength = knowledge_word_index(knowledge_string_of(knowledge_member(item, "strength")), STRENGTHS, 2);
        if (strength < 0) {
            knowledge_fail(message, message_size, path, id, "content.stages.strength", "not strong or weak");
            return KNOWLEDGE_ERR_FIELD;
        }
        stage->strong = strength == 0;
        const char *problem = read_degree(knowledge_member(item, "melody"), &stage->melody);
        if (problem == NULL) {
            problem = read_degree(knowledge_member(item, "bass"), &stage->bass);
        }
        if (problem != NULL) {
            knowledge_fail(message, message_size, path, id, "content.stages.degree", problem);
            return KNOWLEDGE_ERR_FIELD;
        }
        const cJSON *figures = knowledge_member(item, "figures");
        const char *numeral = knowledge_string_of(knowledge_member(item, "numeral"));
        const cJSON *dominant = knowledge_member(item, "numeral_dominant");
        if (!cJSON_IsString(figures) || numeral == NULL || (dominant != NULL && knowledge_string_of(dominant) == NULL)) {
            knowledge_fail(message, message_size, path, id, "content.stages.numeral", "figures, numeral or numeral_dominant is not a string");
            return KNOWLEDGE_ERR_FIELD;
        }
        stage->figures = knowledge_own(slot, figures->valuestring);
        stage->numeral = knowledge_own(slot, numeral);
        stage->numeral_dominant = dominant == NULL ? NULL : knowledge_own(slot, dominant->valuestring);
        if (stage->figures == NULL || stage->numeral == NULL || (dominant != NULL && stage->numeral_dominant == NULL)) {
            return KNOWLEDGE_ERR_ALLOC;
        }
        const cJSON *next = knowledge_member(item, "melody_next");
        stage->has_melody_next = next != NULL;
        if (next != NULL && (problem = read_degree(next, &stage->melody_next)) != NULL) {
            knowledge_fail(message, message_size, path, id, "content.stages.melody_next", problem);
            return KNOWLEDGE_ERR_FIELD;
        }
    }
    return KNOWLEDGE_OK;
}

KnowledgeStatus knowledge_schema_read(Slot *slot, const cJSON *entry, const char *subtype, const char *path, char *message,
                                      size_t message_size) {
    const cJSON *content = knowledge_member(entry, "content");
    slot->schema.id = slot->entry.id;
    slot->schema.name = slot->entry.name;
    if (!cJSON_IsObject(content) || subtype == NULL || strcmp(subtype, "galant") != 0) {
        knowledge_fail(message, message_size, path, slot->entry.id, "subtype", "not galant");
        return KNOWLEDGE_ERR_FIELD;
    }
    slot->schema.subtype = SCHEMA_GALANT;
    return read_galant(slot, content, path, message, message_size);
}

const SchemaEntry *knowledge_snapshot_schema(const KnowledgeSnapshot *snapshot, const char *id) {
    const Slot *slot = knowledge_find_slot(snapshot, id);
    return slot != NULL && slot->has_schema ? &slot->schema : NULL;
}

size_t knowledge_snapshot_schema_count(const KnowledgeSnapshot *snapshot) {
    size_t count = 0;
    for (size_t i = 0; snapshot != NULL && i < snapshot->count; i++) {
        count += snapshot->slots[i].has_schema ? 1 : 0;
    }
    return count;
}

const SchemaEntry *knowledge_snapshot_schema_at(const KnowledgeSnapshot *snapshot, size_t index) {
    for (size_t i = 0; snapshot != NULL && i < snapshot->count; i++) {
        if (snapshot->slots[i].has_schema && index-- == 0) {
            return &snapshot->slots[i].schema;
        }
    }
    return NULL;
}
