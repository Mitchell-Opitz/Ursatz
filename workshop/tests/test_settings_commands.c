#include "api_harness.h"

#include <ursatz_test.h>

#include <common/files.h>
#include <common/json.h>

#ifndef SETTINGS_TASTE_NONE
#error "SETTINGS_TASTE_NONE must name tests/fixtures/settings/taste-none.json"
#endif

/*
 * The settings commands (R-671, R-676): every command's effect and refusals through the API (the harness's workshop and its journal), the queries that
 * read the scopes back, a taste profile that changes only by an import that says why, a value outside its limit refused with the Composer's message at the
 * path of the call, and the files the commands leave (workshop_formats validates this test's folder).
 */

static char g_project[96];
static char g_item[96];

static const char *const kMotifBody =
    "{\"uses\":[],\"motif\":{\"role\":\"melodic\",\"form\":\"full\",\"notes\":\"C4:e Eb4:e G4:e Eb4:e C4:e Eb4:e G4:e C5:e\","
    "\"meter\":\"4/4\",\"key\":\"C minor\",\"pickup\":\"0\",\"features\":[],\"analysis\":null,\"derived\":null}}";

/* Runs a settings command with params text (the scope of the item when it has {ITEM}); true on OK, the reply in out (the caller destroys it). */
static UrsatzStatus run(const char *name, const char *params_text, WorkshopReply *reply, UrsatzError *error) {
    char params[3000];
    const char *marker = strstr(params_text, "{ITEM}");
    if (marker) {
        snprintf(params, sizeof params, "%.*s%s%s", (int)(marker - params_text), params_text, g_item, marker + 6);
    } else {
        snprintf(params, sizeof params, "%s", params_text);
    }
    return command(name, NULL, params, -1, reply, error);
}

static void expect_refusal(const char *name, const char *params, UrsatzStatus status, const char *path, const char *message_part) {
    WorkshopReply reply;
    UrsatzError error;
    UrsatzStatus got = run(name, params, &reply, &error);
    CHECKF(got == status && strcmp(error.path, path) == 0 && strstr(error.message, message_part) != NULL,
           "%s %s: wanted %d at %s containing '%s', got %d at %s '%s'", name, params, (int)status, path, message_part, (int)got, error.path, error.message);
    workshop_reply_destroy(&reply);
}

static cJSON *scope_of(const char *scope) {
    char params[200];
    snprintf(params, sizeof params, "{\"scope\":\"%s\"}", scope);
    WorkshopReply reply;
    UrsatzError error;
    cJSON *content = NULL;
    if (query("settings.scope", params, &reply, &error) == URSATZ_OK) {
        content = cJSON_Duplicate(reply.json, true);
    }
    workshop_reply_destroy(&reply);
    return content;
}

static bool journaled(const char *command_name) {
    cJSON *record = last_record();
    bool ok = record && strcmp(str(record, "command"), command_name) == 0 && strcmp(str(record, "actor"), "test") == 0;
    cJSON_Delete(record);
    return ok;
}

static void test_set_and_unset(void) {
    WorkshopReply reply;
    UrsatzError error;
    char scope[160];
    snprintf(scope, sizeof scope, "item:%s", g_item);
    check(run("settings.set", "{\"scope\":\"item:{ITEM}\",\"key\":\"melody.keep\",\"value\":3}", &reply, &error) == URSATZ_OK &&
              num(member(reply.json, "values"), "melody.keep") == 3 && strcmp(str(reply.json, "scope"), scope) == 0,
          "settings.set writes the value and answers with the scope");
    workshop_reply_destroy(&reply);
    check(journaled("settings.set"), "settings.set is journaled with its actor");
    check(run("settings.set", "{\"scope\":\"item:{ITEM}\",\"key\":\"melody.second_nearest_chord_tone\",\"value\":\"1/2\"}", &reply, &error) == URSATZ_OK &&
              strcmp(str(member(reply.json, "values"), "melody.second_nearest_chord_tone"), "1/2") == 0,
          "a ratio is a string n/d");
    workshop_reply_destroy(&reply);
    check(run("settings.set", "{\"scope\":\"item:{ITEM}\",\"key\":\"melody.nct_types\",\"value\":[\"passing\"]}", &reply, &error) == URSATZ_OK, "a list of words is a value");
    workshop_reply_destroy(&reply);
    check(run("settings.set", "{\"scope\":\"item:{ITEM}\",\"key\":\"explore.share\",\"value\":\"1/4\"}", &reply, &error) == URSATZ_OK, "an explore key is a setting");
    workshop_reply_destroy(&reply);
    cJSON *file = scope_of(scope);
    check(num(member(file, "values"), "melody.keep") == 3 && cJSON_GetArraySize(member(file, "values")) == 4, "settings.scope reads the file back");
    cJSON_Delete(file);

    expect_refusal("settings.set", "{\"scope\":\"taste:none\",\"key\":\"melody.keep\",\"value\":3}", URSATZ_ERR_REQUEST, "/params/scope",
                   "a taste setting changes only by keep as default (ratings.keep)");
    expect_refusal("settings.unset", "{\"scope\":\"taste:none\",\"key\":\"melody.keep\"}", URSATZ_ERR_REQUEST, "/params/scope", "keep as default");
    expect_refusal("settings.set", "{\"scope\":\"item:{ITEM}\",\"key\":\"melody.nothing\",\"value\":3}", URSATZ_ERR_REQUEST, "/params/key", "unknown setting 'melody.nothing'");
    expect_refusal("settings.set", "{\"scope\":\"item:{ITEM}\",\"key\":\"melody.keep\",\"value\":99}", URSATZ_ERR_REQUEST, "/params/value", "must be");
    expect_refusal("settings.set", "{\"scope\":\"item:{ITEM}\",\"key\":\"melody.keep\",\"value\":\"many\"}", URSATZ_ERR_REQUEST, "/params/value", "");
    expect_refusal("settings.set", "{\"scope\":\"item:{ITEM}\",\"key\":\"motif.plan\",\"value\":[\"I\",\"V\"]}", URSATZ_ERR_REQUEST, "/params/value",
                   "a chord plan is intent, not a setting: give it with the request");
    expect_refusal("settings.set", "{\"scope\":\"item:{ITEM}\",\"key\":\"explore.share\",\"value\":\"3/4\"}", URSATZ_ERR_REQUEST, "/params/value", "explore.share");
    expect_refusal("settings.set", "{\"scope\":\"item:mot-nothing\",\"key\":\"melody.keep\",\"value\":3}", URSATZ_ERR_NOT_FOUND, "/params/scope", "no item");
    expect_refusal("settings.set", "{\"scope\":\"style:nothing\",\"key\":\"melody.keep\",\"value\":3}", URSATZ_ERR_NOT_FOUND, "/params/scope", "no style");
    expect_refusal("settings.set", "{\"scope\":\"melody\",\"key\":\"melody.keep\",\"value\":3}", URSATZ_ERR_REQUEST, "/params/scope", "a scope is");

    check(run("settings.unset", "{\"scope\":\"item:{ITEM}\",\"key\":\"melody.keep\"}", &reply, &error) == URSATZ_OK && member(member(reply.json, "values"), "melody.keep") == NULL,
          "settings.unset takes the key out");
    workshop_reply_destroy(&reply);
}

static void test_import_and_taste(void) {
    WorkshopReply reply;
    UrsatzError error;
    char *text = test_must_read_file(SETTINGS_TASTE_NONE, NULL);
    cJSON *document = cJSON_Parse(text);
    free(text);
    cJSON *params = cJSON_CreateObject();
    cJSON_AddStringToObject(params, "scope", "taste:none");
    cJSON_AddItemToObject(params, "document", cJSON_Duplicate(document, true));
    char *params_text = cJSON_PrintUnformatted(params);
    UrsatzStatus status = command("settings.import", NULL, params_text, -1, &reply, &error);
    check(status != URSATZ_OK && strcmp(error.path, "/params/note") == 0, "a taste profile import without a note is refused");
    workshop_reply_destroy(&reply);
    cJSON_AddStringToObject(params, "note", "taste-profile.md none");
    free(params_text);
    params_text = cJSON_PrintUnformatted(params);
    status = command("settings.import", NULL, params_text, -1, &reply, &error);
    check(status == URSATZ_OK && cJSON_GetArraySize(member(reply.json, "values")) == 17, "settings.import of the none profile writes its 17 keys");
    const cJSON *kept = member(reply.json, "kept");
    check(cJSON_GetArraySize(kept) == 1 && strcmp(str(cJSON_GetArrayItem(kept, 0), "key"), "*") == 0 && strcmp(str(cJSON_GetArrayItem(kept, 0), "note"), "taste-profile.md none") == 0 &&
              strcmp(str(cJSON_GetArrayItem(kept, 0), "actor"), "test") == 0,
          "and a kept entry that says why, by whom");
    workshop_reply_destroy(&reply);
    check(journaled("settings.import"), "settings.import is journaled");
    free(params_text);
    cJSON_Delete(params);
    cJSON_Delete(document);

    /* a value outside its limit is refused with the Composer's message, at its place in the document */
    expect_refusal("settings.import", "{\"scope\":\"item:{ITEM}\",\"document\":{\"melody\":{\"keep\":99}}}", URSATZ_ERR_REQUEST, "/params/document/melody/keep", "must be");
    expect_refusal("settings.import", "{\"scope\":\"item:{ITEM}\",\"document\":{\"melody\":{\"nonsense\":1}}}", URSATZ_ERR_REQUEST, "/params/document/melody/nonsense", "unknown setting");
    expect_refusal("settings.import", "{\"scope\":\"item:{ITEM}\",\"document\":{\"motif\":{\"plan\":[\"I\",\"V\"]}}}", URSATZ_ERR_REQUEST, "/params/document/motif/plan", "a chord plan is intent");

    /* replace drops what the document does not hold */
    check(run("settings.import", "{\"scope\":\"item:{ITEM}\",\"document\":{\"melody\":{\"keep\":2}}}", &reply, &error) == URSATZ_OK, "an import into an item");
    workshop_reply_destroy(&reply);
    check(run("settings.import", "{\"scope\":\"item:{ITEM}\",\"document\":{\"melody\":{\"attempts\":30}},\"replace\":true}", &reply, &error) == URSATZ_OK &&
              member(member(reply.json, "values"), "melody.keep") == NULL && num(member(reply.json, "values"), "melody.attempts") == 0 + 30,
          "replace drops the settings the document does not hold");
    workshop_reply_destroy(&reply);
}

static void test_override_switch_taste(void) {
    WorkshopReply reply;
    UrsatzError error;
    check(run("settings.override", "{\"scope\":\"item:{ITEM}\",\"rule\":\"parallel-fifths\",\"action\":\"waive\",\"note\":\"ok\"}", &reply, &error) == URSATZ_OK &&
              cJSON_GetArraySize(member(reply.json, "overrides")) == 1 && strcmp(str(cJSON_GetArrayItem(member(reply.json, "overrides"), 0), "actor"), "test") == 0,
          "settings.override writes an override with who and when");
    workshop_reply_destroy(&reply);
    check(run("settings.override", "{\"scope\":\"item:{ITEM}\",\"rule\":\"leaps\",\"action\":\"reweight\",\"weight\":7}", &reply, &error) == URSATZ_OK &&
              cJSON_GetArraySize(member(reply.json, "overrides")) == 2,
          "a second rule adds one");
    workshop_reply_destroy(&reply);
    check(run("settings.override", "{\"scope\":\"item:{ITEM}\",\"rule\":\"leaps\",\"action\":\"reweight\",\"weight\":9}", &reply, &error) == URSATZ_OK &&
              cJSON_GetArraySize(member(reply.json, "overrides")) == 2 && num(cJSON_GetArrayItem(member(reply.json, "overrides"), 1), "weight") == 9,
          "an override of the same rule replaces the old one");
    workshop_reply_destroy(&reply);
    expect_refusal("settings.override", "{\"scope\":\"taste:none\",\"rule\":\"leaps\",\"action\":\"off\"}", URSATZ_ERR_REQUEST, "/params/scope",
                   "overrides go on a project, an item or a request, never in a profile");
    expect_refusal("settings.override", "{\"scope\":\"item:{ITEM}\",\"rule\":\"nothing\",\"action\":\"off\"}", URSATZ_ERR_REQUEST, "/params/rule", "parallel-fifths");
    expect_refusal("settings.override", "{\"scope\":\"item:{ITEM}\",\"rule\":\"leaps\",\"action\":\"reweight\"}", URSATZ_ERR_REQUEST, "/params/rule", "needs a weight");
    expect_refusal("settings.override", "{\"scope\":\"item:{ITEM}\",\"rule\":\"no-crossing\",\"action\":\"reverse\"}", URSATZ_ERR_REQUEST, "/params/rule", "only a soft rule group can be reversed");
    char query_params[200];
    snprintf(query_params, sizeof query_params, "{\"item_id\":\"%s\"}", g_item);
    check(query("settings.resolve", query_params, &reply, &error) == URSATZ_OK && cJSON_GetArraySize(member(reply.json, "overrides")) == 2, "settings.resolve shows the overrides");
    workshop_reply_destroy(&reply);
    check(run("settings.override", "{\"scope\":\"item:{ITEM}\",\"rule\":\"leaps\",\"action\":\"clear\"}", &reply, &error) == URSATZ_OK &&
              cJSON_GetArraySize(member(reply.json, "overrides")) == 1,
          "clear removes a rule's override");
    workshop_reply_destroy(&reply);

    check(run("settings.switch", "{\"scope\":\"item:{ITEM}\",\"off\":[\"taste\",\"project\"]}", &reply, &error) == URSATZ_OK && cJSON_GetArraySize(member(reply.json, "off")) == 2,
          "settings.switch replaces the off list");
    workshop_reply_destroy(&reply);
    char project_params[300];
    snprintf(project_params, sizeof project_params, "{\"scope\":\"project:%s\",\"off\":[\"project\"]}", g_project);
    expect_refusal("settings.switch", project_params, URSATZ_ERR_REQUEST, "/params/off", "only an item can switch its project off");
    expect_refusal("settings.switch", "{\"scope\":\"item:{ITEM}\",\"off\":[\"everything\"]}", URSATZ_ERR_REQUEST, "/params/off/0", "a switch is");
    expect_refusal("settings.switch", "{\"scope\":\"taste:none\",\"off\":[\"style\"]}", URSATZ_ERR_REQUEST, "/params/scope", "project or an item");
    check(run("settings.switch", "{\"scope\":\"item:{ITEM}\",\"off\":[]}", &reply, &error) == URSATZ_OK && member(reply.json, "off") == NULL, "an empty list clears the switches");
    workshop_reply_destroy(&reply);

    check(run("settings.use-taste", "{\"scope\":\"item:{ITEM}\",\"taste\":\"none\"}", &reply, &error) == URSATZ_OK && strcmp(str(reply.json, "taste"), "none") == 0,
          "settings.use-taste names the profile");
    workshop_reply_destroy(&reply);
    check(run("settings.use-taste", "{\"scope\":\"item:{ITEM}\"}", &reply, &error) == URSATZ_OK && member(reply.json, "taste") == NULL, "and no taste clears it");
    workshop_reply_destroy(&reply);
    expect_refusal("settings.use-taste", "{\"scope\":\"taste:none\",\"taste\":\"none\"}", URSATZ_ERR_REQUEST, "/params/scope", "does not name a taste profile");
}

static void test_queries(void) {
    WorkshopReply reply;
    UrsatzError error;
    check(query("settings.list", NULL, &reply, &error) == URSATZ_OK && cJSON_GetArraySize(member(reply.json, "tastes")) == 1 &&
              strcmp(cJSON_GetArrayItem(member(reply.json, "tastes"), 0)->valuestring, "none") == 0 && cJSON_GetArraySize(member(reply.json, "items")) == 1 &&
              strcmp(cJSON_GetArrayItem(member(reply.json, "items"), 0)->valuestring, g_item) == 0,
          "settings.list names the scope files present");
    workshop_reply_destroy(&reply);
    check(query("settings.keys", NULL, &reply, &error) == URSATZ_OK && cJSON_GetArraySize(member(reply.json, "keys")) == 129 && cJSON_GetArraySize(member(reply.json, "rules")) == 20,
          "settings.keys lists the keys (the Composer's 127 and the workshop's two) and the 20 rules");
    workshop_reply_destroy(&reply);
    cJSON *empty = scope_of("project:nothing");
    check(empty == NULL, "a scope of a project that is not there is not read");
    char scope[160];
    snprintf(scope, sizeof scope, "project:%s", g_project);
    empty = scope_of(scope);
    check(empty && cJSON_GetArraySize(member(empty, "values")) == 0, "a scope with no file is empty");
    cJSON_Delete(empty);
}

int main(void) {
    if (!harness_open(18791)) {
        return 1;
    }
    WorkshopReply reply;
    UrsatzError error;
    bool ready = command("project.create", NULL, "{\"name\":\"Settings\"}", -1, &reply, &error) == URSATZ_OK;
    if (ready) {
        snprintf(g_project, sizeof g_project, "%s", str(member(reply.json, "project"), "id"));
    }
    workshop_reply_destroy(&reply);
    char params[3000];
    snprintf(params, sizeof params, "{\"type\":\"motif\",\"name\":\"Hook\",\"project_id\":\"%s\",\"body\":%s}", g_project, kMotifBody);
    ready = ready && command("item.create", NULL, params, -1, &reply, &error) == URSATZ_OK;
    if (ready) {
        snprintf(g_item, sizeof g_item, "%s", str(member(reply.json, "item"), "id"));
    }
    workshop_reply_destroy(&reply);
    check(ready, "a project and a motif are made");
    if (ready) {
        test_set_and_unset();
        test_import_and_taste();
        test_override_switch_taste();
        test_queries();
    }
    harness_close();
    return test_finish("workshop_settings_commands");
}
