#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "theory/knowledge/knowledge_snapshot.h"
#include "model/pitch/pitch_material.h"

/*
 * The knowledge snapshot: the four tonality modes the build embeds, found by id with their fields and steps; the entries it holds by
 * kind (nine materials, eighteen relations, nine chord types) and the steps of the seven diatonic modes; the hash; a user's own
 * file; the files and entries it refuses, each with its status and a message naming the file, the id and the field.
 */

static const char *MODE_IDS[4] = {"omt.material.major", "omt.material.natural-minor", "omt.material.harmonic-minor",
                                  "omt.material.melodic-minor"};
static const int MODE_STEPS[4][7] = {
    {0, 2, 4, 5, 7, 9, 11}, {0, 2, 3, 5, 7, 8, 10}, {0, 2, 3, 5, 7, 8, 11}, {0, 2, 3, 5, 7, 9, 11}};
static const int MELODIC_DESCENDING[7] = {0, 2, 3, 5, 7, 8, 10};

static const char *USER_FILE =
    "{\"schema\":\"ursatz-knowledge\",\"schema_version\":1,\"source\":\"user\",\"entries\":["
    "{\"id\":\"user.material.test\",\"kind\":\"material\",\"subtype\":\"unordered\",\"name\":\"test\","
    "\"content\":{\"steps\":[0,2,7]},\"citations\":[],\"status\":\"active\",\"version\":1},"
    "{\"id\":\"user.chord-type.test\",\"kind\":\"chord-type\",\"name\":\"test chord\",\"content\":{},\"citations\":[],"
    "\"status\":\"reference\",\"version\":2}]}";

static KnowledgeSnapshot *build(const KnowledgeUserFile *files, size_t count) {
    KnowledgeSnapshot *snapshot = NULL;
    char message[256];
    assert(knowledge_snapshot_build(files, count, &snapshot, message, sizeof(message)) == KNOWLEDGE_OK);
    assert(snapshot != NULL && message[0] == '\0');
    return snapshot;
}

static bool lower_hex16(const char *text) {
    if (text == NULL || strlen(text) != 16) {
        return false;
    }
    for (size_t i = 0; i < 16; i++) {
        if (!((text[i] >= '0' && text[i] <= '9') || (text[i] >= 'a' && text[i] <= 'f'))) {
            return false;
        }
    }
    return true;
}

static void test_embedded_modes(void) {
    KnowledgeSnapshot *snapshot = build(NULL, 0);
    assert(knowledge_snapshot_entry_count(snapshot) == 36);
    /* in id order */
    assert(strcmp(knowledge_snapshot_entry_at(snapshot, 0)->id, "omt.chord-type.augmented-triad") == 0);
    assert(strcmp(knowledge_snapshot_entry_at(snapshot, 35)->id, "omt.relation.two-steps-darker") == 0);
    assert(knowledge_snapshot_entry_at(snapshot, 36) == NULL);

    for (int mode = 0; mode < 4; mode++) {
        const KnowledgeEntry *entry = knowledge_snapshot_entry(snapshot, MODE_IDS[mode]);
        assert(entry != NULL);
        assert(strcmp(entry->id, MODE_IDS[mode]) == 0 && strcmp(entry->kind, "material") == 0);
        assert(entry->subtype != NULL && strcmp(entry->subtype, "unordered") == 0);
        assert(entry->status == KNOWLEDGE_STATUS_ACTIVE && entry->version == 1);
        assert(lower_hex16(entry->hash));

        const PitchMaterial *material = knowledge_snapshot_material(snapshot, MODE_IDS[mode]);
        assert(material != NULL && strcmp(material->id, MODE_IDS[mode]) == 0);
        assert(material->step_count == 7);
        for (int d = 0; d < 7; d++) {
            assert(material->steps[d] == MODE_STEPS[mode][d]);
            assert(pitch_material_step(material, d + 1) == MODE_STEPS[mode][d]);
        }
        assert(material->descending_step_count == (mode == 3 ? 7u : 0u));
    }
    assert(strcmp(knowledge_snapshot_entry(snapshot, "omt.material.natural-minor")->name, "natural minor") == 0);
    const PitchMaterial *melodic = knowledge_snapshot_material(snapshot, "omt.material.melodic-minor");
    for (int d = 0; d < 7; d++) {
        assert(melodic->descending_steps[d] == MELODIC_DESCENDING[d]);
    }

    assert(knowledge_snapshot_entry(snapshot, "omt.material.missing") == NULL);
    assert(knowledge_snapshot_material(snapshot, "omt.material.missing") == NULL);
    assert(knowledge_snapshot_entry(snapshot, NULL) == NULL);
    knowledge_snapshot_destroy(snapshot);
}

static const char *DIATONIC_IDS[7] = {"omt.material.locrian", "omt.material.phrygian", "omt.material.natural-minor",
                                      "omt.material.dorian", "omt.material.mixolydian", "omt.material.major",
                                      "omt.material.lydian"};
static const int DIATONIC_STEPS[7][7] = {{0, 1, 3, 5, 6, 8, 10}, {0, 1, 3, 5, 7, 8, 10}, {0, 2, 3, 5, 7, 8, 10},
                                         {0, 2, 3, 5, 7, 9, 10}, {0, 2, 4, 5, 7, 9, 10}, {0, 2, 4, 5, 7, 9, 11},
                                         {0, 2, 4, 6, 7, 9, 11}};

static void test_embedded_counts(void) {
    KnowledgeSnapshot *snapshot = build(NULL, 0);
    size_t materials = 0, relations = 0, chord_types = 0, others = 0;
    for (size_t i = 0; i < knowledge_snapshot_entry_count(snapshot); i++) {
        const KnowledgeEntry *entry = knowledge_snapshot_entry_at(snapshot, i);
        assert(entry != NULL && entry->status == KNOWLEDGE_STATUS_ACTIVE);
        if (strcmp(entry->kind, "material") == 0) {
            materials++;
        } else if (strcmp(entry->kind, "relation") == 0) {
            relations++;
        } else if (strcmp(entry->kind, "chord-type") == 0) {
            chord_types++;
        } else {
            others++;
        }
    }
    assert(materials == 9 && relations == 18 && chord_types == 9 && others == 0);
    /* the seven diatonic modes, darkest to brightest: each a rotation of the major scale with its own steps */
    for (int mode = 0; mode < 7; mode++) {
        const PitchMaterial *material = knowledge_snapshot_material(snapshot, DIATONIC_IDS[mode]);
        assert(material != NULL && material->step_count == 7 && material->descending_step_count == 0);
        for (int d = 0; d < 7; d++) {
            assert(material->steps[d] == DIATONIC_STEPS[mode][d]);
        }
    }
    /* relations and chord types are entries, not pitch materials */
    assert(knowledge_snapshot_entry(snapshot, "omt.relation.supertonic") != NULL);
    assert(knowledge_snapshot_material(snapshot, "omt.relation.supertonic") == NULL);
    assert(knowledge_snapshot_entry(snapshot, "omt.chord-type.diminished-seventh") != NULL);
    assert(knowledge_snapshot_material(snapshot, "omt.chord-type.diminished-seventh") == NULL);
    knowledge_snapshot_destroy(snapshot);
}

static void test_material_queries(void) {
    KnowledgeSnapshot *snapshot = build(NULL, 0);
    const PitchMaterial *major = knowledge_snapshot_material(snapshot, "omt.material.major");
    assert(pitch_material_step(major, 0) == -1 && pitch_material_step(major, 8) == -1 && pitch_material_step(major, -1) == -1);
    assert(pitch_material_step(major, 1) == 0 && pitch_material_step(major, 7) == 11);
    assert(pitch_material_step(NULL, 1) == -1);
    assert(pitch_material_contains(major, 0) && pitch_material_contains(major, 11) && pitch_material_contains(major, 7));
    assert(!pitch_material_contains(major, 1) && !pitch_material_contains(major, 6));
    assert(pitch_material_contains(major, 12) && pitch_material_contains(major, -1) && pitch_material_contains(major, -13));
    assert(!pitch_material_contains(major, 13) && !pitch_material_contains(major, -12 + 1));
    assert(!pitch_material_contains(NULL, 0));
    knowledge_snapshot_destroy(snapshot);
}

static void test_hash(void) {
    KnowledgeSnapshot *first = build(NULL, 0);
    KnowledgeSnapshot *second = build(NULL, 0);
    assert(lower_hex16(knowledge_snapshot_hash(first)));
    assert(strcmp(knowledge_snapshot_hash(first), knowledge_snapshot_hash(second)) == 0);
    /* two snapshots in one process are independent: their entries are not shared */
    assert(knowledge_snapshot_entry(first, "omt.material.major") != knowledge_snapshot_entry(second, "omt.material.major"));
    knowledge_snapshot_destroy(first);
    assert(knowledge_snapshot_entry_count(second) == 36);
    knowledge_snapshot_destroy(second);
}

static void test_user_file(void) {
    KnowledgeSnapshot *plain = build(NULL, 0);
    KnowledgeUserFile file = {"mine.json", USER_FILE};
    KnowledgeSnapshot *with_user = build(&file, 1);
    assert(knowledge_snapshot_entry_count(with_user) == 38);
    assert(strcmp(knowledge_snapshot_hash(plain), knowledge_snapshot_hash(with_user)) != 0);
    const PitchMaterial *test = knowledge_snapshot_material(with_user, "user.material.test");
    assert(test != NULL && test->step_count == 3 && test->steps[2] == 7 && test->descending_step_count == 0);
    const KnowledgeEntry *chord = knowledge_snapshot_entry(with_user, "user.chord-type.test");
    assert(chord != NULL && chord->subtype == NULL && chord->status == KNOWLEDGE_STATUS_REFERENCE && chord->version == 2);
    assert(knowledge_snapshot_material(with_user, "user.chord-type.test") == NULL);
    /* the repository entries are unchanged by it */
    assert(strcmp(knowledge_snapshot_entry(plain, "omt.material.major")->hash,
                  knowledge_snapshot_entry(with_user, "omt.material.major")->hash) == 0);
    knowledge_snapshot_destroy(with_user);
    knowledge_snapshot_destroy(plain);
}

static void expect_refused(const char *text, KnowledgeStatus status, const char *needle_a, const char *needle_b,
                           const char *needle_c) {
    KnowledgeUserFile file = {"broken.json", text};
    KnowledgeSnapshot *snapshot = NULL;
    char message[256];
    assert(knowledge_snapshot_build(&file, 1, &snapshot, message, sizeof(message)) == status);
    assert(snapshot == NULL);
    assert(strstr(message, "broken.json") != NULL);
    assert(needle_a == NULL || strstr(message, needle_a) != NULL);
    assert(needle_b == NULL || strstr(message, needle_b) != NULL);
    assert(needle_c == NULL || strstr(message, needle_c) != NULL);
}

static void test_refused(void) {
    expect_refused("{\"entries\":[{\"id\":\"omt.material.major\",\"kind\":\"material\",\"subtype\":\"unordered\","
                   "\"name\":\"again\",\"content\":{\"steps\":[0,2]},\"status\":\"active\",\"version\":1}]}",
                   KNOWLEDGE_ERR_DUPLICATE, "omt.material.major", "id", NULL);
    expect_refused("{\"entries\":[{\"id\":\"omt.material.other\",\"kind\":\"material\",\"subtype\":\"unordered\","
                   "\"name\":\"other\",\"content\":{\"steps\":[0,2]},\"status\":\"active\",\"version\":1}]}",
                   KNOWLEDGE_ERR_NAMESPACE, "omt.material.other", "id", NULL);
    expect_refused("{\"entries\":[{\"id\":\"user.material.nosteps\",\"kind\":\"material\",\"subtype\":\"unordered\","
                   "\"name\":\"no steps\",\"content\":{},\"status\":\"active\",\"version\":1}]}",
                   KNOWLEDGE_ERR_FIELD, "user.material.nosteps", "content.steps", "missing");
    expect_refused("{\"entries\":[{\"id\":\"user.material.late\",\"kind\":\"material\",\"subtype\":\"unordered\","
                   "\"name\":\"late\",\"content\":{\"steps\":[1,2]},\"status\":\"active\",\"version\":1}]}",
                   KNOWLEDGE_ERR_FIELD, "user.material.late", "content.steps", "ascending from 0");
    expect_refused("{\"entries\":[{\"id\":\"user.material.bad\",\"kind\":\"material\",\"subtype\":\"unordered\","
                   "\"name\":\"bad\",\"content\":{\"steps\":[0,12]},\"status\":\"active\",\"version\":1}]}",
                   KNOWLEDGE_ERR_FIELD, "user.material.bad", "content.steps", NULL);
    expect_refused("{\"entries\":[{\"id\":\"user.term.x\",\"kind\":\"term\",\"name\":\"x\",\"status\":\"soon\",\"version\":1}]}",
                   KNOWLEDGE_ERR_FIELD, "user.term.x", "status", NULL);
    expect_refused("{\"entries\":[{\"id\":\"user.term.x\",\"kind\":\"term\",\"name\":\"x\",\"status\":\"active\",\"version\":0}]}",
                   KNOWLEDGE_ERR_FIELD, "user.term.x", "version", NULL);
    expect_refused("{\"entries\":[{\"kind\":\"term\"}]}", KNOWLEDGE_ERR_FIELD, "id", "missing", NULL);
    expect_refused("{\"entries\":3}", KNOWLEDGE_ERR_FIELD, "entries", NULL, NULL);
    expect_refused("this is not JSON", KNOWLEDGE_ERR_PARSE, "not valid JSON", NULL, NULL);

    /* a user id reused by a second user file is a duplicate too */
    KnowledgeUserFile files[2] = {{"one.json", USER_FILE}, {"two.json", USER_FILE}};
    KnowledgeSnapshot *snapshot = NULL;
    char message[256];
    assert(knowledge_snapshot_build(files, 2, &snapshot, message, sizeof(message)) == KNOWLEDGE_ERR_DUPLICATE);
    assert(snapshot == NULL && strstr(message, "two.json") != NULL && strstr(message, "user.material.test") != NULL);
    /* the message is optional */
    assert(knowledge_snapshot_build(files, 2, &snapshot, NULL, 0) == KNOWLEDGE_ERR_DUPLICATE);
}

static void test_null_arguments(void) {
    KnowledgeSnapshot *snapshot = NULL;
    char message[64];
    KnowledgeUserFile file = {"x.json", USER_FILE};
    KnowledgeUserFile no_text = {"x.json", NULL};
    assert(knowledge_snapshot_build(NULL, 0, NULL, message, sizeof(message)) == KNOWLEDGE_ERR_NULL_ARGUMENT);
    assert(knowledge_snapshot_build(NULL, 1, &snapshot, message, sizeof(message)) == KNOWLEDGE_ERR_NULL_ARGUMENT);
    assert(knowledge_snapshot_build(&no_text, 1, &snapshot, message, sizeof(message)) == KNOWLEDGE_ERR_NULL_ARGUMENT);
    assert(snapshot == NULL);
    assert(knowledge_snapshot_build(&file, 0, &snapshot, NULL, 0) == KNOWLEDGE_OK);
    knowledge_snapshot_destroy(snapshot);
    knowledge_snapshot_destroy(NULL);
    assert(knowledge_snapshot_hash(NULL) == NULL && knowledge_snapshot_entry_count(NULL) == 0);
    assert(knowledge_snapshot_entry_at(NULL, 0) == NULL && knowledge_snapshot_entry(NULL, "x") == NULL);
    assert(knowledge_snapshot_material(NULL, "x") == NULL);
}

int main(void) {
    test_embedded_modes();
    test_embedded_counts();
    test_material_queries();
    test_hash();
    test_user_file();
    test_refused();
    test_null_arguments();
    puts("knowledge snapshot: ok");
    return 0;
}
