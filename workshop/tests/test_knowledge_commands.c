/* The knowledge commands (R-768): add, update and delete her entries in knowledge/user.json, with the Composer's vocabulary following without reopening; the
 * refusals with their messages; keep as my schema from a composed harmony plan, which a progression request then places. Runs in its own working directory. */

#include "api_harness.h"

#include <assert.h>

#include <common/files.h>
#include <ursatz_composer/composer_api.h>

#include <ursatz_test.h>

#include "base/knowledge.h"

static const char *kVoicing =
    "{\"kind\":\"voicing\",\"name\":\"wide fifths\",\"content\":{\"members\":[{\"role\":\"root\",\"semitones\":0},{\"role\":\"fifth\",\"semitones\":7},"
    "{\"role\":\"ninth\",\"semitones\":14}],\"fits\":[\"omt.chord-type.major-triad\"]},\"citations\":[],\"status\":\"active\","
    "\"examples\":[{\"root\":\"C\",\"pitches\":[\"C\",\"G\",\"D\"]}],\"version\":1}";

static const char *kHarmonyBody =
    "{\"uses\":[],\"request\":{\"key\":\"C minor\",\"meter\":\"4/4\",\"bars\":4,\"cadence\":\"none\",\"seed\":1,"
    "\"palette\":{\"with\":\"dorian\",\"spans\":[{\"bars\":[1,4],\"chords\":\"shared\"}]}}}";

static bool vocabulary_has(const char *id) {
    WorkshopReply reply;
    UrsatzError error;
    bool found = false;
    if (query("composer.vocabulary", NULL, &reply, &error) == URSATZ_OK) {
        char *text = cJSON_PrintUnformatted(reply.json);
        found = text && strstr(text, id) != NULL;
        free(text);
    }
    workshop_reply_destroy(&reply);
    return found;
}

static void test_add_update_delete(void) {
    WorkshopReply reply;
    UrsatzError error;
    char params[2048];
    check(!vocabulary_has("user.voicing.wide-fifths"), "the vocabulary does not hold her voicing yet");
    snprintf(params, sizeof(params), "{\"entry\":{\"id\":\"user.voicing.wide-fifths\",%s}", kVoicing + 1);
    check(command("knowledge.add", NULL, params, -1, &reply, &error) == URSATZ_OK && strcmp(str(member(reply.json, "entry"), "id"), "user.voicing.wide-fifths") == 0, "knowledge.add stores her entry and answers it");
    workshop_reply_destroy(&reply);
    check(common_path_exists("knowledge/user.json"), "knowledge/user.json is written");
    check(workshop_knowledge_snapshot() != NULL && knowledge_snapshot_voicing(workshop_knowledge_snapshot(), "user.voicing.wide-fifths") != NULL, "the workshop's snapshot has it");
    check(vocabulary_has("user.voicing.wide-fifths"), "composer.vocabulary offers it without reopening");
    check(query("knowledge.find", "{\"words\":\"wide fifths\"}", &reply, &error) == URSATZ_OK && cJSON_GetArraySize(member(reply.json, "entries")) == 1 &&
              cJSON_IsTrue(member(cJSON_GetArrayItem(member(reply.json, "entries"), 0), "user")),
          "knowledge.find finds it as hers");
    workshop_reply_destroy(&reply);
    snprintf(params, sizeof(params), "{\"id\":\"user.voicing.wide-fifths\",\"entry\":{%s}", kVoicing + 1);
    char *renamed = strdup(params);
    char *at = strstr(renamed, "wide fifths");
    memcpy(at, "wider fifth", 11);
    check(command("knowledge.update", NULL, renamed, -1, &reply, &error) == URSATZ_OK && strcmp(str(member(reply.json, "entry"), "name"), "wider fifth") == 0, "knowledge.update replaces it");
    workshop_reply_destroy(&reply);
    free(renamed);
    check(query("knowledge.show", "{\"id\":\"user.voicing.wide-fifths\"}", &reply, &error) == URSATZ_OK && strcmp(str(member(reply.json, "entry"), "name"), "wider fifth") == 0, "the snapshot shows the new name");
    workshop_reply_destroy(&reply);
    check(command("knowledge.delete", NULL, "{\"id\":\"user.voicing.wide-fifths\"}", -1, &reply, &error) == URSATZ_OK, "knowledge.delete removes it");
    workshop_reply_destroy(&reply);
    check(!vocabulary_has("user.voicing.wide-fifths") && knowledge_snapshot_voicing(workshop_knowledge_snapshot(), "user.voicing.wide-fifths") == NULL, "and the vocabulary and the snapshot follow");
}

static void test_refusals(void) {
    WorkshopReply reply;
    UrsatzError error;
    char params[2048];
    check(command("knowledge.add", NULL, "{\"entry\":{\"id\":\"mine.voicing.x\",\"kind\":\"voicing\",\"name\":\"x\"}}", -1, &reply, &error) == URSATZ_ERR_REQUEST &&
              strstr(error.message, "starts with user.") != NULL,
          "an id without user. is refused");
    check(command("knowledge.add", NULL, "{\"entry\":{\"id\":\"user.material.major\",\"kind\":\"material\",\"name\":\"x\"}}", -1, &reply, &error) != URSATZ_OK, "an entry the loader refuses is refused");
    snprintf(params, sizeof(params), "{\"entry\":{\"id\":\"user.voicing.twice\",%s}", kVoicing + 1);
    check(command("knowledge.add", NULL, params, -1, &reply, &error) == URSATZ_OK, "an entry is added");
    workshop_reply_destroy(&reply);
    check(command("knowledge.add", NULL, params, -1, &reply, &error) == URSATZ_ERR_CONFLICT && strcmp(error.path, "/entry/id") == 0, "the same id again is a duplicate");
    check(command("knowledge.add", NULL, "{\"entry\":{\"id\":\"user.voicing.broken\",\"kind\":\"voicing\",\"name\":\"broken\",\"content\":{\"members\":[{\"role\":\"root\",\"semitones\":0}],\"fits\":[]},\"citations\":[],\"status\":\"active\",\"version\":1}}",
                  -1, &reply, &error) == URSATZ_ERR_REQUEST && strcmp(error.path, "/entry") == 0 && strstr(error.message, "content.members") != NULL,
          "a broken voicing answers the loader's message at /entry");
    check(command("knowledge.update", NULL, "{\"id\":\"user.voicing.never\",\"entry\":{\"kind\":\"voicing\"}}", -1, &reply, &error) == URSATZ_ERR_NOT_FOUND, "an id that is not hers is not found");
    /* an id another file of the folder defines is that file's to edit */
    const char *other = "{\"schema\":\"ursatz-knowledge\",\"schema_version\":1,\"source\":\"user\",\"entries\":[{\"id\":\"user.voicing.elsewhere\",\"kind\":\"voicing\",\"name\":\"elsewhere\","
                        "\"content\":{\"members\":[{\"role\":\"root\",\"semitones\":0},{\"role\":\"fifth\",\"semitones\":7}],\"fits\":[\"omt.chord-type.major-triad\"]},\"citations\":[],\"status\":\"active\","
                        "\"examples\":[{\"root\":\"C\",\"pitches\":[\"C\",\"G\"]}],\"version\":1}]}";
    check(common_file_write_atomic("knowledge/other.json", other, strlen(other), NULL) == URSATZ_OK, "another file of the folder is written by hand");
    check(command("knowledge.delete", NULL, "{\"id\":\"user.voicing.elsewhere\"}", -1, &reply, &error) == URSATZ_ERR_REQUEST && strstr(error.message, "is in other.json; edit that file") != NULL,
          "an id of another file is refused with its file");
    check(command("knowledge.delete", NULL, "{\"id\":\"user.voicing.twice\"}", -1, &reply, &error) == URSATZ_OK, "her own entry is deleted");
    workshop_reply_destroy(&reply);
}

static void test_keep_schema(void) {
    WorkshopReply reply;
    UrsatzError error;
    char params[1024];
    char project[64] = "";
    char harmony[64] = "";
    check(command("project.create", NULL, "{\"name\":\"Work\"}", -1, &reply, &error) == URSATZ_OK, "a project is made");
    snprintf(project, sizeof(project), "%s", str(member(reply.json, "project"), "id"));
    workshop_reply_destroy(&reply);
    snprintf(params, sizeof(params), "{\"type\":\"harmony\",\"name\":\"Sad\",\"project_id\":\"%s\",\"body\":%s}", project, kHarmonyBody);
    check(command("item.create", NULL, params, -1, &reply, &error) == URSATZ_OK, "a harmony plan is created");
    snprintf(harmony, sizeof(harmony), "%s", str(member(reply.json, "item"), "id"));
    workshop_reply_destroy(&reply);
    snprintf(params, sizeof(params), "{\"item\":\"%s\",\"version\":1,\"name\":\"Sad lift\",\"slug\":\"sad-lift\"}", harmony);
    check(command("knowledge.keep-schema", NULL, params, -1, &reply, &error) == URSATZ_ERR_REQUEST && strstr(error.message, "not composed") != NULL, "a plan that is not composed cannot be kept");
    check(command("item.compose", harmony, "{\"seed\":1}", 1, &reply, &error) == URSATZ_OK, "the plan is composed");
    cJSON *job = finish_job(&reply);
    check(job && num(job, "http_status") == 200, "the compose is done");
    cJSON_Delete(job);
    workshop_reply_destroy(&reply);
    snprintf(params, sizeof(params), "{\"item\":\"%s\",\"version\":2,\"name\":\"Sad lift\",\"slug\":\"Not A Slug\"}", harmony);
    check(command("knowledge.keep-schema", NULL, params, -1, &reply, &error) == URSATZ_ERR_REQUEST && strcmp(error.path, "/params/slug") == 0, "a slug is words and hyphens");
    snprintf(params, sizeof(params), "{\"item\":\"%s\",\"version\":2,\"name\":\"Sad lift\",\"slug\":\"sad-lift\",\"note\":\"the sad lift\"}", harmony);
    check(command("knowledge.keep-schema", NULL, params, -1, &reply, &error) == URSATZ_OK && strcmp(str(member(reply.json, "entry"), "id"), "user.schema.sad-lift") == 0, "keep-schema adds user.schema.sad-lift");
    const cJSON *content = member(member(reply.json, "entry"), "content");
    check(cJSON_GetArraySize(member(content, "stages")) >= 1 && cJSON_IsTrue(member(content, "loop")) && strcmp(str(content, "role"), "loop") == 0 &&
              strstr(str(cJSON_GetArrayItem(member(member(reply.json, "entry"), "citations"), 0), "note"), "kept from") != NULL,
          "its stages are the plan's, a loop, with a citation naming the plan");
    workshop_reply_destroy(&reply);
    check(query("knowledge.find", "{\"words\":\"sad lift\",\"kind\":\"schema\"}", &reply, &error) == URSATZ_OK && cJSON_GetArraySize(member(reply.json, "entries")) == 1, "knowledge.find finds the schema");
    workshop_reply_destroy(&reply);
    /* a progression request's span places it (the Composer reads the rebuilt snapshot) */
    static const char *request =
        "{\"key\":\"C minor\",\"meter\":\"4/4\",\"bars\":8,\"cadence\":\"none\",\"seed\":1,\"palette\":{\"with\":\"dorian\",\"spans\":[{\"bars\":[1,8],\"chords\":\"any\","
        "\"schema\":\"user.schema.sad-lift\",\"loop\":true}]}}";
    ComposerResponse response;
    UrsatzStatus status = composer_api_progression(workshop_composer_context_global(), request, NULL, &response);
    check(status == URSATZ_OK && response.json != NULL, "a progression request places her schema");
    composer_response_destroy(&response);
}

int main(void) {
    if (!harness_open(18998)) {
        return 1;
    }
    test_add_update_delete();
    test_refusals();
    test_keep_schema();
    harness_close();
    return test_finish("workshop_knowledge_commands");
}
