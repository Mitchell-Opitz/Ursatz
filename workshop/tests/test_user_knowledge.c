#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <cJSON.h>
#include <common/files.h>
#include <ursatz_composer/composer_api.h>
#include <ursatz_workshop/workshop_api.h>

#include "base/knowledge.h"

#ifndef URSATZ_USER_KNOWLEDGE_FIXTURE
#error "URSATZ_USER_KNOWLEDGE_FIXTURE is not defined"
#endif

/*
 * User knowledge files (R-731): every .json file of the data folder's knowledge folder is read when the workshop opens, in name order, as
 * the snapshot's user files. Her two voicings (workshop/tests/fixtures/knowledge-user/voicings.json) are in the snapshot, and the Composer
 * lists them; a file with a broken entry fails the open and names it; no knowledge folder adds nothing.
 */

static char g_root[64];

static bool open_workshop(const char *data, Workshop **out, UrsatzError *error) {
    WorkshopConfig config = {data, NULL, 0, 0, 0};
    WorkshopConfig resolved;
    char storage[512];
    return workshop_config_resolve(&config, &resolved, storage, sizeof(storage), error) == URSATZ_OK &&
           workshop_open(&resolved, out, error) == URSATZ_OK;
}

static void test_her_voicings_are_in_the_snapshot(void) {
    char data[96];
    char folder[128];
    char file[192];
    assert(common_path_join(data, sizeof(data), g_root, "with-file"));
    assert(common_path_join(folder, sizeof(folder), data, "knowledge") && common_dir_make(folder, NULL) == URSATZ_OK);
    assert(common_path_join(file, sizeof(file), folder, "voicings.json") && common_file_copy(URSATZ_USER_KNOWLEDGE_FIXTURE, file, NULL) == URSATZ_OK);
    Workshop *workshop = NULL;
    UrsatzError error;
    assert(open_workshop(data, &workshop, &error) && workshop);
    const KnowledgeSnapshot *snapshot = workshop_knowledge_snapshot();
    const VoicingEntry *shimmer = knowledge_snapshot_voicing(snapshot, "user.voicing.lydian-shimmer");
    const VoicingEntry *fifths = knowledge_snapshot_voicing(snapshot, "user.voicing.minor-fifths");
    assert(shimmer && shimmer->member_count == 5 && shimmer->members[3].semitones == 18 && shimmer->fit_count == 1 &&
           strcmp(shimmer->fits[0]->id, "omt.chord-type.major-triad") == 0);
    assert(fifths && fifths->member_count == 4 && fifths->members[3].semitones == 15 && strcmp(fifths->fits[0]->id, "omt.chord-type.minor-triad") == 0);
    /* The Composer, over the same snapshot, lists them with the ones it ships. */
    ComposerResponse response;
    assert(composer_api_vocabulary(workshop_composer_context_global(), &response) == URSATZ_OK && response.json);
    assert(strstr(response.json, "user.voicing.lydian-shimmer") != NULL && strstr(response.json, "omt.voicing.shell-dominant") != NULL);
    composer_response_destroy(&response);
    workshop_close(workshop);
}

static void test_no_folder_adds_nothing(void) {
    char data[96];
    assert(common_path_join(data, sizeof(data), g_root, "without"));
    Workshop *workshop = NULL;
    UrsatzError error;
    assert(open_workshop(data, &workshop, &error) && workshop);
    assert(knowledge_snapshot_voicing(workshop_knowledge_snapshot(), "user.voicing.lydian-shimmer") == NULL);
    assert(knowledge_snapshot_voicing(workshop_knowledge_snapshot(), "omt.voicing.shell-dominant") != NULL);
    workshop_close(workshop);
}

static void test_a_broken_entry_fails_the_open(void) {
    char data[96];
    char folder[128];
    char file[192];
    static const char BROKEN[] =
        "{\"schema\":\"ursatz-knowledge\",\"schema_version\":1,\"source\":\"user\",\"entries\":[{\"id\":\"user.voicing.broken\","
        "\"kind\":\"voicing\",\"name\":\"broken\",\"content\":{\"members\":[{\"role\":\"root\",\"semitones\":0}],"
        "\"fits\":[\"omt.chord-type.no-such-type\"]},\"citations\":[],\"status\":\"active\",\"version\":1}]}";
    assert(common_path_join(data, sizeof(data), g_root, "broken"));
    assert(common_path_join(folder, sizeof(folder), data, "knowledge") && common_dir_make(folder, NULL) == URSATZ_OK);
    assert(common_path_join(file, sizeof(file), folder, "broken.json") &&
           common_file_write_atomic(file, BROKEN, strlen(BROKEN), NULL) == URSATZ_OK);
    Workshop *workshop = NULL;
    UrsatzError error;
    assert(!open_workshop(data, &workshop, &error) && workshop == NULL);
    assert(workshop_knowledge_snapshot() == NULL);
}

int main(void) {
    snprintf(g_root, sizeof(g_root), "workshop-user-knowledge-%ld", (long)getpid());
    assert(common_dir_make(g_root, NULL) == URSATZ_OK);
    test_her_voicings_are_in_the_snapshot();
    test_no_folder_adds_nothing();
    test_a_broken_entry_fails_the_open();
    assert(common_dir_remove(g_root, NULL) == URSATZ_OK);
    printf("workshop_user_knowledge: ok\n");
    return 0;
}
