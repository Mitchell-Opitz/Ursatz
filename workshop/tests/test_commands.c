/* Every changing GUI action as a workshop command (R-646): each of the commands runs through workshop_command on a folder of its own, has its
 * effect, and writes its journal record under its name with its actor and target. The long ones answer with a job that is waited for. The four
 * commands of a composed piece (regenerate, variations, choose) and link-form are run end to end by the piece and materials route tests, which
 * the GUI backend now sends through these commands; here they are refused at the member of their params that is wrong. Runs in its own working
 * directory. */

#include "api_harness.h"

#include <ursatz_test.h>

#ifndef URSATZ_WORKSHOP_TEST_LINK_FIXTURES
#error "URSATZ_WORKSHOP_TEST_LINK_FIXTURES must name the link fixtures directory"
#endif

static const char *kMotifBody =
    "{\"uses\":[],\"motif\":{\"role\":\"melodic\",\"form\":\"full\",\"notes\":\"C4:e Eb4:e G4:e Eb4:e C4:e Eb4:e G4:e C5:e\","
    "\"meter\":\"4/4\",\"key\":\"C minor\",\"pickup\":\"0\",\"features\":[],\"analysis\":null,\"derived\":null}}";

/* The commands the journal has records for, with their actors: filled as the test runs and compared at the end. */
static cJSON *g_seen;

/* Runs the command and expects OK; the reply is the caller's. When journaled, the last record is the command's (name, actor "test", target). */
static bool run(const char *name, const char *target, const char *params, long base, WorkshopReply *reply) {
    UrsatzError error;
    UrsatzStatus status = command(name, target, params, base, reply, &error);
    if (status != URSATZ_OK) {
        fprintf(stderr, "%s failed: %s (%s)\n", name, error.message, error.path);
        return false;
    }
    return true;
}

/* Waits for the job of a reply and says whether it was done with a 200 (the job's result is the caller's). */
static bool run_job(const char *name, const char *target, const char *params, long base, const WorkshopBytes *bytes, cJSON **result) {
    WorkshopReply reply;
    UrsatzError error;
    UrsatzStatus status = command_as("test", name, target, params, base, bytes, &reply, &error);
    cJSON *job = status == URSATZ_OK ? finish_job(&reply) : NULL;
    bool done = job && strcmp(str(job, "state"), "done") == 0 && num(job, "http_status") == 200;
    if (!done) {
        fprintf(stderr, "%s: %s%s\n", name, status == URSATZ_OK ? "the job did not finish well" : error.message,
                job ? str(member(job, "result"), "error") : "");
    }
    if (result) {
        *result = job ? cJSON_DetachItemFromObjectCaseSensitive(job, "result") : NULL;
    }
    cJSON_Delete(job);
    workshop_reply_destroy(&reply);
    return done;
}

/* Whether the journal holds a record of command by the actor "test" about target (any target when NULL); the name goes into g_seen. */
static bool journaled(const char *name, const char *target) {
    cJSON *records = journal_records();
    bool found = false;
    const cJSON *record = NULL;
    cJSON_ArrayForEach(record, records) {
        if (strcmp(str(record, "command"), name) == 0 && strcmp(str(record, "actor"), "test") == 0 &&
            (!target || strcmp(str(record, "target"), target) == 0)) {
            found = true;
        }
    }
    cJSON_Delete(records);
    if (found && !member(g_seen, name)) {
        cJSON_AddTrueToObject(g_seen, name);
    }
    return found;
}

static char g_project[64], g_motif[64], g_variant[64], g_promoted[64], g_theme[64], g_subject[64], g_other_subject[64];
static char g_piece_a[64], g_piece_b[64], g_collection[64], g_style[64], g_run[64];

static bool read_fixture(const char *name, char **data, size_t *size) {
    char path[600];
    snprintf(path, sizeof(path), "%s/%s", URSATZ_WORKSHOP_TEST_LINK_FIXTURES, name);
    *data = test_read_file(path, size);
    return *data != NULL;
}

static int version_of(const char *id) {
    WorkshopReply reply;
    UrsatzError error;
    char params[96];
    snprintf(params, sizeof(params), "{\"id\":\"%s\"}", id);
    int version = query("item.get", params, &reply, &error) == URSATZ_OK ? (int)num(member(reply.json, "item"), "current_version") : -1;
    workshop_reply_destroy(&reply);
    return version;
}

static void test_library(void) {
    char *data = NULL;
    size_t size = 0;
    cJSON *result = NULL;
    bool read = read_fixture("a.musicxml", &data, &size);
    WorkshopBytes file = {(const uint8_t *)data, size, "a.musicxml"};
    bool imported = read && run_job("library.import", NULL, "{\"filename\":\"a.musicxml\"}", -1, &file, &result);
    check(imported && str(result, "id")[0], "library.import imports the file as a piece (the job answers its id)");
    snprintf(g_piece_a, sizeof(g_piece_a), "%s", str(result, "id"));
    cJSON_Delete(result);
    free(data);
    check(journaled("library.import", NULL), "the import is journaled as library.import by its actor");
    read = read_fixture("b.musicxml", &data, &size);
    WorkshopBytes second = {(const uint8_t *)data, size, "b.musicxml"};
    imported = read && run_job("library.import", NULL, "{\"filename\":\"b.musicxml\"}", -1, &second, &result);
    snprintf(g_piece_b, sizeof(g_piece_b), "%s", str(result, "id"));
    cJSON_Delete(result);
    free(data);
    check(imported, "a second piece is imported");

    WorkshopReply reply;
    UrsatzError error;
    check(run("library.update", g_piece_a, "{\"composer\":\"Tester\",\"tags\":[\"x\"]}", -1, &reply) &&
              strcmp(str(member(reply.json, "piece"), "composer"), "Tester") == 0 && journaled("library.update", g_piece_a),
          "library.update changes the piece's metadata and is journaled");
    workshop_reply_destroy(&reply);
    check(command("library.update", g_piece_a, "{\"filename\":\"\"}", -1, &reply, &error) == URSATZ_ERR_REQUEST && strcmp(error.path, "filename") == 0,
          "library.update refuses a filename that is not one");
    workshop_reply_destroy(&reply);
    check(run_job("library.reanalyze", g_piece_a, NULL, -1, NULL, NULL) && journaled("library.reanalyze", g_piece_a),
          "library.reanalyze analyzes the stored source again (a job) and is journaled");
}

static void test_collections_and_links(void) {
    WorkshopReply reply;
    UrsatzError error;
    check(run("collection.create", NULL, "{\"name\":\"Set\"}", -1, &reply) && journaled("collection.create", NULL), "collection.create makes a collection");
    snprintf(g_collection, sizeof(g_collection), "%s", str(reply.json, "id"));
    workshop_reply_destroy(&reply);
    check(run("collection.update", g_collection, "{\"name\":\"Renamed set\"}", -1, &reply) && strcmp(str(reply.json, "name"), "Renamed set") == 0 &&
              journaled("collection.update", g_collection),
          "collection.update renames it");
    workshop_reply_destroy(&reply);
    char params[300];
    snprintf(params, sizeof(params), "{\"piece_ids\":[\"%s\",\"%s\"]}", g_piece_a, g_piece_b);
    check(run("collection.add-pieces", g_collection, params, -1, &reply) && num(reply.json, "added") == 2 && journaled("collection.add-pieces", g_collection),
          "collection.add-pieces adds both pieces");
    workshop_reply_destroy(&reply);
    check(run("collection.add-pieces", g_collection, params, -1, &reply) && num(reply.json, "already") == 2,
          "adding them again counts them as already in");
    workshop_reply_destroy(&reply);

    check(run("style.build", NULL, "{\"filter\":{\"tag\":\"x\"}}", -1, &reply), "style.build is accepted as a job");
    cJSON *job = finish_job(&reply);
    const cJSON *style = member(member(job, "result"), "style");
    snprintf(g_style, sizeof(g_style), "%s", str(style, "id"));
    check(job && num(job, "http_status") == 200 && g_style[0] && journaled("style.build", NULL), "the style is built from the piece that has the tag and journaled");
    cJSON_Delete(job);
    workshop_reply_destroy(&reply);
    check(g_style[0] && run_job("style.rebuild", g_style, NULL, -1, NULL, NULL) && journaled("style.rebuild", g_style), "style.rebuild builds it again from its stored source");

    check(run("links.run", NULL, "{\"scope\":{\"kind\":\"collection\",\"collection_id\":\"x\"}}", -1, &reply) == false, "links.run refuses a collection that is not there");
    workshop_reply_destroy(&reply);
    snprintf(params, sizeof(params), "{\"scope\":{\"kind\":\"collection\",\"collection_id\":\"%s\"}}", g_collection);
    check(run("links.run", NULL, params, -1, &reply) && str(member(reply.json, "run"), "id")[0], "links.run queues a run over the collection (the reply holds the run and its job)");
    snprintf(g_run, sizeof(g_run), "%s", str(member(reply.json, "run"), "id"));
    job = finish_job(&reply);
    check(job && num(job, "http_status") == 200 && journaled("links.run", g_run), "the run finishes and its records are named by the command");
    cJSON_Delete(job);
    workshop_reply_destroy(&reply);
    check(run("links.delete", g_run, NULL, -1, &reply) && journaled("links.delete", g_run), "links.delete removes the run");
    workshop_reply_destroy(&reply);
    check(run("style.delete", g_style, NULL, -1, &reply) && journaled("style.delete", g_style), "style.delete removes the style");
    workshop_reply_destroy(&reply);

    check(run("collection.remove-piece", g_collection, "{\"piece_id\":\"" "x\"}", -1, &reply) == false, "collection.remove-piece refuses a piece that is not in it");
    workshop_reply_destroy(&reply);
    snprintf(params, sizeof(params), "{\"piece_id\":\"%s\"}", g_piece_b);
    check(run("collection.remove-piece", g_collection, params, -1, &reply) && journaled("collection.remove-piece", g_collection), "collection.remove-piece takes one out");
    workshop_reply_destroy(&reply);
    check(run("collection.delete", g_collection, NULL, -1, &reply) && journaled("collection.delete", g_collection), "collection.delete removes the collection");
    workshop_reply_destroy(&reply);
    check(command("job.cancel", "job-0000000000000000", NULL, -1, &reply, &error) == URSATZ_ERR_NOT_FOUND, "job.cancel of a job that is not there is not found");
    workshop_reply_destroy(&reply);
}

static void test_projects_and_items(void) {
    WorkshopReply reply;
    UrsatzError error;
    check(run("project.create", NULL, "{\"name\":\"Work\"}", -1, &reply) && journaled("project.create", NULL), "project.create makes a project");
    snprintf(g_project, sizeof(g_project), "%s", str(member(reply.json, "project"), "id"));
    workshop_reply_destroy(&reply);
    check(run("project.update", g_project, "{\"description\":\"work\"}", -1, &reply) && journaled("project.update", g_project), "project.update changes it");
    workshop_reply_destroy(&reply);

    char params[3000];
    snprintf(params, sizeof(params), "{\"type\":\"motif\",\"name\":\"Source\",\"project_id\":\"%s\",\"body\":%s}", g_project, kMotifBody);
    check(run("item.create", NULL, params, -1, &reply) && journaled("item.create", NULL), "item.create makes a motif (its body is analyzed)");
    snprintf(g_motif, sizeof(g_motif), "%s", str(member(reply.json, "item"), "id"));
    workshop_reply_destroy(&reply);
    check(run("item.update", g_motif, "{\"name\":\"Source renamed\"}", 1, &reply) && journaled("item.update", g_motif), "item.update renames it");
    workshop_reply_destroy(&reply);
    snprintf(params, sizeof(params),
             "{\"note\":\"longer\",\"body\":{\"uses\":[],\"motif\":{\"role\":\"melodic\",\"form\":\"full\",\"notes\":\"C4:q Eb4:e G4:e C5:h\",\"meter\":\"4/4\","
             "\"key\":\"C minor\",\"pickup\":\"0\",\"features\":[],\"analysis\":null,\"derived\":null}}}");
    check(run("item.save", g_motif, params, 1, &reply) && version_of(g_motif) == 2 && journaled("item.save", g_motif), "item.save writes the next version");
    workshop_reply_destroy(&reply);
    check(run("item.reanalyze", g_motif, NULL, 2, &reply) && version_of(g_motif) == 3 && journaled("item.reanalyze", g_motif), "item.reanalyze saves the analysis again as a version");
    workshop_reply_destroy(&reply);
    check(run("item.set-op-checks", g_motif, "{\"checks\":{}}", -1, &reply) && journaled("item.set-op-checks", g_motif), "item.set-op-checks replaces the marks");
    workshop_reply_destroy(&reply);

    /* A candidate from the Composer is saved as a variant, flagged, rerolled and promoted. */
    WorkshopReply candidates;
    UrsatzError candidate_error;
    snprintf(params, sizeof(params), "{\"id\":\"%s\",\"chain\":{\"steps\":[{\"op\":\"invert\"}]},\"count\":1,\"seed\":1}", g_motif);
    cJSON *query_params = cJSON_Parse(params);
    UrsatzStatus asked = workshop_query(g_workshop, "item.variants", query_params, NULL, &candidates, &candidate_error);
    cJSON_Delete(query_params);
    cJSON *job = asked == URSATZ_OK ? finish_job(&candidates) : NULL;
    const cJSON *candidate = cJSON_GetArrayItem(member(member(job, "result"), "variants"), 0);
    check(candidate != NULL, "the Composer's variants of the motif are asked for (a query)");
    cJSON *variant_params = cJSON_CreateObject();
    cJSON_AddItemToObject(variant_params, "variant", candidate ? cJSON_Duplicate(candidate, true) : cJSON_CreateNull());
    cJSON_AddStringToObject(variant_params, "flag", "approved");
    cJSON_AddStringToObject(variant_params, "name", "Source inverted");
    char *variant_text = cJSON_PrintUnformatted(variant_params);
    cJSON_Delete(variant_params);
    cJSON_Delete(job);
    workshop_reply_destroy(&candidates);
    check(run("item.save-variant", g_motif, variant_text, -1, &reply) && journaled("item.save-variant", NULL), "item.save-variant keeps the candidate as a variant of the motif");
    free(variant_text);
    snprintf(g_variant, sizeof(g_variant), "%s", str(member(reply.json, "item"), "id"));
    workshop_reply_destroy(&reply);
    int variant_version = version_of(g_variant);
    check(run("item.flag", g_variant, "{\"flag\":\"rejected\"}", variant_version, &reply) && journaled("item.flag", g_variant), "item.flag marks the variant");
    workshop_reply_destroy(&reply);
    variant_version = version_of(g_variant);
    check(run_job("item.reroll", g_variant, NULL, variant_version, NULL, NULL) && journaled("item.reroll", g_variant), "item.reroll runs its chain again with a new seed (a job)");
    variant_version = version_of(g_variant);
    snprintf(params, sizeof(params), "{\"name\":\"Source's own\",\"project_id\":\"%s\"}", g_project);
    check(run("item.promote", g_variant, params, variant_version, &reply) && journaled("item.promote", g_variant), "item.promote makes the variant a motif of its own");
    snprintf(g_promoted, sizeof(g_promoted), "%s", str(member(reply.json, "item"), "id"));
    workshop_reply_destroy(&reply);

    check(command("item.link-form", g_promoted, "{\"source\":5}", version_of(g_promoted), &reply, &error) == URSATZ_ERR_REQUEST,
          "item.link-form refuses a source that is not a reference");
    workshop_reply_destroy(&reply);
    check(command("item.regenerate", g_motif, "{\"version\":1}", version_of(g_motif), &reply, &error) == URSATZ_ERR_REQUEST && error.path[0] == '/',
          "item.regenerate refuses a request without its section");
    workshop_reply_destroy(&reply);
    check(command("item.variations", g_motif, "{\"version\":1,\"section_id\":\"a\",\"lane\":\"melody\",\"count\":9}", -1, &reply, &error) == URSATZ_ERR_REQUEST,
          "item.variations refuses a count of 9");
    workshop_reply_destroy(&reply);
    check(command("item.choose", g_motif, "{\"version\":1,\"set\":1,\"index\":0}", version_of(g_motif), &reply, &error) == URSATZ_ERR_REQUEST,
          "item.choose refuses an index of 0");
    workshop_reply_destroy(&reply);

    /* A theme that uses the motif, composed after the motif got a new version. */
    char body[1500];
    snprintf(body, sizeof(body),
             "{\"uses\":[],\"theme\":{\"form\":{\"into\":\"sentence\",\"hybrid\":null,\"mode\":\"minor\",\"meter\":\"4/4\",\"bars\":8,\"cadence\":\"PAC\","
             "\"first_cadence\":null},\"slots\":[{\"slot\":\"basic\",\"item_id\":\"%s\",\"version\":1},{\"slot\":\"cadential\",\"item_id\":\"%s\",\"version\":1}]}}",
             g_motif, g_motif);
    snprintf(params, sizeof(params), "{\"type\":\"theme\",\"name\":\"Theme\",\"project_id\":\"%s\",\"body\":%s}", g_project, body);
    check(run("item.create", NULL, params, -1, &reply), "item.create makes a theme that uses the motif");
    snprintf(g_theme, sizeof(g_theme), "%s", str(member(reply.json, "item"), "id"));
    workshop_reply_destroy(&reply);
    check(run_job("item.compose", g_theme, "{\"seed\":2}", 1, NULL, NULL) && journaled("item.compose", g_theme), "item.compose composes the theme (a job)");
    check(run("item.update-use", g_theme, NULL, -1, &reply) == false, "item.update-use needs a base version and the source");
    workshop_reply_destroy(&reply);
    snprintf(params, sizeof(params), "{\"source_id\":\"%s\"}", g_motif);
    check(run("item.update-use", g_theme, params, version_of(g_theme), &reply) && journaled("item.update-use", g_theme), "item.update-use moves the use to the source's current version");
    workshop_reply_destroy(&reply);
    check(command("item.delete", g_motif, NULL, version_of(g_motif), &reply, &error) == URSATZ_ERR_CONFLICT && member(reply.json, "used_by"),
          "item.delete of a motif a theme uses is refused and lists what uses it");
    workshop_reply_destroy(&reply);
    check(run("item.delete", g_motif, "{\"force\":true}", version_of(g_motif), &reply) && journaled("item.delete", g_motif), "item.delete with force removes it and its variants");
    workshop_reply_destroy(&reply);
    check(run("project.delete", g_project, NULL, -1, &reply) == false, "project.delete of a project that still has items is refused");
    workshop_reply_destroy(&reply);
}

static void test_subjects(void) {
    WorkshopReply reply;
    check(run("subject.create", NULL, "{\"name\":\"Anna\",\"kind\":\"character\"}", -1, &reply) && journaled("subject.create", NULL), "subject.create makes a subject");
    snprintf(g_subject, sizeof(g_subject), "%s", str(member(reply.json, "subject"), "id"));
    workshop_reply_destroy(&reply);
    check(run("subject.create", NULL, "{\"name\":\"Tower\",\"kind\":\"place\"}", -1, &reply), "and a second one");
    snprintf(g_other_subject, sizeof(g_other_subject), "%s", str(member(reply.json, "subject"), "id"));
    workshop_reply_destroy(&reply);
    check(run("subject.update", g_subject, "{\"note\":\"a note\"}", -1, &reply) && journaled("subject.update", g_subject), "subject.update changes it");
    workshop_reply_destroy(&reply);
    char params[200];
    snprintf(params, sizeof(params), "{\"piece_id\":\"%s\"}", g_piece_a);
    check(run("subject.link-piece", g_subject, params, -1, &reply) && journaled("subject.link-piece", g_subject), "subject.link-piece links a piece");
    workshop_reply_destroy(&reply);
    check(run("subject.unlink-piece", g_subject, params, -1, &reply) && journaled("subject.unlink-piece", g_subject), "subject.unlink-piece takes it away");
    workshop_reply_destroy(&reply);
    snprintf(params, sizeof(params), "{\"item_id\":\"%s\"}", g_theme);
    check(run("subject.link-item", g_subject, params, -1, &reply) && journaled("subject.link-item", g_subject), "subject.link-item links an item");
    workshop_reply_destroy(&reply);
    check(run("subject.unlink-item", g_subject, params, -1, &reply) && journaled("subject.unlink-item", g_subject), "subject.unlink-item takes it away");
    workshop_reply_destroy(&reply);
    snprintf(params, sizeof(params), "{\"other_id\":\"%s\"}", g_other_subject);
    check(run("subject.relate", g_subject, params, -1, &reply) && journaled("subject.relate", g_subject), "subject.relate relates two subjects");
    workshop_reply_destroy(&reply);
    check(run("subject.unrelate", g_subject, params, -1, &reply) && journaled("subject.unrelate", g_subject), "subject.unrelate ends it");
    workshop_reply_destroy(&reply);
    check(run("subject.delete", g_other_subject, NULL, -1, &reply) && journaled("subject.delete", g_other_subject), "subject.delete removes a subject");
    workshop_reply_destroy(&reply);
    check(run("library.delete", g_piece_b, NULL, -1, &reply) && journaled("library.delete", g_piece_b), "library.delete removes a piece");
    workshop_reply_destroy(&reply);
}

/* Every command that writes a record has one by its name and its actor. */
static void test_every_command_is_journaled(void) {
    static const char *const kJournaled[] = {
        "library.import", "library.update", "library.delete", "library.reanalyze", "collection.create", "collection.update", "collection.delete",
        "collection.add-pieces", "collection.remove-piece", "style.build", "style.rebuild", "style.delete", "links.run", "links.delete", "project.create",
        "project.update", "item.create", "item.update", "item.delete", "item.save", "item.reanalyze", "item.compose", "item.set-op-checks",
        "item.save-variant", "item.flag", "item.promote", "item.reroll", "item.update-use", "subject.create", "subject.update", "subject.delete",
        "subject.link-item", "subject.unlink-item", "subject.link-piece", "subject.unlink-piece", "subject.relate", "subject.unrelate"};
    int missing = 0;
    for (size_t i = 0; i < sizeof(kJournaled) / sizeof(kJournaled[0]); i++) {
        if (!member(g_seen, kJournaled[i])) {
            fprintf(stderr, "no record of %s\n", kJournaled[i]);
            missing++;
        }
    }
    check(missing == 0, "every command that changes the data folder wrote a record under its name with its actor");
}

int main(void) {
    if (!harness_open(18992)) {
        return 1;
    }
    g_seen = cJSON_CreateObject();
    test_library();
    test_projects_and_items();
    test_collections_and_links();
    test_subjects();
    test_every_command_is_journaled();
    cJSON_Delete(g_seen);
    harness_close();
    return test_finish("workshop_commands");
}
