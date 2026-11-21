#include "api_harness.h"

#include <ursatz_test.h>

#include <common/files.h>
#include <common/json.h>

#include "items/formats.h"
#include "settings/settings_keys.h"
#include "settings/settings_resolve.h"
#include "settings/settings_scopes.h"

/*
 * The settings resolver (R-671, R-673): scope files written by hand in a temporary data folder, the key tables installed when the workshop
 * opened (from the Composer's vocabulary): the order and the sources of the scopes, the nearest winning; the switches of the request, the item and
 * the project; the choice of the taste profile and `all` beneath it; every override action mapped; the answer's values and document.
 */

static char g_project[96];
static char g_item[96];

/* Writes a scope file by hand: its values, and any other members, as JSON texts. */
static void write_scope(const char *path, const char *scope, const char *values, const char *extra_members) {
    char folder[320];
    snprintf(folder, sizeof folder, "%s", path);
    char *slash = strrchr(folder, '/');
    if (slash) {
        *slash = '\0';
        char command[400];
        snprintf(command, sizeof command, "mkdir -p %s", folder);
        CHECKF(system(command) == 0, "mkdir %s", folder);
    }
    cJSON *file = cJSON_CreateObject();
    cJSON_AddStringToObject(file, "scope", scope);
    cJSON_AddItemToObject(file, "values", cJSON_Parse(values));
    if (extra_members) {
        cJSON *extra = cJSON_Parse(extra_members);
        for (cJSON *member = extra ? extra->child : NULL; member;) {
            cJSON *next = member->next;
            cJSON_AddItemToObject(file, member->string, cJSON_DetachItemViaPointer(extra, member));
            member = next;
        }
        cJSON_Delete(extra);
    }
    UrsatzError error = {{0}, {0}};
    CHECKF(workshop_format_write(path, WORKSHOP_FORMAT_SETTINGS, file, &error) == URSATZ_OK, "write %s: %s", path, error.message);
    cJSON_Delete(file);
}

static void write_tastes(void) {
    write_scope("settings/taste/all.json", "taste:all", "{\"melody.attempts\":20}", NULL);
    write_scope("settings/taste/none.json", "taste:none", "{\"melody.density\":2}", NULL);
}

/* One resolution as JSON text: the request members the test names, as the settings.resolve query takes them. */
static cJSON *resolve(const char *params, bool all) {
    cJSON *parsed = cJSON_Parse(params);
    WorkshopSettingsRequest request;
    memset(&request, 0, sizeof request);
    request.project_id = cJSON_IsString(member(parsed, "project_id")) ? str(parsed, "project_id") : NULL;
    request.item_id = cJSON_IsString(member(parsed, "item_id")) ? str(parsed, "item_id") : NULL;
    request.style_id = cJSON_IsString(member(parsed, "style_id")) ? str(parsed, "style_id") : NULL;
    request.taste = cJSON_IsString(member(parsed, "taste")) ? str(parsed, "taste") : NULL;
    request.values = member(parsed, "settings");
    request.overrides = member(parsed, "overrides");
    request.off = member(parsed, "off");
    request.kind = "theme";
    cJSON *answer = NULL;
    UrsatzError error = {{0}, {0}};
    UrsatzStatus status = workshop_settings_resolve_request(&request, all, &answer, &error);
    CHECKF(status == URSATZ_OK, "resolve %s: %d %s", params, (int)status, error.message);
    /* the answer outlives the parsed request: nothing of it points into it */
    cJSON_Delete(parsed);
    return answer;
}

/* The row of key in the answer's values, or NULL. */
static const cJSON *row(const cJSON *answer, const char *key) {
    const cJSON *entry;
    cJSON_ArrayForEach(entry, member(answer, "values")) {
        if (strcmp(str(entry, "key"), key) == 0) {
            return entry;
        }
    }
    return NULL;
}

static bool value_is(const cJSON *answer, const char *key, double number, const char *from) {
    const cJSON *entry = row(answer, key);
    const cJSON *value = entry ? member(entry, "value") : NULL;
    bool ok = entry && cJSON_IsNumber(value) && value->valuedouble == number && strcmp(str(entry, "from"), from) == 0;
    if (!ok) {
        char *text = entry ? cJSON_PrintUnformatted(entry) : NULL;
        fprintf(stderr, "%s: wanted %g from %s, got %s\n", key, number, from, text ? text : "(not listed)");
        cJSON_free(text);
    }
    return ok;
}

static bool absent(const cJSON *answer, const char *key) {
    return row(answer, key) == NULL;
}

static bool list_has(const cJSON *answer, const char *key, const char *word, const char *from) {
    const cJSON *entry = row(answer, key);
    const cJSON *item;
    cJSON_ArrayForEach(item, member(entry, "value")) {
        if (cJSON_IsString(item) && strcmp(item->valuestring, word) == 0) {
            return strcmp(str(entry, "from"), from) == 0;
        }
    }
    return false;
}

static void test_nothing_set(void) {
    char params[200];
    snprintf(params, sizeof params, "{\"item_id\":\"%s\"}", g_item);
    cJSON *answer = resolve(params, false);
    check(strcmp(str(answer, "taste"), "none") == 0, "the profile is none when no scope names one");
    check(cJSON_GetArraySize(member(answer, "values")) == 0 && cJSON_GetArraySize(member(answer, "overrides")) == 0 && cJSON_GetArraySize(member(answer, "off")) == 0,
          "nothing set lists no values, overrides or switches");
    check(cJSON_IsNull(member(answer, "document")), "and its document is null");
    cJSON_Delete(answer);
    answer = resolve(params, true);
    check((size_t)cJSON_GetArraySize(member(answer, "values")) == workshop_settings_key_count() && value_is(answer, "melody.keep", 6, "default"),
          "all lists every key, a default from default");
    check(cJSON_IsNull(member(answer, "document")), "a default is not part of the document");
    cJSON_Delete(answer);
}

static void test_order_and_sources(void) {
    char scope[200];
    char path[320];
    write_tastes();
    CHECKF(system("mkdir -p settings/styles/sty-a") == 0, "style folder");
    write_scope("settings/styles/sty-a/settings.json", "style:sty-a", "{\"melody.climax_rise\":5}", NULL);
    snprintf(scope, sizeof scope, "project:%s", g_project);
    workshop_layout_project_settings_file(g_project, path, sizeof path);
    write_scope(path, scope, "{\"melody.keep\":7}", NULL);
    snprintf(scope, sizeof scope, "item:%s", g_item);
    workshop_layout_item_settings_file(g_project, g_item, path, sizeof path);
    write_scope(path, scope, "{\"melody.leaps\":3}", NULL);
    char params[400];
    snprintf(params, sizeof params, "{\"item_id\":\"%s\",\"style_id\":\"sty-a\",\"settings\":{\"melody\":{\"register\":4},\"explore\":{\"distance\":3}}}", g_item);
    cJSON *answer = resolve(params, false);
    char project_from[160];
    char item_from[160];
    snprintf(project_from, sizeof project_from, "project:%s", g_project);
    snprintf(item_from, sizeof item_from, "item:%s", g_item);
    check(value_is(answer, "melody.attempts", 20, "taste:all"), "taste:all gives its value");
    check(value_is(answer, "melody.density", 2, "taste:none"), "the named profile gives its value");
    check(value_is(answer, "melody.climax_rise", 5, "style:sty-a"), "the style gives its value");
    check(value_is(answer, "melody.keep", 7, project_from), "the project gives its value");
    check(value_is(answer, "melody.leaps", 3, item_from), "the item gives its value");
    check(value_is(answer, "melody.register", 4, "request"), "the request gives its value");
    check(value_is(answer, "explore.distance", 3, "request"), "an explore key is listed with the request");
    const cJSON *document = member(answer, "document");
    check(cJSON_IsObject(document) && cJSON_GetObjectItemCaseSensitive(member(document, "melody"), "keep") != NULL && member(document, "explore") == NULL,
          "the document holds the settings nested by their dots and no explore key");
    check(num(member(document, "melody"), "register") == 4, "the document's register is the request's");
    cJSON_Delete(answer);

    /* the nearest wins: the item over the project over the style over the taste, each over the one before */
    write_scope("settings/taste/all.json", "taste:all", "{\"melody.keep\":4,\"melody.attempts\":20}", NULL);
    snprintf(path, sizeof path, "settings/styles/sty-a/settings.json");
    write_scope(path, "style:sty-a", "{\"melody.climax_rise\":5,\"melody.keep\":5}", NULL);
    snprintf(params, sizeof params, "{\"item_id\":\"%s\",\"style_id\":\"sty-a\"}", g_item);
    answer = resolve(params, false);
    check(value_is(answer, "melody.keep", 7, project_from), "a later scope wins over an earlier one (taste:all 4, style 5, project 7)");
    cJSON_Delete(answer);
    snprintf(params, sizeof params, "{\"style_id\":\"sty-a\"}");
    answer = resolve(params, false);
    check(value_is(answer, "melody.keep", 4, "taste:all"), "without a project the taste's 4 is over the style's 5 (the style is the first scope)");
    cJSON_Delete(answer);
}

static void test_switches_and_profiles(void) {
    char params[400];
    char path[320];
    char scope[200];
    snprintf(params, sizeof params, "{\"item_id\":\"%s\",\"style_id\":\"sty-a\",\"off\":[\"taste\"]}", g_item);
    cJSON *answer = resolve(params, false);
    check(absent(answer, "melody.attempts") && absent(answer, "melody.density") && value_is(answer, "melody.climax_rise", 5, "style:sty-a"),
          "the request's off taste skips both profiles and nothing else");
    check(cJSON_GetArraySize(member(answer, "off")) == 1 && strcmp(cJSON_GetArrayItem(member(answer, "off"), 0)->valuestring, "taste") == 0, "the off list is answered");
    cJSON_Delete(answer);
    snprintf(params, sizeof params, "{\"item_id\":\"%s\",\"style_id\":\"sty-a\",\"off\":[\"style\"]}", g_item);
    answer = resolve(params, false);
    check(absent(answer, "melody.climax_rise"), "the request's off style skips the style");
    cJSON_Delete(answer);
    snprintf(params, sizeof params, "{\"item_id\":\"%s\",\"off\":[\"project\"]}", g_item);
    answer = resolve(params, false);
    check(value_is(answer, "melody.keep", 4, "taste:all"), "the request's off project skips the project");
    cJSON_Delete(answer);

    /* the item switches its project off, and the project's own switch is then not read; a project's taste switch skips the profiles */
    snprintf(scope, sizeof scope, "item:%s", g_item);
    workshop_layout_item_settings_file(g_project, g_item, path, sizeof path);
    write_scope(path, scope, "{\"melody.leaps\":3}", "{\"off\":[\"project\"]}");
    snprintf(params, sizeof params, "{\"item_id\":\"%s\"}", g_item);
    answer = resolve(params, false);
    check(value_is(answer, "melody.keep", 4, "taste:all"), "an item that switches its project off skips the project's values");
    cJSON_Delete(answer);
    write_scope(path, scope, "{\"melody.leaps\":3}", "{\"off\":[\"taste\"]}");
    answer = resolve(params, false);
    check(absent(answer, "melody.attempts") && absent(answer, "melody.density") && value_is(answer, "melody.leaps", 3, scope),
          "an item that switches the taste off skips both profiles");
    cJSON_Delete(answer);

    /* the profile: the request's, else the item's, the project's, the style's, else none */
    write_scope("settings/taste/rq.json", "taste:rq", "{\"melody.density\":4}", NULL);
    write_scope("settings/taste/it.json", "taste:it", "{\"melody.density\":3}", NULL);
    write_scope("settings/taste/pr.json", "taste:pr", "{\"melody.density\":1}", NULL);
    write_scope("settings/taste/st.json", "taste:st", "{\"melody.density\":0}", NULL);
    snprintf(scope, sizeof scope, "item:%s", g_item);
    write_scope(path, scope, "{}", "{\"taste\":\"it\"}");
    char project_scope[200];
    snprintf(project_scope, sizeof project_scope, "project:%s", g_project);
    char project_path[320];
    workshop_layout_project_settings_file(g_project, project_path, sizeof project_path);
    write_scope(project_path, project_scope, "{}", "{\"taste\":\"pr\"}");
    write_scope("settings/styles/sty-a/settings.json", "style:sty-a", "{}", "{\"taste\":\"st\"}");
    snprintf(params, sizeof params, "{\"item_id\":\"%s\",\"style_id\":\"sty-a\",\"taste\":\"rq\"}", g_item);
    answer = resolve(params, false);
    check(strcmp(str(answer, "taste"), "rq") == 0 && value_is(answer, "melody.density", 4, "taste:rq"), "the request's profile wins");
    cJSON_Delete(answer);
    snprintf(params, sizeof params, "{\"item_id\":\"%s\",\"style_id\":\"sty-a\"}", g_item);
    answer = resolve(params, false);
    check(strcmp(str(answer, "taste"), "it") == 0 && value_is(answer, "melody.density", 3, "taste:it"), "then the item's");
    cJSON_Delete(answer);
    write_scope(path, scope, "{}", NULL);
    answer = resolve(params, false);
    check(strcmp(str(answer, "taste"), "pr") == 0 && value_is(answer, "melody.density", 1, "taste:pr"), "then the project's");
    cJSON_Delete(answer);
    write_scope(project_path, project_scope, "{}", NULL);
    answer = resolve(params, false);
    check(strcmp(str(answer, "taste"), "st") == 0 && value_is(answer, "melody.density", 0, "taste:st"), "then the style's");
    cJSON_Delete(answer);
    write_scope("settings/styles/sty-a/settings.json", "style:sty-a", "{}", NULL);
    answer = resolve(params, false);
    check(strcmp(str(answer, "taste"), "none") == 0 && value_is(answer, "melody.density", 2, "taste:none"), "else none");
    cJSON_Delete(answer);
    snprintf(params, sizeof params, "{\"item_id\":\"%s\",\"taste\":\"nobody\"}", g_item);
    answer = resolve(params, false);
    check(strcmp(str(answer, "taste"), "nobody") == 0 && absent(answer, "melody.density") && value_is(answer, "melody.attempts", 20, "taste:all"),
          "a profile with no file is empty, and taste:all still counts");
    cJSON_Delete(answer);
}

static void test_overrides(void) {
    char path[320];
    char scope[200];
    snprintf(scope, sizeof scope, "item:%s", g_item);
    workshop_layout_item_settings_file(g_project, g_item, path, sizeof path);
    write_scope(path, scope, "{}",
                "{\"overrides\":[{\"rule\":\"leaps\",\"action\":\"reweight\",\"weight\":9},{\"rule\":\"seam\",\"action\":\"off\"},"
                "{\"rule\":\"parallel-fifths\",\"action\":\"waive\"},{\"rule\":\"no-crossing\",\"action\":\"off\"},{\"rule\":\"contour\",\"action\":\"reverse\"},"
                "{\"rule\":\"closure\",\"action\":\"reweight\",\"weight\":4},{\"rule\":\"waves\",\"action\":\"waive\"}]}");
    char params[300];
    snprintf(params, sizeof params, "{\"item_id\":\"%s\"}", g_item);
    cJSON *answer = resolve(params, false);
    check(value_is(answer, "melody.leaps", 9, scope), "reweight sets the group's weight");
    check(value_is(answer, "melody.weights.seam", 0, scope), "off on a soft group sets its weight to 0");
    check(value_is(answer, "melody.closure.final", 4, scope) && value_is(answer, "melody.closure.first", 4, scope), "reweight of closure sets both weights");
    check(value_is(answer, "melody.weights.waves", 0, scope), "waive on a soft group sets its weight to 0");
    check(list_has(answer, "melody.waive", "parallel-fifths", scope) && list_has(answer, "melody.waive", "no-crossing", scope),
          "waive and off on a hard rule add it to melody.waive");
    check(list_has(answer, "melody.reverse", "contour", scope), "reverse adds the group to melody.reverse");
    check(cJSON_GetArraySize(member(answer, "overrides")) == 7 && strcmp(str(cJSON_GetArrayItem(member(answer, "overrides"), 0), "rule"), "leaps") == 0 &&
              num(cJSON_GetArrayItem(member(answer, "overrides"), 0), "weight") == 9 && strcmp(str(cJSON_GetArrayItem(member(answer, "overrides"), 0), "from"), scope) == 0,
          "the answer lists the overrides in the order they applied, with their sources");
    const cJSON *document = member(answer, "document");
    check(cJSON_IsObject(document) && cJSON_GetArraySize(member(member(document, "melody"), "waive")) == 2, "the document holds the waived rules");
    cJSON_Delete(answer);

    /* a request's overrides come last */
    snprintf(params, sizeof params, "{\"item_id\":\"%s\",\"overrides\":[{\"rule\":\"leaps\",\"action\":\"reweight\",\"weight\":1}]}", g_item);
    answer = resolve(params, false);
    check(value_is(answer, "melody.leaps", 1, "request"), "a request's override comes after the item's");
    cJSON_Delete(answer);

    /* refusals of the request */
    UrsatzError error = {{0}, {0}};
    cJSON *refused = NULL;
    cJSON *bad = cJSON_Parse("[{\"rule\":\"nothing\",\"action\":\"off\"}]");
    WorkshopSettingsRequest request;
    memset(&request, 0, sizeof request);
    request.item_id = g_item;
    request.overrides = bad;
    check(workshop_settings_resolve_request(&request, false, &refused, &error) == URSATZ_ERR_REQUEST && strcmp(error.path, "/params/overrides/0") == 0 &&
              strstr(error.message, "parallel-fifths") != NULL,
          "an unknown rule is refused naming the rules");
    cJSON_Delete(bad);
    request.overrides = NULL;
    cJSON *unknown = cJSON_Parse("{\"melody\":{\"nonsense\":1}}");
    request.values = unknown;
    check(workshop_settings_resolve_request(&request, false, &refused, &error) == URSATZ_ERR_REQUEST && strcmp(error.message, "unknown setting 'melody.nonsense'") == 0,
          "an unknown key of the request is refused");
    cJSON_Delete(unknown);
    request.values = NULL;
    request.item_id = "mot-nothing";
    check(workshop_settings_resolve_request(&request, false, &refused, &error) == URSATZ_ERR_NOT_FOUND, "an item that is not there is NOT_FOUND");
}

static void test_explore_only_is_no_document(void) {
    char params[300];
    snprintf(params, sizeof params, "{\"off\":[\"taste\"],\"settings\":{\"explore\":{\"share\":\"1/4\"}}}");
    cJSON *answer = resolve(params, false);
    check(row(answer, "explore.share") != NULL && cJSON_IsNull(member(answer, "document")), "a request holding only explore keys sends no document");
    cJSON_Delete(answer);
}

static const char *const kMotifBody =
    "{\"uses\":[],\"motif\":{\"role\":\"melodic\",\"form\":\"full\",\"notes\":\"C4:e Eb4:e G4:e Eb4:e C4:e Eb4:e G4:e C5:e\","
    "\"meter\":\"4/4\",\"key\":\"C minor\",\"pickup\":\"0\",\"features\":[],\"analysis\":null,\"derived\":null}}";

int main(void) {
    if (!harness_open(18790)) {
        return 1;
    }
    WorkshopReply reply;
    UrsatzError error;
    bool ready = command("project.create", NULL, "{\"name\":\"Settings\"}", -1, &reply, &error) == URSATZ_OK;
    if (ready) {
        snprintf(g_project, sizeof g_project, "%s", str(member(reply.json, "project"), "id"));
    }
    workshop_reply_destroy(&reply);
    char params[400];
    snprintf(params, sizeof params, "{\"type\":\"motif\",\"name\":\"Hook\",\"project_id\":\"%s\",\"body\":%s}", g_project, kMotifBody);
    ready = ready && command("item.create", NULL, params, -1, &reply, &error) == URSATZ_OK;
    if (ready) {
        snprintf(g_item, sizeof g_item, "%s", str(member(reply.json, "item"), "id"));
    } else {
        fprintf(stderr, "item.create: %s\n", error.message);
    }
    workshop_reply_destroy(&reply);
    check(ready, "a project and a motif are made");
    if (ready) {
        test_nothing_set();
        test_order_and_sources();
        test_switches_and_profiles();
        test_overrides();
        test_explore_only_is_no_document();
    }
    harness_close();
    return test_finish("workshop_settings_resolve");
}
