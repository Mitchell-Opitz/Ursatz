/* The version a command was made against (R-646): a command that changes an item that has versions is refused when it is not the current one,
 * with the current version in the reply; a job command is checked when it is submitted and again when it writes; a missing base version is
 * refused; the journal holds the actor and the version. Runs in its own working directory. */

#include "api_harness.h"
#include "theme_body.h"

static const char *kMotifBody =
    "{\"uses\":[],\"motif\":{\"role\":\"melodic\",\"form\":\"full\",\"notes\":\"C4:e Eb4:e G4:e Eb4:e C4:e Eb4:e G4:e C5:e\","
    "\"meter\":\"4/4\",\"key\":\"C minor\",\"pickup\":\"0\",\"features\":[],\"analysis\":null,\"derived\":null}}";

static char g_project[64];
static char g_motif[64];
static char g_other[64];
static char g_theme[64];
static char g_theme_body[1200];

static bool make_item(const char *type, const char *name, const char *body_text, char *out, size_t size) {
    char params[3000];
    snprintf(params, sizeof(params), "{\"type\":\"%s\",\"name\":\"%s\",\"project_id\":\"%s\",\"body\":%s}", type, name, g_project, body_text);
    WorkshopReply reply;
    UrsatzError error;
    UrsatzStatus status = command("item.create", NULL, params, -1, &reply, &error);
    bool ok = status == URSATZ_OK;
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

static void save_params(char *out, size_t size, const char *notes) {
    snprintf(out, size,
             "{\"note\":\"changed\",\"body\":{\"uses\":[],\"motif\":{\"role\":\"melodic\",\"form\":\"full\",\"notes\":\"%s\",\"meter\":\"4/4\","
             "\"key\":\"C minor\",\"pickup\":\"0\",\"features\":[],\"analysis\":null,\"derived\":null}}}",
             notes);
}

static void test_refusals(void) {
    WorkshopReply reply;
    UrsatzError error;
    char params[1000];
    save_params(params, sizeof(params), "C4:q Eb4:e G4:e C5:h");

    UrsatzStatus status = command("item.save", g_motif, params, 1, &reply, &error);
    check(status == URSATZ_OK && current_version(g_motif) == 2, "a save made against the current version writes the next one");
    check(num(reply.json, "journal_seq") > 0, "the reply of a command holds the journal sequence number of its record");
    workshop_reply_destroy(&reply);
    cJSON *record = last_record();
    check(strcmp(str(record, "command"), "item.save") == 0 && strcmp(str(record, "actor"), "test") == 0 &&
              strcmp(str(record, "target"), g_motif) == 0 && num(record, "base_version") == 1,
          "the record holds the command, the actor, the target and the version the command was made against");
    cJSON_Delete(record);

    status = command("item.save", g_motif, params, 1, &reply, &error);
    check(status == URSATZ_ERR_CONFLICT && strcmp(error.message, "changed since you read it") == 0 && strcmp(error.path, "/base_version") == 0,
          "a save made against a version that is no longer the current one is refused: conflict, \"changed since you read it\"");
    check(num(reply.json, "current_version") == 2 && current_version(g_motif) == 2, "the refusal names the current version and nothing was written");
    workshop_reply_destroy(&reply);

    status = command("item.save", g_motif, params, -1, &reply, &error);
    check(status == URSATZ_ERR_REQUEST && strcmp(error.path, "/base_version") == 0, "a save with no base version is refused at /base_version");
    workshop_reply_destroy(&reply);

    for (int i = 0; i < 2; i++) {
        const char *name = i == 0 ? "item.update" : "item.delete";
        status = command(name, g_motif, i == 0 ? "{\"name\":\"Renamed\"}" : "{}", 1, &reply, &error);
        CHECKF(status == URSATZ_ERR_CONFLICT && num(reply.json, "current_version") == 2, "a stale rename or delete is refused with the current version");
        workshop_reply_destroy(&reply);
    }
    status = command("item.update", g_motif, "{\"name\":\"Renamed\"}", 2, &reply, &error);
    check(status == URSATZ_OK, "a rename made against the current version is applied");
    workshop_reply_destroy(&reply);

    /* A command that makes a new item, or changes one that has no versions, needs no base version. */
    status = command("project.update", g_project, "{\"description\":\"d\"}", -1, &reply, &error);
    check(status == URSATZ_OK, "a project change needs no base version");
    workshop_reply_destroy(&reply);

    status = command_as(NULL, "project.update", g_project, "{\"description\":\"d\"}", -1, NULL, &reply, &error);
    check(status == URSATZ_ERR_REQUEST && strcmp(error.path, "/actor") == 0, "a command without an actor is refused at /actor");
    status = command("item.nope", NULL, NULL, -1, &reply, &error);
    check(status == URSATZ_ERR_REQUEST && strcmp(error.path, "/command") == 0, "an unknown command is refused at /command");
    status = command("project.update", g_project, "[1]", -1, &reply, &error);
    check(status == URSATZ_ERR_REQUEST && strcmp(error.path, "/params") == 0, "params of the wrong type are refused at /params");
    status = command("project.update", g_project, "{\"description\":5}", -1, &reply, &error);
    check(status == URSATZ_ERR_REQUEST && strcmp(error.path, "/params/description") == 0 && strcmp(error.message, "description must be a string") == 0,
          "a params member of the wrong type is refused at its path, by the schema");
}

static bool make_theme(void) {
    char body[1200];
    theme_body_text(body, sizeof(body), g_other, g_other, -1);
    snprintf(g_theme_body, sizeof(g_theme_body), "%s", body);
    return make_item("theme", "Theme", body, g_theme, sizeof(g_theme));
}

/* The job of a compose is checked when it is submitted and when it writes. */
static void test_compose(void) {
    WorkshopReply reply;
    UrsatzError error;
    UrsatzStatus status = command("item.compose", g_theme, "{\"seed\":3}", 5, &reply, &error);
    check(status == URSATZ_ERR_CONFLICT && num(reply.json, "current_version") == 1 && !member(reply.json, "job"),
          "a compose made against a version that is not the current one is refused at submit and queues no job");
    workshop_reply_destroy(&reply);

    status = command("item.compose", g_theme, "{\"seed\":3}", 1, &reply, &error);
    cJSON *job = status == URSATZ_OK ? finish_job(&reply) : NULL;
    check(status == URSATZ_OK && member(reply.json, "job") && job && strcmp(str(job, "state"), "done") == 0 && num(job, "http_status") == 200,
          "a compose made against the current version is queued and composes the next version");
    check(current_version(g_theme) == 2, "the composed version is the theme's version 2");
    cJSON_Delete(job);
    workshop_reply_destroy(&reply);
    cJSON *record = last_record();
    check(strcmp(str(record, "command"), "item.compose") == 0 && strcmp(str(record, "actor"), "test") == 0 && num(record, "base_version") == 1,
          "the job's record is the command's: item.compose, its actor, its version");
    cJSON_Delete(record);

    /* A compose and a save made against the same version: whichever writes second is refused, a job too. */
    WorkshopReply composing;
    UrsatzError compose_error;
    UrsatzStatus queued = command("item.compose", g_theme, "{\"seed\":4}", 2, &composing, &compose_error);
    char params[1500];
    snprintf(params, sizeof(params), "{\"note\":\"by hand\",\"body\":{\"uses\":[],\"request\":{}}}");
    UrsatzStatus saved = command("item.save", g_theme, params, 2, &reply, &error);
    cJSON *finished = queued == URSATZ_OK ? finish_job(&composing) : NULL;
    bool compose_done = finished && strcmp(str(finished, "state"), "done") == 0;
    bool compose_refused = finished && num(finished, "http_status") == 409 && strcmp(str(member(finished, "result"), "error"), "changed since you read it") == 0;
    check(queued == URSATZ_OK && ((saved == URSATZ_OK && compose_refused) || (saved == URSATZ_ERR_CONFLICT && compose_done)),
          "of a compose and a save made against one version the second to write is refused (a job fails with the same refusal)");
    check(current_version(g_theme) == 3, "and exactly one of them wrote version 3");
    cJSON_Delete(finished);
    workshop_reply_destroy(&composing);
    workshop_reply_destroy(&reply);
}

/* A compose that moves its uses to their sources' newest versions first is not refused for its own first write. */
static void test_update_uses(void) {
    WorkshopReply reply;
    UrsatzError error;
    char params[1000];
    save_params(params, sizeof(params), "G4:e F4:e Eb4:e D4:e F4:e Eb4:e D4:e C4:e");
    check(command("item.save", g_other, params, 1, &reply, &error) == URSATZ_OK, "the motif the theme uses gets a new version");
    workshop_reply_destroy(&reply);
    /* the theme as first saved, which uses the motif at version 1 */
    char theme_params[1500];
    snprintf(theme_params, sizeof(theme_params), "{\"note\":\"uses again\",\"body\":%s}", g_theme_body);
    check(command("item.save", g_theme, theme_params, current_version(g_theme), &reply, &error) == URSATZ_OK, "the theme is saved with its uses again");
    workshop_reply_destroy(&reply);
    int base = current_version(g_theme);
    UrsatzStatus status = command("item.compose", g_theme, "{\"seed\":5,\"update_uses\":true}", base, &reply, &error);
    cJSON *job = status == URSATZ_OK ? finish_job(&reply) : NULL;
    check(job && strcmp(str(job, "state"), "done") == 0 && num(job, "http_status") == 200,
          "a compose with update_uses writes the version that moves the uses and the composed one");
    check(current_version(g_theme) == base + 2, "two versions were written on top of the version the command was made against");
    cJSON_Delete(job);
    workshop_reply_destroy(&reply);
}

/* The four commands that took no base version before (R-679): item.variations, item.set-op-checks, item.save-variant and
   ratings.rate-version are refused without one and on a stale one, as every other change of an item is. */
static void test_four_commands(void) {
    static const struct {
        const char *name;
        const char *params;
    } COMMANDS[] = {
        {"item.variations", "{\"version\":1,\"section_id\":\"a\",\"lane\":\"melody\",\"count\":1}"},
        {"item.set-op-checks", "{\"checks\":{}}"},
        {"item.save-variant", "{\"variant\":{\"chain\":{\"steps\":[]},\"seed\":1,\"kind\":\"variant\",\"derivation\":\"x\"}}"},
        {"ratings.rate-version", "{\"version\":1,\"rating\":3}"},
    };
    int current = current_version(g_other);
    for (size_t i = 0; i < sizeof(COMMANDS) / sizeof(COMMANDS[0]); i++) {
        WorkshopReply reply;
        UrsatzError error;
        UrsatzStatus status = command(COMMANDS[i].name, g_other, COMMANDS[i].params, -1, &reply, &error);
        CHECKF(status == URSATZ_ERR_REQUEST && strcmp(error.path, "/base_version") == 0, "%s without a base version is refused", COMMANDS[i].name);
        workshop_reply_destroy(&reply);
        status = command(COMMANDS[i].name, g_other, COMMANDS[i].params, current + 5, &reply, &error);
        CHECKF(status == URSATZ_ERR_CONFLICT && strcmp(error.path, "/base_version") == 0 && (int)num(reply.json, "current_version") == current,
              "%s on a stale base version is refused with the current version", COMMANDS[i].name);
        workshop_reply_destroy(&reply);
    }
}

int main(void) {
    if (!harness_open(18991)) {
        return 1;
    }
    WorkshopReply reply;
    UrsatzError error;
    bool ready = command("project.create", NULL, "{\"name\":\"Versions\"}", -1, &reply, &error) == URSATZ_OK;
    if (ready) {
        snprintf(g_project, sizeof(g_project), "%s", str(member(reply.json, "project"), "id"));
    }
    workshop_reply_destroy(&reply);
    ready = ready && make_item("motif", "A", kMotifBody, g_motif, sizeof(g_motif)) &&
            make_item("motif", "B", kMotifBody, g_other, sizeof(g_other)) && make_theme();
    check(ready, "the project, two motifs and a theme are made");
    if (ready) {
        test_refusals();
        test_compose();
        test_update_uses();
        test_four_commands();
    }
    harness_close();
    return test_finish("workshop_base_version");
}
