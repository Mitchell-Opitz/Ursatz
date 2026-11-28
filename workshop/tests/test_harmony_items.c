/* Harmony plans as items (R-767): create, compose (files, result, playback, export), validate, apply to a theme and to a piece section, and the
 * refusals. Runs in its own working directory. */

#include "api_harness.h"
#include "theme_body.h"

#include <ursatz_test.h>

static const char *kMotifBody =
    "{\"uses\":[],\"motif\":{\"role\":\"melodic\",\"form\":\"full\",\"notes\":\"C4:e Eb4:e G4:e Eb4:e C4:e Eb4:e G4:e C5:e\","
    "\"meter\":\"4/4\",\"key\":\"C minor\",\"pickup\":\"0\",\"features\":[],\"analysis\":null,\"derived\":null}}";

static const char *kHarmonyBody =
    "{\"uses\":[],\"request\":{\"key\":\"C minor\",\"meter\":\"4/4\",\"bars\":8,\"cadence\":\"PAC\",\"seed\":1,"
    "\"palette\":{\"with\":\"dorian\",\"spans\":[{\"bars\":[1,6],\"chords\":\"shared\"}]},"
    "\"accompaniment\":{\"pattern\":\"block\"}}}";

static char g_project[64], g_motif[64], g_harmony[64], g_theme[64], g_uncomposed[64];

static bool create(const char *type, const char *name, const char *body, char *id, size_t size) {
    WorkshopReply reply;
    UrsatzError error;
    char *params = malloc(strlen(body) + 256);
    snprintf(params, strlen(body) + 256, "{\"type\":\"%s\",\"name\":\"%s\",\"project_id\":\"%s\",\"body\":%s}", type, name, g_project, body);
    bool ok = command("item.create", NULL, params, -1, &reply, &error) == URSATZ_OK;
    snprintf(id, size, "%s", ok ? str(member(reply.json, "item"), "id") : "");
    workshop_reply_destroy(&reply);
    free(params);
    return ok && id[0] != '\0';
}

static bool compose(const char *id, long base) {
    WorkshopReply reply;
    UrsatzError error;
    bool ok = command("item.compose", id, "{\"seed\":1}", base, &reply, &error) == URSATZ_OK;
    cJSON *job = ok ? finish_job(&reply) : NULL;
    ok = ok && job && num(job, "http_status") == 200;
    cJSON_Delete(job);
    workshop_reply_destroy(&reply);
    return ok;
}

static bool setup(void) {
    WorkshopReply reply;
    UrsatzError error;
    bool ok = command("project.create", NULL, "{\"name\":\"Work\"}", -1, &reply, &error) == URSATZ_OK;
    snprintf(g_project, sizeof(g_project), "%s", str(member(reply.json, "project"), "id"));
    workshop_reply_destroy(&reply);
    ok = ok && create("motif", "Source", kMotifBody, g_motif, sizeof(g_motif));
    char body[1500];
    theme_body_text(body, sizeof(body), g_motif, g_motif, -1);
    ok = ok && create("theme", "Theme", body, g_theme, sizeof(g_theme));
    ok = ok && create("harmony", "Sad", kHarmonyBody, g_harmony, sizeof(g_harmony)) && create("harmony", "Draft", kHarmonyBody, g_uncomposed, sizeof(g_uncomposed));
    return ok && strncmp(g_harmony, "hrm-", 4) == 0;
}

static void test_compose(void) {
    WorkshopReply reply;
    UrsatzError error;
    char params[256];
    check(compose(g_harmony, 1), "a harmony plan composes into a new version");
    snprintf(params, sizeof(params), "{\"id\":\"%s\",\"version\":2}", g_harmony);
    check(query("item.version", params, &reply, &error) == URSATZ_OK, "version 2 is there");
    const cJSON *result = member(member(reply.json, "body"), "result");
    const cJSON *harmony = member(result, "harmony");
    check(cJSON_GetArraySize(member(harmony, "chords")) == 8 && cJSON_GetArraySize(member(result, "measures")) == 8 && num(result, "seed") == 1,
          "its result holds the plan's chords, a row of measures for each bar and the seed");
    check(strcmp(str(cJSON_GetArrayItem(member(harmony, "stages"), 0), "chord"), "") != 0, "and its stages");
    workshop_reply_destroy(&reply);
    snprintf(params, sizeof(params), "{\"id\":\"%s\",\"version\":2,\"name\":\"harmony.mid\"}", g_harmony);
    check(query("item.file", params, &reply, &error) == URSATZ_OK && reply.bytes && reply.size > 4 && memcmp(reply.bytes, "MThd", 4) == 0, "harmony.mid is a file of the version");
    workshop_reply_destroy(&reply);
    snprintf(params, sizeof(params), "{\"id\":\"%s\",\"version\":2,\"name\":\"harmony.musicxml\"}", g_harmony);
    check(query("item.file", params, &reply, &error) == URSATZ_OK && reply.bytes && reply.size > 100, "harmony.musicxml is another");
    workshop_reply_destroy(&reply);
    snprintf(params, sizeof(params), "{\"id\":\"%s\",\"version\":2,\"name\":\"request.json\"}", g_harmony);
    check(query("item.file", params, &reply, &error) == URSATZ_OK && reply.bytes, "and request.json");
    workshop_reply_destroy(&reply);
    snprintf(params, sizeof(params), "{\"id\":\"%s\",\"version\":2,\"file\":\"harmony\"}", g_harmony);
    check(query("item.playback", params, &reply, &error) == URSATZ_OK && cJSON_IsArray(member(reply.json, "notes")), "item.playback plays the harmony file");
    workshop_reply_destroy(&reply);
    snprintf(params, sizeof(params), "{\"id\":\"%s\",\"version\":2}", g_harmony);
    check(query("item.export", params, &reply, &error) == URSATZ_OK && reply.bytes && strncmp(reply.content_type, "text/plain", 10) == 0, "item.export exports the version");
    workshop_reply_destroy(&reply);
}

static void test_validate(void) {
    WorkshopReply reply;
    UrsatzError error;
    char params[1024];
    snprintf(params, sizeof(params), "{\"type\":\"harmony\",\"body\":%s}", kHarmonyBody);
    check(query("item.validate", params, &reply, &error) == URSATZ_OK && cJSON_IsTrue(member(reply.json, "complete")) && member(reply.json, "request"), "item.validate runs the request through the progression");
    workshop_reply_destroy(&reply);
    check(query("item.validate", "{\"type\":\"harmony\",\"body\":{\"uses\":[],\"request\":{\"key\":\"C major\",\"meter\":\"4/4\",\"bars\":4,\"cadence\":\"DC\",\"palette\":{}}}}", &reply,
                &error) == URSATZ_ERR_REQUEST && strcmp(error.path, "/cadence") == 0,
          "and answers the Composer's refusal with its path");
    check(query("item.validate", "{\"type\":\"harmony\",\"body\":{\"uses\":[],\"request\":{\"key\":\"C major\"}}}", &reply, &error) == URSATZ_OK &&
              !cJSON_IsTrue(member(reply.json, "complete")),
          "a request that is not yet written is not complete");
    workshop_reply_destroy(&reply);
}

/* The request of a theme item's current version: the chords it holds. */
static const cJSON *theme_chords(cJSON *document, const char *id, int version) {
    WorkshopReply reply;
    UrsatzError error;
    char params[128];
    snprintf(params, sizeof(params), "{\"id\":\"%s\",\"version\":%d}", id, version);
    if (query("item.version", params, &reply, &error) != URSATZ_OK) {
        return NULL;
    }
    cJSON *body = cJSON_DetachItemFromObjectCaseSensitive(reply.json, "body");
    workshop_reply_destroy(&reply);
    cJSON_AddItemToObject(document, "body", body);
    return member(member(body, "request"), "chords");
}

static void test_apply_to_a_theme(void) {
    WorkshopReply reply;
    UrsatzError error;
    char params[256];
    snprintf(params, sizeof(params), "{\"harmony\":\"%s\",\"version\":2,\"note\":\"the sad plan\"}", g_harmony);
    check(command("item.apply-harmony", g_theme, params, 1, &reply, &error) == URSATZ_OK, "item.apply-harmony writes a new version of the theme");
    workshop_reply_destroy(&reply);
    cJSON *holder = cJSON_CreateObject();
    const cJSON *chords = theme_chords(holder, g_theme, 2);
    check(cJSON_IsObject(chords) && strcmp(str(member(chords, "$item"), "item_id"), g_harmony) == 0 && num(member(chords, "$item"), "version") == 2,
          "it holds the reference as the theme's chords");
    cJSON_Delete(holder);
    snprintf(params, sizeof(params), "{\"id\":\"%s\",\"version\":2}", g_theme);
    check(query("item.version", params, &reply, &error) == URSATZ_OK, "the version is read");
    bool uses_it = false;
    const cJSON *use;
    cJSON_ArrayForEach(use, member(member(reply.json, "body"), "uses")) {
        uses_it = uses_it || (strcmp(str(use, "item_id"), g_harmony) == 0 && num(use, "version") == 2);
    }
    check(uses_it, "and its uses name the harmony version");
    workshop_reply_destroy(&reply);
    bool composed = compose(g_theme, 2);
    if (!composed) {
        WorkshopReply failed;
        UrsatzError failure;
        command("item.compose", g_theme, "{\"seed\":1}", 2, &failed, &failure);
        cJSON *job = finish_job(&failed);
        char *text = cJSON_Print(job);
        fprintf(stderr, "job: %s\n", text ? text : "?");
        free(text);
        cJSON_Delete(job);
        workshop_reply_destroy(&failed);
    }
    check(composed, "the theme composes with the plan's chords Given");
    snprintf(params, sizeof(params), "{\"id\":\"%s\",\"version\":3,\"name\":\"request.json\"}", g_theme);
    check(query("item.file", params, &reply, &error) == URSATZ_OK && reply.bytes, "its request.json is written");
    cJSON *written = reply.bytes ? cJSON_Parse((const char *)reply.bytes) : NULL;
    const cJSON *resolved = member(written, "chords");
    check(cJSON_GetArraySize(resolved) == 8 && num(cJSON_GetArrayItem(resolved, 0), "bar") == 0 && num(cJSON_GetArrayItem(resolved, 7), "bar") == 7,
          "the resolved request has the plan's chords, each bar less one");
    cJSON_Delete(written);
    workshop_reply_destroy(&reply);
}

static void test_apply_to_a_piece_section(void) {
    WorkshopReply reply;
    UrsatzError error;
    char piece[64];
    char params[256];
    static const char *const kPieceBody =
        "{\"uses\":[],\"request\":{\"key\":\"D\",\"mode\":\"major\",\"meter\":\"4/4\",\"sections\":[{\"id\":\"s0\",\"part\":\"body\",\"beats\":8,\"pickup\":\"0\","
        "\"cadence\":null,\"fill\":null}]}}";
    check(create("piece", "Loop", kPieceBody, piece, sizeof(piece)), "a piece item is created");
    snprintf(params, sizeof(params), "{\"harmony\":\"%s\",\"version\":2,\"section\":\"s9\"}", g_harmony);
    check(command("item.apply-harmony", piece, params, 1, &reply, &error) == URSATZ_ERR_NOT_FOUND && strcmp(error.path, "/params/section") == 0, "an unknown section is refused");
    snprintf(params, sizeof(params), "{\"harmony\":\"%s\",\"version\":2}", g_harmony);
    check(command("item.apply-harmony", piece, params, 1, &reply, &error) == URSATZ_ERR_REQUEST && strcmp(error.path, "/params/section") == 0, "a piece needs its section named");
    snprintf(params, sizeof(params), "{\"harmony\":\"%s\",\"version\":2,\"section\":\"s0\"}", g_harmony);
    check(command("item.apply-harmony", piece, params, 1, &reply, &error) == URSATZ_OK, "the plan is applied to the section");
    workshop_reply_destroy(&reply);
    snprintf(params, sizeof(params), "{\"id\":\"%s\",\"version\":2}", piece);
    check(query("item.version", params, &reply, &error) == URSATZ_OK, "the new version is read");
    const cJSON *section = cJSON_GetArrayItem(member(member(member(reply.json, "body"), "request"), "sections"), 0);
    check(strcmp(str(member(member(section, "harmony"), "$item"), "item_id"), g_harmony) == 0 && num(member(member(section, "harmony"), "$item"), "version") == 2,
          "the section's harmony is the reference");
    workshop_reply_destroy(&reply);
}

static void test_refusals(void) {
    WorkshopReply reply;
    UrsatzError error;
    char params[256];
    snprintf(params, sizeof(params), "{\"harmony\":\"%s\",\"version\":1}", g_uncomposed);
    check(command("item.apply-harmony", g_theme, params, 3, &reply, &error) == URSATZ_ERR_REQUEST && strstr(error.message, "not composed") != NULL,
          "applying a version that is not composed is refused");
    snprintf(params, sizeof(params), "{\"harmony\":\"%s\",\"version\":2}", g_harmony);
    check(command("item.apply-harmony", g_motif, params, 1, &reply, &error) == URSATZ_ERR_REQUEST && strcmp(error.path, "/target") == 0, "a motif cannot take a harmony plan");
    snprintf(params, sizeof(params), "{\"harmony\":\"%s\",\"version\":2,\"section\":\"A\"}", g_harmony);
    check(command("item.apply-harmony", g_theme, params, 3, &reply, &error) == URSATZ_ERR_REQUEST && strcmp(error.path, "/params/section") == 0, "a theme has no section to name");
    snprintf(params, sizeof(params), "{\"harmony\":\"%s\",\"version\":9}", g_harmony);
    check(command("item.apply-harmony", g_theme, params, 3, &reply, &error) != URSATZ_OK, "a version that is not there is refused");
    snprintf(params, sizeof(params), "{\"harmony\":\"%s\",\"version\":2}", g_harmony);
    check(command("item.apply-harmony", g_theme, params, 1, &reply, &error) != URSATZ_OK, "a base older than the item's version is refused");
}

int main(void) {
    if (!harness_open(18997)) {
        return 1;
    }
    bool ready = setup();
    check(ready, "the folder holds a project, a motif, a theme and two harmony plans");
    if (ready) {
        test_compose();
        test_validate();
        test_apply_to_a_theme();
        test_apply_to_a_piece_section();
        test_refusals();
    }
    harness_close();
    return test_finish("workshop_harmony_items");
}
