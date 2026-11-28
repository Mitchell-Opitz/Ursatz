#include <string.h>

#include "theory/knowledge/knowledge_internal.h"

/* K5 schema: a galant schema (content.function, bass_defining, melody_in_tenths, stages) or a progression (stages, loop, rotations, role, materials, realizes). */

static const char *const FUNCTIONS[] = {"opening-gambit", "continuation", "pre-cadential", "cadence", "post-cadential"};
static const char *const STRENGTHS[] = {"strong", "weak"};
static const char *const ALTERATIONS[] = {"natural", "lowered", "either", "raised"};
static const int ALTERATION_VALUES[] = {0, -1, 2, 1};
static const char *const ROLES[] = {"loop", "opening", "continuation", "cadence"};

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

static KnowledgeStatus read_progression(Slot *slot, const cJSON *content, const char *path, char *message, size_t message_size) {
    const char *id = slot->entry.id;
    SchemaEntry *schema = &slot->schema;
    const cJSON *stages = knowledge_member(content, "stages");
    int count = cJSON_IsArray(stages) ? cJSON_GetArraySize(stages) : -1;
    if (count < 1 || count > SCHEMA_STAGES_MAX) {
        knowledge_fail(message, message_size, path, id, "content.stages", "not 1 to 16 stages");
        return KNOWLEDGE_ERR_FIELD;
    }
    const cJSON *item;
    cJSON_ArrayForEach(item, stages) {
        SchemaProgressionStage *stage = &schema->stages[schema->stage_count++];
        const char *chord = knowledge_string_of(knowledge_member(item, "chord"));
        if (chord == NULL) {
            knowledge_fail(message, message_size, path, id, "content.stages.chord", "a stage has no chord text");
            return KNOWLEDGE_ERR_FIELD;
        }
        const cJSON *units = knowledge_member(item, "units");
        if (units != NULL && !knowledge_in_range(units, 1, 8)) {
            knowledge_fail(message, message_size, path, id, "content.stages.units", "not an integer from 1 to 8");
            return KNOWLEDGE_ERR_FIELD;
        }
        stage->units = units == NULL ? 1u : (unsigned)units->valueint;
        stage->chord = knowledge_own(slot, chord);
        if (stage->chord == NULL) {
            return KNOWLEDGE_ERR_ALLOC;
        }
    }
    const cJSON *loop = knowledge_member(content, "loop");
    if (!cJSON_IsBool(loop)) {
        knowledge_fail(message, message_size, path, id, "content.loop", "not a boolean");
        return KNOWLEDGE_ERR_FIELD;
    }
    schema->loop = cJSON_IsTrue(loop);
    const cJSON *rotations = knowledge_member(content, "rotations");
    if (rotations != NULL && !cJSON_IsArray(rotations)) {
        knowledge_fail(message, message_size, path, id, "content.rotations", "not a list");
        return KNOWLEDGE_ERR_FIELD;
    }
    cJSON_ArrayForEach(item, rotations) {
        bool inside = knowledge_in_range(item, 0, (int)schema->stage_count - 1);
        for (size_t r = 0; inside && r < schema->rotation_count; r++) {
            inside = schema->rotations[r] != (unsigned)item->valueint;
        }
        if (!inside || schema->rotation_count == SCHEMA_ROTATIONS_MAX) {
            knowledge_fail(message, message_size, path, id, "content.rotations", "a rotation is repeated or is not a stage index of the schema");
            return KNOWLEDGE_ERR_FIELD;
        }
        schema->rotations[schema->rotation_count++] = (unsigned)item->valueint;
    }
    int role = knowledge_word_index(knowledge_string_of(knowledge_member(content, "role")), ROLES, 4);
    if (role < 0) {
        knowledge_fail(message, message_size, path, id, "content.role", "not loop, opening, continuation or cadence");
        return KNOWLEDGE_ERR_FIELD;
    }
    schema->role = ROLES[role];
    const cJSON *materials = knowledge_member(content, "materials");
    if (materials != NULL && (!cJSON_IsArray(materials) || cJSON_GetArraySize(materials) > SCHEMA_MATERIALS_MAX)) {
        knowledge_fail(message, message_size, path, id, "content.materials", "not a list of up to 8 material ids");
        return KNOWLEDGE_ERR_FIELD;
    }
    cJSON_ArrayForEach(item, materials) {
        const char *material = knowledge_string_of(item);
        schema->materials[schema->material_count] = material == NULL ? NULL : knowledge_own(slot, material);
        if (schema->materials[schema->material_count++] == NULL) {
            knowledge_fail(message, message_size, path, id, "content.materials", "a material is not an id text");
            return KNOWLEDGE_ERR_FIELD;
        }
    }
    const char *realizes = knowledge_string_of(knowledge_member(content, "realizes"));
    schema->realizes = realizes == NULL ? NULL : knowledge_own(slot, realizes);
    if (schema->realizes == NULL) {
        knowledge_fail(message, message_size, path, id, "content.realizes", "missing or not an id text");
        return KNOWLEDGE_ERR_FIELD;
    }
    return KNOWLEDGE_OK;
}

KnowledgeStatus knowledge_schema_read(Slot *slot, const cJSON *entry, const char *subtype, const char *path, char *message,
                                      size_t message_size) {
    const cJSON *content = knowledge_member(entry, "content");
    slot->schema.id = slot->entry.id;
    slot->schema.name = slot->entry.name;
    bool galant = subtype != NULL && strcmp(subtype, "galant") == 0;
    if (!cJSON_IsObject(content) || (!galant && (subtype == NULL || strcmp(subtype, "progression") != 0))) {
        knowledge_fail(message, message_size, path, slot->entry.id, "subtype", "not galant or progression");
        return KNOWLEDGE_ERR_FIELD;
    }
    slot->schema.subtype = galant ? SCHEMA_GALANT : SCHEMA_PROGRESSION;
    return galant ? read_galant(slot, content, path, message, message_size) : read_progression(slot, content, path, message, message_size);
}

KnowledgeStatus knowledge_schema_link(const KnowledgeSnapshot *snapshot, Slot *slot, char *message, size_t message_size) {
    if (!slot->has_schema || slot->schema.subtype != SCHEMA_PROGRESSION) {
        return KNOWLEDGE_OK;
    }
    const Slot *realizes = knowledge_find_slot(snapshot, slot->schema.realizes);
    if (realizes == NULL || !realizes->has_vocabulary || realizes->vocabulary.subtype != VOCABULARY_PROGRESSION_TYPE) {
        knowledge_fail(message, message_size, slot->path, slot->entry.id, "content.realizes", "is not a progression-type vocabulary entry");
        return KNOWLEDGE_ERR_FIELD;
    }
    for (size_t i = 0; i < slot->schema.material_count; i++) {
        const Slot *material = knowledge_find_slot(snapshot, slot->schema.materials[i]);
        if (material == NULL || !material->has_material) {
            knowledge_fail(message, message_size, slot->path, slot->entry.id, "content.materials", "an id is not a pitch material entry");
            return KNOWLEDGE_ERR_FIELD;
        }
    }
    return KNOWLEDGE_OK;
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
