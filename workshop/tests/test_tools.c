#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <common/files.h>

#include <ursatz_workshop/workshop_api.h>

/*
 * The third call, tools (R-656): a workshop with no data folder, the tools' refusals and the four tools the Composer's command line needs. The
 * test runs in a working directory of its own; a tools workshop must leave nothing in it.
 */

static char *read_text(const char *path, size_t *size) {
    char *data = NULL;
    size_t length = 0;
    UrsatzError error;
    assert(common_file_read(path, &data, &length, &error) == URSATZ_OK);
    if (size) {
        *size = length;
    }
    return data;
}

static cJSON *parse(const char *text) {
    cJSON *tree = cJSON_Parse(text);
    assert(tree);
    return tree;
}

static const char *text_member(const cJSON *object, const char *name) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);
    return cJSON_IsString(item) ? item->valuestring : NULL;
}

/* Calls a tool with params printed from text ("" for none); returns the status with the reply and the error filled. */
static UrsatzStatus call(Workshop *workshop, const char *name, const char *params_text, const WorkshopBytes *bytes, WorkshopReply *reply,
                         UrsatzError *error) {
    cJSON *params = params_text[0] ? parse(params_text) : NULL;
    memset(error, 0, sizeof(*error));
    UrsatzStatus status = workshop_tool(workshop, name, params, bytes, reply, error);
    cJSON_Delete(params);
    return status;
}

static void test_a_tools_workshop_has_no_data_folder(void) {
    UrsatzError error;
    Workshop *workshop = NULL;
    assert(workshop_tools_open(&workshop, &error) == URSATZ_OK && workshop);
    assert(!common_path_exists(".ursatz.lock") && !common_path_exists("data-folder.json") && !common_path_exists("journal"));

    WorkshopReply reply;
    cJSON *params = cJSON_CreateObject();
    cJSON *command = parse("{\"command\":\"project.create\",\"actor\":\"claude\",\"params\":{\"name\":\"P\"}}");
    assert(workshop_query(workshop, "project.list", params, NULL, &reply, &error) == URSATZ_ERR_REQUEST);
    assert(strcmp(error.message, "this workshop has no data folder") == 0 && error.path[0] == '\0');
    workshop_reply_destroy(&reply);
    assert(workshop_command(workshop, command, NULL, &reply, &error) == URSATZ_ERR_REQUEST);
    assert(strcmp(error.message, "this workshop has no data folder") == 0 && error.path[0] == '\0');
    workshop_reply_destroy(&reply);
    assert(workshop_start(workshop, &error) == URSATZ_ERR_REQUEST);
    assert(strcmp(error.message, "this workshop has no data folder") == 0 && error.path[0] == '\0');
    cJSON_Delete(params);
    cJSON_Delete(command);

    cJSON *status = NULL;
    assert(workshop_status(workshop, &status, &error) == URSATZ_OK);
    assert(cJSON_IsNull(cJSON_GetObjectItemCaseSensitive(status, "data_dir")));
    assert(cJSON_IsString(cJSON_GetObjectItemCaseSensitive(status, "composer_version")));
    cJSON_Delete(status);

    /* one workshop at a time in a process */
    Workshop *second = NULL;
    assert(workshop_tools_open(&second, &error) == URSATZ_ERR_CONFLICT && !second);
    assert(strcmp(error.message, "a workshop is already open in this process") == 0);
    workshop_close(workshop);
    assert(!common_path_exists(".ursatz.lock") && !common_path_exists("data-folder.json") && !common_path_exists("journal"));
    assert(workshop_tools_open(&second, &error) == URSATZ_OK && second); /* closed: a new one opens */
    workshop_close(second);
}

static void test_unknown_names_say_their_paths(void) {
    UrsatzError error;
    Workshop *workshop = NULL;
    assert(workshop_tools_open(&workshop, &error) == URSATZ_OK);
    WorkshopReply reply;
    assert(call(workshop, "bogus", "", NULL, &reply, &error) == URSATZ_ERR_REQUEST);
    assert(strcmp(error.path, "/name") == 0 && strcmp(error.message, "unknown tool 'bogus'") == 0);
    workshop_reply_destroy(&reply);
    assert(call(workshop, "composer", "{\"entry\":\"bogus\",\"request\":{}}", NULL, &reply, &error) == URSATZ_ERR_REQUEST);
    assert(strcmp(error.path, "/params/entry") == 0);
    workshop_reply_destroy(&reply);
    assert(call(workshop, "composer", "{\"entry\":\"progression\"}", NULL, &reply, &error) == URSATZ_ERR_REQUEST);
    assert(strcmp(error.path, "/params/request") == 0);
    workshop_reply_destroy(&reply);
    workshop_close(workshop);
}

static void test_composer_entries(void) {
    UrsatzError error;
    Workshop *workshop = NULL;
    assert(workshop_tools_open(&workshop, &error) == URSATZ_OK);
    WorkshopReply reply;

    assert(call(workshop, "composer", "{\"entry\":\"versions\"}", NULL, &reply, &error) == URSATZ_OK);
    cJSON *versions = parse(text_member(reply.json, "json"));
    assert(cJSON_IsString(cJSON_GetObjectItemCaseSensitive(versions, "composer_version")));
    cJSON_Delete(versions);
    assert(cJSON_IsNull(cJSON_GetObjectItemCaseSensitive(reply.json, "plan_json")));
    workshop_reply_destroy(&reply);

    assert(call(workshop, "composer", "{\"entry\":\"vocabulary\"}", NULL, &reply, &error) == URSATZ_OK);
    cJSON *vocabulary = parse(text_member(reply.json, "json"));
    assert(cJSON_IsObject(vocabulary));
    cJSON_Delete(vocabulary);
    workshop_reply_destroy(&reply);

    const char *progression =
        "{\"entry\":\"progression\",\"request\":{\"key\":\"D minor\",\"meter\":\"4/4\",\"bars\":4,\"cadence\":\"PAC\",\"seed\":1,"
        "\"embellish\":\"none\",\"plans\":true}}";
    assert(call(workshop, "composer", progression, NULL, &reply, &error) == URSATZ_OK);
    cJSON *document = parse(text_member(reply.json, "json"));
    assert(cJSON_IsArray(cJSON_GetObjectItemCaseSensitive(document, "chords")));
    cJSON_Delete(document);
    cJSON *plan = parse(text_member(reply.json, "plan_json"));
    assert(cJSON_IsObject(plan));
    cJSON_Delete(plan);
    workshop_reply_destroy(&reply);

    /* a refusal keeps the Composer's status and path */
    assert(call(workshop, "composer", "{\"entry\":\"random_motive\",\"request\":{\"mode\":\"major\",\"meter\":\"4/4\",\"preferences\":{\"melody\":{\"keep\":17}}}}", NULL,
                &reply, &error) == URSATZ_ERR_REQUEST);
    assert(strncmp(error.path, "/preferences", 12) == 0);
    workshop_reply_destroy(&reply);

    /* export of a plan file: MIDI as bytes, MusicXML as text */
    size_t size = 0;
    char *plan_text = read_text(URSATZ_TEST_COMPOSER "/snapshots/fill.plan.json", &size);
    cJSON *request = parse("{\"midi\":true,\"musicxml\":true}");
    cJSON_AddItemToObject(request, "plan", parse(plan_text));
    cJSON *params = cJSON_CreateObject();
    cJSON_AddStringToObject(params, "entry", "export");
    cJSON_AddItemToObject(params, "request", request);
    memset(&error, 0, sizeof(error));
    assert(workshop_tool(workshop, "composer", params, NULL, &reply, &error) == URSATZ_OK);
    cJSON_Delete(params);
    free(plan_text);
    assert(reply.size > 4 && memcmp(reply.bytes, "MThd", 4) == 0 && strcmp(reply.content_type, "audio/midi") == 0);
    assert(text_member(reply.json, "musicxml") && strstr(text_member(reply.json, "musicxml"), "score-partwise"));
    workshop_reply_destroy(&reply);

    /* a motive from a MIDI file's bytes; without bytes the call names /bytes */
    char *midi = read_text(URSATZ_TEST_COMPOSER "/fixtures/motive_d_minor.mid", &size);
    WorkshopBytes bytes = {(const uint8_t *)midi, size, "motive_d_minor.mid"};
    assert(call(workshop, "composer", "{\"entry\":\"motive_from_midi\",\"request\":{}}", &bytes, &reply, &error) == URSATZ_OK);
    cJSON *motive = parse(text_member(reply.json, "json"));
    assert(cJSON_IsObject(motive));
    cJSON_Delete(motive);
    workshop_reply_destroy(&reply);
    free(midi);
    assert(call(workshop, "composer", "{\"entry\":\"motive_from_midi\",\"request\":{}}", NULL, &reply, &error) == URSATZ_ERR_REQUEST);
    assert(strcmp(error.path, "/bytes") == 0);
    workshop_reply_destroy(&reply);
    workshop_close(workshop);
}

static void test_corpus_passages(void) {
    UrsatzError error;
    Workshop *workshop = NULL;
    assert(workshop_tools_open(&workshop, &error) == URSATZ_OK);
    WorkshopReply reply;
    char params[1024];
    snprintf(params, sizeof(params), "{\"dir\":\"%s\",\"numerals\":\"I IV V I\",\"limit\":5}", URSATZ_TEST_SEED_CORPUS);
    assert(call(workshop, "corpus-passages", params, NULL, &reply, &error) == URSATZ_OK);
    const cJSON *passages = cJSON_GetObjectItemCaseSensitive(reply.json, "passages");
    assert(cJSON_IsArray(passages) && cJSON_GetArraySize(passages) >= 1 && cJSON_GetArraySize(passages) <= 5);
    workshop_reply_destroy(&reply);

    assert(call(workshop, "corpus-passages", "{\"dir\":\"no-such-corpus\",\"numerals\":\"I IV V I\"}", NULL, &reply, &error) ==
           URSATZ_ERR_NOT_FOUND);
    assert(strcmp(error.path, "/params/dir") == 0);
    workshop_reply_destroy(&reply);
    assert(call(workshop, "corpus-passages", "{\"dir\":\"x\",\"numerals\":\"I\",\"like\":\"y\"}", NULL, &reply, &error) == URSATZ_ERR_REQUEST);
    workshop_reply_destroy(&reply);
    snprintf(params, sizeof(params), "{\"dir\":\"%s\",\"like\":\"no-such-file.mid\"}", URSATZ_TEST_SEED_CORPUS);
    assert(call(workshop, "corpus-passages", params, NULL, &reply, &error) == URSATZ_ERR_REQUEST);
    assert(strcmp(error.path, "/params/like") == 0);
    workshop_reply_destroy(&reply);
    workshop_close(workshop);
}

static void test_pitch_names(void) {
    UrsatzError error;
    Workshop *workshop = NULL;
    assert(workshop_tools_open(&workshop, &error) == URSATZ_OK);
    WorkshopReply reply;
    assert(call(workshop, "pitch-names", "{\"midi\":[60,61,127,0]}", NULL, &reply, &error) == URSATZ_OK);
    const cJSON *names = cJSON_GetObjectItemCaseSensitive(reply.json, "names");
    assert(cJSON_GetArraySize(names) == 4);
    assert(strcmp(cJSON_GetArrayItem(names, 0)->valuestring, "C4") == 0);
    assert(strcmp(cJSON_GetArrayItem(names, 1)->valuestring, "C#4") == 0);
    assert(strcmp(cJSON_GetArrayItem(names, 2)->valuestring, "G9") == 0);
    assert(strcmp(cJSON_GetArrayItem(names, 3)->valuestring, "C-1") == 0);
    workshop_reply_destroy(&reply);
    assert(call(workshop, "pitch-names", "{\"midi\":[60,128]}", NULL, &reply, &error) == URSATZ_ERR_REQUEST);
    assert(strcmp(error.path, "/params/midi/1") == 0);
    workshop_reply_destroy(&reply);
    workshop_close(workshop);
}

static void test_a_data_folder_workshop_answers_tools_before_start(void) {
    char data[96];
    snprintf(data, sizeof(data), "tools-data-%ld", (long)getpid());
    WorkshopConfig config = {data, NULL, 0, 0, 0};
    WorkshopConfig resolved;
    char storage[512];
    UrsatzError error;
    assert(workshop_config_resolve(&config, &resolved, storage, sizeof(storage), &error) == URSATZ_OK);
    Workshop *workshop = NULL;
    assert(workshop_open(&resolved, &workshop, &error) == URSATZ_OK && workshop);
    WorkshopReply reply;
    assert(call(workshop, "versions", "", NULL, &reply, &error) == URSATZ_OK);
    assert(cJSON_IsString(cJSON_GetObjectItemCaseSensitive(reply.json, "composer_version")));
    assert(!cJSON_GetObjectItemCaseSensitive(reply.json, "data_dir"));
    workshop_reply_destroy(&reply);
    /* a tools workshop is refused while this one is open */
    Workshop *second = NULL;
    assert(workshop_tools_open(&second, &error) == URSATZ_ERR_CONFLICT);
    workshop_close(workshop);
    assert(common_dir_remove(data, NULL) == URSATZ_OK);
}

int main(void) {
    test_a_tools_workshop_has_no_data_folder();
    test_unknown_names_say_their_paths();
    test_composer_entries();
    test_corpus_passages();
    test_pitch_names();
    test_a_data_folder_workshop_answers_tools_before_start();
    puts("ok");
    return 0;
}
