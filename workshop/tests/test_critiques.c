#include "api_harness.h"
#include "theme_body.h"

#include <ursatz_test.h>

#include <common/files.h>
#include <common/json.h>

#include "items/layout.h"
#include "items/store.h"

/*
 * The critic's judgments in the data folder (R-723): a composed theme critiqued at its current version (ratings.critique: the record with its ids, in
 * ratings/critiques/<item>.json, journaled against the item), a verdict on one observation, the refusals (a verdict on an id the file does not hold, one that
 * is neither agree nor disagree, a stale base, a version that is not there, a piece) and the settings param; then a theme composed with critic.rank on keeps
 * the judgment and the rejected candidates in its body's result.critic. workshop_formats validates the file this test leaves.
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

/* item.compose on the theme at its current version; the version it made, 0 when it failed. */
static int compose(const char *params_text) {
    int base = current_version(g_theme);
    WorkshopReply reply;
    UrsatzError error;
    UrsatzStatus status = command("item.compose", g_theme, params_text, base, &reply, &error);
    cJSON *job = status == URSATZ_OK ? finish_job(&reply) : NULL;
    bool ok = status == URSATZ_OK && job && strcmp(str(job, "state"), "done") == 0;
    if (!ok) {
        fprintf(stderr, "item.compose %s: %d %s %s\n", params_text, (int)status, error.message, job ? str(job, "state") : "");
    }
    cJSON_Delete(job);
    workshop_reply_destroy(&reply);
    return ok ? current_version(g_theme) : 0;
}

static void expect_refusal(const char *name, const char *target, const char *params, long base, UrsatzStatus status, const char *path, const char *message_part) {
    WorkshopReply reply;
    UrsatzError error;
    UrsatzStatus got = command(name, target, params, base, &reply, &error);
    CHECKF(got == status && strcmp(error.path, path) == 0 && strstr(error.message, message_part) != NULL, "%s %s: wanted %d at %s containing '%s', got %d at %s '%s'", name, params,
           (int)status, path, message_part, (int)got, error.path, error.message);
    workshop_reply_destroy(&reply);
}

/* The critiques file of the theme as the query gives it (the caller frees it). */
static cJSON *critiques(void) {
    WorkshopReply reply;
    UrsatzError error;
    char params[160];
    snprintf(params, sizeof params, "{\"item\":\"%s\"}", g_theme);
    cJSON *file = NULL;
    if (query("ratings.critiques", params, &reply, &error) == URSATZ_OK) {
        file = cJSON_Duplicate(reply.json, true);
    } else {
        fprintf(stderr, "ratings.critiques: %s\n", error.message);
    }
    workshop_reply_destroy(&reply);
    return file;
}

static cJSON *version_body(int version) {
    char path[400];
    char *text = NULL;
    size_t size = 0;
    if (!workshop_layout_version_file(g_project, g_theme, version, WORKSHOP_FILE_BODY, path, sizeof path) || common_file_read(path, &text, &size, NULL) != URSATZ_OK) {
        return NULL;
    }
    cJSON *body = cJSON_Parse(text);
    free(text);
    return body;
}

static char g_observation[96];

static void test_critique_and_verdict(int version) {
    WorkshopReply reply;
    UrsatzError error;
    char params[200];
    cJSON *none = critiques();
    check(none && cJSON_GetArraySize(member(none, "critiques")) == 0 && cJSON_GetArraySize(member(none, "verdicts")) == 0, "an item with no critique has empty lists");
    cJSON_Delete(none);
    snprintf(params, sizeof params, "{\"version\":%d,\"note\":\"first listen\"}", version);
    check(command("ratings.critique", g_theme, params, version, &reply, &error) == URSATZ_OK, "ratings.critique judges the current version");
    check(strncmp(str(reply.json, "id"), "crt-", 4) == 0 && num(reply.json, "version") == version && cJSON_IsObject(member(reply.json, "score")), "it answers an id, the version and a score");
    const cJSON *observations = member(reply.json, "observations");
    check(cJSON_IsArray(observations) && cJSON_GetArraySize(observations) >= 1, "and observations");
    snprintf(g_observation, sizeof g_observation, "%s", str(cJSON_GetArrayItem(observations, 0), "id"));
    check(strncmp(g_observation, "obs-", 4) == 0 && str(cJSON_GetArrayItem(observations, 0), "message")[0] != '\0', "each observation has an id and its words");
    int observation_count = cJSON_GetArraySize(observations);
    char critique_id[96];
    snprintf(critique_id, sizeof critique_id, "%s", str(reply.json, "id"));
    workshop_reply_destroy(&reply);
    cJSON *record = last_record();
    check(record && strcmp(str(record, "command"), "ratings.critique") == 0 && strcmp(str(record, "target"), g_theme) == 0 && strcmp(str(record, "actor"), "test") == 0,
          "the critique is journaled against the item with its actor");
    cJSON_Delete(record);
    cJSON *file = critiques();
    const cJSON *kept = file ? cJSON_GetArrayItem(member(file, "critiques"), 0) : NULL;
    check(kept && strcmp(str(kept, "id"), critique_id) == 0 && strcmp(str(kept, "note"), "first listen") == 0 && strcmp(str(kept, "actor"), "test") == 0 &&
              cJSON_GetArraySize(member(kept, "evaluators")) == 8 && cJSON_GetArraySize(member(kept, "observations")) == observation_count,
          "the file holds the critique with its evaluators and observations");
    cJSON_Delete(file);

    snprintf(params, sizeof params, "{\"observation\":\"%s\",\"verdict\":\"disagree\",\"note\":\"I hear it fine\"}", g_observation);
    check(command("ratings.verdict", g_theme, params, -1, &reply, &error) == URSATZ_OK && strncmp(str(reply.json, "id"), "vrd-", 4) == 0, "ratings.verdict records a verdict on an observation");
    workshop_reply_destroy(&reply);
    record = last_record();
    check(record && strcmp(str(record, "command"), "ratings.verdict") == 0 && strcmp(str(record, "target"), g_theme) == 0, "the verdict is journaled against the item");
    cJSON_Delete(record);
    file = critiques();
    const cJSON *verdict = file ? cJSON_GetArrayItem(member(file, "verdicts"), 0) : NULL;
    check(verdict && strcmp(str(verdict, "observation"), g_observation) == 0 && strcmp(str(verdict, "verdict"), "disagree") == 0 && strcmp(str(verdict, "note"), "I hear it fine") == 0,
          "the file holds the verdict");
    cJSON_Delete(file);
}

static void test_refusals(int version) {
    char params[300];
    expect_refusal("ratings.verdict", g_theme, "{\"observation\":\"obs-0000000000000000\",\"verdict\":\"agree\"}", -1, URSATZ_ERR_NOT_FOUND, "/params/observation", "no observation");
    snprintf(params, sizeof params, "{\"observation\":\"%s\",\"verdict\":\"maybe\"}", g_observation);
    expect_refusal("ratings.verdict", g_theme, params, -1, URSATZ_ERR_REQUEST, "/params/verdict", "");
    snprintf(params, sizeof params, "{\"version\":%d}", version);
    expect_refusal("ratings.critique", g_theme, params, version - 1, URSATZ_ERR_CONFLICT, "/base_version", WORKSHOP_CHANGED_SINCE_READ);
    expect_refusal("ratings.critique", g_theme, "{\"version\":99}", version, URSATZ_ERR_NOT_FOUND, "/params/version", "no version 99");
    expect_refusal("ratings.critique", "itm-0000000000000000", params, version, URSATZ_ERR_NOT_FOUND, "/target", "no item");
    expect_refusal("ratings.verdict", "itm-0000000000000000", "{\"observation\":\"obs-1\",\"verdict\":\"agree\"}", -1, URSATZ_ERR_NOT_FOUND, "/target", "no item");
    expect_refusal("ratings.critique", g_theme, "{\"version\":2,\"settings\":[]}", version, URSATZ_ERR_REQUEST, "/params/settings", "");
    cJSON *file = critiques();
    check(file && cJSON_GetArraySize(member(file, "critiques")) == 1 && cJSON_GetArraySize(member(file, "verdicts")) == 1, "a refused command leaves the file as it was");
    cJSON_Delete(file);
}

/* The settings param reaches the critic: leap_load weighs 0 and wide_range counts the other way in the recorded evaluators. */
static void test_settings_param(int version) {
    WorkshopReply reply;
    UrsatzError error;
    char params[400];
    snprintf(params, sizeof params, "{\"version\":%d,\"settings\":{\"critic\":{\"weights\":{\"leap_load\":0},\"reverse\":[\"wide_range\"]}}}", version);
    check(command("ratings.critique", g_theme, params, version, &reply, &error) == URSATZ_OK, "a critique with the settings param");
    workshop_reply_destroy(&reply);
    cJSON *file = critiques();
    const cJSON *kept = file ? cJSON_GetArrayItem(member(file, "critiques"), 1) : NULL;
    bool found_weight = false;
    bool found_reverse = false;
    const cJSON *evaluator;
    const cJSON *evaluators = kept ? member(kept, "evaluators") : NULL;
    cJSON_ArrayForEach(evaluator, evaluators) {
        found_weight = found_weight || (strcmp(str(evaluator, "rule"), "leap_load") == 0 && num(evaluator, "weight") == 0);
        found_reverse = found_reverse || (strcmp(str(evaluator, "rule"), "wide_range") == 0 && cJSON_IsTrue(member(evaluator, "reversed")));
    }
    check(found_weight && found_reverse, "the recorded evaluators carry the weight 0 and the reversal");
    cJSON_Delete(file);
}

/* A theme composed with critic.rank on keeps the judgment and the rejected candidates in result.critic. */
static void test_rank_on(void) {
    WorkshopReply reply;
    UrsatzError error;
    char params[300];
    snprintf(params, sizeof params, "{\"scope\":\"item:%s\",\"key\":\"critic.rank\",\"value\":true}", g_theme);
    check(command("settings.set", NULL, params, -1, &reply, &error) == URSATZ_OK, "critic.rank is set for the theme");
    workshop_reply_destroy(&reply);
    snprintf(params, sizeof params, "{\"scope\":\"item:%s\",\"key\":\"critic.candidates\",\"value\":3}", g_theme);
    check(command("settings.set", NULL, params, -1, &reply, &error) == URSATZ_OK, "and critic.candidates");
    workshop_reply_destroy(&reply);
    int version = compose("{\"seed\":1}");
    check(version > 1, "the theme composes with the critic choosing");
    cJSON *body = version_body(version);
    const cJSON *critic = member(member(body, "result"), "critic");
    check(cJSON_IsObject(member(critic, "critic")) && cJSON_IsObject(member(member(critic, "critic"), "score")) && cJSON_IsArray(member(critic, "rejected")) &&
              cJSON_GetArraySize(member(critic, "rejected")) == 2,
          "the version's body holds result.critic with the judgment and the two rejected candidates");
    cJSON_Delete(body);
    /* without the critic a version has none */
    snprintf(params, sizeof params, "{\"scope\":\"item:%s\",\"key\":\"critic.rank\",\"value\":false}", g_theme);
    check(command("settings.set", NULL, params, -1, &reply, &error) == URSATZ_OK, "critic.rank is off again");
    workshop_reply_destroy(&reply);
    version = compose("{\"seed\":1}");
    body = version_body(version);
    check(version > 2 && body && member(member(body, "result"), "critic") == NULL, "a version composed without ranking has no result.critic");
    cJSON_Delete(body);
}

int main(void) {
    if (!harness_open(18995)) {
        return 1;
    }
    WorkshopReply reply;
    UrsatzError error;
    bool ready = command("project.create", NULL, "{\"name\":\"Critic\"}", -1, &reply, &error) == URSATZ_OK;
    if (ready) {
        snprintf(g_project, sizeof(g_project), "%s", str(member(reply.json, "project"), "id"));
    }
    workshop_reply_destroy(&reply);
    char body[1200];
    ready = ready && make_item("motif", "Motif", kMotifBody, g_motif, sizeof(g_motif));
    theme_body_text(body, sizeof(body), g_motif, g_motif, -1);
    ready = ready && make_item("theme", "Theme", body, g_theme, sizeof(g_theme));
    check(ready, "a project, a motif and a theme are made");
    int version = ready ? compose("{\"seed\":1}") : 0;
    check(version >= 2, "the theme composes");
    if (ready && version >= 2) {
        test_critique_and_verdict(version);
        test_refusals(version);
        test_settings_param(version);
        test_rank_on();
    }
    harness_close();
    return test_finish("workshop_critiques");
}
