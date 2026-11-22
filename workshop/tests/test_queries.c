/* Every read of the GUI as a workshop query (R-646): each query answers what its route answers on the same fixture (the members the route
 * tests look at), a query that is refused says why, and the queries together change no file outside cache/. Runs in its own working
 * directory. */

#include "api_harness.h"
#include "theme_body.h"

#include <dirent.h>
#include <sys/stat.h>

#include <ursatz_test.h>

#ifndef URSATZ_WORKSHOP_TEST_LINK_FIXTURES
#error "URSATZ_WORKSHOP_TEST_LINK_FIXTURES must name the link fixtures directory"
#endif

static const char *kMotifBody =
    "{\"uses\":[],\"motif\":{\"role\":\"melodic\",\"form\":\"full\",\"notes\":\"C4:e Eb4:e G4:e Eb4:e C4:e Eb4:e G4:e C5:e\","
    "\"meter\":\"4/4\",\"key\":\"C minor\",\"pickup\":\"0\",\"features\":[],\"analysis\":null,\"derived\":null}}";

static char g_piece[64], g_collection[64], g_empty_collection[64], g_project[64], g_motif[64], g_theme[64], g_subject[64], g_style[64], g_run[64];

/* A fingerprint of the folder outside cache/: every file's path, size and modification time, summed into one number. */
static unsigned long long fingerprint(const char *path) {
    unsigned long long sum = 0;
    DIR *dir = opendir(path);
    struct dirent *entry;
    while (dir && (entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0 || (strcmp(path, ".") == 0 && strcmp(entry->d_name, "cache") == 0) ||
            strcmp(entry->d_name, ".ursatz.lock") == 0) {
            continue;
        }
        char child[1024];
        snprintf(child, sizeof(child), "%s/%s", path, entry->d_name);
        struct stat info;
        if (stat(child, &info) != 0) {
            continue;
        }
        for (const char *c = child; *c; c++) {
            sum = sum * 131 + (unsigned char)*c;
        }
        if (S_ISDIR(info.st_mode)) {
            sum += fingerprint(child);
        } else {
            sum = sum * 31 + (unsigned long long)info.st_size + (unsigned long long)info.st_mtime;
        }
    }
    if (dir) {
        closedir(dir);
    }
    return sum;
}

static bool ask(const char *name, const char *params, WorkshopReply *reply, UrsatzError *error) {
    return query(name, params, reply, error) == URSATZ_OK;
}

static bool setup(void) {
    WorkshopReply reply;
    UrsatzError error;
    char *data = NULL;
    char path[600];
    snprintf(path, sizeof(path), "%s/a.musicxml", URSATZ_WORKSHOP_TEST_LINK_FIXTURES);
    size_t size = 0;
    data = test_read_file(path, &size);
    WorkshopBytes file = {(const uint8_t *)data, size, "a.musicxml"};
    UrsatzStatus status = command_as("test", "library.import", NULL, "{\"filename\":\"a.musicxml\"}", -1, &file, &reply, &error);
    cJSON *job = status == URSATZ_OK ? finish_job(&reply) : NULL;
    snprintf(g_piece, sizeof(g_piece), "%s", str(member(job, "result"), "id"));
    cJSON_Delete(job);
    workshop_reply_destroy(&reply);
    free(data);
    bool ok = g_piece[0] != '\0';
    char params[2000];
    snprintf(params, sizeof(params), "{\"piece_ids\":[\"%s\"]}", g_piece);
    ok = ok && command("library.update", g_piece, "{\"composer\":\"Tester\",\"tags\":[\"t\"]}", -1, &reply, &error) == URSATZ_OK;
    workshop_reply_destroy(&reply);
    ok = ok && command("collection.create", NULL, "{\"name\":\"Set\"}", -1, &reply, &error) == URSATZ_OK;
    snprintf(g_collection, sizeof(g_collection), "%s", str(reply.json, "id"));
    workshop_reply_destroy(&reply);
    ok = ok && command("collection.add-pieces", g_collection, params, -1, &reply, &error) == URSATZ_OK;
    workshop_reply_destroy(&reply);
    ok = ok && command("collection.create", NULL, "{\"name\":\"Empty\"}", -1, &reply, &error) == URSATZ_OK;
    snprintf(g_empty_collection, sizeof(g_empty_collection), "%s", str(reply.json, "id"));
    workshop_reply_destroy(&reply);
    ok = ok && command("project.create", NULL, "{\"name\":\"Work\"}", -1, &reply, &error) == URSATZ_OK;
    snprintf(g_project, sizeof(g_project), "%s", str(member(reply.json, "project"), "id"));
    workshop_reply_destroy(&reply);
    snprintf(params, sizeof(params), "{\"type\":\"motif\",\"name\":\"Source\",\"project_id\":\"%s\",\"body\":%s}", g_project, kMotifBody);
    ok = ok && command("item.create", NULL, params, -1, &reply, &error) == URSATZ_OK;
    snprintf(g_motif, sizeof(g_motif), "%s", str(member(reply.json, "item"), "id"));
    workshop_reply_destroy(&reply);
    char body[1500];
    theme_body_text(body, sizeof(body), g_motif, g_motif, -1);
    snprintf(params, sizeof(params), "{\"type\":\"theme\",\"name\":\"Theme\",\"project_id\":\"%s\",\"body\":%s}", g_project, body);
    ok = ok && command("item.create", NULL, params, -1, &reply, &error) == URSATZ_OK;
    snprintf(g_theme, sizeof(g_theme), "%s", str(member(reply.json, "item"), "id"));
    workshop_reply_destroy(&reply);
    ok = ok && command("item.compose", g_theme, "{\"seed\":1}", 1, &reply, &error) == URSATZ_OK;
    job = ok ? finish_job(&reply) : NULL;
    ok = ok && job && num(job, "http_status") == 200;
    cJSON_Delete(job);
    workshop_reply_destroy(&reply);
    ok = ok && command("subject.create", NULL, "{\"name\":\"Anna\",\"kind\":\"character\"}", -1, &reply, &error) == URSATZ_OK;
    snprintf(g_subject, sizeof(g_subject), "%s", str(member(reply.json, "subject"), "id"));
    workshop_reply_destroy(&reply);
    ok = ok && command("style.build", NULL, "{\"filter\":{\"tag\":\"t\"}}", -1, &reply, &error) == URSATZ_OK;
    job = ok ? finish_job(&reply) : NULL;
    snprintf(g_style, sizeof(g_style), "%s", str(member(member(job, "result"), "style"), "id"));
    cJSON_Delete(job);
    workshop_reply_destroy(&reply);
    snprintf(params, sizeof(params), "{\"scope\":{\"kind\":\"collection\",\"collection_id\":\"%s\"}}", g_collection);
    ok = ok && command("links.run", NULL, params, -1, &reply, &error) == URSATZ_OK;
    snprintf(g_run, sizeof(g_run), "%s", str(member(reply.json, "run"), "id"));
    job = ok ? finish_job(&reply) : NULL;
    cJSON_Delete(job);
    workshop_reply_destroy(&reply);
    return ok && g_style[0] && g_run[0];
}

static void ids(char *out, size_t size, const char *id) {
    snprintf(out, size, "{\"id\":\"%s\"}", id);
}

static void test_library_queries(void) {
    WorkshopReply reply;
    UrsatzError error;
    char params[256];
    check(ask("library.list", "{\"composer\":\"Tester\"}", &reply, &error) && cJSON_GetArraySize(member(reply.json, "pieces")) == 1 &&
              strcmp(str(cJSON_GetArrayItem(member(reply.json, "pieces"), 0), "id"), g_piece) == 0,
          "library.list lists the pieces that match a filter");
    workshop_reply_destroy(&reply);
    check(ask("library.list", "{\"composer\":\"Nobody\"}", &reply, &error) && cJSON_GetArraySize(member(reply.json, "pieces")) == 0, "and none for another composer");
    workshop_reply_destroy(&reply);
    check(ask("library.groups", NULL, &reply, &error) && num(reply.json, "total") == 1 && cJSON_IsArray(member(reply.json, "composers")), "library.groups counts composers, sources and tags");
    workshop_reply_destroy(&reply);
    snprintf(params, sizeof(params), "{\"collection\":\"%s\"}", g_collection);
    check(ask("library.list", params, &reply, &error) && cJSON_GetArraySize(member(reply.json, "pieces")) == 1 &&
              strcmp(str(cJSON_GetArrayItem(member(reply.json, "pieces"), 0), "id"), g_piece) == 0,
          "library.list with a collection lists its members");
    workshop_reply_destroy(&reply);
    check(ask("library.groups", params, &reply, &error) && num(reply.json, "total") == 1, "library.groups with a collection counts its members");
    workshop_reply_destroy(&reply);
    snprintf(params, sizeof(params), "{\"collection\":\"%s\",\"composer\":\"Nobody\"}", g_collection);
    check(ask("library.list", params, &reply, &error) && cJSON_GetArraySize(member(reply.json, "pieces")) == 0, "a collection and a filter both hold");
    workshop_reply_destroy(&reply);
    snprintf(params, sizeof(params), "{\"collection\":\"%s\"}", g_empty_collection);
    check(ask("library.list", params, &reply, &error) && cJSON_GetArraySize(member(reply.json, "pieces")) == 0, "an empty collection lists no pieces");
    workshop_reply_destroy(&reply);
    check(ask("library.groups", params, &reply, &error) && num(reply.json, "total") == 0, "and has no groups");
    workshop_reply_destroy(&reply);
    check(query("library.list", "{\"collection\":\"col-nothing\"}", &reply, &error) == URSATZ_ERR_NOT_FOUND, "a collection that is not there is not found");
    workshop_reply_destroy(&reply);
    check(query("library.groups", "{\"collection\":\"a/b\"}", &reply, &error) == URSATZ_ERR_REQUEST, "and an id that cannot name one is refused");
    workshop_reply_destroy(&reply);
    ids(params, sizeof(params), g_piece);
    check(ask("library.piece", params, &reply, &error) && strcmp(str(reply.json, "id"), g_piece) == 0 && member(reply.json, "document") != NULL,
          "library.piece is the piece with its analysis document");
    workshop_reply_destroy(&reply);
    check(query("library.piece", "{\"id\":\"nope\"}", &reply, &error) == URSATZ_ERR_NOT_FOUND && strcmp(error.message, "piece not found") == 0,
          "a piece that is not there is not found");
    workshop_reply_destroy(&reply);
    snprintf(params, sizeof(params), "{\"id\":\"%s\",\"sections\":\"metadata,notes\"}", g_piece);
    check(ask("library.document", params, &reply, &error) && member(member(reply.json, "sections"), "metadata") != NULL, "library.document gives the named sections");
    workshop_reply_destroy(&reply);
    snprintf(params, sizeof(params), "{\"id\":\"%s\",\"sections\":\"Bad Name\"}", g_piece);
    check(query("library.document", params, &reply, &error) == URSATZ_ERR_REQUEST && strcmp(error.path, "/params/sections") == 0, "a section name that is no name is refused at its member");
    workshop_reply_destroy(&reply);
    snprintf(params, sizeof(params), "{\"id\":\"%s\",\"view\":\"reading\",\"measures\":{\"first\":2,\"last\":1}}", g_piece);
    check(query("library.view", params, &reply, &error) == URSATZ_ERR_REQUEST, "library.view refuses a range that is backwards");
    workshop_reply_destroy(&reply);
    snprintf(params, sizeof(params), "{\"id\":\"%s\",\"view\":\"reading\"}", g_piece);
    check(ask("library.view", params, &reply, &error) && cJSON_IsString(member(reply.json, "text")), "library.view reading is the text export");
    const char *framed = str(reply.json, "text");
    check(strncmp(framed, "ursatz-text 1 view reading piece:", 33) == 0 && strstr(framed, "\n# Ursatz export v2: piece\n") != NULL &&
              strstr(framed, "\n# leaves out: nothing the analysis document holds; the notes are the import's\n# expand: ursatz show piece:") != NULL,
          "the export is framed as a view: its own lines sit between the frame lines");
    workshop_reply_destroy(&reply);
    ids(params, sizeof(params), g_piece);
    check(query("library.export", params, &reply, &error) == URSATZ_ERR_REQUEST && strstr(error.message, "unknown query") != NULL,
          "library.export is gone: library.view replaced it");
    workshop_reply_destroy(&reply);
    check(ask("library.musicxml", params, &reply, &error) && reply.bytes && strncmp(reply.content_type, "application/xml", 15) == 0 && strstr((char *)reply.bytes, "<score-partwise") != NULL,
          "library.musicxml is the score (application/xml)");
    workshop_reply_destroy(&reply);
    check(ask("library.meter-defaulted", params, &reply, &error) && cJSON_IsBool(member(reply.json, "meter_defaulted")), "library.meter-defaulted says whether the meter was defaulted");
    workshop_reply_destroy(&reply);
    check(ask("library.playback", params, &reply, &error) && cJSON_IsArray(member(reply.json, "notes")) && cJSON_GetArraySize(member(reply.json, "notes")) > 0,
          "library.playback is the notes a browser plays");
    workshop_reply_destroy(&reply);
}

static void test_collection_style_link_queries(void) {
    WorkshopReply reply;
    UrsatzError error;
    char params[256];
    check(ask("collection.list", NULL, &reply, &error) && cJSON_GetArraySize(member(reply.json, "corpora")) == 2 &&
              num(cJSON_GetArrayItem(member(reply.json, "corpora"), 0), "piece_count") == 0 &&
              num(cJSON_GetArrayItem(member(reply.json, "corpora"), 1), "piece_count") == 1,
          "collection.list lists the collections with their counts");
    workshop_reply_destroy(&reply);
    ids(params, sizeof(params), g_collection);
    check(ask("collection.get", params, &reply, &error) && strcmp(str(reply.json, "name"), "Set") == 0 && cJSON_GetArraySize(member(reply.json, "pieces")) == 1 &&
              member(reply.json, "created_at") != NULL,
          "collection.get is the collection with its piece ids");
    workshop_reply_destroy(&reply);
    check(ask("collection.pieces", params, &reply, &error) && strcmp(str(cJSON_GetArrayItem(member(reply.json, "pieces"), 0), "id"), g_piece) == 0, "collection.pieces lists them as {id}");
    workshop_reply_destroy(&reply);
    check(query("collection.get", "{\"id\":\"col-0000000000000000\"}", &reply, &error) == URSATZ_ERR_NOT_FOUND, "a collection that is not there is not found");
    workshop_reply_destroy(&reply);
    check(query("collection.get", "{\"id\":\"../x\"}", &reply, &error) == URSATZ_ERR_REQUEST && strcmp(error.message, "invalid id") == 0, "an id that cannot name one is refused");
    workshop_reply_destroy(&reply);

    check(ask("style.list", NULL, &reply, &error) && cJSON_GetArraySize(member(reply.json, "styles")) == 1, "style.list lists the styles");
    workshop_reply_destroy(&reply);
    ids(params, sizeof(params), g_style);
    check(ask("style.get", params, &reply, &error) && member(member(reply.json, "style"), "statistics") != NULL, "style.get is the style with its statistics");
    workshop_reply_destroy(&reply);
    check(query("style.get", "{\"id\":\"bad id\"}", &reply, &error) == URSATZ_ERR_NOT_FOUND, "an id that names no style is not found");
    workshop_reply_destroy(&reply);

    check(ask("links.list", NULL, &reply, &error) && cJSON_GetArraySize(member(reply.json, "runs")) == 1, "links.list lists the runs");
    workshop_reply_destroy(&reply);
    ids(params, sizeof(params), g_run);
    check(ask("links.get", params, &reply, &error) && strcmp(str(member(reply.json, "run"), "id"), g_run) == 0, "links.get is the run");
    workshop_reply_destroy(&reply);
    ids(params, sizeof(params), g_piece);
    check(ask("links.piece", params, &reply, &error) && cJSON_IsArray(member(reply.json, "links")), "links.piece is the links of a piece");
    workshop_reply_destroy(&reply);
    check(query("job.get", "{\"id\":\"job-0000000000000000\"}", &reply, &error) == URSATZ_ERR_NOT_FOUND && strcmp(error.message, "job not found") == 0, "job.get of a job that is not there is not found");
    workshop_reply_destroy(&reply);
    check(ask("job.list", NULL, &reply, &error) && cJSON_IsArray(member(reply.json, "jobs")), "job.list lists the jobs that are queued or running");
    workshop_reply_destroy(&reply);

    /* the journal: newest first, filtered by target, limited */
    check(ask("journal.list", "{\"limit\":1000}", &reply, &error) && cJSON_IsArray(member(reply.json, "records")) &&
              cJSON_GetArraySize(member(reply.json, "records")) >= 2,
          "journal.list lists the records of the commands that changed the data folder");
    const cJSON *records = member(reply.json, "records");
    const cJSON *newest = cJSON_GetArrayItem(records, 0);
    const cJSON *older = cJSON_GetArrayItem(records, 1);
    check(num(newest, "seq") > num(older, "seq") && str(newest, "command")[0] && str(newest, "actor")[0] && str(newest, "at")[0] &&
              cJSON_IsArray(member(newest, "writes")) && cJSON_IsArray(member(newest, "removes")) && !member(newest, "headers"),
          "a record is {seq, id, command, actor, at, target, base_version, writes, removes}, newest first, without its headers");
    char target_params[256];
    const char *target = "";
    const cJSON *record = NULL;
    cJSON_ArrayForEach(record, records) {
        if (str(record, "target")[0]) {
            target = str(record, "target");
            break;
        }
    }
    snprintf(target_params, sizeof(target_params), "{\"target\":\"%s\"}", target);
    WorkshopReply filtered;
    check(target[0] && ask("journal.list", target_params, &filtered, &error) && cJSON_GetArraySize(member(filtered.json, "records")) >= 1 &&
              strcmp(str(cJSON_GetArrayItem(member(filtered.json, "records"), 0), "target"), target) == 0,
          "journal.list with a target lists the records about it");
    workshop_reply_destroy(&filtered);
    workshop_reply_destroy(&reply);
    check(ask("journal.list", "{\"limit\":1}", &reply, &error) && cJSON_GetArraySize(member(reply.json, "records")) == 1, "journal.list with a limit stops there");
    workshop_reply_destroy(&reply);
    check(query("journal.list", "{\"limit\":0}", &reply, &error) == URSATZ_ERR_REQUEST && strcmp(error.path, "/params/limit") == 0, "a limit of 0 is refused");
    workshop_reply_destroy(&reply);
}

static void test_composer_queries(void) {
    WorkshopReply reply;
    UrsatzError error;
    check(ask("composer.status", NULL, &reply, &error) && member(reply.json, "available") && strncmp(str(reply.json, "version"), "ursatz-composer ", 16) == 0 &&
              str(reply.json, "producer_key")[0] && str(reply.json, "analyzer_version")[0],
          "composer.status holds the versions this build was made with");
    workshop_reply_destroy(&reply);
    check(ask("composer.vocabulary", NULL, &reply, &error) && cJSON_IsObject(reply.json) && member(reply.json, "limits") != NULL, "composer.vocabulary is the words the Composer reads");
    workshop_reply_destroy(&reply);
    check(ask("composer.forms", NULL, &reply, &error) && cJSON_IsObject(reply.json), "composer.forms is the forms it builds");
    workshop_reply_destroy(&reply);
    check(ask("composer.variant-operations", NULL, &reply, &error) && cJSON_IsObject(reply.json), "composer.variant-operations lists the operations");
    workshop_reply_destroy(&reply);
    check(ask("composer.render", "{\"form\":\"full\",\"degrees\":\"1 2 3 4\",\"meter\":\"4/4\"}", &reply, &error) ||
              query("composer.render", "{\"form\":\"full\",\"degrees\":\"1 2 3 4\",\"meter\":\"4/4\"}", &reply, &error) == URSATZ_ERR_REQUEST,
          "composer.render renders a motif or refuses the request at its member");
    workshop_reply_destroy(&reply);
    check(query("composer.render", "{\"form\":\"nonsense\"}", &reply, &error) == URSATZ_ERR_REQUEST, "composer.render refuses a form that is none");
    workshop_reply_destroy(&reply);
    check(ask("composer.random-motifs", "{\"material\":\"minor\",\"meter\":\"4/4\",\"count\":1,\"seed\":1}", &reply, &error) &&
              cJSON_IsArray(member(reply.json, "motives")),
          "composer.random-motifs draws motives");
    workshop_reply_destroy(&reply);
    check(query("composer.import-motif", NULL, &reply, &error) == URSATZ_ERR_REQUEST && strcmp(error.path, "/bytes") == 0, "composer.import-motif needs a MIDI file");
    workshop_reply_destroy(&reply);
    check(query("composer.recognize", "{\"source\":5}", &reply, &error) == URSATZ_ERR_REQUEST, "composer.recognize refuses a source that is not a reference");
    workshop_reply_destroy(&reply);
}

/* composer.recognize and composer.import-motif answer the members their routes document. The second motif is made with a command, so this is
   after the check that no query changed a file. */
static void test_recognize_and_import(void) {
    WorkshopReply reply;
    UrsatzError error;
    /* composer.recognize of the motif against a second motif answers the members the recognize route documents. */
    char params[3000];
    snprintf(params, sizeof(params),
             "{\"type\":\"motif\",\"name\":\"Other\",\"project_id\":\"%s\",\"body\":{\"uses\":[],\"motif\":{\"role\":\"melodic\",\"form\":\"full\","
             "\"notes\":\"C4:q Eb4:e G4:e C5:h\",\"meter\":\"4/4\",\"key\":\"C minor\",\"pickup\":\"0\",\"features\":[],\"analysis\":null,\"derived\":null}}}",
             g_project);
    check(command("item.create", NULL, params, -1, &reply, &error) == URSATZ_OK, "a second motif is made to recognize against");
    char other[64];
    snprintf(other, sizeof(other), "%s", str(member(reply.json, "item"), "id"));
    workshop_reply_destroy(&reply);
    snprintf(params, sizeof(params), "{\"source\":{\"item\":\"%s\",\"version\":1},\"target\":{\"item\":\"%s\",\"version\":1}}", g_motif, other);
    static const char *const kRecognized[] = {"chain", "changed", "derivation", "distance", "found", "kept", "ok", "remoteness"};
    check(ask("composer.recognize", params, &reply, &error), "composer.recognize answers for two motifs");
    for (size_t i = 0; i < sizeof(kRecognized) / sizeof(kRecognized[0]); i++) {
        check(member(reply.json, kRecognized[i]) != NULL, "composer.recognize answers each member the route documents");
    }
    workshop_reply_destroy(&reply);
    check(command("item.delete", other, "{}", 1, &reply, &error) == URSATZ_OK, "the second motif is deleted again");
    workshop_reply_destroy(&reply);

    /* composer.import-motif reads the MIDI of one voice (hex as the materials route test sends it) into a motif's members. */
    static const char kMidiHex[] = "4d546864000000060000000101e04d54726b0000001600903e648360803e0000904164836080410000ff2f00";
    uint8_t midi[sizeof(kMidiHex) / 2];
    size_t midi_size = 0;
    for (size_t i = 0; kMidiHex[i] && kMidiHex[i + 1]; i += 2) {
        char pair[3] = {kMidiHex[i], kMidiHex[i + 1], 0};
        midi[midi_size++] = (uint8_t)strtoul(pair, NULL, 16);
    }
    WorkshopBytes bytes = {midi, midi_size, "motif.mid"};
    memset(&error, 0, sizeof(error));
    cJSON *import_params = cJSON_Parse("{}");
    UrsatzStatus imported = workshop_query(g_workshop, "composer.import-motif", import_params, &bytes, &reply, &error);
    cJSON_Delete(import_params);
    check(imported == URSATZ_OK, "composer.import-motif reads the MIDI bytes");
    static const char *const kImported[] = {"notes_text", "meter", "key", "key_found", "moved_from", "pickup", "analysis"};
    for (size_t i = 0; imported == URSATZ_OK && i < sizeof(kImported) / sizeof(kImported[0]); i++) {
        check(member(reply.json, kImported[i]) != NULL, "composer.import-motif answers each member the import route documents");
    }
    workshop_reply_destroy(&reply);
}

static void test_item_and_subject_queries(void) {
    WorkshopReply reply;
    UrsatzError error;
    char params[512];
    bool listed = ask("project.list", NULL, &reply, &error);
    bool has_project = false;
    const cJSON *entry = NULL;
    const cJSON *projects = listed ? member(reply.json, "projects") : NULL;
    cJSON_ArrayForEach(entry, projects) {
        has_project = has_project || strcmp(str(entry, "id"), g_project) == 0;
    }
    check(listed && has_project, "project.list lists the projects, the one made among them");
    workshop_reply_destroy(&reply);
    ids(params, sizeof(params), g_project);
    check(ask("project.get", params, &reply, &error) && strcmp(str(member(reply.json, "project"), "name"), "Work") == 0, "project.get is the project");
    workshop_reply_destroy(&reply);
    snprintf(params, sizeof(params), "{\"project\":\"%s\",\"type\":\"theme\"}", g_project);
    check(ask("item.list", params, &reply, &error) && cJSON_GetArraySize(member(reply.json, "items")) == 1, "item.list filters by project and type");
    workshop_reply_destroy(&reply);
    ids(params, sizeof(params), g_theme);
    check(ask("item.get", params, &reply, &error) && num(member(reply.json, "item"), "current_version") == 2 && member(reply.json, "body") && member(reply.json, "uses_status") &&
              member(reply.json, "used_by") && member(reply.json, "subjects"),
          "item.get is the item, its current body, what it uses, what uses it and its subjects");
    workshop_reply_destroy(&reply);
    check(query("item.get", "{\"id\":\"thm-0000000000000000\"}", &reply, &error) == URSATZ_ERR_NOT_FOUND, "an item that is not there is not found");
    workshop_reply_destroy(&reply);
    snprintf(params, sizeof(params), "{\"id\":\"%s\",\"version\":1}", g_theme);
    check(ask("item.version", params, &reply, &error) && num(reply.json, "number") == 1 && member(reply.json, "body"), "item.version is one version's body");
    workshop_reply_destroy(&reply);
    snprintf(params, sizeof(params), "{\"id\":\"%s\",\"version\":\"0\"}", g_theme);
    check(query("item.version", params, &reply, &error) == URSATZ_ERR_NOT_FOUND && strcmp(error.message, "version not found") == 0, "a version that is no number from 1 is not found");
    workshop_reply_destroy(&reply);
    snprintf(params, sizeof(params), "{\"id\":\"%s\",\"version\":2}", g_theme);
    check(ask("item.export", params, &reply, &error) && reply.bytes && strncmp(reply.content_type, "text/plain", 10) == 0, "item.export is the text export of a version");
    workshop_reply_destroy(&reply);
    snprintf(params, sizeof(params), "{\"id\":\"%s\",\"version\":2,\"name\":\"theme.mid\"}", g_theme);
    check(ask("item.file", params, &reply, &error) && reply.bytes && reply.size > 4 && memcmp(reply.bytes, "MThd", 4) == 0 && strcmp(reply.filename, "theme.mid") == 0,
          "item.file is a file of a composed version (the MIDI, named)");
    workshop_reply_destroy(&reply);
    snprintf(params, sizeof(params), "{\"id\":\"%s\",\"version\":2,\"name\":\"nope.bin\"}", g_theme);
    check(query("item.file", params, &reply, &error) == URSATZ_ERR_NOT_FOUND && strcmp(error.message, "file not found") == 0, "a file that is none of a version's is not found");
    workshop_reply_destroy(&reply);
    snprintf(params, sizeof(params), "{\"id\":\"%s\",\"version\":2,\"file\":\"theme\"}", g_theme);
    check(ask("item.playback", params, &reply, &error) && cJSON_IsArray(member(reply.json, "notes")), "item.playback is the playback of the version's own MIDI file");
    workshop_reply_destroy(&reply);
    snprintf(params, sizeof(params), "{\"id\":\"%s\",\"version\":1}", g_motif);
    check(ask("item.score", params, &reply, &error) && reply.bytes && strncmp(reply.content_type, "application/xml", 15) == 0, "item.score is a motif's score (MusicXML)");
    workshop_reply_destroy(&reply);
    snprintf(params, sizeof(params), "{\"id\":\"%s\",\"version\":1,\"set\":1,\"name\":\"variation-1.mid\"}", g_theme);
    check(query("item.variation-file", params, &reply, &error) == URSATZ_ERR_NOT_FOUND && query("item.variation-playback", params, &reply, &error) == URSATZ_ERR_NOT_FOUND,
          "the files of a variations set that is not there are not found");
    workshop_reply_destroy(&reply);
    ids(params, sizeof(params), g_motif);
    check(ask("item.op-checks", params, &reply, &error) && member(reply.json, "checks") != NULL, "item.op-checks holds the marks");
    workshop_reply_destroy(&reply);
    check(ask("item.variant-tree", params, &reply, &error) && strcmp(str(member(reply.json, "tree"), "id"), g_motif) == 0, "item.variant-tree is the item and the motifs derived from it");
    workshop_reply_destroy(&reply);
    snprintf(params, sizeof(params), "{\"type\":\"motif\",\"body\":%s}", kMotifBody);
    check(ask("item.validate", params, &reply, &error) && cJSON_IsTrue(member(reply.json, "complete")) && member(reply.json, "request"), "item.validate answers the request the Composer would read");
    workshop_reply_destroy(&reply);
    check(query("item.validate", "{\"type\":\"chord\",\"body\":{}}", &reply, &error) == URSATZ_ERR_REQUEST, "item.validate refuses a type that is none");
    workshop_reply_destroy(&reply);
    snprintf(params, sizeof(params), "{\"id\":\"%s\",\"chain\":{\"steps\":[{\"op\":\"invert\"}]},\"count\":1,\"seed\":1}", g_motif);
    check(ask("item.variants", params, &reply, &error) && str(member(reply.json, "job"), "kind")[0] && strcmp(str(member(reply.json, "job"), "state"), "queued") == 0,
          "item.variants queues a job that stores nothing");
    cJSON *job = finish_job(&reply);
    check(job && num(job, "http_status") == 200, "and the job finishes");
    cJSON_Delete(job);
    workshop_reply_destroy(&reply);
    ids(params, sizeof(params), g_motif);
    check(ask("item.each-op", params, &reply, &error) && member(reply.json, "job"), "item.each-op queues one too");
    job = finish_job(&reply);
    cJSON_Delete(job);
    workshop_reply_destroy(&reply);

    check(ask("subject.list", NULL, &reply, &error) && cJSON_GetArraySize(member(reply.json, "subjects")) == 1, "subject.list lists the subjects");
    workshop_reply_destroy(&reply);
    check(ask("subject.groups", NULL, &reply, &error) && member(reply.json, "groups") != NULL, "subject.groups groups them by kind");
    workshop_reply_destroy(&reply);
    ids(params, sizeof(params), g_subject);
    check(ask("subject.get", params, &reply, &error) && strcmp(str(member(reply.json, "subject"), "name"), "Anna") == 0, "subject.get is the subject");
    workshop_reply_destroy(&reply);
    check(query("nothing.here", NULL, &reply, &error) == URSATZ_ERR_REQUEST && strcmp(error.path, "/query") == 0, "an unknown query is refused at /query");
    workshop_reply_destroy(&reply);
    check(query("subject.get", "[1]", &reply, &error) == URSATZ_ERR_REQUEST && strcmp(error.path, "/params") == 0, "params that are not an object are refused at /params");
    workshop_reply_destroy(&reply);
}

int main(void) {
    if (!harness_open(18993)) {
        return 1;
    }
    bool ready = setup();
    check(ready, "the folder holds a piece, a collection, a style, a link run, a motif, a composed theme and a subject");
    if (ready) {
        unsigned long long before = fingerprint(".");
        test_library_queries();
        test_collection_style_link_queries();
        test_composer_queries();
        test_item_and_subject_queries();
        check(fingerprint(".") == before, "no query changed a file outside cache/");
        test_recognize_and_import();
    }
    harness_close();
    return test_finish("workshop_queries");
}
