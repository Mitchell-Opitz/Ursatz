#include "theory/knowledge/knowledge_snapshot.h"

#include <cJSON.h>
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <common/hash.h>
#include "theory/knowledge/knowledge_embedded.h"
#include "theory/knowledge/knowledge_internal.h"

#define SOURCES_SCHEMA "ursatz-knowledge-sources"
#define USER_PREFIX "user."

void knowledge_fail(char *message, size_t message_size, const char *path, const char *id, const char *field,
                    const char *problem) {
    if (message != NULL && message_size > 0) {
        snprintf(message, message_size, "%s: %s: %s: %s", path, id != NULL ? id : "-", field, problem);
    }
}

static uint64_t fnv(uint64_t hash, const char *text) {
    return common_fnv1a64(text, strlen(text), hash);
}

static void slot_release(Slot *slot) {
    for (size_t i = 0; i < slot->owned_count; i++) {
        free(slot->owned[i]);
    }
    free(slot->owned);
    free(slot->aliases);
    free(slot->ensemble_slots);
    free(slot->strings);
    cJSON_Delete(slot->json);
}

void knowledge_snapshot_destroy(KnowledgeSnapshot *snapshot) {
    if (snapshot == NULL) {
        return;
    }
    for (size_t i = 0; i < snapshot->count; i++) {
        slot_release(&snapshot->slots[i]);
    }
    free(snapshot->slots);
    free(snapshot);
}

const cJSON *knowledge_member(const cJSON *object, const char *name) {
    return cJSON_GetObjectItemCaseSensitive(object, name);
}

bool knowledge_is_integer(const cJSON *item) {
    return cJSON_IsNumber(item) && item->valuedouble == (double)item->valueint;
}

/* Reads a steps array: 1 to 12 integers from 0 to 11, ascending, the first 0. Returns NULL, or the problem. */
static const char *read_steps(const cJSON *array, int steps[MATERIAL_STEP_MAX], size_t *count) {
    if (!cJSON_IsArray(array)) {
        return "not an array";
    }
    size_t n = (size_t)cJSON_GetArraySize(array);
    if (n < 1 || n > MATERIAL_STEP_MAX) {
        return "not 1 to 12 steps";
    }
    size_t i = 0;
    const cJSON *item;
    cJSON_ArrayForEach(item, array) {
        if (!knowledge_is_integer(item) || item->valueint < 0 || item->valueint > 11) {
            return "a step is not an integer from 0 to 11";
        }
        if ((i == 0 && item->valueint != 0) || (i > 0 && item->valueint <= steps[i - 1])) {
            return "steps are not ascending from 0";
        }
        steps[i++] = item->valueint;
    }
    *count = n;
    return NULL;
}

static bool read_status(const char *text, KnowledgeEntryStatus *status) {
    if (strcmp(text, "reference") == 0) {
        *status = KNOWLEDGE_STATUS_REFERENCE;
    } else if (strcmp(text, "active") == 0) {
        *status = KNOWLEDGE_STATUS_ACTIVE;
    } else if (strcmp(text, "retired") == 0) {
        *status = KNOWLEDGE_STATUS_RETIRED;
    } else {
        return false;
    }
    return true;
}

const cJSON *knowledge_text_field(const cJSON *entry, const char *name, bool required, const char **problem) {
    const cJSON *item = knowledge_member(entry, name);
    if (item == NULL) {
        *problem = required ? "missing" : NULL;
        return NULL;
    }
    if (!cJSON_IsString(item) || item->valuestring[0] == '\0') {
        *problem = "not a non-empty string";
        return NULL;
    }
    *problem = NULL;
    return item;
}

char *knowledge_own(Slot *slot, const char *text) {
    char **grown = realloc(slot->owned, (slot->owned_count + 1) * sizeof(char *));
    if (grown == NULL) {
        return NULL;
    }
    slot->owned = grown;
    size_t size = strlen(text) + 1;
    char *copy = malloc(size);
    if (copy == NULL) {
        return NULL;
    }
    memcpy(copy, text, size);
    slot->owned[slot->owned_count++] = copy;
    return copy;
}

/* A non-empty string member of an object, or NULL. */
const char *knowledge_string_of(const cJSON *item) {
    return cJSON_IsString(item) && item->valuestring[0] != '\0' ? item->valuestring : NULL;
}

/* Lowercase letter words joined by `separator`: "minor", "harmonic minor" (' ') or "neo-riemannian" ('-'). */
bool knowledge_lowercase_words(const char *text, char separator) {
    size_t length = strlen(text);
    if (length == 0 || text[0] == separator || text[length - 1] == separator) {
        return false;
    }
    for (size_t i = 0; i < length; i++) {
        bool letter = text[i] >= 'a' && text[i] <= 'z';
        if (!letter && (text[i] != separator || text[i + 1] == separator)) {
            return false;
        }
    }
    return true;
}

static bool nocase_equal(const char *a, const char *b) {
    while (*a != '\0' && *b != '\0') {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) {
            return false;
        }
        a++;
        b++;
    }
    return *a == *b;
}

static KnowledgeStatus read_aliases(Slot *slot, const cJSON *entry, const char *path, char *message, size_t message_size) {
    const cJSON *aliases = knowledge_member(entry, "aliases");
    if (aliases == NULL) {
        return KNOWLEDGE_OK;
    }
    if (!cJSON_IsArray(aliases)) {
        knowledge_fail(message, message_size, path, slot->entry.id, "aliases", "not an array of non-empty strings");
        return KNOWLEDGE_ERR_FIELD;
    }
    size_t count = (size_t)cJSON_GetArraySize(aliases);
    if (count == 0) {
        return KNOWLEDGE_OK;
    }
    slot->aliases = calloc(count, sizeof(char *));
    if (slot->aliases == NULL) {
        return KNOWLEDGE_ERR_ALLOC;
    }
    const cJSON *item;
    cJSON_ArrayForEach(item, aliases) {
        const char *text = knowledge_string_of(item);
        if (text == NULL) {
            knowledge_fail(message, message_size, path, slot->entry.id, "aliases", "not an array of non-empty strings");
            return KNOWLEDGE_ERR_FIELD;
        }
        slot->aliases[slot->alias_count] = knowledge_own(slot, text);
        if (slot->aliases[slot->alias_count] == NULL) {
            return KNOWLEDGE_ERR_ALLOC;
        }
        slot->alias_count++;
    }
    return KNOWLEDGE_OK;
}

/* "b3", "#4", "bb7": the degree (1 to 13) and the alteration against major. */
static bool read_color_note(const char *text, int *degree, int *alteration) {
    int shift = 0;
    size_t i = 0;
    for (; text[i] == '#' || text[i] == 'b'; i++) {
        shift += text[i] == '#' ? 1 : -1;
        if (i >= 2) {
            return false;
        }
    }
    if (text[i] < '1' || text[i] > '9') {
        return false;
    }
    int value = text[i] - '0';
    if (text[i + 1] != '\0') {
        if (text[i] != '1' || text[i + 1] < '0' || text[i + 1] > '3' || text[i + 2] != '\0') {
            return false;
        }
        value = 10 + (text[i + 1] - '0');
    }
    *degree = value;
    *alteration = shift;
    return true;
}

/* The links a material names: {"type": "variant-of" or "rotation-of", "target": id}. Other types are not read. */
static KnowledgeStatus read_material_links(Slot *slot, const cJSON *entry, const char *path, char *message, size_t message_size) {
    const cJSON *relations = knowledge_member(entry, "relations");
    if (relations == NULL) {
        return KNOWLEDGE_OK;
    }
    if (!cJSON_IsArray(relations)) {
        knowledge_fail(message, message_size, path, slot->entry.id, "relations", "not an array");
        return KNOWLEDGE_ERR_FIELD;
    }
    const cJSON *item;
    cJSON_ArrayForEach(item, relations) {
        const char *type = cJSON_IsObject(item) ? knowledge_string_of(knowledge_member(item, "type")) : NULL;
        const char *target = cJSON_IsObject(item) ? knowledge_string_of(knowledge_member(item, "target")) : NULL;
        if (type == NULL || target == NULL) {
            knowledge_fail(message, message_size, path, slot->entry.id, "relations", "an entry has no type and target");
            return KNOWLEDGE_ERR_FIELD;
        }
        const char **place = strcmp(type, "variant-of") == 0    ? &slot->variant_target
                             : strcmp(type, "rotation-of") == 0 ? &slot->rotation_target
                                                                : NULL;
        if (place == NULL) {
            continue;
        }
        if (*place != NULL) {
            knowledge_fail(message, message_size, path, slot->entry.id, "relations", "more than one entry of the same type");
            return KNOWLEDGE_ERR_FIELD;
        }
        *place = knowledge_own(slot, target);
        if (*place == NULL) {
            return KNOWLEDGE_ERR_ALLOC;
        }
    }
    return KNOWLEDGE_OK;
}

bool knowledge_read_numeral(const char *text, int *degree) {
    if (text == NULL) {
        return false;
    }
    size_t letters = strspn(text, "IViv");
    if (letters == 0 || (text[letters] != '\0' && (text[letters] != 'o' || text[letters + 1] != '\0'))) {
        return false;
    }
    bool upper = isupper((unsigned char)text[0]) != 0;
    for (int candidate = 1; candidate <= 7; candidate++) {
        char spelled[8] = {0};
        size_t length = 0;
        if (candidate == 4) {
            spelled[length++] = 'I';
            spelled[length++] = 'V';
        } else {
            if (candidate >= 5) {
                spelled[length++] = 'V';
            }
            for (int ones = candidate >= 5 ? candidate - 5 : candidate; ones > 0; ones--) {
                spelled[length++] = 'I';
            }
        }
        bool same = length == letters;
        for (size_t i = 0; same && i < length; i++) {
            char letter = upper ? spelled[i] : (char)(spelled[i] + ('a' - 'A'));
            same = text[i] == letter;
        }
        if (same) {
            *degree = candidate;
            return true;
        }
    }
    return false;
}

/* content.<field>: 0 to 7 chord numerals as degrees no higher than the step count, an alteration or a name past VII refused. */
static KnowledgeStatus read_numerals(const cJSON *content, const char *field, const char *path, const char *id, size_t step_count,
                                     size_t *count, int degrees[7], char *message, size_t message_size) {
    const cJSON *list = knowledge_member(content, field);
    if (list == NULL) {
        return KNOWLEDGE_OK;
    }
    char name[48];
    snprintf(name, sizeof(name), "content.%s", field);
    int size = cJSON_IsArray(list) ? cJSON_GetArraySize(list) : -1;
    if (size < 0 || size > 7) {
        knowledge_fail(message, message_size, path, id, name, "not an array of 0 to 7 chord numerals");
        return KNOWLEDGE_ERR_FIELD;
    }
    const cJSON *item;
    cJSON_ArrayForEach(item, list) {
        int degree = 0;
        const char *text = knowledge_string_of(item);
        if (text == NULL || !knowledge_read_numeral(text, &degree)) {
            knowledge_fail(message, message_size, path, id, name, "not a chord numeral without alteration such as \"ii\" or \"vio\"");
            return KNOWLEDGE_ERR_FIELD;
        }
        if ((size_t)degree > step_count) {
            knowledge_fail(message, message_size, path, id, name, "a numeral names a degree above the material's steps");
            return KNOWLEDGE_ERR_FIELD;
        }
        degrees[(*count)++] = degree;
    }
    return KNOWLEDGE_OK;
}

/* The modal fields of a material: primary and avoided chords, characteristic degrees and the cadence kind its mode closes with. */
static KnowledgeStatus read_material_modal(Slot *slot, const cJSON *content, const char *path, char *message, size_t message_size) {
    const char *id = slot->entry.id;
    PitchMaterial *material = &slot->material;
    KnowledgeStatus status = read_numerals(content, "primary_chords", path, id, material->step_count, &material->primary_count,
                                           material->primary_degrees, message, message_size);
    if (status == KNOWLEDGE_OK) {
        status = read_numerals(content, "chords_to_avoid", path, id, material->step_count, &material->avoided_count,
                               material->avoided_degrees, message, message_size);
    }
    if (status != KNOWLEDGE_OK) {
        return status;
    }
    const cJSON *degrees = knowledge_member(content, "characteristic_degrees");
    if (degrees != NULL) {
        int size = cJSON_IsArray(degrees) ? cJSON_GetArraySize(degrees) : -1;
        if (size < 1 || size > 3) {
            knowledge_fail(message, message_size, path, id, "content.characteristic_degrees", "not an array of 1 to 3 degrees");
            return KNOWLEDGE_ERR_FIELD;
        }
        const cJSON *item;
        cJSON_ArrayForEach(item, degrees) {
            if (!knowledge_in_range(item, 2, 7) || (size_t)item->valueint > material->step_count) {
                knowledge_fail(message, message_size, path, id, "content.characteristic_degrees", "not integers from 2 to the number of steps");
                return KNOWLEDGE_ERR_FIELD;
            }
            for (size_t i = 0; i < material->characteristic_count; i++) {
                if (material->characteristic_degrees[i] == item->valueint) {
                    knowledge_fail(message, message_size, path, id, "content.characteristic_degrees", "a degree is listed twice");
                    return KNOWLEDGE_ERR_FIELD;
                }
            }
            material->characteristic_degrees[material->characteristic_count++] = item->valueint;
        }
    }
    const cJSON *cadence = knowledge_member(content, "cadence");
    if (cadence != NULL) {
        const char *text = knowledge_string_of(cadence);
        if (text == NULL || strstr(text, ".vocabulary.") == NULL) {
            knowledge_fail(message, message_size, path, id, "content.cadence", "not the id of a vocabulary entry");
            return KNOWLEDGE_ERR_FIELD;
        }
        material->cadence = knowledge_own(slot, text);
        if (material->cadence == NULL) {
            return KNOWLEDGE_ERR_ALLOC;
        }
    }
    return KNOWLEDGE_OK;
}

/* The details of a K1 unordered material: steps and descending steps (read by the caller's steps), key word, family, brightness. */
static KnowledgeStatus read_material_details(Slot *slot, const cJSON *entry, const cJSON *content, const char *path, char *message,
                                             size_t message_size) {
    const char *id = slot->entry.id;
    PitchMaterial *material = &slot->material;
    const cJSON *key_word = knowledge_member(content, "key_word");
    material->key_word = slot->entry.name;
    if (key_word != NULL) {
        const char *text = knowledge_string_of(key_word);
        if (text == NULL || !knowledge_lowercase_words(text, ' ')) {
            knowledge_fail(message, message_size, path, id, "content.key_word", "not lowercase words");
            return KNOWLEDGE_ERR_FIELD;
        }
        material->key_word = knowledge_own(slot, text);
        if (material->key_word == NULL) {
            return KNOWLEDGE_ERR_ALLOC;
        }
    }
    const cJSON *family = knowledge_member(content, "family");
    if (family != NULL) {
        const char *text = knowledge_string_of(family);
        if (text == NULL || !knowledge_lowercase_words(text, '-')) {
            knowledge_fail(message, message_size, path, id, "content.family", "not lowercase words joined by hyphens");
            return KNOWLEDGE_ERR_FIELD;
        }
        material->family = knowledge_own(slot, text);
        if (material->family == NULL) {
            return KNOWLEDGE_ERR_ALLOC;
        }
    }
    const cJSON *rank = knowledge_member(content, "brightness_rank");
    if (rank != NULL) {
        if (!knowledge_is_integer(rank)) {
            knowledge_fail(message, message_size, path, id, "content.brightness_rank", "not an integer");
            return KNOWLEDGE_ERR_FIELD;
        }
        if (material->family == NULL || strcmp(material->family, "diatonic") != 0) {
            knowledge_fail(message, message_size, path, id, "content.brightness_rank", "needs the family \"diatonic\"");
            return KNOWLEDGE_ERR_FIELD;
        }
        material->has_brightness = true;
        material->brightness_rank = rank->valueint;
    }
    const cJSON *notes = knowledge_member(content, "color_notes");
    if (notes != NULL) {
        int count = cJSON_IsArray(notes) ? cJSON_GetArraySize(notes) : -1;
        if (count < 0 || count > MATERIAL_STEP_MAX) {
            knowledge_fail(message, message_size, path, id, "content.color_notes", "not an array of 0 to 12 color notes");
            return KNOWLEDGE_ERR_FIELD;
        }
        const cJSON *item;
        cJSON_ArrayForEach(item, notes) {
            const char *text = knowledge_string_of(item);
            size_t at = material->color_note_count;
            if (text == NULL || !read_color_note(text, &material->color_notes[at].degree, &material->color_notes[at].alteration)) {
                knowledge_fail(message, message_size, path, id, "content.color_notes", "not a color note such as \"b3\" or \"#4\"");
                return KNOWLEDGE_ERR_FIELD;
            }
            material->color_note_count++;
        }
    }
    KnowledgeStatus modal = read_material_modal(slot, content, path, message, message_size);
    if (modal != KNOWLEDGE_OK) {
        return modal;
    }
    return read_material_links(slot, entry, path, message, message_size);
}

bool knowledge_chord_role(const char *text, ChordMemberRole *role) {
    static const struct {
        const char *name;
        ChordMemberRole role;
    } ROLES[] = {{"root", CHORD_MEMBER_ROOT},
                 {"third", CHORD_MEMBER_THIRD},
                 {"fifth", CHORD_MEMBER_FIFTH},
                 {"seventh", CHORD_MEMBER_SEVENTH},
                 {"ninth", CHORD_MEMBER_NINTH},
                 {"eleventh", CHORD_MEMBER_ELEVENTH},
                 {"thirteenth", CHORD_MEMBER_THIRTEENTH},
                 {"added-second", CHORD_MEMBER_ADDED_SECOND},
                 {"added-sixth", CHORD_MEMBER_ADDED_SIXTH},
                 {"suspended-fourth", CHORD_MEMBER_SUSPENDED_FOURTH},
                 {"suspended-second", CHORD_MEMBER_SUSPENDED_SECOND}};
    for (size_t i = 0; i < sizeof(ROLES) / sizeof(ROLES[0]); i++) {
        if (strcmp(text, ROLES[i].name) == 0) {
            *role = ROLES[i].role;
            return true;
        }
    }
    return false;
}

/* K2: content.members, 1 to 8 members, the first the root at 0, each a role and 0 to 23 semitones. */
static KnowledgeStatus read_chord_type(Slot *slot, const cJSON *entry, const char *path, char *message, size_t message_size) {
    const char *id = slot->entry.id;
    const cJSON *content = knowledge_member(entry, "content");
    const cJSON *members = cJSON_IsObject(content) ? knowledge_member(content, "members") : NULL;
    if (members == NULL) {
        knowledge_fail(message, message_size, path, id, "content.members", "missing");
        return KNOWLEDGE_ERR_FIELD;
    }
    int count = cJSON_IsArray(members) ? cJSON_GetArraySize(members) : -1;
    if (count < 1 || count > CHORD_TYPE_MEMBER_MAX) {
        knowledge_fail(message, message_size, path, id, "content.members", "not 1 to 8 members");
        return KNOWLEDGE_ERR_FIELD;
    }
    const cJSON *construction = knowledge_member(content, "construction");
    if (construction != NULL) {
        static const char *const CONSTRUCTIONS[] = {"tertian", "quartal", "secundal", "added-note", "polychord", "compound", "cluster", "power"};
        int index = knowledge_word_index(knowledge_string_of(construction), CONSTRUCTIONS, 8);
        if (index < 0) {
            knowledge_fail(message, message_size, path, id, "content.construction",
                           "not tertian, quartal, secundal, added-note, polychord, compound, cluster or power");
            return KNOWLEDGE_ERR_FIELD;
        }
        slot->chord_type.construction = (ChordConstruction)index;
    }
    const cJSON *item;
    cJSON_ArrayForEach(item, members) {
        const char *role_text = cJSON_IsObject(item) ? knowledge_string_of(knowledge_member(item, "role")) : NULL;
        const cJSON *semitones = cJSON_IsObject(item) ? knowledge_member(item, "semitones") : NULL;
        ChordMemberRole role = CHORD_MEMBER_ROOT;
        if (role_text == NULL || !knowledge_chord_role(role_text, &role)) {
            knowledge_fail(message, message_size, path, id, "content.members", "a member has no known role");
            return KNOWLEDGE_ERR_FIELD;
        }
        if (!knowledge_is_integer(semitones) || semitones->valueint < 0 || semitones->valueint > 23) {
            knowledge_fail(message, message_size, path, id, "content.members", "a member's semitones are not an integer from 0 to 23");
            return KNOWLEDGE_ERR_FIELD;
        }
        size_t at = slot->chord_type.member_count;
        if (at == 0 && (role != CHORD_MEMBER_ROOT || semitones->valueint != 0)) {
            knowledge_fail(message, message_size, path, id, "content.members", "the first member is not the root at 0");
            return KNOWLEDGE_ERR_FIELD;
        }
        slot->chord_type.members[at].role = role;
        slot->chord_type.members[at].semitones = semitones->valueint;
        slot->chord_type.member_count++;
    }
    return KNOWLEDGE_OK;
}

static KnowledgeStatus read_anchor(Slot *slot, const cJSON *content, const char *path, char *message, size_t message_size) {
    const cJSON *anchor = knowledge_member(content, "anchor_default");
    slot->relation.anchor_default = RELATION_ANCHOR_HOME;
    if (anchor == NULL) {
        return KNOWLEDGE_OK;
    }
    const char *text = knowledge_string_of(anchor);
    if (text != NULL && strcmp(text, "home") == 0) {
        return KNOWLEDGE_OK;
    }
    if (text != NULL && strcmp(text, "previous") == 0) {
        slot->relation.anchor_default = RELATION_ANCHOR_PREVIOUS;
        return KNOWLEDGE_OK;
    }
    knowledge_fail(message, message_size, path, slot->entry.id, "content.anchor_default", "not home or previous");
    return KNOWLEDGE_ERR_FIELD;
}

bool knowledge_in_range(const cJSON *item, int low, int high) {
    return knowledge_is_integer(item) && item->valueint >= low && item->valueint <= high;
}

KnowledgeStatus knowledge_read_text(Slot *slot, const cJSON *content, const char *name, const char **out, const char *path,
                                    char *message, size_t message_size) {
    const char *text = knowledge_string_of(knowledge_member(content, name));
    if (text == NULL) {
        char field[64];
        snprintf(field, sizeof(field), "content.%s", name);
        knowledge_fail(message, message_size, path, slot->entry.id, field,
                       knowledge_member(content, name) == NULL ? "missing" : "not a non-empty string");
        return KNOWLEDGE_ERR_FIELD;
    }
    *out = knowledge_own(slot, text);
    return *out != NULL ? KNOWLEDGE_OK : KNOWLEDGE_ERR_ALLOC;
}

int knowledge_word_index(const char *text, const char *const *words, size_t count) {
    for (size_t i = 0; text != NULL && i < count; i++) {
        if (strcmp(text, words[i]) == 0) {
            return (int)i;
        }
    }
    return -1;
}

KnowledgeStatus knowledge_read_word(Slot *slot, const cJSON *content, const char *name, const char *const *words, size_t count,
                                    const char *problem, int *index, const char *path, char *message, size_t message_size) {
    *index = knowledge_word_index(knowledge_string_of(knowledge_member(content, name)), words, count);
    if (*index < 0) {
        char field[64];
        snprintf(field, sizeof(field), "content.%s", name);
        knowledge_fail(message, message_size, path, slot->entry.id, field,
                       knowledge_member(content, name) == NULL ? "missing" : problem);
        return KNOWLEDGE_ERR_FIELD;
    }
    return KNOWLEDGE_OK;
}

static KnowledgeStatus read_tonal(Slot *slot, const cJSON *content, const char *path, char *message, size_t message_size) {
    const char *id = slot->entry.id;
    const cJSON *semitones = knowledge_member(content, "root_semitones");
    const cJSON *degree = knowledge_member(content, "degree");
    if ((semitones == NULL) == (degree == NULL)) {
        knowledge_fail(message, message_size, path, id, "content", "needs exactly one of root_semitones and degree");
        return KNOWLEDGE_ERR_FIELD;
    }
    if (semitones != NULL && !knowledge_in_range(semitones, 0, 11)) {
        knowledge_fail(message, message_size, path, id, "content.root_semitones", "not an integer from 0 to 11");
        return KNOWLEDGE_ERR_FIELD;
    }
    if (degree != NULL && !knowledge_in_range(degree, 1, 7)) {
        knowledge_fail(message, message_size, path, id, "content.degree", "not an integer from 1 to 7");
        return KNOWLEDGE_ERR_FIELD;
    }
    slot->relation.tonal.by_degree = degree != NULL;
    slot->relation.tonal.root_semitones = semitones != NULL ? semitones->valueint : 0;
    slot->relation.tonal.degree = degree != NULL ? degree->valueint : 0;
    const char *material = knowledge_string_of(knowledge_member(content, "material"));
    if (material == NULL) {
        knowledge_fail(message, message_size, path, id, "content.material", "missing");
        return KNOWLEDGE_ERR_FIELD;
    }
    if (strcmp(material, "same") == 0) {
        slot->relation.tonal.material = RELATION_MATERIAL_SAME;
    } else if (strcmp(material, "parallel") == 0) {
        slot->relation.tonal.material = RELATION_MATERIAL_PARALLEL;
    } else if (strcmp(material, "diatonic") == 0) {
        slot->relation.tonal.material = RELATION_MATERIAL_DIATONIC;
        if (degree == NULL) {
            knowledge_fail(message, message_size, path, id, "content.material", "\"diatonic\" needs a degree");
            return KNOWLEDGE_ERR_FIELD;
        }
    } else {
        slot->relation.tonal.material = RELATION_MATERIAL_ENTRY;
        slot->material_target = knowledge_own(slot, material);
        if (slot->material_target == NULL) {
            return KNOWLEDGE_ERR_ALLOC;
        }
    }
    return read_anchor(slot, content, path, message, message_size);
}

static KnowledgeStatus read_modal(Slot *slot, const cJSON *content, const char *path, char *message, size_t message_size) {
    const char *id = slot->entry.id;
    const cJSON *center = knowledge_member(content, "center");
    if (cJSON_IsString(center) && strcmp(center->valuestring, "same") == 0) {
        slot->relation.modal.same_center = true;
    } else if (knowledge_in_range(center, 0, 11)) {
        slot->relation.modal.center = center->valueint;
    } else {
        knowledge_fail(message, message_size, path, id, "content.center", center == NULL ? "missing" : "not \"same\" or an integer from 0 to 11");
        return KNOWLEDGE_ERR_FIELD;
    }
    const cJSON *steps = knowledge_member(content, "brightness_steps");
    if (!knowledge_is_integer(steps)) {
        knowledge_fail(message, message_size, path, id, "content.brightness_steps", steps == NULL ? "missing" : "not an integer");
        return KNOWLEDGE_ERR_FIELD;
    }
    slot->relation.modal.brightness_steps = steps->valueint;
    return read_anchor(slot, content, path, message, message_size);
}

static KnowledgeStatus read_neo_riemannian(Slot *slot, const cJSON *content, const char *path, char *message,
                                           size_t message_size) {
    const cJSON *moves = knowledge_member(content, "moves");
    int count = cJSON_IsArray(moves) ? cJSON_GetArraySize(moves) : -1;
    if (count < 1 || count > RELATION_MOVE_MAX) {
        knowledge_fail(message, message_size, path, slot->entry.id, "content.moves", "not 1 to 8 moves");
        return KNOWLEDGE_ERR_FIELD;
    }
    const cJSON *item;
    cJSON_ArrayForEach(item, moves) {
        const char *text = knowledge_string_of(item);
        if (text == NULL || text[1] != '\0' || (text[0] != 'P' && text[0] != 'L' && text[0] != 'R')) {
            knowledge_fail(message, message_size, path, slot->entry.id, "content.moves", "a move is not P, L or R");
            return KNOWLEDGE_ERR_FIELD;
        }
        slot->relation.neo_riemannian.moves[slot->relation.neo_riemannian.move_count++] = text[0];
    }
    return KNOWLEDGE_OK;
}

static KnowledgeStatus read_set(Slot *slot, const cJSON *content, const char *path, char *message, size_t message_size) {
    const cJSON *operation = knowledge_member(content, "operation");
    const char *text = knowledge_string_of(operation);
    if (text == NULL || (strcmp(text, "T") != 0 && strcmp(text, "I") != 0)) {
        knowledge_fail(message, message_size, path, slot->entry.id, "content.operation", "not T or I");
        return KNOWLEDGE_ERR_FIELD;
    }
    const cJSON *n = knowledge_member(content, "n");
    if (!knowledge_in_range(n, 0, 11)) {
        knowledge_fail(message, message_size, path, slot->entry.id, "content.n", "not an integer from 0 to 11");
        return KNOWLEDGE_ERR_FIELD;
    }
    slot->relation.set.inversion = text[0] == 'I';
    slot->relation.set.n = n->valueint;
    return KNOWLEDGE_OK;
}

/* K6: by subtype. A serial entry is kept by its common fields only. */
static KnowledgeStatus read_relation(Slot *slot, const cJSON *entry, const char *subtype, const char *path, char *message,
                                     size_t message_size) {
    const cJSON *content = knowledge_member(entry, "content");
    if (subtype == NULL) {
        knowledge_fail(message, message_size, path, slot->entry.id, "subtype", "missing");
        return KNOWLEDGE_ERR_FIELD;
    }
    if (strcmp(subtype, "serial") == 0) {
        slot->relation.family = RELATION_FAMILY_SERIAL;
        return KNOWLEDGE_OK;
    }
    static const struct {
        const char *subtype;
        RelationFamily family;
    } FAMILIES[] = {{"tonal", RELATION_FAMILY_TONAL},
                    {"modal", RELATION_FAMILY_MODAL},
                    {"neo-riemannian", RELATION_FAMILY_NEO_RIEMANNIAN},
                    {"set", RELATION_FAMILY_SET}};
    for (size_t i = 0; i < sizeof(FAMILIES) / sizeof(FAMILIES[0]); i++) {
        if (strcmp(subtype, FAMILIES[i].subtype) != 0) {
            continue;
        }
        if (!cJSON_IsObject(content)) {
            knowledge_fail(message, message_size, path, slot->entry.id, "content", "missing");
            return KNOWLEDGE_ERR_FIELD;
        }
        slot->relation.family = FAMILIES[i].family;
        switch (FAMILIES[i].family) {
        case RELATION_FAMILY_TONAL:
            return read_tonal(slot, content, path, message, message_size);
        case RELATION_FAMILY_MODAL:
            return read_modal(slot, content, path, message, message_size);
        case RELATION_FAMILY_NEO_RIEMANNIAN:
            return read_neo_riemannian(slot, content, path, message, message_size);
        default:
            return read_set(slot, content, path, message, message_size);
        }
    }
    knowledge_fail(message, message_size, path, slot->entry.id, "subtype", "not tonal, modal, neo-riemannian, set or serial");
    return KNOWLEDGE_ERR_FIELD;
}

static bool clef_of(const char *text, InstrumentClef *clef) {
    static const struct {
        const char *name;
        InstrumentClef clef;
    } CLEFS[] = {{"treble", INSTRUMENT_CLEF_TREBLE},
                 {"bass", INSTRUMENT_CLEF_BASS},
                 {"alto", INSTRUMENT_CLEF_ALTO},
                 {"tenor", INSTRUMENT_CLEF_TENOR}};
    for (size_t i = 0; i < sizeof(CLEFS) / sizeof(CLEFS[0]); i++) {
        if (strcmp(text, CLEFS[i].name) == 0) {
            *clef = CLEFS[i].clef;
            return true;
        }
    }
    return false;
}

/* A {lowest, highest} object of MIDI numbers: 0 to 127, lowest at most highest. Returns NULL, or the problem. */
static const char *read_midi_range(const cJSON *object, int *lowest, int *highest) {
    if (!cJSON_IsObject(object)) {
        return "not an object with lowest and highest";
    }
    const cJSON *low = knowledge_member(object, "lowest");
    const cJSON *high = knowledge_member(object, "highest");
    if (!knowledge_in_range(low, 0, 127) || !knowledge_in_range(high, 0, 127)) {
        return "lowest and highest are not integers from 0 to 127";
    }
    if (low->valueint > high->valueint) {
        return "lowest is above highest";
    }
    *lowest = low->valueint;
    *highest = high->valueint;
    return NULL;
}

static bool instrument_family(const char *text) {
    static const char *const FAMILIES[] = {"keyboard", "strings", "woodwinds", "brass", "percussion", "voice", "plucked"};
    for (size_t i = 0; i < sizeof(FAMILIES) / sizeof(FAMILIES[0]); i++) {
        if (strcmp(text, FAMILIES[i]) == 0) {
            return true;
        }
    }
    return false;
}

/* K12 instrument: family, transposition, clefs, range, comfortable_range, general_midi_program and pedals of content. */
static KnowledgeStatus read_instrument(Slot *slot, const cJSON *entry, const char *path, char *message, size_t message_size) {
    const char *id = slot->entry.id;
    const cJSON *content = knowledge_member(entry, "content");
    if (!cJSON_IsObject(content)) {
        knowledge_fail(message, message_size, path, id, "content", "not an object");
        return KNOWLEDGE_ERR_FIELD;
    }
    Instrument *instrument = &slot->instrument;
    const char *family = knowledge_string_of(knowledge_member(content, "family"));
    if (family == NULL || !instrument_family(family)) {
        knowledge_fail(message, message_size, path, id, "content.family",
             family == NULL ? "missing" : "not keyboard, strings, woodwinds, brass, percussion, voice or plucked");
        return KNOWLEDGE_ERR_FIELD;
    }
    instrument->family = knowledge_own(slot, family);
    if (instrument->family == NULL) {
        return KNOWLEDGE_ERR_ALLOC;
    }
    if (!knowledge_in_range(knowledge_member(content, "transposition"), -36, 36)) {
        knowledge_fail(message, message_size, path, id, "content.transposition", "not an integer from -36 to 36");
        return KNOWLEDGE_ERR_FIELD;
    }
    instrument->transposition = knowledge_member(content, "transposition")->valueint;
    const cJSON *clefs = knowledge_member(content, "clefs");
    int clef_count = cJSON_IsArray(clefs) ? cJSON_GetArraySize(clefs) : -1;
    if (clef_count < 1 || clef_count > INSTRUMENT_STAVES_MAX) {
        knowledge_fail(message, message_size, path, id, "content.clefs", "not 1 to 4 clefs");
        return KNOWLEDGE_ERR_FIELD;
    }
    const cJSON *item;
    cJSON_ArrayForEach(item, clefs) {
        const char *text = knowledge_string_of(item);
        if (text == NULL || !clef_of(text, &instrument->clefs[instrument->staff_count])) {
            knowledge_fail(message, message_size, path, id, "content.clefs", "a clef is not treble, bass, alto or tenor");
            return KNOWLEDGE_ERR_FIELD;
        }
        instrument->staff_count++;
    }
    const char *problem = read_midi_range(knowledge_member(content, "range"), &instrument->lowest, &instrument->highest);
    if (problem != NULL) {
        knowledge_fail(message, message_size, path, id, "content.range", knowledge_member(content, "range") == NULL ? "missing" : problem);
        return KNOWLEDGE_ERR_FIELD;
    }
    const cJSON *comfortable = knowledge_member(content, "comfortable_range");
    if (comfortable != NULL) {
        problem = read_midi_range(comfortable, &instrument->comfortable_lowest, &instrument->comfortable_highest);
        if (problem == NULL && (instrument->comfortable_lowest < instrument->lowest ||
                                instrument->comfortable_highest > instrument->highest)) {
            problem = "is not inside the range";
        }
        if (problem != NULL) {
            knowledge_fail(message, message_size, path, id, "content.comfortable_range", problem);
            return KNOWLEDGE_ERR_FIELD;
        }
        instrument->has_comfortable_range = true;
    }
    if (!knowledge_in_range(knowledge_member(content, "general_midi_program"), 0, 127)) {
        knowledge_fail(message, message_size, path, id, "content.general_midi_program",
             knowledge_member(content, "general_midi_program") == NULL ? "missing" : "not an integer from 0 to 127");
        return KNOWLEDGE_ERR_FIELD;
    }
    instrument->general_midi_program = knowledge_member(content, "general_midi_program")->valueint;
    const cJSON *pedals = knowledge_member(content, "pedals");
    if (pedals != NULL) {
        if (!cJSON_IsArray(pedals)) {
            knowledge_fail(message, message_size, path, id, "content.pedals", "not an array");
            return KNOWLEDGE_ERR_FIELD;
        }
        cJSON_ArrayForEach(item, pedals) {
            const char *text = knowledge_string_of(item);
            if (text == NULL || strcmp(text, "sustain") != 0 || instrument->sustain_pedal) {
                knowledge_fail(message, message_size, path, id, "content.pedals", "not a list of distinct pedals from: sustain");
                return KNOWLEDGE_ERR_FIELD;
            }
            instrument->sustain_pedal = true;
        }
    }
    return KNOWLEDGE_OK;
}

/* K12 ensemble template: content.slots, each an instrument id, a name and a player count. The ids are resolved later. */
static KnowledgeStatus read_ensemble(Slot *slot, const cJSON *entry, const char *path, char *message, size_t message_size) {
    const char *id = slot->entry.id;
    const cJSON *content = knowledge_member(entry, "content");
    const cJSON *slots = cJSON_IsObject(content) ? knowledge_member(content, "slots") : NULL;
    int count = cJSON_IsArray(slots) ? cJSON_GetArraySize(slots) : -1;
    if (count < 1) {
        knowledge_fail(message, message_size, path, id, "content.slots", slots == NULL ? "missing" : "not an array of at least one slot");
        return KNOWLEDGE_ERR_FIELD;
    }
    slot->ensemble_slots = calloc((size_t)count, sizeof(EnsembleSlot));
    if (slot->ensemble_slots == NULL) {
        return KNOWLEDGE_ERR_ALLOC;
    }
    const cJSON *item;
    cJSON_ArrayForEach(item, slots) {
        const char *instrument = cJSON_IsObject(item) ? knowledge_string_of(knowledge_member(item, "instrument")) : NULL;
        const char *name = cJSON_IsObject(item) ? knowledge_string_of(knowledge_member(item, "name")) : NULL;
        if (instrument == NULL || name == NULL || !knowledge_in_range(knowledge_member(item, "players"), 1, INT32_MAX)) {
            knowledge_fail(message, message_size, path, id, "content.slots",
                 "a slot needs an instrument id, a name and players from 1");
            return KNOWLEDGE_ERR_FIELD;
        }
        for (size_t i = 0; i < slot->ensemble.slot_count; i++) {
            if (strcmp(slot->ensemble_slots[i].name, name) == 0) {
                knowledge_fail(message, message_size, path, id, "content.slots", "two slots have the same name");
                return KNOWLEDGE_ERR_FIELD;
            }
        }
        EnsembleSlot *out = &slot->ensemble_slots[slot->ensemble.slot_count];
        out->instrument = knowledge_own(slot, instrument);
        out->name = knowledge_own(slot, name);
        if (out->instrument == NULL || out->name == NULL) {
            return KNOWLEDGE_ERR_ALLOC;
        }
        out->players = knowledge_member(item, "players")->valueint;
        slot->ensemble.slot_count++;
    }
    slot->ensemble.slots = slot->ensemble_slots;
    return KNOWLEDGE_OK;
}

/* The fields the loaders read, by kind. Fills the slot's material, chord type, relation, instrument or ensemble. */
static KnowledgeStatus read_details(Slot *slot, const cJSON *entry, const char *path, char *message, size_t message_size) {
    const char *kind = slot->entry.kind;
    const char *subtype = slot->entry.subtype;
    KnowledgeStatus status = read_aliases(slot, entry, path, message, message_size);
    if (status != KNOWLEDGE_OK) {
        return status;
    }
    if (strcmp(kind, "material") == 0 && subtype != NULL && strcmp(subtype, "unordered") == 0) {
        const cJSON *content = knowledge_member(entry, "content");
        const cJSON *steps = cJSON_IsObject(content) ? knowledge_member(content, "steps") : NULL;
        if (steps == NULL) {
            knowledge_fail(message, message_size, path, slot->entry.id, "content.steps", "missing");
            return KNOWLEDGE_ERR_FIELD;
        }
        const char *problem = read_steps(steps, slot->material.steps, &slot->material.step_count);
        if (problem != NULL) {
            knowledge_fail(message, message_size, path, slot->entry.id, "content.steps", problem);
            return KNOWLEDGE_ERR_FIELD;
        }
        const cJSON *descending = knowledge_member(content, "descending_steps");
        if (descending != NULL) {
            problem = read_steps(descending, slot->material.descending_steps, &slot->material.descending_step_count);
            if (problem != NULL) {
                knowledge_fail(message, message_size, path, slot->entry.id, "content.descending_steps", problem);
                return KNOWLEDGE_ERR_FIELD;
            }
        }
        slot->material.id = slot->entry.id;
        slot->material.name = slot->entry.name;
        status = read_material_details(slot, entry, content, path, message, message_size);
        slot->has_material = status == KNOWLEDGE_OK;
    } else if (strcmp(kind, "chord-type") == 0) {
        slot->chord_type.id = slot->entry.id;
        slot->chord_type.name = slot->entry.name;
        status = read_chord_type(slot, entry, path, message, message_size);
        slot->has_chord_type = status == KNOWLEDGE_OK;
    } else if (strcmp(kind, "voicing") == 0) {
        slot->voicing.id = slot->entry.id;
        slot->voicing.name = slot->entry.name;
        status = knowledge_voicing_read(slot, entry, path, message, message_size);
        slot->has_voicing = status == KNOWLEDGE_OK;
    } else if (strcmp(kind, "pattern") == 0) {
        status = knowledge_pattern_read(slot, entry, subtype, path, message, message_size);
        slot->has_pattern = status == KNOWLEDGE_OK;
    } else if (strcmp(kind, "schema") == 0) {
        status = knowledge_schema_read(slot, entry, subtype, path, message, message_size);
        slot->has_schema = status == KNOWLEDGE_OK;
    } else if (strcmp(kind, "relation") == 0) {
        slot->relation.id = slot->entry.id;
        slot->relation.name = slot->entry.name;
        status = read_relation(slot, entry, subtype, path, message, message_size);
        slot->has_relation = status == KNOWLEDGE_OK;
    } else if (strcmp(kind, "instrument") == 0 && subtype != NULL && strcmp(subtype, "instrument") == 0) {
        slot->instrument.id = slot->entry.id;
        slot->instrument.name = slot->entry.name;
        status = read_instrument(slot, entry, path, message, message_size);
        slot->has_instrument = status == KNOWLEDGE_OK;
    } else if (strcmp(kind, "instrument") == 0 && subtype != NULL && strcmp(subtype, "ensemble") == 0) {
        slot->ensemble.id = slot->entry.id;
        slot->ensemble.name = slot->entry.name;
        status = read_ensemble(slot, entry, path, message, message_size);
        slot->has_ensemble = status == KNOWLEDGE_OK;
    } else if (strcmp(kind, "instrument") == 0) {
        knowledge_fail(message, message_size, path, slot->entry.id, "subtype", "not instrument or ensemble");
        return KNOWLEDGE_ERR_FIELD;
    } else if (strcmp(kind, "function") == 0) {
        slot->function.id = slot->entry.id;
        slot->function.name = slot->entry.name;
        status = knowledge_form_read_function(slot, entry, subtype, path, message, message_size);
        slot->has_function = status == KNOWLEDGE_OK;
    } else if (strcmp(kind, "theme-type") == 0) {
        slot->theme_type.id = slot->entry.id;
        slot->theme_type.name = slot->entry.name;
        status = knowledge_form_read_theme_type(slot, entry, path, message, message_size);
        slot->has_theme_type = status == KNOWLEDGE_OK;
    } else if (strcmp(kind, "vocabulary") == 0) {
        slot->vocabulary.id = slot->entry.id;
        slot->vocabulary.name = slot->entry.name;
        status = knowledge_vocabulary_read(slot, entry, subtype, path, message, message_size);
        slot->has_vocabulary = status == KNOWLEDGE_OK;
    } else if (strcmp(kind, "rule") == 0) {
        status = knowledge_rule_read(slot, entry, subtype, path, message, message_size);
        slot->has_rule = status == KNOWLEDGE_OK;
    }
    return status;
}

/* Reads one entry into a new slot. */
static KnowledgeStatus read_entry(KnowledgeSnapshot *snapshot, const cJSON *entry, const char *path, bool user, char *message,
                                  size_t message_size) {
    const char *problem = NULL;
    const char *id = NULL;
    if (!cJSON_IsObject(entry)) {
        knowledge_fail(message, message_size, path, NULL, "entry", "not an object");
        return KNOWLEDGE_ERR_FIELD;
    }
    const cJSON *id_item = knowledge_text_field(entry, "id", true, &problem);
    if (id_item == NULL) {
        knowledge_fail(message, message_size, path, NULL, "id", problem);
        return KNOWLEDGE_ERR_FIELD;
    }
    id = id_item->valuestring;
    for (size_t i = 0; i < snapshot->count; i++) {
        if (strcmp(snapshot->slots[i].entry.id, id) == 0) {
            knowledge_fail(message, message_size, path, id, "id", "already defined");
            return KNOWLEDGE_ERR_DUPLICATE;
        }
    }
    bool user_id = strncmp(id, USER_PREFIX, strlen(USER_PREFIX)) == 0;
    if (user != user_id) {
        knowledge_fail(message, message_size, path, id, "id", user ? "a user id starts with \"user.\"" : "a repository id may not start with \"user.\"");
        return KNOWLEDGE_ERR_NAMESPACE;
    }
    const cJSON *kind = knowledge_text_field(entry, "kind", true, &problem);
    if (kind == NULL) {
        knowledge_fail(message, message_size, path, id, "kind", problem);
        return KNOWLEDGE_ERR_FIELD;
    }
    const cJSON *subtype = knowledge_text_field(entry, "subtype", false, &problem);
    if (problem != NULL) {
        knowledge_fail(message, message_size, path, id, "subtype", problem);
        return KNOWLEDGE_ERR_FIELD;
    }
    const cJSON *name = knowledge_text_field(entry, "name", true, &problem);
    if (name == NULL) {
        knowledge_fail(message, message_size, path, id, "name", problem);
        return KNOWLEDGE_ERR_FIELD;
    }
    const cJSON *status_item = knowledge_text_field(entry, "status", true, &problem);
    KnowledgeEntryStatus status = KNOWLEDGE_STATUS_REFERENCE;
    if (status_item == NULL || !read_status(status_item->valuestring, &status)) {
        knowledge_fail(message, message_size, path, id, "status", status_item == NULL ? problem : "not reference, active or retired");
        return KNOWLEDGE_ERR_FIELD;
    }
    const cJSON *version = knowledge_member(entry, "version");
    if (!knowledge_is_integer(version) || version->valueint < 1) {
        knowledge_fail(message, message_size, path, id, "version", "not an integer from 1");
        return KNOWLEDGE_ERR_FIELD;
    }

    char *text = cJSON_PrintUnformatted(entry);
    if (text == NULL) {
        return KNOWLEDGE_ERR_ALLOC;
    }
    size_t id_size = strlen(id) + 1;
    size_t kind_size = strlen(kind->valuestring) + 1;
    size_t subtype_size = subtype != NULL ? strlen(subtype->valuestring) + 1 : 0;
    size_t name_size = strlen(name->valuestring) + 1;
    Slot slot;
    memset(&slot, 0, sizeof(slot));
    char *strings = malloc(id_size + kind_size + subtype_size + name_size);
    if (strings == NULL) {
        cJSON_free(text);
        return KNOWLEDGE_ERR_ALLOC;
    }
    slot.strings = strings;
    slot.entry.id = strings;
    memcpy(strings, id, id_size);
    strings += id_size;
    slot.entry.kind = strings;
    memcpy(strings, kind->valuestring, kind_size);
    strings += kind_size;
    if (subtype != NULL) {
        slot.entry.subtype = strings;
        memcpy(strings, subtype->valuestring, subtype_size);
        strings += subtype_size;
    }
    slot.entry.name = strings;
    memcpy(strings, name->valuestring, name_size);
    slot.entry.status = status;
    slot.entry.version = version->valueint;
    slot.path = path;
    common_fnv1a64_hex(fnv(COMMON_FNV1A64_OFFSET, text), slot.entry.hash);
    cJSON_free(text);
    slot.json = cJSON_Duplicate(entry, true);
    if (slot.json == NULL) {
        free(slot.strings);
        return KNOWLEDGE_ERR_ALLOC;
    }

    KnowledgeStatus loaded = read_details(&slot, entry, path, message, message_size);
    if (loaded == KNOWLEDGE_OK && snapshot->count == snapshot->capacity) {
        size_t capacity = snapshot->capacity == 0 ? 16 : snapshot->capacity * 2;
        Slot *grown = realloc(snapshot->slots, capacity * sizeof(Slot));
        if (grown == NULL) {
            loaded = KNOWLEDGE_ERR_ALLOC;
        } else {
            snapshot->slots = grown;
            snapshot->capacity = capacity;
        }
    }
    if (loaded != KNOWLEDGE_OK) {
        slot_release(&slot);
        return loaded;
    }
    snapshot->slots[snapshot->count++] = slot;
    return KNOWLEDGE_OK;
}

static KnowledgeStatus read_file(KnowledgeSnapshot *snapshot, const char *path, const char *text, size_t size, bool user,
                                 char *message, size_t message_size) {
    cJSON *root = cJSON_ParseWithLength(text, size);
    if (root == NULL) {
        knowledge_fail(message, message_size, path, NULL, "file", "not valid JSON");
        return KNOWLEDGE_ERR_PARSE;
    }
    KnowledgeStatus status = KNOWLEDGE_OK;
    const cJSON *schema = knowledge_member(root, "schema");
    if (cJSON_IsString(schema) && strcmp(schema->valuestring, SOURCES_SCHEMA) == 0) {
        cJSON_Delete(root);
        return KNOWLEDGE_OK; /* the list of sources has no entries */
    }
    const cJSON *entries = knowledge_member(root, "entries");
    if (!cJSON_IsArray(entries)) {
        knowledge_fail(message, message_size, path, NULL, "entries", "not an array");
        status = KNOWLEDGE_ERR_FIELD;
    } else {
        const cJSON *entry;
        cJSON_ArrayForEach(entry, entries) {
            status = read_entry(snapshot, entry, path, user, message, message_size);
            if (status != KNOWLEDGE_OK) {
                break;
            }
        }
    }
    cJSON_Delete(root);
    return status;
}

static int compare_slots(const void *a, const void *b) {
    return strcmp(((const Slot *)a)->entry.id, ((const Slot *)b)->entry.id);
}


/* The material an entry names (a link or a relation's material): the slot's, or the problem. */
static const PitchMaterial *linked_material(const KnowledgeSnapshot *snapshot, const char *target, const char *what,
                                            char *problem, size_t problem_size) {
    const Slot *found = knowledge_find_slot(snapshot, target);
    if (found == NULL || !found->has_material) {
        snprintf(problem, problem_size, "%s %s is not a pitch material", what, target);
        return NULL;
    }
    return &found->material;
}

/* True when a texture entry has this code. */
static bool texture_code_found(const KnowledgeSnapshot *snapshot, const char *code) {
    for (size_t i = 0; i < snapshot->count; i++) {
        const Slot *slot = &snapshot->slots[i];
        if (slot->has_vocabulary && slot->vocabulary.subtype == VOCABULARY_TEXTURE && strcmp(slot->vocabulary.code, code) == 0) {
            return true;
        }
    }
    return false;
}

/* Points each material's links and each relation's material at the entries they name, once every entry is read and sorted. */
static KnowledgeStatus resolve_links(KnowledgeSnapshot *snapshot, char *message, size_t message_size) {
    char problem[256];
    for (size_t i = 0; i < snapshot->count; i++) {
        Slot *slot = &snapshot->slots[i];
        const char *field = "relations";
        if (slot->has_material) {
            if (slot->variant_target != NULL) {
                slot->material.variant_of = linked_material(snapshot, slot->variant_target, "variant-of target", problem, sizeof(problem));
            }
            if (slot->rotation_target != NULL && (slot->variant_target == NULL || slot->material.variant_of != NULL)) {
                slot->material.rotation_of = linked_material(snapshot, slot->rotation_target, "rotation-of target", problem, sizeof(problem));
            }
            if ((slot->variant_target != NULL && slot->material.variant_of == NULL) ||
                (slot->rotation_target != NULL && slot->material.rotation_of == NULL)) {
                knowledge_fail(message, message_size, slot->path, slot->entry.id, field, problem);
                return KNOWLEDGE_ERR_FIELD;
            }
        }
        if (slot->has_relation && slot->material_target != NULL) {
            slot->relation.tonal.material_entry = linked_material(snapshot, slot->material_target, "material", problem, sizeof(problem));
            if (slot->relation.tonal.material_entry == NULL) {
                knowledge_fail(message, message_size, slot->path, slot->entry.id, "content.material", problem);
                return KNOWLEDGE_ERR_FIELD;
            }
        }
        if (slot->has_vocabulary && slot->vocabulary.subtype == VOCABULARY_TEXTURE && !texture_code_found(snapshot, slot->vocabulary.texture.category)) {
            knowledge_fail(message, message_size, slot->path, slot->entry.id, "content.category", "no texture entry has that code");
            return KNOWLEDGE_ERR_FIELD;
        }
        if (slot->has_vocabulary && slot->vocabulary.subtype == VOCABULARY_CADENCE_KIND && slot->cadence_material_target != NULL) {
            VocabularyCadence *cadence = &slot->vocabulary.cadence;
            cadence->material = linked_material(snapshot, slot->cadence_material_target, "material", problem, sizeof(problem));
            if (cadence->material == NULL) {
                knowledge_fail(message, message_size, slot->path, slot->entry.id, "content.material", problem);
                return KNOWLEDGE_ERR_FIELD;
            }
            bool within = (size_t)cadence->arrival_degree <= cadence->material->step_count;
            for (size_t a = 0; a < cadence->approach_count; a++) {
                within = within && (size_t)cadence->approach_degrees[a] <= cadence->material->step_count;
            }
            if (!within) {
                knowledge_fail(message, message_size, slot->path, slot->entry.id, "content.approach", "a numeral names a degree above the material's steps");
                return KNOWLEDGE_ERR_FIELD;
            }
        }
        KnowledgeStatus linked = knowledge_voicing_link(snapshot, slot, message, message_size);
        if (linked == KNOWLEDGE_OK) {
            linked = knowledge_form_link(snapshot, slot, message, message_size);
        }
        if (linked == KNOWLEDGE_OK) {
            linked = knowledge_schema_link(snapshot, slot, message, message_size);
        }
        if (linked != KNOWLEDGE_OK) {
            return linked;
        }
        for (size_t s = 0; slot->has_ensemble && s < slot->ensemble.slot_count; s++) {
            const Slot *found = knowledge_find_slot(snapshot, slot->ensemble_slots[s].instrument);
            if (found == NULL || !found->has_instrument) {
                knowledge_fail(message, message_size, slot->path, slot->entry.id, "content.slots", "an ensemble slot's instrument is not an instrument entry");
                return KNOWLEDGE_ERR_FIELD;
            }
        }
    }
    for (size_t i = 0; i < snapshot->count; i++) {
        snapshot->slots[i].path = NULL;
    }
    return KNOWLEDGE_OK;
}

KnowledgeStatus knowledge_snapshot_build(const KnowledgeUserFile *user_files, size_t user_file_count, KnowledgeSnapshot **out,
                                         char *message, size_t message_size) {
    if (message != NULL && message_size > 0) {
        message[0] = '\0';
    }
    if (out == NULL || (user_files == NULL && user_file_count > 0)) {
        knowledge_fail(message, message_size, "-", NULL, "argument", "NULL");
        return KNOWLEDGE_ERR_NULL_ARGUMENT;
    }
    for (size_t i = 0; i < user_file_count; i++) {
        if (user_files[i].path == NULL || user_files[i].text == NULL) {
            knowledge_fail(message, message_size, "-", NULL, "user file", "NULL path or text");
            return KNOWLEDGE_ERR_NULL_ARGUMENT;
        }
    }
    KnowledgeSnapshot *snapshot = calloc(1, sizeof(*snapshot));
    if (snapshot == NULL) {
        return KNOWLEDGE_ERR_ALLOC;
    }
    KnowledgeStatus status = KNOWLEDGE_OK;
    for (size_t i = 0; i < knowledge_embedded_file_count && status == KNOWLEDGE_OK; i++) {
        const KnowledgeEmbeddedFile *file = &knowledge_embedded_files[i];
        status = read_file(snapshot, file->path, file->text, file->size, false, message, message_size);
    }
    for (size_t i = 0; i < user_file_count && status == KNOWLEDGE_OK; i++) {
        status = read_file(snapshot, user_files[i].path, user_files[i].text, strlen(user_files[i].text), true, message,
                           message_size);
    }
    if (status != KNOWLEDGE_OK) {
        knowledge_snapshot_destroy(snapshot);
        return status;
    }
    qsort(snapshot->slots, snapshot->count, sizeof(Slot), compare_slots);
    status = resolve_links(snapshot, message, message_size);
    if (status != KNOWLEDGE_OK) {
        knowledge_snapshot_destroy(snapshot);
        return status;
    }
    uint64_t hash = COMMON_FNV1A64_OFFSET;
    for (size_t i = 0; i < snapshot->count; i++) {
        hash = fnv(hash, snapshot->slots[i].entry.id);
        hash = fnv(hash, "=");
        hash = fnv(hash, snapshot->slots[i].entry.hash);
        hash = fnv(hash, "\n");
    }
    common_fnv1a64_hex(hash, snapshot->hash);
    *out = snapshot;
    return KNOWLEDGE_OK;
}

const char *knowledge_snapshot_hash(const KnowledgeSnapshot *snapshot) {
    return snapshot != NULL ? snapshot->hash : NULL;
}

size_t knowledge_snapshot_entry_count(const KnowledgeSnapshot *snapshot) {
    return snapshot != NULL ? snapshot->count : 0;
}

const KnowledgeEntry *knowledge_snapshot_entry_at(const KnowledgeSnapshot *snapshot, size_t index) {
    return snapshot != NULL && index < snapshot->count ? &snapshot->slots[index].entry : NULL;
}

const struct cJSON *knowledge_snapshot_entry_json(const KnowledgeSnapshot *snapshot, const char *id) {
    const Slot *found = knowledge_find_slot(snapshot, id);
    return found != NULL ? found->json : NULL;
}

size_t knowledge_snapshot_entry_alias_count(const KnowledgeSnapshot *snapshot, const char *id) {
    const Slot *found = knowledge_find_slot(snapshot, id);
    return found != NULL ? found->alias_count : 0;
}

const char *knowledge_snapshot_entry_alias(const KnowledgeSnapshot *snapshot, const char *id, size_t index) {
    const Slot *found = knowledge_find_slot(snapshot, id);
    return found != NULL && index < found->alias_count ? found->aliases[index] : NULL;
}

const Slot *knowledge_find_slot(const KnowledgeSnapshot *snapshot, const char *id) {
    if (snapshot == NULL || id == NULL) {
        return NULL;
    }
    size_t low = 0;
    size_t high = snapshot->count;
    while (low < high) {
        size_t middle = low + (high - low) / 2;
        int order = strcmp(id, snapshot->slots[middle].entry.id);
        if (order == 0) {
            return &snapshot->slots[middle];
        }
        if (order < 0) {
            high = middle;
        } else {
            low = middle + 1;
        }
    }
    return NULL;
}

const KnowledgeEntry *knowledge_snapshot_entry(const KnowledgeSnapshot *snapshot, const char *id) {
    const Slot *slot = knowledge_find_slot(snapshot, id);
    return slot != NULL ? &slot->entry : NULL;
}

const PitchMaterial *knowledge_snapshot_material(const KnowledgeSnapshot *snapshot, const char *id) {
    const Slot *slot = knowledge_find_slot(snapshot, id);
    return slot != NULL && slot->has_material ? &slot->material : NULL;
}

size_t knowledge_snapshot_material_count(const KnowledgeSnapshot *snapshot) {
    size_t count = 0;
    for (size_t i = 0; snapshot != NULL && i < snapshot->count; i++) {
        count += snapshot->slots[i].has_material ? 1 : 0;
    }
    return count;
}

const PitchMaterial *knowledge_snapshot_material_at(const KnowledgeSnapshot *snapshot, size_t index) {
    for (size_t i = 0; snapshot != NULL && i < snapshot->count; i++) {
        if (snapshot->slots[i].has_material && index-- == 0) {
            return &snapshot->slots[i].material;
        }
    }
    return NULL;
}

static bool alias_matches(const Slot *slot, const char *word) {
    for (size_t i = 0; i < slot->alias_count; i++) {
        if (nocase_equal(slot->aliases[i], word)) {
            return true;
        }
    }
    return false;
}

const PitchMaterial *knowledge_snapshot_material_named(const KnowledgeSnapshot *snapshot, const char *word) {
    if (snapshot == NULL || word == NULL) {
        return NULL;
    }
    for (int level = 0; level < 4; level++) {
        size_t matches = 0;
        const Slot *found = NULL;
        for (size_t i = 0; i < snapshot->count; i++) {
            const Slot *slot = &snapshot->slots[i];
            if (!slot->has_material) {
                continue;
            }
            bool hit = level == 0   ? nocase_equal(slot->entry.id, word)
                       : level == 1 ? nocase_equal(slot->material.key_word, word)
                       : level == 2 ? nocase_equal(slot->entry.name, word)
                                    : alias_matches(slot, word);
            if (hit) {
                matches++;
                found = slot;
            }
        }
        if (matches > 0) {
            return matches == 1 ? &found->material : NULL;
        }
    }
    return NULL;
}

const ChordType *knowledge_snapshot_chord_type(const KnowledgeSnapshot *snapshot, const char *id) {
    const Slot *slot = knowledge_find_slot(snapshot, id);
    return slot != NULL && slot->has_chord_type ? &slot->chord_type : NULL;
}

size_t knowledge_snapshot_chord_type_count(const KnowledgeSnapshot *snapshot) {
    size_t count = 0;
    for (size_t i = 0; snapshot != NULL && i < snapshot->count; i++) {
        count += snapshot->slots[i].has_chord_type ? 1 : 0;
    }
    return count;
}

const ChordType *knowledge_snapshot_chord_type_at(const KnowledgeSnapshot *snapshot, size_t index) {
    for (size_t i = 0; snapshot != NULL && i < snapshot->count; i++) {
        if (snapshot->slots[i].has_chord_type && index-- == 0) {
            return &snapshot->slots[i].chord_type;
        }
    }
    return NULL;
}

const VoicingEntry *knowledge_snapshot_voicing(const KnowledgeSnapshot *snapshot, const char *id) {
    const Slot *slot = knowledge_find_slot(snapshot, id);
    return slot != NULL && slot->has_voicing ? &slot->voicing : NULL;
}

size_t knowledge_snapshot_voicing_count(const KnowledgeSnapshot *snapshot) {
    size_t count = 0;
    for (size_t i = 0; snapshot != NULL && i < snapshot->count; i++) {
        count += snapshot->slots[i].has_voicing ? 1 : 0;
    }
    return count;
}

const VoicingEntry *knowledge_snapshot_voicing_at(const KnowledgeSnapshot *snapshot, size_t index) {
    for (size_t i = 0; snapshot != NULL && i < snapshot->count; i++) {
        if (snapshot->slots[i].has_voicing && index-- == 0) {
            return &snapshot->slots[i].voicing;
        }
    }
    return NULL;
}

const ChordType *knowledge_snapshot_chord_type_of(const KnowledgeSnapshot *snapshot, unsigned mask_above_root) {
    if (snapshot == NULL) {
        return NULL;
    }
    unsigned wanted = mask_above_root & ~1u;
    for (size_t i = 0; i < snapshot->count; i++) {
        const Slot *slot = &snapshot->slots[i];
        if (!slot->has_chord_type || slot->entry.status == KNOWLEDGE_STATUS_RETIRED) {
            continue;
        }
        unsigned mask = 0;
        for (size_t m = 0; m < slot->chord_type.member_count; m++) {
            mask |= 1u << (slot->chord_type.members[m].semitones % 12);
        }
        if ((mask & ~1u) == wanted) {
            return &slot->chord_type;
        }
    }
    return NULL;
}

const Relation *knowledge_snapshot_relation(const KnowledgeSnapshot *snapshot, const char *id) {
    const Slot *slot = knowledge_find_slot(snapshot, id);
    return slot != NULL && slot->has_relation ? &slot->relation : NULL;
}

const Relation *knowledge_snapshot_relation_named(const KnowledgeSnapshot *snapshot, const char *word) {
    if (snapshot == NULL || word == NULL) {
        return NULL;
    }
    for (int level = 0; level < 3; level++) {
        size_t matches = 0;
        const Slot *found = NULL;
        for (size_t i = 0; i < snapshot->count; i++) {
            const Slot *slot = &snapshot->slots[i];
            if (!slot->has_relation) {
                continue;
            }
            bool hit = level == 0   ? nocase_equal(slot->entry.id, word)
                       : level == 1 ? nocase_equal(slot->entry.name, word)
                                    : alias_matches(slot, word);
            if (hit) {
                matches++;
                found = slot;
            }
        }
        if (matches > 0) {
            return matches == 1 ? &found->relation : NULL;
        }
    }
    return NULL;
}

const Instrument *knowledge_snapshot_instrument(const KnowledgeSnapshot *snapshot, const char *id) {
    const Slot *slot = knowledge_find_slot(snapshot, id);
    return slot != NULL && slot->has_instrument ? &slot->instrument : NULL;
}

const EnsembleTemplate *knowledge_snapshot_ensemble(const KnowledgeSnapshot *snapshot, const char *id) {
    const Slot *slot = knowledge_find_slot(snapshot, id);
    return slot != NULL && slot->has_ensemble ? &slot->ensemble : NULL;
}

const VocabularyEntry *knowledge_snapshot_vocabulary(const KnowledgeSnapshot *snapshot, const char *id) {
    const Slot *slot = knowledge_find_slot(snapshot, id);
    return slot != NULL && slot->has_vocabulary ? &slot->vocabulary : NULL;
}

const RuleEntry *knowledge_snapshot_rule(const KnowledgeSnapshot *snapshot, const char *id) {
    const Slot *slot = knowledge_find_slot(snapshot, id);
    return slot != NULL && slot->has_rule ? &slot->rule : NULL;
}

const ThemeType *knowledge_snapshot_theme_type_of(const KnowledgeSnapshot *snapshot, const char *beginning_code, const char *ending_code) {
    for (size_t i = 0; snapshot != NULL && beginning_code != NULL && ending_code != NULL && i < snapshot->count; i++) {
        const ThemeType *type = snapshot->slots[i].has_theme_type ? &snapshot->slots[i].theme_type : NULL;
        if (type != NULL && strcmp(type->beginning->code, beginning_code) == 0 && strcmp(type->ending->code, ending_code) == 0) {
            return type;
        }
    }
    return NULL;
}
