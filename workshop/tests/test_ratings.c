#include <stdarg.h>

#include "api_harness.h"

#include <ursatz_test.h>

#include <common/files.h>
#include <common/json.h>

/*
 * Listening sessions and ratings (R-674): a session with two batches of two samples, a rating of both batches at one seed and a pick, then keep as
 * default citing them: the taste file holds the value and a kept entry with the ids. The refusals (a sample that was not recorded, one that was stuck, a
 * rating of 6, a batch recorded twice, keep without ratings, an id that was never recorded, a pick naming no batch), the ratings of an item's version and
 * the journal's actor. workshop_formats validates the files this test leaves.
 */

static char g_project[96];
static char g_item[96];
static char g_session[96];

static const char *const kMotifBody =
    "{\"uses\":[],\"motif\":{\"role\":\"melodic\",\"form\":\"full\",\"notes\":\"C4:e Eb4:e G4:e Eb4:e C4:e Eb4:e G4:e C5:e\","
    "\"meter\":\"4/4\",\"key\":\"C minor\",\"pickup\":\"0\",\"features\":[],\"analysis\":null,\"derived\":null}}";

static UrsatzStatus run(const char *name, const char *target, const char *params, WorkshopReply *reply, UrsatzError *error) {
    /* a rating of a version of an item is made against the version read: the item's current one (1) */
    return command(name, target, params, strcmp(name, "ratings.rate-version") == 0 ? 1 : -1, reply, error);
}

static void expect_refusal(const char *name, const char *target, const char *params, UrsatzStatus status, const char *path, const char *message_part) {
    WorkshopReply reply;
    UrsatzError error;
    UrsatzStatus got = run(name, target, params, &reply, &error);
    CHECKF(got == status && strcmp(error.path, path) == 0 && strstr(error.message, message_part) != NULL, "%s %s: wanted %d at %s containing '%s', got %d at %s '%s'", name, params, (int)status, path,
           message_part, (int)got, error.path, error.message);
    workshop_reply_destroy(&reply);
}

static bool journaled(const char *command_name) {
    cJSON *record = last_record();
    bool ok = record && strcmp(str(record, "command"), command_name) == 0 && strcmp(str(record, "actor"), "test") == 0;
    cJSON_Delete(record);
    return ok;
}

static void with_session(char *out, size_t size, const char *format, ...) __attribute__((format(printf, 3, 4)));

static void with_session(char *out, size_t size, const char *format, ...) {
    char text[1500];
    va_list arguments;
    va_start(arguments, format);
    vsnprintf(text, sizeof text, format, arguments);
    va_end(arguments);
    const char *marker = strstr(text, "SESSION");
    if (!marker) {
        snprintf(out, size, "%s", text);
        return;
    }
    snprintf(out, size, "%.*s%s%s", (int)(marker - text), text, g_session, marker + 7);
}

static const char *kBatchSamples =
    "[{\"index\":1,\"seed\":1,\"status\":\"ok\",\"files\":[{\"name\":\"01.mid\",\"sha256\":\"aa\"}],\"settings\":null},"
    "{\"index\":2,\"seed\":2,\"status\":\"stuck: nothing\",\"files\":[],\"settings\":null}]";

static void test_session_flow(void) {
    WorkshopReply reply;
    UrsatzError error;
    char params[1800];
    check(run("ratings.session", NULL, "{\"what\":\"motifs\",\"taste\":\"none\",\"note\":\"round one\"}", &reply, &error) == URSATZ_OK &&
              strncmp(str(member(reply.json, "session"), "id"), "ses-", 4) == 0,
          "ratings.session opens a session with an id");
    snprintf(g_session, sizeof g_session, "%s", str(member(reply.json, "session"), "id"));
    check(strcmp(str(member(reply.json, "session"), "taste"), "none") == 0 && strcmp(str(member(reply.json, "session"), "what"), "motifs") == 0, "with its taste and what");
    workshop_reply_destroy(&reply);
    check(journaled("ratings.session"), "ratings.session is journaled with its actor");

    with_session(params, sizeof params, "{\"session\":\"SESSION\",\"round\":1,\"batch\":\"A\",\"samples\":%s}", kBatchSamples);
    check(run("ratings.batch", NULL, params, &reply, &error) == URSATZ_OK && cJSON_GetArraySize(member(member(reply.json, "session"), "batches")) == 1, "batch A of two samples is recorded");
    workshop_reply_destroy(&reply);
    with_session(params, sizeof params, "{\"session\":\"SESSION\",\"round\":1,\"batch\":\"B\",\"samples\":%s}", kBatchSamples);
    check(run("ratings.batch", NULL, params, &reply, &error) == URSATZ_OK && cJSON_GetArraySize(member(member(reply.json, "session"), "batches")) == 2, "and batch B");
    workshop_reply_destroy(&reply);
    expect_refusal("ratings.batch", NULL, params, URSATZ_ERR_REQUEST, "/params/batch", "batch B of round 1 is recorded");

    char rating_a[96];
    char rating_b[96];
    char pick[96];
    with_session(params, sizeof params, "{\"session\":\"SESSION\",\"round\":1,\"batch\":\"A\",\"seed\":1,\"rating\":3}");
    check(run("ratings.rate", NULL, params, &reply, &error) == URSATZ_OK && strncmp(str(reply.json, "id"), "rat-", 4) == 0, "ratings.rate rates a sample of batch A");
    snprintf(rating_a, sizeof rating_a, "%s", str(reply.json, "id"));
    workshop_reply_destroy(&reply);
    with_session(params, sizeof params, "{\"session\":\"SESSION\",\"round\":1,\"batch\":\"B\",\"seed\":1,\"rating\":4,\"note\":\"better\"}");
    check(run("ratings.rate", NULL, params, &reply, &error) == URSATZ_OK, "and the same seed of batch B");
    snprintf(rating_b, sizeof rating_b, "%s", str(reply.json, "id"));
    workshop_reply_destroy(&reply);
    with_session(params, sizeof params, "{\"session\":\"SESSION\",\"round\":1,\"batch\":\"A\",\"seed\":1,\"rating\":2}");
    check(run("ratings.rate", NULL, params, &reply, &error) == URSATZ_OK && strcmp(str(reply.json, "id"), rating_a) != 0, "a new rating of a sample replaces the earlier one");
    workshop_reply_destroy(&reply);
    with_session(params, sizeof params, "{\"session\":\"SESSION\",\"round\":1,\"seed\":1,\"better\":\"B\",\"note\":\"B won\"}");
    check(run("ratings.pick", NULL, params, &reply, &error) == URSATZ_OK && strncmp(str(reply.json, "id"), "pck-", 4) == 0, "ratings.pick says which batch was better");
    snprintf(pick, sizeof pick, "%s", str(reply.json, "id"));
    workshop_reply_destroy(&reply);
    check(journaled("ratings.pick"), "ratings.pick is journaled with its actor");

    char query_params[200];
    snprintf(query_params, sizeof query_params, "{\"session\":\"%s\"}", g_session);
    check(query("ratings.session", query_params, &reply, &error) == URSATZ_OK && cJSON_GetArraySize(member(reply.json, "ratings")) == 2 && cJSON_GetArraySize(member(reply.json, "picks")) == 1 &&
              cJSON_GetArraySize(member(reply.json, "batches")) == 2,
          "ratings.session reads the session: two batches, two ratings (the replaced one is gone) and a pick");
    workshop_reply_destroy(&reply);
    check(query("ratings.sessions", NULL, &reply, &error) == URSATZ_OK && cJSON_GetArraySize(member(reply.json, "sessions")) == 1 &&
              num(cJSON_GetArrayItem(member(reply.json, "sessions"), 0), "ratings") == 2,
          "ratings.sessions lists the session with its counts");
    workshop_reply_destroy(&reply);

    /* refusals */
    with_session(params, sizeof params, "{\"session\":\"SESSION\",\"round\":1,\"batch\":\"A\",\"seed\":99,\"rating\":3}");
    expect_refusal("ratings.rate", NULL, params, URSATZ_ERR_REQUEST, "/params/seed", "is not recorded");
    with_session(params, sizeof params, "{\"session\":\"SESSION\",\"round\":1,\"batch\":\"A\",\"seed\":2,\"rating\":3}");
    expect_refusal("ratings.rate", NULL, params, URSATZ_ERR_REQUEST, "/params/seed", "was not made");
    with_session(params, sizeof params, "{\"session\":\"SESSION\",\"round\":1,\"batch\":\"A\",\"seed\":1,\"rating\":6}");
    expect_refusal("ratings.rate", NULL, params, URSATZ_ERR_REQUEST, "/params/rating", "rating must be from 1 to 5");
    with_session(params, sizeof params, "{\"session\":\"SESSION\",\"round\":1,\"seed\":1,\"better\":\"Z\"}");
    expect_refusal("ratings.pick", NULL, params, URSATZ_ERR_REQUEST, "/params/better", "no batch Z");
    expect_refusal("ratings.rate", NULL, "{\"session\":\"ses-nothing\",\"round\":1,\"batch\":\"A\",\"seed\":1,\"rating\":3}", URSATZ_ERR_NOT_FOUND, "/params/session", "no session");

    /* keep as default */
    char keep[600];
    snprintf(keep, sizeof keep, "{\"taste\":\"none\",\"key\":\"motif.features.dotted\",\"value\":1,\"ratings\":[\"%s\",\"%s\"],\"note\":\"B won\"}", rating_b, pick);
    expect_refusal("ratings.keep", NULL, "{\"taste\":\"none\",\"key\":\"motif.features.dotted\",\"value\":1,\"ratings\":[]}", URSATZ_ERR_REQUEST, "/params/ratings", "keep as default cites the ratings behind it");
    expect_refusal("ratings.keep", NULL, "{\"taste\":\"none\",\"key\":\"motif.features.dotted\",\"value\":1,\"ratings\":[\"rat-nothing\"]}", URSATZ_ERR_REQUEST, "/params/ratings/0", "rat-nothing is not a recorded rating or pick");
    char bad[600];
    snprintf(bad, sizeof bad, "{\"taste\":\"none\",\"key\":\"motif.features.dotted\",\"value\":99,\"ratings\":[\"%s\"]}", pick);
    expect_refusal("ratings.keep", NULL, bad, URSATZ_ERR_REQUEST, "/params/value", "must be");
    check(run("ratings.keep", NULL, keep, &reply, &error) == URSATZ_OK && num(member(reply.json, "values"), "motif.features.dotted") == 1, "ratings.keep sets the taste profile's value");
    const cJSON *kept = member(reply.json, "kept");
    const cJSON *entry = cJSON_GetArrayItem(kept, cJSON_GetArraySize(kept) - 1);
    check(strcmp(str(entry, "key"), "motif.features.dotted") == 0 && cJSON_GetArraySize(member(entry, "ratings")) == 2 &&
              strcmp(cJSON_GetArrayItem(member(entry, "ratings"), 1)->valuestring, pick) == 0 && strcmp(str(entry, "actor"), "test") == 0,
          "and a kept entry citing the rating and the pick");
    workshop_reply_destroy(&reply);
    check(journaled("ratings.keep"), "ratings.keep is journaled with its actor");
    snprintf(bad, sizeof bad, "{\"taste\":\"none\",\"key\":\"motif.features.dotted\",\"ratings\":[\"%s\"],\"note\":\"dropped\"}", pick);
    check(run("ratings.keep", NULL, bad, &reply, &error) == URSATZ_OK && member(member(reply.json, "values"), "motif.features.dotted") == NULL && cJSON_GetArraySize(member(reply.json, "kept")) == 2,
          "a null value drops the key and is kept too");
    workshop_reply_destroy(&reply);
}

static void test_item_ratings(void) {
    WorkshopReply reply;
    UrsatzError error;
    char target[200];
    snprintf(target, sizeof target, "%s", g_item);
    check(run("ratings.rate-version", target, "{\"version\":1,\"rating\":5,\"note\":\"keeper\"}", &reply, &error) == URSATZ_OK && strncmp(str(reply.json, "id"), "rat-", 4) == 0,
          "ratings.rate-version rates a version of an item");
    workshop_reply_destroy(&reply);
    check(journaled("ratings.rate-version"), "ratings.rate-version is journaled with its actor");
    char params[200];
    snprintf(params, sizeof params, "{\"item_id\":\"%s\"}", g_item);
    check(query("ratings.item", params, &reply, &error) == URSATZ_OK && cJSON_GetArraySize(member(reply.json, "ratings")) == 1 && num(cJSON_GetArrayItem(member(reply.json, "ratings"), 0), "version") == 1,
          "ratings.item reads them back");
    workshop_reply_destroy(&reply);
    expect_refusal("ratings.rate-version", target, "{\"version\":9,\"rating\":5}", URSATZ_ERR_NOT_FOUND, "/params/version", "no version 9");
}

int main(void) {
    if (!harness_open(18794)) {
        return 1;
    }
    WorkshopReply reply;
    UrsatzError error;
    bool ready = command("project.create", NULL, "{\"name\":\"Ratings\"}", -1, &reply, &error) == URSATZ_OK;
    if (ready) {
        snprintf(g_project, sizeof(g_project), "%s", str(member(reply.json, "project"), "id"));
    }
    workshop_reply_destroy(&reply);
    char params[3000];
    snprintf(params, sizeof(params), "{\"type\":\"motif\",\"name\":\"Hook\",\"project_id\":\"%s\",\"body\":%s}", g_project, kMotifBody);
    ready = ready && command("item.create", NULL, params, -1, &reply, &error) == URSATZ_OK;
    if (ready) {
        snprintf(g_item, sizeof(g_item), "%s", str(member(reply.json, "item"), "id"));
    }
    workshop_reply_destroy(&reply);
    check(ready, "a project and a motif are made");
    if (ready) {
        test_session_flow();
        test_item_ratings();
    }
    harness_close();
    return test_finish("workshop_ratings");
}
