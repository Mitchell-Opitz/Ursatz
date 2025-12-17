/* Link service: each scope kind resolves the expected pieces (including a project through its subjects), a run links
 * the four fixture pieces, a second run over the same scope compares nothing new because of the shared cache, a
 * document without features is analyzed again, a piece that is gone is skipped, and a run left running by an earlier
 * process reads failed. Runs in its own working directory (pieces/, collections/, data/, links/ and the material and
 * subject stores are created there). */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#define MKDIR(path) _mkdir(path)
#else
#include <sys/stat.h>
#include <unistd.h>
#define MKDIR(path) mkdir(path, 0755)
#endif

#include <cJSON.h>

#include "service/analysis_service.h"
#include "service/collection_service.h"
#include "service/id_service.h"
#include "service/link_service.h"
#include "service/material_service.h"
#include "service/piece_metadata.h"
#include "service/subject_service.h"

#ifndef URSATZ_GUI_TEST_LINK_FIXTURES
#error "URSATZ_GUI_TEST_LINK_FIXTURES must name the link fixtures directory"
#endif

static int failures = 0;

#define CHECK(cond, msg)                                            \
    do {                                                            \
        if (!(cond)) {                                              \
            fprintf(stderr, "FAIL: %s (line %d)\n", msg, __LINE__); \
            failures++;                                             \
        }                                                           \
    } while (0)

static char g_materials[64];
static char g_subjects[64];
static char g_ids[5][64]; /* a, b, c, d, and a copy of c that is deleted later */

static void sleep_ms(int ms) {
#ifdef _WIN32
    Sleep((DWORD)ms);
#else
    struct timespec ts = {0, (long)ms * 1000 * 1000};
    nanosleep(&ts, NULL);
#endif
}

static const char *str(const cJSON *object, const char *name) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);
    return cJSON_IsString(item) ? item->valuestring : "";
}

static int num(const cJSON *object, const char *name) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);
    return cJSON_IsNumber(item) ? item->valueint : -1;
}

static unsigned char *read_file(const char *path, size_t *size) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        return NULL;
    }
    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);
    unsigned char *data = malloc((size_t)length + 1);
    if (data && fread(data, 1, (size_t)length, file) != (size_t)length) {
        free(data);
        data = NULL;
    }
    fclose(file);
    *size = (size_t)length;
    return data;
}

static bool import_piece(const char *name, char *out_id, size_t out_size) {
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", URSATZ_GUI_TEST_LINK_FIXTURES, name);
    size_t size = 0;
    unsigned char *data = read_file(path, &size);
    char *id = NULL;
    bool ok = data && ursatz_gui_analysis_import_and_save("pieces", data, size, name, &id) == URSATZ_GUI_ANALYSIS_OK;
    free(data);
    if (ok) {
        snprintf(out_id, out_size, "%s", id);
    }
    free(id);
    return ok;
}

static int compare_text(const void *left, const void *right) {
    return strcmp(*(char *const *)left, *(char *const *)right);
}

/* Resolves the scope and checks it names exactly the given piece ids (any order). */
static void expect_scope(const UrsatzGuiLinkScope *scope, const char *const *expected, size_t count, const char *what) {
    char **ids = NULL;
    size_t found = 0;
    UrsatzGuiLinkResult result = ursatz_gui_link_scope_resolve(scope, &ids, &found);
    CHECK(result == URSATZ_GUI_LINK_OK, what);
    if (result != URSATZ_GUI_LINK_OK) {
        return;
    }
    CHECK(found == count, what);
    char *want[8];
    for (size_t i = 0; i < count && i < 8; i++) {
        want[i] = (char *)expected[i];
    }
    qsort(want, count, sizeof(*want), compare_text);
    for (size_t i = 0; i < found && i < count; i++) {
        CHECK(strcmp(ids[i], want[i]) == 0, what);
    }
    ursatz_gui_link_ids_free(ids, found);
}

static void expect_error(const UrsatzGuiLinkScope *scope, UrsatzGuiLinkResult error, const char *what) {
    char **ids = NULL;
    size_t found = 0;
    CHECK(ursatz_gui_link_scope_resolve(scope, &ids, &found) == error && ids == NULL && found == 0, what);
}

static void tag_piece(const char *id, const char *composer, const char *tag) {
    const char *tags[] = {tag};
    UrsatzGuiMetaUpdate update;
    memset(&update, 0, sizeof(update));
    update.composer = composer;
    update.add_tags = tag ? tags : NULL;
    update.add_tag_count = tag ? 1 : 0;
    char message[128];
    CHECK(ursatz_gui_analysis_update_metadata("pieces", id, &update, message, sizeof(message)) ==
              URSATZ_GUI_ANALYSIS_OK,
          "update metadata");
}

static void test_scopes(void) {
    tag_piece(g_ids[0], "Anna", "major");
    tag_piece(g_ids[1], "Anna", "major");
    tag_piece(g_ids[2], "Zed", "minor");

    UrsatzGuiLinkScope scope;
    memset(&scope, 0, sizeof(scope));
    scope.kind = URSATZ_GUI_LINK_SCOPE_ALL;
    expect_scope(&scope, (const char *const[]){g_ids[0], g_ids[1], g_ids[2], g_ids[3], g_ids[4]}, 5, "all pieces");

    /* A collection. */
    char *collection = NULL;
    CHECK(ursatz_gui_collection_create("Pair", "", &collection) == URSATZ_GUI_COLLECTION_OK, "create collection");
    const char *members[] = {g_ids[1], g_ids[0]};
    size_t added = 0;
    size_t already = 0;
    CHECK(ursatz_gui_collection_add_pieces(collection, members, 2, &added, &already) == URSATZ_GUI_COLLECTION_OK &&
              added == 2,
          "fill collection");
    memset(&scope, 0, sizeof(scope));
    scope.kind = URSATZ_GUI_LINK_SCOPE_COLLECTION;
    scope.collection_id = collection;
    expect_scope(&scope, members, 2, "collection pieces");
    scope.collection_id = "col-missing";
    expect_error(&scope, URSATZ_GUI_LINK_ERR_NOT_FOUND, "unknown collection");
    scope.collection_id = NULL;
    expect_error(&scope, URSATZ_GUI_LINK_ERR_INVALID_SCOPE, "collection without an id");
    char *empty_collection = NULL;
    CHECK(ursatz_gui_collection_create("Nothing", "", &empty_collection) == URSATZ_GUI_COLLECTION_OK, "empty one");
    scope.collection_id = empty_collection;
    expect_error(&scope, URSATZ_GUI_LINK_ERR_EMPTY, "empty collection");
    free(empty_collection);
    free(collection);

    /* A filter, as the Library filters. */
    memset(&scope, 0, sizeof(scope));
    scope.kind = URSATZ_GUI_LINK_SCOPE_FILTER;
    scope.composer = "Anna";
    expect_scope(&scope, (const char *const[]){g_ids[0], g_ids[1]}, 2, "filter by composer");
    scope.composer = NULL;
    scope.tag = "minor";
    expect_scope(&scope, (const char *const[]){g_ids[2]}, 1, "filter by tag");
    scope.tag = "";
    expect_error(&scope, URSATZ_GUI_LINK_ERR_INVALID_SCOPE, "filter without a value");
    scope.composer = "Nobody";
    expect_error(&scope, URSATZ_GUI_LINK_ERR_EMPTY, "filter that matches nothing");

    /* A subject, and a project through its subjects. */
    cJSON *anna = NULL;
    cJSON *zed = NULL;
    cJSON *other = NULL;
    CHECK(ursatz_gui_subject_create("Anna", "character", "", &anna) == URSATZ_GUI_SUBJECT_OK, "subject anna");
    CHECK(ursatz_gui_subject_create("Zed", "place", "", &zed) == URSATZ_GUI_SUBJECT_OK, "subject zed");
    CHECK(ursatz_gui_subject_create("Other", "idea", "", &other) == URSATZ_GUI_SUBJECT_OK, "subject other");
    const char *anna_id = str(anna, "id");
    const char *zed_id = str(zed, "id");
    const char *other_id = str(other, "id");
    CHECK(ursatz_gui_subject_link_piece(anna_id, g_ids[0], NULL, NULL) == URSATZ_GUI_SUBJECT_OK &&
              ursatz_gui_subject_link_piece(anna_id, g_ids[3], NULL, NULL) == URSATZ_GUI_SUBJECT_OK &&
              ursatz_gui_subject_link_piece(zed_id, g_ids[1], NULL, NULL) == URSATZ_GUI_SUBJECT_OK &&
              ursatz_gui_subject_link_piece(zed_id, g_ids[3], NULL, NULL) == URSATZ_GUI_SUBJECT_OK &&
              ursatz_gui_subject_link_piece(other_id, g_ids[2], NULL, NULL) == URSATZ_GUI_SUBJECT_OK,
          "link pieces to subjects");

    memset(&scope, 0, sizeof(scope));
    scope.kind = URSATZ_GUI_LINK_SCOPE_SUBJECT;
    scope.subject_id = anna_id;
    expect_scope(&scope, (const char *const[]){g_ids[0], g_ids[3]}, 2, "subject pieces");
    scope.subject_id = "sub-missing";
    expect_error(&scope, URSATZ_GUI_LINK_ERR_NOT_FOUND, "unknown subject");
    scope.subject_id = NULL;
    expect_error(&scope, URSATZ_GUI_LINK_ERR_INVALID_SCOPE, "subject without an id");

    cJSON *project = NULL;
    cJSON *quiet = NULL;
    CHECK(ursatz_gui_material_project_create("Score", "", &project) == URSATZ_GUI_MATERIAL_OK, "project");
    CHECK(ursatz_gui_material_project_create("Quiet", "", &quiet) == URSATZ_GUI_MATERIAL_OK, "quiet project");
    const char *project_id = str(project, "id");
    cJSON *body_a = cJSON_Parse("{\"uses\":[],\"piece\":{\"structure\":null}}");
    cJSON *body_b = cJSON_Parse("{\"uses\":[],\"piece\":{\"structure\":null}}");
    cJSON *item_a = NULL;
    cJSON *item_b = NULL;
    CHECK(ursatz_gui_material_item_create("piece", "First", project_id, body_a, &item_a) == URSATZ_GUI_MATERIAL_OK &&
              ursatz_gui_material_item_create("piece", "Second", project_id, body_b, &item_b) ==
                  URSATZ_GUI_MATERIAL_OK,
          "items");
    CHECK(ursatz_gui_subject_link_item(anna_id, str(item_a, "id"), NULL, NULL) == URSATZ_GUI_SUBJECT_OK &&
              ursatz_gui_subject_link_item(zed_id, str(item_b, "id"), NULL, NULL) == URSATZ_GUI_SUBJECT_OK,
          "link items to subjects");

    /* Anna and Zed link items of the project; Other does not, so its piece stays out. */
    memset(&scope, 0, sizeof(scope));
    scope.kind = URSATZ_GUI_LINK_SCOPE_PROJECT;
    scope.project_id = project_id;
    expect_scope(&scope, (const char *const[]){g_ids[0], g_ids[1], g_ids[3]}, 3, "project pieces through subjects");
    scope.project_id = str(quiet, "id");
    expect_error(&scope, URSATZ_GUI_LINK_ERR_EMPTY, "project without linked items");
    scope.project_id = "prj-missing";
    expect_error(&scope, URSATZ_GUI_LINK_ERR_NOT_FOUND, "unknown project");
    scope.project_id = NULL;
    expect_error(&scope, URSATZ_GUI_LINK_ERR_INVALID_SCOPE, "project without an id");

    cJSON_Delete(body_a);
    cJSON_Delete(body_b);
    cJSON_Delete(item_a);
    cJSON_Delete(item_b);
    cJSON_Delete(project);
    cJSON_Delete(quiet);
    cJSON_Delete(anna);
    cJSON_Delete(zed);
    cJSON_Delete(other);
}

/* Starts a run and waits for it to end. Returns the run (caller frees), or NULL. */
static cJSON *run_and_wait(const UrsatzGuiLinkScope *scope, char *out_id, size_t out_size) {
    char *id = NULL;
    UrsatzGuiLinkResult started = ursatz_gui_link_run_start(scope, &id);
    CHECK(started == URSATZ_GUI_LINK_OK && id, "run starts");
    if (started != URSATZ_GUI_LINK_OK || !id) {
        return NULL;
    }
    snprintf(out_id, out_size, "%s", id);
    cJSON *run = NULL;
    for (int waited = 0; waited < 600000; waited += 50) {
        cJSON_Delete(run);
        run = NULL;
        if (ursatz_gui_link_run_get(id, &run) != URSATZ_GUI_LINK_OK) {
            break;
        }
        if (strcmp(str(run, "status"), "running") != 0) {
            break;
        }
        sleep_ms(50);
    }
    free(id);
    return run;
}

static int result_number(const cJSON *run, const char *name) {
    return num(cJSON_GetObjectItemCaseSensitive(run, "result"), name);
}

static bool link_between(const cJSON *run, const char *first, const char *second) {
    const cJSON *link = NULL;
    cJSON_ArrayForEach(link, cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(run, "result"), "links")) {
        const char *a = str(cJSON_GetObjectItemCaseSensitive(link, "a"), "piece");
        const char *b = str(cJSON_GetObjectItemCaseSensitive(link, "b"), "piece");
        if ((strcmp(a, first) == 0 && strcmp(b, second) == 0) || (strcmp(a, second) == 0 && strcmp(b, first) == 0)) {
            return true;
        }
    }
    return false;
}

/* Removes every "features" member under value. */
static void strip_features(cJSON *value) {
    if (cJSON_IsObject(value)) {
        cJSON_DeleteItemFromObjectCaseSensitive(value, "features");
    }
    const cJSON *child = NULL;
    cJSON_ArrayForEach(child, value) {
        strip_features((cJSON *)child);
    }
}

static void test_runs(void) {
    /* The fifth piece is gone but a collection still names it: a run skips it. */
    char *five_collection = NULL;
    CHECK(ursatz_gui_collection_create("Five", "", &five_collection) == URSATZ_GUI_COLLECTION_OK, "five collection");
    size_t added = 0;
    size_t already = 0;
    CHECK(ursatz_gui_collection_add_pieces(five_collection, (const char *const[]){g_ids[0], g_ids[1], g_ids[2], g_ids[3], g_ids[4]}, 5,
                                           &added, &already) == URSATZ_GUI_COLLECTION_OK &&
              added == 5,
          "fill the five collection");
    UrsatzGuiLinkScope all;
    memset(&all, 0, sizeof(all));
    all.kind = URSATZ_GUI_LINK_SCOPE_COLLECTION;
    all.collection_id = five_collection;

    char gone_dir[128];
    snprintf(gone_dir, sizeof(gone_dir), "pieces/%s", g_ids[4]);
#ifdef _WIN32
    char command[256];
    snprintf(command, sizeof(command), "rmdir /s /q %s", gone_dir);
    system(command);
#else
    char command[256];
    snprintf(command, sizeof(command), "rm -rf %s", gone_dir);
    system(command);
#endif

    /* The first run: the four pieces, every pair compared. */
    char first_id[64] = "";
    cJSON *run = run_and_wait(&all, first_id, sizeof(first_id));
    CHECK(run && strcmp(str(run, "status"), "done") == 0, "the first run is done");
    CHECK(run && strncmp(str(run, "id"), "lnk-", 4) == 0, "run id prefix");
    CHECK(result_number(run, "pieces") == 4, "four pieces were linked");
    CHECK(result_number(run, "pairs") > 0 && result_number(run, "compared") == result_number(run, "pairs"),
          "the first run compares every pair");
    CHECK(result_number(run, "cached") == 0, "the first run has nothing cached");
    CHECK(num(run, "done") == result_number(run, "pairs") && num(run, "total") == result_number(run, "pairs"),
          "done and total are the pairs");
    CHECK(link_between(run, g_ids[0], g_ids[1]), "b links to a");
    CHECK(link_between(run, g_ids[0], g_ids[3]), "d links to a");
    const cJSON *skipped = cJSON_GetObjectItemCaseSensitive(run, "skipped");
    CHECK(cJSON_GetArraySize(skipped) == 1 && strcmp(str(cJSON_GetArrayItem(skipped, 0), "id"), g_ids[4]) == 0 &&
              strcmp(str(cJSON_GetArrayItem(skipped, 0), "reason"), "not found") == 0,
          "the piece that is gone is skipped with a reason");
    int pairs = result_number(run, "pairs");
    cJSON_Delete(run);

    /* The second run over the same scope compares nothing. */
    char second_id[64] = "";
    run = run_and_wait(&all, second_id, sizeof(second_id));
    CHECK(run && strcmp(str(run, "status"), "done") == 0, "the second run is done");
    CHECK(result_number(run, "compared") == 0 && result_number(run, "cached") == pairs,
          "the second run over the same scope compares 0");
    CHECK(strcmp(first_id, second_id) != 0, "each run has its own id");
    cJSON_Delete(run);

    /* The cache is shared by every scope: two of the pieces are compared again for nothing. */
    char third_id[64] = "";
    UrsatzGuiLinkScope pair;
    memset(&pair, 0, sizeof(pair));
    pair.kind = URSATZ_GUI_LINK_SCOPE_FILTER;
    pair.composer = "Anna";
    run = run_and_wait(&pair, third_id, sizeof(third_id));
    CHECK(run && strcmp(str(run, "status"), "done") == 0, "the filtered run is done");
    CHECK(result_number(run, "pieces") == 2 && result_number(run, "compared") == 0, "another scope reuses the cache");
    cJSON_Delete(run);

    /* A document without features is analyzed again. */
    char path[256];
    snprintf(path, sizeof(path), "pieces/%s/analysis.json", g_ids[0]);
    size_t size = 0;
    unsigned char *text = read_file(path, &size);
    cJSON *document = text ? cJSON_Parse((const char *)text) : NULL;
    free(text);
    CHECK(document != NULL, "the stored analysis reads");
    cJSON *motives = document ? cJSON_GetObjectItemCaseSensitive(document, "motives") : NULL;
    CHECK(motives != NULL, "the analysis has motives");
    strip_features(motives);
    char *stripped = document ? cJSON_PrintUnformatted(document) : NULL;
    FILE *file = stripped ? fopen(path, "wb") : NULL;
    CHECK(file != NULL, "the stripped analysis is written");
    if (file) {
        fputs(stripped, file);
        fclose(file);
    }
    free(stripped);
    cJSON_Delete(document);

    char fourth_id[64] = "";
    run = run_and_wait(&pair, fourth_id, sizeof(fourth_id));
    CHECK(run && strcmp(str(run, "status"), "done") == 0 && cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(run, "skipped")) == 0,
          "a document without features is analyzed again, not skipped");
    CHECK(result_number(run, "pieces") == 2, "both pieces are in the run");
    cJSON_Delete(run);
    text = read_file(path, &size);
    CHECK(text && strstr((const char *)text, "\"features\"") != NULL, "the stored analysis has features again");
    free(text);

    /* Listing, the links of a piece, and deleting. */
    cJSON *runs = NULL;
    CHECK(ursatz_gui_link_run_list(&runs) == URSATZ_GUI_LINK_OK && cJSON_GetArraySize(runs) == 4, "four runs are listed");
    CHECK(strcmp(str(cJSON_GetArrayItem(runs, 0), "id"), fourth_id) == 0 &&
              strcmp(str(cJSON_GetArrayItem(runs, 3), "id"), first_id) == 0,
          "runs are listed newest first");
    CHECK(strcmp(str(cJSON_GetArrayItem(runs, 0), "status"), "done") == 0 &&
              cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(runs, 0), "result") == NULL,
          "a listed run has no result");
    cJSON_Delete(runs);

    cJSON *links = NULL;
    CHECK(ursatz_gui_link_piece_links(g_ids[0], &links) == URSATZ_GUI_LINK_OK, "links of a");
    bool from_second = false;
    bool from_fourth = false;
    bool from_first = false;
    const cJSON *entry = NULL;
    cJSON_ArrayForEach(entry, links) {
        from_first = from_first || strcmp(str(entry, "run"), first_id) == 0;
        from_second = from_second || strcmp(str(entry, "run"), second_id) == 0;
        from_fourth = from_fourth || strcmp(str(entry, "run"), fourth_id) == 0;
    }
    CHECK(from_second && from_fourth && !from_first, "links come from the newest done run of each scope");
    CHECK(cJSON_GetArraySize(links) >= 3, "a has links to b and d, and to b again in the other scope");
    cJSON_Delete(links);
    CHECK(ursatz_gui_link_piece_links(g_ids[2], &links) == URSATZ_GUI_LINK_OK && cJSON_GetArraySize(links) == 0,
          "c has no links");
    cJSON_Delete(links);

    CHECK(ursatz_gui_link_run_delete(first_id) == URSATZ_GUI_LINK_OK, "delete a run");
    CHECK(ursatz_gui_link_run_delete(first_id) == URSATZ_GUI_LINK_ERR_NOT_FOUND, "delete it again");
    CHECK(ursatz_gui_link_run_get(first_id, &links) == URSATZ_GUI_LINK_ERR_NOT_FOUND, "a deleted run is gone");
    CHECK(ursatz_gui_link_run_get("../x", &links) == URSATZ_GUI_LINK_ERR_NOT_FOUND, "a bad run id is not found");

    free(five_collection);
}

static void test_titles(void) {
    char *title = ursatz_gui_link_piece_title(g_ids[0]);
    CHECK(title && strcmp(title, "a") == 0, "a title is the filename without its extension");
    free(title);
    title = ursatz_gui_link_piece_title(g_ids[4]);
    CHECK(title && strcmp(title, g_ids[4]) == 0, "a piece that is gone is titled by its id");
    free(title);
}

static void test_interrupted(void) {
    ursatz_gui_link_service_shutdown();
    CHECK(MKDIR("links/runs/lnk-interrupted") == 0, "make a run directory");
    FILE *file = fopen("links/runs/lnk-interrupted/run.json", "wb");
    CHECK(file != NULL, "write a run left running");
    if (file) {
        fputs("{\"id\":\"lnk-interrupted\",\"scope\":{\"kind\":\"all\"},\"created_at\":\"2020-01-01T00:00:00.000Z\","
              "\"finished_at\":null,\"status\":\"running\",\"message\":\"\",\"done\":3,\"total\":9,\"skipped\":[]}",
              file);
        fclose(file);
    }
    CHECK(ursatz_gui_link_service_init("links"), "init again");
    cJSON *run = NULL;
    CHECK(ursatz_gui_link_run_get("lnk-interrupted", &run) == URSATZ_GUI_LINK_OK, "the run is still there");
    CHECK(strcmp(str(run, "status"), "failed") == 0 && strcmp(str(run, "message"), "interrupted") == 0,
          "an interrupted run reads failed");
    CHECK(cJSON_IsString(cJSON_GetObjectItemCaseSensitive(run, "finished_at")), "it has an end time");
    CHECK(num(run, "done") == 3 && num(run, "total") == 9, "its progress is kept");
    cJSON_Delete(run);
}

int main(void) {
    snprintf(g_materials, sizeof(g_materials), "materials_%d", (int)time(NULL));
    snprintf(g_subjects, sizeof(g_subjects), "subjects_%d", (int)time(NULL));

    /* Fresh state: this test's working directory is its own. */
#ifdef _WIN32
    system("rmdir /s /q pieces collections data links materials_* subjects_* 2> nul");
#else
    system("rm -rf pieces collections data links materials_* subjects_*");
#endif
    MKDIR("pieces");
    MKDIR("data");

    CHECK(ursatz_gui_collection_service_init("collections", "data/corpus.db"), "init collections");
    CHECK(ursatz_gui_material_service_init(g_materials), "init materials");
    CHECK(ursatz_gui_subject_service_init(g_subjects), "init subjects");
    CHECK(ursatz_gui_link_service_init("links"), "init links");

    const char *files[] = {"a.musicxml", "b.musicxml", "c.musicxml", "d.musicxml", "c.musicxml"};
    bool imported = true;
    for (int i = 0; i < 5; i++) {
        imported = import_piece(files[i], g_ids[i], sizeof(g_ids[i])) && imported;
    }
    CHECK(imported, "import the fixtures");
    if (imported) {
        test_scopes();
        test_runs();
        test_titles();
        test_interrupted();
    }

    ursatz_gui_link_service_shutdown();
    ursatz_gui_subject_service_shutdown();
    ursatz_gui_material_service_shutdown();
    ursatz_gui_collection_service_shutdown();
#ifdef _WIN32
    system("rmdir /s /q pieces collections data links materials_* subjects_* 2> nul");
#else
    system("rm -rf pieces collections data links materials_* subjects_*");
#endif
    if (failures) {
        fprintf(stderr, "%d check(s) failed\n", failures);
        return 1;
    }
    printf("link service tests passed\n");
    return 0;
}
