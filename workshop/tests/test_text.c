#include <stdarg.h>

#include "api_harness.h"

#include <ursatz_test.h>

/*
 * Ursatz Text from the workshop (R-662, R-663): the tool `text` on a tools workshop (a layer file or a text, a view of it, its JSON, every
 * refusal), then the queries item.view and library.view on a migrated copy of the layout 0 fixture folder (a piece version, a motif version,
 * an imported piece). Each runs in its own working directory, which it clears.
 */

#ifndef TEXT_FIXTURES
#error "TEXT_FIXTURES must name library/text/tests/fixtures"
#endif
#ifndef TEXT_SNAPSHOTS
#error "TEXT_SNAPSHOTS must name library/text/tests/snapshots"
#endif
#ifndef TEXT_FILL_PLAN
#error "TEXT_FILL_PLAN must name composer/tests/snapshots/fill.plan.json"
#endif
#ifndef TEXT_DATA_V0
#error "TEXT_DATA_V0 must name the layout 0 fixture folder"
#endif
#ifndef TEXT_LINK_FIXTURES
#error "TEXT_LINK_FIXTURES must name the link fixtures folder"
#endif

#define PIECE "pce-2c8d54834a9c499f"

static bool contains(const char *text, const char *part) {
    return text && strstr(text, part) != NULL;
}

/* The tool on a tools workshop. */
static UrsatzStatus tool(Workshop *workshop, const char *params_text, WorkshopReply *reply, UrsatzError *error) {
    cJSON *params = cJSON_Parse(params_text);
    memset(error, 0, sizeof(*error));
    UrsatzStatus status = workshop_tool(workshop, "text", params, NULL, reply, error);
    cJSON_Delete(params);
    return status;
}

static char *path_params(const char *file, const char *rest) {
    char *out = malloc(strlen(file) + strlen(rest) + 64);
    sprintf(out, "{\"path\":\"%s\"%s%s}", file, rest[0] ? "," : "", rest);
    return out;
}

/* The tool's text with every occurrence of `from` replaced by `to`. */
static char *replaced(const char *text, const char *from, const char *to) {
    char *out = malloc(strlen(text) * 2 + strlen(to) * 8 + 16);
    char *w = out;
    size_t n = strlen(from);
    for (const char *p = text; *p;) {
        if (strncmp(p, from, n) == 0) {
            w += sprintf(w, "%s", to);
            p += n;
        } else {
            *w++ = *p++;
        }
    }
    *w = '\0';
    return out;
}

static void test_tool(void) {
    UrsatzError error;
    Workshop *workshop = NULL;
    check(workshop_tools_open(&workshop, &error) == URSATZ_OK && workshop, "a tools workshop opens");
    WorkshopReply reply;
    char path[512];

    /* the summary of a plan file is B2's golden */
    char *params = path_params(TEXT_FILL_PLAN, "\"view\":\"summary\"");
    check(tool(workshop, params, &reply, &error) == URSATZ_OK && strcmp(str(reply.json, "kind"), "plan") == 0, "the tool prints a plan file's summary");
    char *golden = test_must_read_file(TEXT_SNAPSHOTS "/fill.summary.txt", NULL);
    char *view = replaced(str(reply.json, "text"), TEXT_FILL_PLAN, "fill");
    check(strcmp(view, golden) == 0, "the summary of a plan file is the golden of the view");
    free(view);
    free(golden);
    workshop_reply_destroy(&reply);
    free(params);

    /* its Ursatz Text: full, then full again from the text, then json: the codec's JSON */
    snprintf(path, sizeof(path), "%s/loop.plan.json", TEXT_FIXTURES);
    params = path_params(path, "\"view\":\"full\"");
    check(tool(workshop, params, &reply, &error) == URSATZ_OK && strcmp(str(reply.json, "kind"), "plan") == 0, "full prints the plan as Ursatz Text");
    char *plan_text = strdup(str(reply.json, "text"));
    check(strncmp(plan_text, "ursatz-text 1 plan\n", 19) == 0, "its first line is the header");
    workshop_reply_destroy(&reply);
    free(params);
    cJSON *own = cJSON_CreateObject();
    cJSON_AddStringToObject(own, "text", plan_text);
    cJSON_AddStringToObject(own, "view", "json");
    char *own_text = cJSON_PrintUnformatted(own);
    cJSON_Delete(own);
    check(tool(workshop, own_text, &reply, &error) == URSATZ_OK && strcmp(str(reply.json, "kind"), "plan") == 0 && cJSON_IsString(member(reply.json, "json")),
          "the text gives the codec's JSON back");
    char *from_text = strdup(str(reply.json, "json"));
    workshop_reply_destroy(&reply);
    free(own_text);
    params = path_params(path, "\"view\":\"json\"");
    check(tool(workshop, params, &reply, &error) == URSATZ_OK && strcmp(str(reply.json, "json"), from_text) == 0,
          "the JSON of the text equals the JSON of the file (both through the codec)");
    workshop_reply_destroy(&reply);
    free(params);
    free(from_text);
    free(plan_text);

    /* a reading as a summary; a realization's line and part; the limits */
    snprintf(path, sizeof(path), "%s/loop.reading.json", TEXT_FIXTURES);
    params = path_params(path, "\"view\":\"summary\"");
    check(tool(workshop, params, &reply, &error) == URSATZ_OK && strcmp(str(reply.json, "kind"), "reading") == 0 && contains(str(reply.json, "text"), "bars 1-"),
          "a reading has a summary");
    workshop_reply_destroy(&reply);
    free(params);
    snprintf(path, sizeof(path), "%s/loop.realization.json", TEXT_FIXTURES);
    params = path_params(path, "\"view\":\"part:piano\",\"measures\":{\"first\":2,\"last\":3}");
    check(tool(workshop, params, &reply, &error) == URSATZ_OK && strcmp(str(reply.json, "kind"), "realization") == 0 &&
              contains(str(reply.json, "text"), "\nbar 2\n") && !contains(str(reply.json, "text"), "\nbar 4\n") &&
              contains(str(reply.json, "text"), "bars outside 2-3"),
          "a realization's part for a measure range");
    workshop_reply_destroy(&reply);
    free(params);

    /* every refusal */
    snprintf(path, sizeof(path), "%s/loop.realization.json", TEXT_FIXTURES);
    params = path_params(path, "\"view\":\"harmony\"");
    check(tool(workshop, params, &reply, &error) == URSATZ_ERR_REQUEST && strcmp(error.path, "/params/view") == 0 &&
              strcmp(error.message, "harmony needs a plan or a reading") == 0,
          "a view the layer cannot give is refused at /params/view with the view's message");
    workshop_reply_destroy(&reply);
    free(params);
    params = path_params(path, "\"view\":\"nothing\"");
    check(tool(workshop, params, &reply, &error) == URSATZ_ERR_REQUEST && strcmp(error.message, "unknown view 'nothing'") == 0, "an unknown view is refused");
    workshop_reply_destroy(&reply);
    free(params);
    params = path_params(path, "\"view\":\"part:piano\",\"section\":\"zz\"");
    check(tool(workshop, params, &reply, &error) == URSATZ_ERR_REQUEST && strcmp(error.path, "/params/section") == 0, "an unknown section is refused at /params/section");
    workshop_reply_destroy(&reply);
    free(params);
    params = path_params(path, "\"view\":\"part:piano\",\"measures\":{\"first\":500,\"last\":600}");
    check(tool(workshop, params, &reply, &error) == URSATZ_ERR_REQUEST && strcmp(error.path, "/params/measures") == 0, "a measure range outside the piece is refused at /params/measures");
    workshop_reply_destroy(&reply);
    free(params);
    params = path_params(path, "\"view\":\"part:piano\",\"measures\":{\"first\":\"x\"}");
    check(tool(workshop, params, &reply, &error) == URSATZ_ERR_REQUEST && strcmp(error.path, "/params/measures") == 0, "measures must be whole numbers");
    workshop_reply_destroy(&reply);
    free(params);
    check(tool(workshop, "{\"path\":\"/no/such/file.json\"}", &reply, &error) == URSATZ_ERR_NOT_FOUND && strcmp(error.path, "/params/path") == 0, "a file that does not read is not found");
    workshop_reply_destroy(&reply);
    check(tool(workshop, "{\"text\":\"ursatz-text 1 json\\na 1\\nb [1 2\\n\"}", &reply, &error) == URSATZ_ERR_REQUEST && strcmp(error.path, "/params/text") == 0 &&
              strcmp(error.message, "line 3: unterminated [") == 0,
          "a text that does not parse names its line");
    workshop_reply_destroy(&reply);
    check(tool(workshop, "{\"text\":\"ursatz-text 1 plan\\nformat ursatz-plan\\n\"}", &reply, &error) == URSATZ_ERR_REQUEST && strcmp(error.path, "/params/text") == 0,
          "a plan text the codec refuses is refused at /params/text with the codec's path");
    workshop_reply_destroy(&reply);
    check(tool(workshop, "{}", &reply, &error) == URSATZ_ERR_REQUEST && strcmp(error.path, "/params") == 0, "neither path nor text is refused");
    workshop_reply_destroy(&reply);
    check(tool(workshop, "{\"path\":\"a\",\"text\":\"b\"}", &reply, &error) == URSATZ_ERR_REQUEST && strcmp(error.path, "/params") == 0, "both path and text are refused");
    workshop_reply_destroy(&reply);
    check(tool(workshop, "{\"text\":\"{\\\"a\\\":1}\",\"view\":\"full\"}", &reply, &error) == URSATZ_OK && strcmp(str(reply.json, "kind"), "json") == 0 &&
              strcmp(str(reply.json, "text"), "ursatz-text 1 json\na 1\n") == 0,
          "any JSON object is a json document");
    workshop_reply_destroy(&reply);
    workshop_close(workshop);
}

static void sh(const char *format, ...) __attribute__((format(printf, 1, 2)));

static void sh(const char *format, ...) {
    char command_text[2048];
    va_list args;
    va_start(args, format);
    vsnprintf(command_text, sizeof(command_text), format, args);
    va_end(args);
    if (system(command_text) != 0) {
        fprintf(stderr, "command failed: %s\n", command_text);
    }
}

/* A migrated copy of the fixture folder, opened and started in the working directory. */
static bool open_copy(void) {
    char cwd[512];
    if (!getcwd(cwd, sizeof(cwd)) || strstr(cwd, "/workshop/tests/") == NULL) {
        return false;
    }
    sh("rm -rf ./* ./.[!.]* && cp -r '%s'/. . && rm -f .pre-overhaul-checked.lock", TEXT_DATA_V0);
    UrsatzError error = {{0}, {0}};
    cJSON *report = NULL;
    WorkshopMigrateOptions options = {false, NULL};
    if (workshop_migrate(".", &options, &report, &error) != URSATZ_OK) {
        fprintf(stderr, "cannot migrate: %s\n", error.message);
        return false;
    }
    cJSON_Delete(report);
    sh("rm -rf ./*-backup-*");
    WorkshopConfig flags = {.data_dir = ".", .config_path = NULL, .port = 0, .http_threads = 2, .job_workers = 2};
    WorkshopConfig resolved;
    char storage[4096];
    if (workshop_config_resolve(&flags, &resolved, storage, sizeof(storage), &error) != URSATZ_OK ||
        workshop_open(&resolved, &g_workshop, &error) != URSATZ_OK || workshop_start(g_workshop, &error) != URSATZ_OK) {
        fprintf(stderr, "cannot open the workshop: %s\n", error.message);
        return false;
    }
    return true;
}

static void test_queries(void) {
    WorkshopReply reply;
    UrsatzError error;
    char params[512];

    /* a piece version: summary, structure, full */
    check(query("item.view", "{\"id\":\"" PIECE "\",\"version\":2}", &reply, &error) == URSATZ_OK && strcmp(str(reply.json, "view"), "summary") == 0 &&
              contains(str(reply.json, "text"), "ursatz-text 1 view summary item:" PIECE "@v2\n") && contains(str(reply.json, "text"), "  s0  bars "),
          "item.view is the summary of a piece version, framed with its address");
    workshop_reply_destroy(&reply);
    check(query("item.view", "{\"id\":\"" PIECE "\"}", &reply, &error) == URSATZ_OK && contains(str(reply.json, "text"), "item:" PIECE "@v2"),
          "the version defaults to the current one");
    workshop_reply_destroy(&reply);
    check(query("item.view", "{\"id\":\"" PIECE "\",\"version\":2,\"view\":\"structure\",\"section\":\"s0\"}", &reply, &error) == URSATZ_OK &&
              contains(str(reply.json, "text"), "section s0") && contains(str(reply.json, "text"), "bars outside "),
          "the structure of a section");
    workshop_reply_destroy(&reply);
    check(query("item.view", "{\"id\":\"" PIECE "\",\"version\":2,\"view\":\"full\"}", &reply, &error) == URSATZ_OK &&
              strncmp(str(reply.json, "text"), "ursatz-text 1 plan\n", 19) == 0,
          "full is the plan's text");
    workshop_reply_destroy(&reply);
    check(query("item.view", "{\"id\":\"" PIECE "\",\"version\":2,\"view\":\"summary\",\"measures\":{\"first\":900,\"last\":901}}", &reply, &error) ==
                  URSATZ_ERR_REQUEST && strcmp(error.path, "/params/measures") == 0,
          "a measure range outside the piece is refused");
    workshop_reply_destroy(&reply);

    /* json is what item.version answers */
    WorkshopReply version;
    check(query("item.view", "{\"id\":\"" PIECE "\",\"version\":2,\"view\":\"json\"}", &reply, &error) == URSATZ_OK &&
              query("item.version", "{\"id\":\"" PIECE "\",\"version\":2}", &version, &error) == URSATZ_OK,
          "json answers");
    char *a = cJSON_PrintUnformatted(reply.json);
    char *b = cJSON_PrintUnformatted(version.json);
    check(a && b && strcmp(a, b) == 0, "item.view json equals item.version");
    free(a);
    free(b);
    workshop_reply_destroy(&reply);
    workshop_reply_destroy(&version);

    /* a motif version has a body and no layer */
    WorkshopReply items;
    char motif[64] = "";
    if (query("item.list", "{\"type\":\"motif\"}", &items, &error) == URSATZ_OK) {
        snprintf(motif, sizeof(motif), "%s", str(cJSON_GetArrayItem(member(items.json, "items"), 0), "id"));
    }
    workshop_reply_destroy(&items);
    check(motif[0] != '\0', "the fixture has a motif");
    snprintf(params, sizeof(params), "{\"id\":\"%s\",\"version\":1,\"view\":\"full\"}", motif);
    check(query("item.view", params, &reply, &error) == URSATZ_OK && strncmp(str(reply.json, "text"), "ursatz-text 1 json\n", 19) == 0 && contains(str(reply.json, "text"), "motif"),
          "a motif version's full is its body as json");
    workshop_reply_destroy(&reply);
    snprintf(params, sizeof(params), "{\"id\":\"%s\",\"version\":1,\"view\":\"summary\"}", motif);
    check(query("item.view", params, &reply, &error) == URSATZ_ERR_NOT_FOUND && strcmp(error.path, "/params/view") == 0 &&
              strcmp(error.message, "this version holds no plan, realization or reading") == 0,
          "a motif version refuses a summary");
    workshop_reply_destroy(&reply);
    check(query("item.view", "{\"id\":\"" PIECE "\",\"version\":9}", &reply, &error) == URSATZ_ERR_NOT_FOUND, "a version that is not there is not found");
    workshop_reply_destroy(&reply);
    check(query("item.view", "{\"id\":\"mot-nope\"}", &reply, &error) == URSATZ_ERR_NOT_FOUND, "an item that is not there is not found");
    workshop_reply_destroy(&reply);

    /* an imported piece: the reading view is the Analyzer's export, framed; its layers answer the other views */
    char file[600];
    snprintf(file, sizeof(file), "%s/a.musicxml", TEXT_LINK_FIXTURES);
    size_t size = 0;
    char *data = test_read_file(file, &size);
    WorkshopBytes bytes = {(const uint8_t *)data, size, "a.musicxml"};
    check(command_as("test", "library.import", NULL, "{\"filename\":\"a.musicxml\"}", -1, &bytes, &reply, &error) == URSATZ_OK, "a piece imports");
    cJSON *job = finish_job(&reply);
    char piece[64];
    snprintf(piece, sizeof(piece), "%s", str(member(job, "result"), "id"));
    cJSON_Delete(job);
    workshop_reply_destroy(&reply);
    free(data);
    check(piece[0] != '\0', "the import made a piece");
    snprintf(params, sizeof(params), "{\"id\":\"%s\",\"view\":\"reading\"}", piece);
    check(query("library.view", params, &reply, &error) == URSATZ_OK && contains(str(reply.json, "text"), "ursatz-text 1 view reading piece:") &&
              contains(str(reply.json, "text"), "\n# Ursatz export v2: piece\n") && contains(str(reply.json, "text"), "\n# leaves out: nothing the analysis document holds"),
          "library.view reading is the framed export");
    workshop_reply_destroy(&reply);
    snprintf(params, sizeof(params), "{\"id\":\"%s\",\"view\":\"reading\",\"title\":\"Own title\",\"measures\":{\"first\":1,\"last\":1}}", piece);
    check(query("library.view", params, &reply, &error) == URSATZ_OK && contains(str(reply.json, "text"), "title: Own title") && contains(str(reply.json, "text"), "--measures 1-1"),
          "the title and the measures are the export's");
    workshop_reply_destroy(&reply);
    snprintf(params, sizeof(params), "{\"id\":\"%s\"}", piece);
    check(query("library.view", params, &reply, &error) == URSATZ_OK && strcmp(str(reply.json, "view"), "summary") == 0 &&
              contains(str(reply.json, "text"), "ursatz-text 1 view summary piece:") && contains(str(reply.json, "text"), "\nparts "),
          "library.view defaults to the summary, read from the piece's layers");
    char part[64] = "";
    const char *at = contains(str(reply.json, "text"), "\nparts ") ? strstr(str(reply.json, "text"), "\nparts ") + 7 : NULL;
    if (at) {
        size_t n = strcspn(at, ":\n");
        snprintf(part, sizeof(part), "%.*s", (int)n, at);
    }
    workshop_reply_destroy(&reply);
    check(part[0] != '\0', "the summary names a part");
    snprintf(params, sizeof(params), "{\"id\":\"%s\",\"view\":\"part:%s\"}", piece, part);
    check(query("library.view", params, &reply, &error) == URSATZ_OK && contains(str(reply.json, "text"), "\nbar 1\n"), "library.view reads a part of the piece");
    workshop_reply_destroy(&reply);
    snprintf(params, sizeof(params), "{\"id\":\"%s\",\"view\":\"full\"}", piece);
    check(query("library.view", params, &reply, &error) == URSATZ_OK && strncmp(str(reply.json, "text"), "ursatz-text 1 realization\n", 26) == 0 &&
              contains(str(reply.json, "text"), "\nursatz-text 1 reading\n"),
          "full is the realization's text and the reading's");
    workshop_reply_destroy(&reply);
    snprintf(params, sizeof(params), "{\"id\":\"%s\",\"view\":\"json\"}", piece);
    check(query("library.view", params, &reply, &error) == URSATZ_OK && strcmp(str(reply.json, "id"), piece) == 0 && member(reply.json, "document") != NULL,
          "json is what library.piece answers");
    workshop_reply_destroy(&reply);
    check(query("library.view", "{\"id\":\"nope\"}", &reply, &error) == URSATZ_ERR_NOT_FOUND, "a piece that is not there is not found");
    workshop_reply_destroy(&reply);

    /* a piece analyzed before the layers were stored */
    WorkshopReply listed;
    char old_piece[64] = "";
    if (query("library.list", NULL, &listed, &error) == URSATZ_OK) {
        const cJSON *entry;
        cJSON_ArrayForEach(entry, member(listed.json, "pieces")) {
            if (strcmp(str(entry, "id"), piece) != 0) {
                snprintf(old_piece, sizeof(old_piece), "%s", str(entry, "id"));
                break;
            }
        }
    }
    workshop_reply_destroy(&listed);
    check(old_piece[0] != '\0', "the fixture has a piece that was stored before the layers");
    snprintf(params, sizeof(params), "{\"id\":\"%s\",\"view\":\"summary\"}", old_piece);
    check(query("library.view", params, &reply, &error) == URSATZ_ERR_NOT_FOUND && strcmp(error.path, "/params/view") == 0 &&
              strcmp(error.message, "re-analyze the piece to store its layers") == 0,
          "a piece with no layers says to re-analyze it");
    workshop_reply_destroy(&reply);
}

int main(void) {
    test_tool();
    if (!open_copy()) {
        return 1;
    }
    test_queries();
    harness_close();
    return test_finish("workshop_text");
}
