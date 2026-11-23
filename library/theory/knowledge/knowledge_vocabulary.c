#include <stdio.h>
#include <string.h>

#include "theory/knowledge/knowledge_internal.h"

/* K4 vocabulary: the common `code` and the fields of the entry's subtype. */

/* Lowercase letter words joined by single spaces, hyphens or underscores: "escape-tone", "melody and accompaniment". */
static bool code_text(const char *text) {
    size_t length = strlen(text);
    if (length == 0 || text[0] < 'a' || text[0] > 'z' || text[length - 1] < 'a' || text[length - 1] > 'z') {
        return false;
    }
    for (size_t i = 1; i + 1 < length; i++) {
        bool letter = text[i] >= 'a' && text[i] <= 'z';
        bool separator = text[i] == ' ' || text[i] == '_' || text[i] == '-';
        if (!letter && (!separator || text[i + 1] < 'a' || text[i + 1] > 'z')) {
            return false;
        }
    }
    return true;
}

/* A modal cadence kind: content.material, content.approach (1 to 3 numerals) and content.arrival, all three or none. The degrees are checked against the material's steps when it is linked. */
static KnowledgeStatus read_cadence_degrees(Slot *slot, const cJSON *content, const char *path, char *message, size_t message_size) {
    VocabularyCadence *cadence = &slot->vocabulary.cadence;
    const cJSON *material = knowledge_member(content, "material");
    const cJSON *approach = knowledge_member(content, "approach");
    const cJSON *arrival = knowledge_member(content, "arrival");
    if (material == NULL && approach == NULL && arrival == NULL) {
        return KNOWLEDGE_OK;
    }
    const char *target = knowledge_string_of(material);
    if (target == NULL || approach == NULL || arrival == NULL) {
        knowledge_fail(message, message_size, path, slot->entry.id, "content.material",
                       "material, approach and arrival come together: one is missing or the material is not an id");
        return KNOWLEDGE_ERR_FIELD;
    }
    int size = cJSON_IsArray(approach) ? cJSON_GetArraySize(approach) : -1;
    if (size < 1 || size > 3) {
        knowledge_fail(message, message_size, path, slot->entry.id, "content.approach", "not an array of 1 to 3 chord numerals");
        return KNOWLEDGE_ERR_FIELD;
    }
    const cJSON *item;
    cJSON_ArrayForEach(item, approach) {
        if (!knowledge_read_numeral(knowledge_string_of(item), &cadence->approach_degrees[cadence->approach_count])) {
            knowledge_fail(message, message_size, path, slot->entry.id, "content.approach", "not a chord numeral without alteration");
            return KNOWLEDGE_ERR_FIELD;
        }
        cadence->approach_count++;
    }
    if (!knowledge_read_numeral(knowledge_string_of(arrival), &cadence->arrival_degree)) {
        knowledge_fail(message, message_size, path, slot->entry.id, "content.arrival", "not a chord numeral without alteration");
        return KNOWLEDGE_ERR_FIELD;
    }
    slot->cadence_material_target = knowledge_own(slot, target);
    return slot->cadence_material_target != NULL ? KNOWLEDGE_OK : KNOWLEDGE_ERR_ALLOC;
}

static KnowledgeStatus read_cadence_kind(Slot *slot, const cJSON *content, const char *path, char *message, size_t message_size) {
    VocabularyCadence *cadence = &slot->vocabulary.cadence;
    KnowledgeStatus status = knowledge_read_text(slot, content, "label", &cadence->label, path, message, message_size);
    if (status == KNOWLEDGE_OK) {
        status = knowledge_read_text(slot, content, "abbreviation", &cadence->abbreviation, path, message, message_size);
    }
    if (status == KNOWLEDGE_OK) {
        status = knowledge_read_text(slot, content, "core", &cadence->core, path, message, message_size);
    }
    if (status != KNOWLEDGE_OK) {
        return status;
    }
    size_t length = strlen(cadence->abbreviation);
    bool capitals = length >= 2 && length <= 3;
    for (size_t i = 0; i < length; i++) {
        capitals = capitals && cadence->abbreviation[i] >= 'A' && cadence->abbreviation[i] <= 'Z';
    }
    if (!capitals) {
        knowledge_fail(message, message_size, path, slot->entry.id, "content.abbreviation", "not 2 or 3 capital letters");
        return KNOWLEDGE_ERR_FIELD;
    }
    const cJSON *strength = knowledge_member(content, "strength");
    if (!knowledge_in_range(strength, -1, 10)) {
        knowledge_fail(message, message_size, path, slot->entry.id, "content.strength",
                       strength == NULL ? "missing" : "not an integer from -1 to 10");
        return KNOWLEDGE_ERR_FIELD;
    }
    cadence->strength = strength->valueint;
    const cJSON *conclusive = knowledge_member(content, "conclusive");
    if (!cJSON_IsBool(conclusive)) {
        knowledge_fail(message, message_size, path, slot->entry.id, "content.conclusive",
                       conclusive == NULL ? "missing" : "not true or false");
        return KNOWLEDGE_ERR_FIELD;
    }
    cadence->conclusive = cJSON_IsTrue(conclusive);
    return read_cadence_degrees(slot, content, path, message, message_size);
}

static KnowledgeStatus read_embellishment(Slot *slot, const cJSON *content, const char *path, char *message, size_t message_size) {
    static const char *const MOTIONS[] = {"step", "leap", "static", "none"};
    static const char *const DIRECTIONS[] = {"same", "opposite", "up", "down", "none"};
    static const char *const METRICS[] = {"strong", "weak", "any"};
    VocabularyEmbellishment *tone = &slot->vocabulary.embellishment;
    int index = 0;
    KnowledgeStatus status = knowledge_read_word(slot, content, "approach", MOTIONS, 4, "not step, leap, static or none", &index, path, message,
                                       message_size);
    tone->approach = (EmbellishmentMotion)index;
    if (status == KNOWLEDGE_OK) {
        status = knowledge_read_word(slot, content, "leave", MOTIONS, 4, "not step, leap, static or none", &index, path, message, message_size);
        tone->leave = (EmbellishmentMotion)index;
    }
    if (status == KNOWLEDGE_OK) {
        status = knowledge_read_word(slot, content, "direction", DIRECTIONS, 5, "not same, opposite, up, down or none", &index, path, message,
                           message_size);
        tone->direction = (EmbellishmentDirection)index;
    }
    if (status == KNOWLEDGE_OK) {
        status = knowledge_read_word(slot, content, "metric", METRICS, 3, "not strong, weak or any", &index, path, message, message_size);
        tone->metric = (EmbellishmentMetric)index;
    }
    return status;
}

KnowledgeStatus knowledge_vocabulary_read(Slot *slot, const cJSON *entry, const char *subtype, const char *path, char *message,
                                          size_t message_size) {
    static const char *const SUBTYPES[] = {"cadence-kind", "embellishment", "texture", "progression-type", "function"};
    int index = knowledge_word_index(subtype, SUBTYPES, 5);
    if (index < 0) {
        knowledge_fail(message, message_size, path, slot->entry.id, "subtype",
                       subtype == NULL ? "missing" : "not cadence-kind, embellishment, texture, progression-type or function");
        return KNOWLEDGE_ERR_FIELD;
    }
    const cJSON *content = knowledge_member(entry, "content");
    if (!cJSON_IsObject(content)) {
        knowledge_fail(message, message_size, path, slot->entry.id, "content", "not an object");
        return KNOWLEDGE_ERR_FIELD;
    }
    slot->vocabulary.subtype = (VocabularySubtype)index;
    const char *code = knowledge_string_of(knowledge_member(content, "code"));
    if (code == NULL || !code_text(code)) {
        knowledge_fail(message, message_size, path, slot->entry.id, "content.code",
                       code == NULL ? "missing" : "not lowercase words");
        return KNOWLEDGE_ERR_FIELD;
    }
    slot->vocabulary.code = knowledge_own(slot, code);
    if (slot->vocabulary.code == NULL) {
        return KNOWLEDGE_ERR_ALLOC;
    }
    switch (slot->vocabulary.subtype) {
    case VOCABULARY_CADENCE_KIND:
        return read_cadence_kind(slot, content, path, message, message_size);
    case VOCABULARY_EMBELLISHMENT:
        return read_embellishment(slot, content, path, message, message_size);
    case VOCABULARY_TEXTURE:
        return knowledge_read_text(slot, content, "category", &slot->vocabulary.texture.category, path, message, message_size);
    default:
        return KNOWLEDGE_OK;
    }
}
