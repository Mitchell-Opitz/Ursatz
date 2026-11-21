#include "api_harness.h"
#include "theme_body.h"

#include <ursatz_test.h>

#include <common/files.h>
#include <common/json.h>

/*
 * Composing with settings (R-671, R-673): a theme composed with nothing set has no preferences and no settings.json; a taste profile switched off for the
 * item composes the same bytes; the random motifs query resolves settings and answers the empty folder's reply byte for byte when the taste is off;
 * an item override waive reaches request.json and settings.json; item.explore moves settings (the same seed, the same moves); a piece takes no settings.
 */

static char g_project[96];
static char g_motif[96];
static char g_theme[96];

static const char *const kMotifBody =
    "{\"uses\":[],\"motif\":{\"role\":\"melodic\",\"form\":\"full\",\"notes\":\"C4:e Eb4:e G4:e Eb4:e C4:e Eb4:e G4:e C5:e\","
    "\"meter\":\"4/4\",\"key\":\"C minor\",\"pickup\":\"0\",\"features\":[],\"analysis\":null,\"derived\":null}}";

static bool make_item(const char *type, const char *name, const char *body_text, char *out, size_t size) {
    char params[3000];
    snprintf(params, sizeof(params), "{\"type\":\"%s\",\"name\":\"%s\",\"project_id\":\"%s\",\"body\":%s}", type, name, g_project, body_text);
    WorkshopReply reply;
    UrsatzError error;
    bool ok = command("item.create", NULL, params, -1, &reply, &error) == URSATZ_OK;
    if (ok) {
        snprintf(out, size, "%s", str(member(reply.json, "item"), "id"));
    } else {
        fprintf(stderr, "item.create %s: %s\n", name, error.message);
    }
    workshop_reply_destroy(&reply);
    return ok;
}

static int current_version(const char *id) {
    WorkshopReply reply;
    UrsatzError error;
    char params[96];
    snprintf(params, sizeof(params), "{\"id\":\"%s\"}", id);
    int version = query("item.get", params, &reply, &error) == URSATZ_OK ? (int)num(member(reply.json, "item"), "current_version") : -1;
    workshop_reply_destroy(&reply);
    return version;
}

/* Runs item.compose or item.explore on the theme at its current version with params text; the version it made, 0 when it failed. */
static int compose(const char *name, const char *params_text) {
    int base = current_version(g_theme);
    WorkshopReply reply;
    UrsatzError error;
    UrsatzStatus status = command(name, g_theme, params_text, base, &reply, &error);
    cJSON *job = status == URSATZ_OK ? finish_job(&reply) : NULL;
    bool ok = status == URSATZ_OK && job && strcmp(str(job, "state"), "done") == 0;
    if (!ok) {
        fprintf(stderr, "%s %s: %d %s %s\n", name, params_text, (int)status, error.message, job ? str(job, "state") : "");
    }
    cJSON_Delete(job);
    workshop_reply_destroy(&reply);
    return ok ? current_version(g_theme) : 0;
}

static char *version_file(int version, const char *name) {
    char path[400];
    char *data = NULL;
    if (!workshop_layout_version_file(g_project, g_theme, version, name, path, sizeof path) || common_file_read(path, &data, NULL, NULL) != URSATZ_OK) {
        return NULL;
    }
    return data;
}

static bool version_has(int version, const char *name) {
    char path[400];
    return workshop_layout_version_file(g_project, g_theme, version, name, path, sizeof path) && common_path_exists(path);
}

static cJSON *version_json(int version, const char *name) {
    char *text = version_file(version, name);
    cJSON *json = text ? cJSON_Parse(text) : NULL;
    free(text);
    return json;
}

static bool same_file(int a, int b, const char *name) {
    char path_a[400];
    char path_b[400];
    char *data_a = NULL;
    char *data_b = NULL;
    size_t size_a = 0;
    size_t size_b = 0;
    bool same = workshop_layout_version_file(g_project, g_theme, a, name, path_a, sizeof path_a) && workshop_layout_version_file(g_project, g_theme, b, name, path_b, sizeof path_b) &&
                common_file_read(path_a, &data_a, &size_a, NULL) == URSATZ_OK && common_file_read(path_b, &data_b, &size_b, NULL) == URSATZ_OK && size_a == size_b &&
                memcmp(data_a, data_b, size_a) == 0;
    free(data_a);
    free(data_b);
    return same;
}

static char *random_motifs(const char *extra) {
    char params[400];
    snprintf(params, sizeof params, "{\"material\":\"omt.material.major\",\"meter\":\"4/4\",\"count\":1,\"seed\":1%s}", extra);
    WorkshopReply reply;
    UrsatzError error;
    char *text = NULL;
    if (query("composer.random-motifs", params, &reply, &error) == URSATZ_OK) {
        text = cJSON_PrintUnformatted(reply.json);
    } else {
        fprintf(stderr, "composer.random-motifs: %s\n", error.message);
    }
    workshop_reply_destroy(&reply);
    return text;
}

static void test_nothing_set(void) {
    int v2 = compose("item.compose", "{\"seed\":1}");
    check(v2 == 2, "a theme composes at seed 1");
    cJSON *request = version_json(v2, "request.json");
    check(request && member(request, "preferences") == NULL && !version_has(v2, "settings.json"), "with nothing set request.json has no preferences and there is no settings.json");
    cJSON_Delete(request);
}

static void test_taste_off_composes_the_same(void) {
    WorkshopReply reply;
    UrsatzError error;
    char *empty_motifs = random_motifs("");
    check(empty_motifs != NULL, "the random motifs query on an empty folder");
    char *taste_text = test_must_read_file(SETTINGS_TASTE_NONE, NULL);
    cJSON *document = cJSON_Parse(taste_text);
    free(taste_text);
    cJSON *params = cJSON_CreateObject();
    cJSON_AddStringToObject(params, "scope", "taste:none");
    cJSON_AddItemToObject(params, "document", document);
    cJSON_AddStringToObject(params, "note", "taste-profile.md none");
    char *params_text = cJSON_PrintUnformatted(params);
    check(command("settings.import", NULL, params_text, -1, &reply, &error) == URSATZ_OK, "the none profile is imported");
    workshop_reply_destroy(&reply);
    free(params_text);
    cJSON_Delete(params);
    char switch_params[300];
    snprintf(switch_params, sizeof switch_params, "{\"scope\":\"item:%s\",\"off\":[\"taste\"]}", g_theme);
    check(command("settings.switch", NULL, switch_params, -1, &reply, &error) == URSATZ_OK, "the theme switches the taste off");
    workshop_reply_destroy(&reply);
    int v3 = compose("item.compose", "{\"seed\":1}");
    check(v3 == 3 && same_file(2, v3, "request.json") && same_file(2, v3, "theme.mid"),
          "a compose with the taste switched off is the same request and the same theme.mid, byte for byte");

    /* the random motifs query */
    char *with_taste = random_motifs("");
    check(with_taste && empty_motifs && strcmp(with_taste, empty_motifs) != 0 && strstr(with_taste, "\"settings\"") != NULL,
          "with the none profile the random motifs query answers other motives and holds settings");
    char *off_taste = random_motifs(",\"off\":[\"taste\"]");
    check(off_taste && empty_motifs && strcmp(off_taste, empty_motifs) == 0, "with the taste off it answers the empty folder's reply byte for byte");
    free(empty_motifs);
    free(with_taste);
    free(off_taste);
}

static void test_override_and_explore(void) {
    WorkshopReply reply;
    UrsatzError error;
    char switch_params[300];
    snprintf(switch_params, sizeof switch_params, "{\"scope\":\"item:%s\",\"off\":[]}", g_theme);
    check(command("settings.switch", NULL, switch_params, -1, &reply, &error) == URSATZ_OK, "the switch is cleared");
    workshop_reply_destroy(&reply);
    char override_params[300];
    snprintf(override_params, sizeof override_params, "{\"scope\":\"item:%s\",\"rule\":\"parallel-fifths\",\"action\":\"waive\"}", g_theme);
    check(command("settings.override", NULL, override_params, -1, &reply, &error) == URSATZ_OK, "an item override waives parallel-fifths");
    workshop_reply_destroy(&reply);
    int v4 = compose("item.compose", "{\"seed\":1}");
    cJSON *request = version_json(v4, "request.json");
    const cJSON *waive = request ? member(member(member(request, "preferences"), "melody"), "waive") : NULL;
    check(v4 == 4 && cJSON_GetArraySize(waive) == 1 && strcmp(cJSON_GetArrayItem(waive, 0)->valuestring, "parallel-fifths") == 0,
          "the waived rule reaches request.json as preferences.melody.waive");
    cJSON *snapshot = version_json(v4, "settings.json");
    const cJSON *recorded = snapshot ? cJSON_GetArrayItem(member(snapshot, "overrides"), 0) : NULL;
    char from[160];
    snprintf(from, sizeof from, "item:%s", g_theme);
    check(snapshot && strcmp(str(snapshot, "format"), "ursatz-settings-snapshot") == 0 && strcmp(str(recorded, "rule"), "parallel-fifths") == 0 && strcmp(str(recorded, "from"), from) == 0,
          "settings.json lists the override from the item");
    cJSON_Delete(request);
    cJSON_Delete(snapshot);

    /* explore: two moves, recorded with their values; the same seed twice gives the same moves */
    snprintf(override_params, sizeof override_params, "{\"scope\":\"item:%s\",\"rule\":\"parallel-fifths\",\"action\":\"clear\"}", g_theme);
    check(command("settings.override", NULL, override_params, -1, &reply, &error) == URSATZ_OK, "the override is cleared");
    workshop_reply_destroy(&reply);
    int first = compose("item.explore", "{\"distance\":2,\"seed\":7}");
    int second = compose("item.explore", "{\"distance\":2,\"seed\":7}");
    cJSON *one = first ? version_json(first, "settings.json") : NULL;
    cJSON *two = second ? version_json(second, "settings.json") : NULL;
    const cJSON *moves = one ? member(member(one, "explore"), "moves") : NULL;
    check(first && second && cJSON_GetArraySize(moves) == 2 && num(member(one, "explore"), "seed") == 7 && num(member(one, "explore"), "distance") == 2,
          "item.explore with distance 2 and seed 7 records two moves in settings.json");
    check(two && cJSON_Compare(member(one, "explore"), member(two, "explore"), true), "the same seed twice gives the same moves");
    cJSON *explored = first ? version_json(first, "request.json") : NULL;
    bool values_there = explored != NULL;
    const cJSON *move;
    cJSON_ArrayForEach(move, moves) {
        const cJSON *at = member(explored, "preferences");
        char key[120];
        snprintf(key, sizeof key, "%s", str(move, "key"));
        for (char *part = strtok(key, "."); part && at; part = strtok(NULL, ".")) {
            at = member(at, part);
        }
        values_there = values_there && at && cJSON_Compare(at, member(move, "to"), true);
    }
    check(values_there, "the moved values are in request.json");
    cJSON_Delete(one);
    cJSON_Delete(two);
    cJSON_Delete(explored);
}

static void test_piece_takes_no_settings(void) {
    char piece[96];
    const char *body = "{\"uses\":[],\"request\":{\"schema\":\"ursatz-piece-request\",\"schema_version\":2,\"form\":\"binary\",\"key\":\"C\","
                       "\"material\":\"omt.material.major\",\"meter\":\"4/4\",\"seed\":1}}";
    if (!make_item("piece", "Piece", body, piece, sizeof piece)) {
        check(false, "a piece is made");
        return;
    }
    WorkshopReply reply;
    UrsatzError error;
    int base = current_version(piece);
    UrsatzStatus status = command("item.compose", piece, "{\"settings\":{\"melody\":{\"keep\":2}}}", base, &reply, &error);
    check(status == URSATZ_ERR_REQUEST && strcmp(error.path, "/params/settings") == 0 && strstr(error.message, "a piece takes no settings until the plan node scope (P71)") != NULL,
          "a piece refuses settings at /params/settings");
    workshop_reply_destroy(&reply);
    status = command("item.compose", piece, "{\"taste\":\"none\"}", base, &reply, &error);
    check(status == URSATZ_ERR_REQUEST && strcmp(error.path, "/params/taste") == 0, "and a taste at /params/taste");
    workshop_reply_destroy(&reply);
    status = command("item.explore", piece, "{}", base, &reply, &error);
    check(status == URSATZ_ERR_REQUEST && strstr(error.message, "explore composes themes") != NULL, "item.explore of a piece is refused");
    workshop_reply_destroy(&reply);
}

int main(void) {
    if (!harness_open(18793)) {
        return 1;
    }
    WorkshopReply reply;
    UrsatzError error;
    bool ready = command("project.create", NULL, "{\"name\":\"Compose\"}", -1, &reply, &error) == URSATZ_OK;
    if (ready) {
        snprintf(g_project, sizeof(g_project), "%s", str(member(reply.json, "project"), "id"));
    }
    workshop_reply_destroy(&reply);
    char body[1200];
    ready = ready && make_item("motif", "Motif", kMotifBody, g_motif, sizeof(g_motif));
    theme_body_text(body, sizeof(body), g_motif, g_motif, -1);
    ready = ready && make_item("theme", "Theme", body, g_theme, sizeof(g_theme));
    check(ready, "a project, a motif and a theme are made");
    if (ready) {
        test_nothing_set();
        test_taste_off_composes_the_same();
        test_override_and_explore();
        test_piece_takes_no_settings();
    }
    harness_close();
    return test_finish("workshop_compose_settings");
}
