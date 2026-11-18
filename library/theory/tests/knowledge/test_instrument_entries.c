#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "theory/knowledge/knowledge_snapshot.h"

/*
 * The K12 instrument entries: the piano's fields as the build embeds them, the range test at both ends, the solo piano ensemble
 * resolving its one slot to the piano, user files with a reversed range and with an unknown instrument refused with the file, the id
 * and the field, and the lookups of other kinds returning NULL.
 */

static KnowledgeSnapshot *build(const KnowledgeUserFile *files, size_t count) {
    KnowledgeSnapshot *snapshot = NULL;
    char message[256];
    assert(knowledge_snapshot_build(files, count, &snapshot, message, sizeof(message)) == KNOWLEDGE_OK);
    assert(snapshot != NULL && message[0] == '\0');
    return snapshot;
}

static void test_piano(void) {
    KnowledgeSnapshot *snapshot = build(NULL, 0);
    const Instrument *piano = knowledge_snapshot_instrument(snapshot, "omt.instrument.piano");
    assert(piano != NULL && strcmp(piano->id, "omt.instrument.piano") == 0 && strcmp(piano->name, "Piano") == 0);
    assert(strcmp(piano->family, "keyboard") == 0 && piano->transposition == 0);
    assert(piano->staff_count == 2 && piano->clefs[0] == INSTRUMENT_CLEF_TREBLE && piano->clefs[1] == INSTRUMENT_CLEF_BASS);
    assert(piano->lowest == 21 && piano->highest == 108 && !piano->has_comfortable_range);
    assert(piano->general_midi_program == 0 && piano->sustain_pedal);
    assert(!instrument_in_range(piano, 20) && instrument_in_range(piano, 21));
    assert(instrument_in_range(piano, 108) && !instrument_in_range(piano, 109));
    assert(!instrument_in_range(NULL, 60));
    knowledge_snapshot_destroy(snapshot);
}

static void test_ensemble(void) {
    KnowledgeSnapshot *snapshot = build(NULL, 0);
    const EnsembleTemplate *solo = knowledge_snapshot_ensemble(snapshot, "omt.instrument.solo-piano");
    assert(solo != NULL && strcmp(solo->name, "Solo piano") == 0 && solo->slot_count == 1);
    assert(strcmp(solo->slots[0].name, "Piano") == 0 && solo->slots[0].players == 1);
    assert(knowledge_snapshot_instrument(snapshot, solo->slots[0].instrument) ==
           knowledge_snapshot_instrument(snapshot, "omt.instrument.piano"));
    knowledge_snapshot_destroy(snapshot);
}

static const char *USER_HEAD = "{\"schema\":\"ursatz-knowledge\",\"schema_version\":1,\"source\":\"user\",\"entries\":[";

static void refused(const char *entries, const char *path, const char *id, const char *field) {
    char text[1024];
    snprintf(text, sizeof(text), "%s%s]}", USER_HEAD, entries);
    KnowledgeUserFile file = {path, text};
    KnowledgeSnapshot *snapshot = NULL;
    char message[256];
    assert(knowledge_snapshot_build(&file, 1, &snapshot, message, sizeof(message)) == KNOWLEDGE_ERR_FIELD);
    assert(snapshot == NULL);
    assert(strstr(message, path) != NULL && strstr(message, id) != NULL && strstr(message, field) != NULL);
}

static void test_refusals(void) {
    refused("{\"id\":\"user.instrument.flute\",\"kind\":\"instrument\",\"subtype\":\"instrument\",\"name\":\"flute\","
            "\"content\":{\"family\":\"woodwinds\",\"transposition\":0,\"clefs\":[\"treble\"],"
            "\"range\":{\"lowest\":96,\"highest\":60},\"general_midi_program\":73},"
            "\"citations\":[],\"status\":\"reference\",\"version\":1}",
            "flute.json", "user.instrument.flute", "content.range");
    refused("{\"id\":\"user.instrument.flute\",\"kind\":\"instrument\",\"subtype\":\"instrument\",\"name\":\"flute\","
            "\"content\":{\"family\":\"woodwinds\",\"transposition\":0,\"clefs\":[\"treble\",\"bass\",\"alto\",\"tenor\",\"treble\"],"
            "\"range\":{\"lowest\":60,\"highest\":96},\"general_midi_program\":73},"
            "\"citations\":[],\"status\":\"reference\",\"version\":1}",
            "flute.json", "user.instrument.flute", "content.clefs");
    refused("{\"id\":\"user.instrument.duo\",\"kind\":\"instrument\",\"subtype\":\"ensemble\",\"name\":\"duo\","
            "\"content\":{\"slots\":[{\"instrument\":\"user.instrument.none\",\"name\":\"One\",\"players\":1}]},"
            "\"citations\":[],\"status\":\"reference\",\"version\":1}",
            "duo.json", "user.instrument.duo", "content.slots");
}

static void test_user_ensemble(void) {
    const char *entries =
        "{\"id\":\"user.instrument.flute\",\"kind\":\"instrument\",\"subtype\":\"instrument\",\"name\":\"flute\","
        "\"content\":{\"family\":\"woodwinds\",\"transposition\":0,\"clefs\":[\"treble\"],"
        "\"range\":{\"lowest\":60,\"highest\":96},\"comfortable_range\":{\"lowest\":62,\"highest\":90},\"general_midi_program\":73},"
        "\"citations\":[],\"status\":\"reference\",\"version\":1},"
        "{\"id\":\"user.instrument.duo\",\"kind\":\"instrument\",\"subtype\":\"ensemble\",\"name\":\"duo\","
        "\"content\":{\"slots\":[{\"instrument\":\"user.instrument.flute\",\"name\":\"Flute\",\"players\":1},"
        "{\"instrument\":\"omt.instrument.piano\",\"name\":\"Piano\",\"players\":1}]},"
        "\"citations\":[],\"status\":\"reference\",\"version\":1}";
    char text[2048];
    snprintf(text, sizeof(text), "%s%s]}", USER_HEAD, entries);
    KnowledgeUserFile file = {"duo.json", text};
    KnowledgeSnapshot *snapshot = build(&file, 1);
    const Instrument *flute = knowledge_snapshot_instrument(snapshot, "user.instrument.flute");
    assert(flute != NULL && flute->staff_count == 1 && flute->has_comfortable_range && flute->comfortable_lowest == 62);
    assert(!flute->sustain_pedal);
    const EnsembleTemplate *duo = knowledge_snapshot_ensemble(snapshot, "user.instrument.duo");
    assert(duo != NULL && duo->slot_count == 2);
    assert(knowledge_snapshot_instrument(snapshot, duo->slots[1].instrument) ==
           knowledge_snapshot_instrument(snapshot, "omt.instrument.piano"));
    knowledge_snapshot_destroy(snapshot);
}

static void test_other_kinds(void) {
    KnowledgeSnapshot *snapshot = build(NULL, 0);
    assert(knowledge_snapshot_instrument(snapshot, "omt.material.major") == NULL);
    assert(knowledge_snapshot_ensemble(snapshot, "omt.material.major") == NULL);
    assert(knowledge_snapshot_instrument(snapshot, "omt.instrument.none") == NULL);
    assert(knowledge_snapshot_instrument(NULL, "omt.instrument.piano") == NULL);
    knowledge_snapshot_destroy(snapshot);
}

int main(void) {
    test_piano();
    test_ensemble();
    test_refusals();
    test_user_ensemble();
    test_other_kinds();
    puts("instrument_entries: all tests passed");
    return 0;
}
