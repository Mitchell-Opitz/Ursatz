#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <cJSON.h>

#include "theory/harmony/chord_value.h"
#include "theory/harmony/harmonic_function.h"
#include "theory/harmony/harmony_cadence_kinds.h"
#include "theory/knowledge/knowledge_snapshot.h"
#include "theory/vocabulary/cadence_type.h"
#include "theory/vocabulary/non_chord_tone.h"
#include "theory/vocabulary/texture_rules.h"

/*
 * The K4 vocabulary entries (R-707): the C tables pinned to the entries both ways. Every row of the cadence table but
 * authentic-unspecified has its cadence-kind entry with equal fields; every non-chord tone, texture and harmonic function name is
 * the code of exactly one entry of its subtype, and every entry's code is one of them; each texture's category is
 * standard_texture_category's. A user file with a vocabulary entry of an unknown subtype, or a texture without a category or with
 * one no texture has, is refused with the file, the id and the field.
 */

static KnowledgeSnapshot *build(void) {
    KnowledgeSnapshot *snapshot = NULL;
    char message[256];
    assert(knowledge_snapshot_build(NULL, 0, &snapshot, message, sizeof(message)) == KNOWLEDGE_OK);
    assert(snapshot != NULL && message[0] == '\0');
    return snapshot;
}

/* The entries of a subtype with this code (NULL code: any), and how many entries the subtype has. */
static size_t with_code(const KnowledgeSnapshot *snapshot, VocabularySubtype subtype, const char *code, const VocabularyEntry **found) {
    size_t matches = 0;
    for (size_t i = 0; i < knowledge_snapshot_entry_count(snapshot); i++) {
        const VocabularyEntry *entry = knowledge_snapshot_vocabulary(snapshot, knowledge_snapshot_entry_at(snapshot, i)->id);
        if (entry != NULL && entry->subtype == subtype && (code == NULL || strcmp(entry->code, code) == 0)) {
            matches++;
            if (found != NULL) {
                *found = entry;
            }
        }
    }
    return matches;
}

static void test_cadence_kinds(void) {
    KnowledgeSnapshot *snapshot = build();
    assert(with_code(snapshot, VOCABULARY_CADENCE_KIND, NULL, NULL) == harmony_cadence_kind_count() - 1);
    for (size_t i = 0; i < harmony_cadence_kind_count(); i++) {
        const HarmonyCadenceKind *row = harmony_cadence_kind_at(i);
        const VocabularyEntry *entry = NULL;
        size_t matches = with_code(snapshot, VOCABULARY_CADENCE_KIND, row->name, &entry);
        if (strcmp(row->name, "authentic-unspecified") == 0) {
            assert(matches == 0);
            continue;
        }
        assert(matches == 1);
        assert(strcmp(entry->cadence.label, row->label) == 0 && strcmp(entry->cadence.abbreviation, row->abbreviation) == 0);
        assert(strcmp(entry->cadence.core, cadence_type_name(row->core)) == 0);
        assert(entry->cadence.strength == row->strength && entry->cadence.conclusive == row->conclusive);
    }
    assert(harmony_cadence_kind_at(harmony_cadence_kind_count()) == NULL);
    knowledge_snapshot_destroy(snapshot);
}

/* Every name is the code of one entry of the subtype, and the subtype has no other entry. */
static void names_match(const KnowledgeSnapshot *snapshot, VocabularySubtype subtype, const char *const *names, size_t count) {
    for (size_t i = 0; i < count; i++) {
        assert(names[i] != NULL);
        assert(with_code(snapshot, subtype, names[i], NULL) == 1);
    }
    assert(with_code(snapshot, subtype, NULL, NULL) == count);
}

static void test_tables(void) {
    KnowledgeSnapshot *snapshot = build();
    const char *tones[NON_CHORD_TONE_COUNT];
    for (int i = 0; i < NON_CHORD_TONE_COUNT; i++) {
        tones[i] = non_chord_tone_name((NonChordTone)i);
    }
    names_match(snapshot, VOCABULARY_EMBELLISHMENT, tones, NON_CHORD_TONE_COUNT);
    const char *textures[STANDARD_TEXTURE_COUNT];
    for (int i = 0; i < STANDARD_TEXTURE_COUNT; i++) {
        textures[i] = standard_texture_name((StandardTexture)i);
    }
    names_match(snapshot, VOCABULARY_TEXTURE, textures, STANDARD_TEXTURE_COUNT);
    const char *functions[HARMONIC_FUNCTION_COUNT];
    for (int i = 0; i < HARMONIC_FUNCTION_COUNT; i++) {
        functions[i] = harmonic_function_name((HarmonicFunction)i);
    }
    names_match(snapshot, VOCABULARY_FUNCTION, functions, HARMONIC_FUNCTION_COUNT);
    for (int i = 0; i < STANDARD_TEXTURE_COUNT; i++) {
        StandardTexture category = STANDARD_TEXTURE_MONOPHONY;
        assert(standard_texture_category((StandardTexture)i, &category));
        const VocabularyEntry *entry = NULL;
        assert(with_code(snapshot, VOCABULARY_TEXTURE, textures[i], &entry) == 1);
        assert(strcmp(entry->texture.category, standard_texture_name(category)) == 0);
    }
    /* the progression types have the codes the harmony shapes of the Composer read */
    static const char *const PROGRESSIONS[] = {"prolongation", "cadential", "phrase_model", "sequence"};
    for (size_t i = 0; i < 4; i++) {
        assert(with_code(snapshot, VOCABULARY_PROGRESSION_TYPE, PROGRESSIONS[i], NULL) == 1);
    }
    assert(with_code(snapshot, VOCABULARY_PROGRESSION_TYPE, NULL, NULL) == 4);
    knowledge_snapshot_destroy(snapshot);
}

static void test_fields(void) {
    KnowledgeSnapshot *snapshot = build();
    const VocabularyEntry *entry = knowledge_snapshot_vocabulary(snapshot, "omt.vocabulary.suspension");
    assert(entry != NULL && entry->subtype == VOCABULARY_EMBELLISHMENT && strcmp(entry->code, "suspension") == 0);
    assert(entry->embellishment.approach == EMBELLISHMENT_MOTION_STATIC && entry->embellishment.leave == EMBELLISHMENT_MOTION_STEP);
    assert(entry->embellishment.direction == EMBELLISHMENT_DIRECTION_DOWN && entry->embellishment.metric == EMBELLISHMENT_METRIC_STRONG);
    entry = knowledge_snapshot_vocabulary(snapshot, "omt.vocabulary.melody-and-accompaniment");
    assert(entry != NULL && strcmp(entry->code, "melody and accompaniment") == 0 && strcmp(entry->texture.category, "homophony") == 0);
    entry = knowledge_snapshot_vocabulary(snapshot, "omt.vocabulary.abandoned");
    assert(entry != NULL && entry->cadence.strength == -1 && strcmp(entry->cadence.core, "unknown") == 0);
    assert(knowledge_snapshot_vocabulary(snapshot, "omt.material.major") == NULL);
    assert(knowledge_snapshot_vocabulary(snapshot, "omt.vocabulary.nothing") == NULL);
    assert(knowledge_snapshot_vocabulary(NULL, "omt.vocabulary.tonic") == NULL);
    knowledge_snapshot_destroy(snapshot);
}

/* Hindemith's tables (R-764): each C row has its entry with equal fields, each entry a row, and every example classifies to its code. */
static void test_hindemith(void) {
    KnowledgeSnapshot *snapshot = build();
    assert(with_code(snapshot, VOCABULARY_INTERVAL_CLASS, NULL, NULL) == CHORD_VALUE_RANKS);
    assert(with_code(snapshot, VOCABULARY_CHORD_GROUP, NULL, NULL) == CHORD_VALUE_RANKS);
    for (int i = 0; i < CHORD_VALUE_RANKS; i++) {
        const ChordValueInterval *row = &CHORD_VALUE_SERIES_TWO[i];
        const VocabularyEntry *entry = NULL;
        assert(with_code(snapshot, VOCABULARY_INTERVAL_CLASS, row->code, &entry) == 1);
        assert(entry->interval.semitones == row->semitones && entry->interval.rank == row->rank && entry->interval.root == row->root);
        const ChordValueGroup *group = &CHORD_VALUE_GROUPS[i];
        assert(with_code(snapshot, VOCABULARY_CHORD_GROUP, group->code, &entry) == 1);
        assert(entry->chord_group.group == group->group && entry->chord_group.rank == group->rank &&
               entry->chord_group.tritones == group->tritones && entry->chord_group.sharp == group->sharp &&
               entry->chord_group.root == group->root);
    }
    /* the examples of the file: {"pitches": [...], "group": code} classifies to its code, {"low", "high"} spans the interval's semitones */
    FILE *file = fopen(HINDEMITH_FILE, "rb");
    assert(file != NULL);
    static char text[200000];
    size_t size = fread(text, 1, sizeof(text) - 1, file);
    fclose(file);
    text[size] = '\0';
    cJSON *document = cJSON_Parse(text);
    assert(document != NULL);
    size_t checked = 0;
    const cJSON *json;
    cJSON_ArrayForEach(json, cJSON_GetObjectItemCaseSensitive(document, "entries")) {
        const VocabularyEntry *entry = knowledge_snapshot_vocabulary(snapshot, cJSON_GetObjectItemCaseSensitive(json, "id")->valuestring);
        assert(entry != NULL);
        const cJSON *example = cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(json, "examples"), 0);
        assert(example != NULL);
        if (entry->subtype == VOCABULARY_INTERVAL_CLASS) {
            int low = cJSON_GetObjectItemCaseSensitive(example, "low")->valueint;
            int high = cJSON_GetObjectItemCaseSensitive(example, "high")->valueint;
            assert(high - low == entry->interval.semitones);
        } else {
            assert(entry->subtype == VOCABULARY_CHORD_GROUP);
            int pitches[8];
            size_t count = 0;
            const cJSON *item;
            cJSON_ArrayForEach(item, cJSON_GetObjectItemCaseSensitive(example, "pitches")) {
                pitches[count++] = item->valueint;
            }
            ChordValue value;
            assert(chord_value_classify(pitches, count, &value));
            assert(strcmp(value.group, entry->code) == 0 && strcmp(cJSON_GetObjectItemCaseSensitive(example, "group")->valuestring, entry->code) == 0);
            assert(value.rank == entry->chord_group.rank);
        }
        checked++;
    }
    assert(checked == 2 * CHORD_VALUE_RANKS);
    cJSON_Delete(document);
    knowledge_snapshot_destroy(snapshot);
}

static void refused(const char *entries, const char *path, const char *id, const char *field) {
    char text[1024];
    snprintf(text, sizeof(text), "{\"schema\":\"ursatz-knowledge\",\"schema_version\":1,\"source\":\"user\",\"entries\":[%s]}", entries);
    KnowledgeUserFile file = {path, text};
    KnowledgeSnapshot *snapshot = NULL;
    char message[256];
    assert(knowledge_snapshot_build(&file, 1, &snapshot, message, sizeof(message)) == KNOWLEDGE_ERR_FIELD);
    assert(snapshot == NULL);
    assert(strstr(message, path) != NULL && strstr(message, id) != NULL && strstr(message, field) != NULL);
}

static void test_refusals(void) {
    refused("{\"id\":\"user.vocabulary.mood\",\"kind\":\"vocabulary\",\"subtype\":\"mood\",\"name\":\"mood\","
            "\"content\":{\"code\":\"mood\"},\"citations\":[],\"status\":\"reference\",\"version\":1}",
            "mine.json", "user.vocabulary.mood", "subtype");
    refused("{\"id\":\"user.vocabulary.mood\",\"kind\":\"vocabulary\",\"name\":\"mood\","
            "\"content\":{\"code\":\"mood\"},\"citations\":[],\"status\":\"reference\",\"version\":1}",
            "mine.json", "user.vocabulary.mood", "subtype");
    refused("{\"id\":\"user.vocabulary.chorale\",\"kind\":\"vocabulary\",\"subtype\":\"texture\",\"name\":\"chorale\","
            "\"content\":{\"code\":\"chorale\"},\"citations\":[],\"status\":\"reference\",\"version\":1}",
            "mine.json", "user.vocabulary.chorale", "content.category");
    refused("{\"id\":\"user.vocabulary.chorale\",\"kind\":\"vocabulary\",\"subtype\":\"texture\",\"name\":\"chorale\","
            "\"content\":{\"code\":\"chorale\",\"category\":\"chorales\"},\"citations\":[],\"status\":\"reference\",\"version\":1}",
            "mine.json", "user.vocabulary.chorale", "content.category");
    refused("{\"id\":\"user.vocabulary.loud\",\"kind\":\"vocabulary\",\"subtype\":\"function\",\"name\":\"loud\","
            "\"content\":{\"code\":\"Loud\"},\"citations\":[],\"status\":\"reference\",\"version\":1}",
            "mine.json", "user.vocabulary.loud", "content.code");
    refused("{\"id\":\"user.vocabulary.close\",\"kind\":\"vocabulary\",\"subtype\":\"cadence-kind\",\"name\":\"close\","
            "\"content\":{\"code\":\"close\",\"label\":\"close\",\"abbreviation\":\"CL\",\"core\":\"half\",\"strength\":11,"
            "\"conclusive\":true},\"citations\":[],\"status\":\"reference\",\"version\":1}",
            "mine.json", "user.vocabulary.close", "content.strength");
    refused("{\"id\":\"user.vocabulary.skip\",\"kind\":\"vocabulary\",\"subtype\":\"embellishment\",\"name\":\"skip\","
            "\"content\":{\"code\":\"skip\",\"approach\":\"jump\",\"leave\":\"step\",\"direction\":\"same\",\"metric\":\"any\"},"
            "\"citations\":[],\"status\":\"reference\",\"version\":1}",
            "mine.json", "user.vocabulary.skip", "content.approach");
}

int main(void) {
    test_cadence_kinds();
    test_tables();
    test_fields();
    test_hindemith();
    test_refusals();
    return 0;
}
