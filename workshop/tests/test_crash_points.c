/* A stop at every step of every kind of change (P63 B8, R-643, spec/target-architecture.md 10.2). For each command (an item created, saved,
 * renamed and moved, an item deleted with its subject links, a Library piece imported and deleted with its collection and subject links, a theme
 * composed) a forked child makes the change in a copy of one data folder and stops (_exit(70)) at the nth occurrence of each crash point of
 * the commit, for every n the command reaches. The parent opens the folder, which recovers it, and checks what it holds: the old state for a
 * stop before the journal record is synced and the new state from there on, never a mix, with the journal and the files agreeing, and no
 * pending, removed or staging entry left. */

#include <assert.h>
#include <dirent.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cJSON.h>
#include "theme_body.h"

#include <common/files.h>
#include <common/json.h>
#include <ursatz_workshop/workshop_api.h>

#include "base/knowledge.h"
#include "items/collections.h"
#include "items/journal.h"
#include "items/layout.h"
#include "items/store.h"
#include "items/subjects.h"
#include "items/txn.h"
#include "analysis/analysis.h"
#include "analysis/links.h"
#include "composition/motifs.h"
#include "composition/themes.h"
#include "command_context_support.h"

#ifndef URSATZ_GUI_TEST_LINK_FIXTURES
#error "URSATZ_GUI_TEST_LINK_FIXTURES must name the link fixtures directory"
#endif

static char g_home[512];
static int g_failures = 0;
static int g_checked = 0;

#define CHECK(cond, ...)                                                       \
    do {                                                                       \
        g_checked++;                                                           \
        if (!(cond)) {                                                         \
            g_failures++;                                                      \
            fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__);               \
            fprintf(stderr, __VA_ARGS__);                                      \
            fprintf(stderr, "\n");                                             \
        }                                                                      \
    } while (0)

/* The ids the base folder holds, by role. */
typedef struct {
    char general[64];
    char other[64];
    char motif_a[64];
    char motif_b[64];
    char save[64];
    char rename[64];
    char move[64];
    char del[64];
    char theme[64];
    char piece[64];
    char collection[64];
    char subject[64];
    int pieces;
    int items;
    long long seq;
} Base;

static Base g_base;

static const char *text(const cJSON *object, const char *name) {
    const cJSON *item = object ? cJSON_GetObjectItemCaseSensitive(object, name) : NULL;
    return cJSON_IsString(item) ? item->valuestring : "";
}

static cJSON *parse(const char *json) {
    cJSON *value = cJSON_Parse(json);
    assert(value);
    return value;
}

static const char kMotif[] =
    "{\"uses\":[],\"motif\":{\"role\":\"melodic\",\"form\":\"full\",\"notes\":\"%s\",\"meter\":\"4/4\",\"material\":\"omt.material.natural-minor\","
    "\"pickup\":\"0\",\"features\":[],\"analysis\":null,\"derived\":null}}";

static cJSON *motif_body(const char *notes) {
    char json[1024];
    snprintf(json, sizeof(json), kMotif, notes);
    return parse(json);
}

static void new_motif(const char *name, const char *project, const char *notes, char *out_id) {
    cJSON *body = motif_body(notes);
    cJSON *item = NULL;
    /* a motif is saved analyzed: its degrees are the Composer's reading of its notes */
    char analysis_error[WORKSHOP_MOTIF_ANALYSIS_ERROR_SIZE];
    char refusal[512] = "";
    char refusal_path[128] = "";
    assert(workshop_motif_analyze_body(body, NULL, analysis_error, refusal, sizeof(refusal), refusal_path, sizeof(refusal_path)) ==
           WORKSHOP_MOTIF_ANALYSIS_DONE);
    assert(workshop_material_item_create_noted("motif", name, project, body, NULL, &item) == URSATZ_OK);
    snprintf(out_id, 64, "%s", text(item, "id"));
    cJSON_Delete(item);
    cJSON_Delete(body);
}

/* What the coordinator registers at open: an item delete drops the subjects' links to it in its own transaction. */
static void forget_in_subjects(WorkshopTxn *txn, const char *item_id) {
    (void)workshop_subject_forget_item(txn, item_id);
}

static void services_up(void) {
    assert(workshop_knowledge_init());
    assert(workshop_material_service_init());
    assert(workshop_subject_service_init());
    assert(ursatz_gui_collection_service_init());
    assert(workshop_link_service_init());
    workshop_store_set_forget_item(forget_in_subjects);
}

static void services_down(void) {
    workshop_store_set_forget_item(NULL);
    workshop_link_service_shutdown();
    workshop_collection_service_shutdown();
    workshop_subject_service_shutdown();
    workshop_material_service_shutdown();
    workshop_knowledge_shutdown();
}

static void shell(const char *format, const char *a, const char *b) {
    char command[2048];
    snprintf(command, sizeof(command), format, a, b);
    if (system(command) != 0) {
        fprintf(stderr, "command failed: %s\n", command);
    }
}

/* The folder the base is made in, with the data folder marker (workshop_open makes it). */
static void make_base(void) {
    WorkshopConfig config = {"base", NULL, 9401, 2, 2};
    WorkshopConfig resolved;
    char storage[512];
    UrsatzError error;
    memset(&error, 0, sizeof(error));
    shell("rm -rf '%s' '%s'", "base", "case");
    assert(workshop_config_resolve(&config, &resolved, storage, sizeof(storage), &error) == URSATZ_OK);
    Workshop *workshop = NULL;
    assert(workshop_open(&resolved, &workshop, &error) == URSATZ_OK);
    workshop_close(workshop);

    assert(chdir("base") == 0);
    services_up();
    cJSON *projects = NULL;
    assert(workshop_material_project_list(&projects) == URSATZ_OK && cJSON_GetArraySize(projects) == 1);
    snprintf(g_base.general, sizeof(g_base.general), "%s", text(cJSON_GetArrayItem(projects, 0), "id"));
    cJSON_Delete(projects);
    cJSON *other = NULL;
    assert(workshop_material_project_create("Other", "", &other) == URSATZ_OK);
    snprintf(g_base.other, sizeof(g_base.other), "%s", text(other, "id"));
    cJSON_Delete(other);

    new_motif("A", g_base.general, "C4:e Eb4:e G4:e Eb4:e C4:e Eb4:e G4:e C5:e", g_base.motif_a);
    new_motif("B", g_base.general, "G4:e F4:e Eb4:e D4:e F4:e Eb4:e D4:e C4:e", g_base.motif_b);
    new_motif("Save", g_base.general, "C4:q D4:q E4:h", g_base.save);
    new_motif("Rename", g_base.general, "C4:q D4:q E4:h", g_base.rename);
    new_motif("Move", g_base.general, "C4:q D4:q E4:h", g_base.move);
    new_motif("Delete", g_base.general, "C4:q D4:q E4:h", g_base.del);

    char theme_json[2048];
    theme_body_text(theme_json, sizeof(theme_json), g_base.motif_a, g_base.motif_b, 5);
    cJSON *theme_body = parse(theme_json);
    cJSON *theme = NULL;
    assert(workshop_material_item_create_noted("theme", "Theme", g_base.general, theme_body, NULL, &theme) == URSATZ_OK);
    snprintf(g_base.theme, sizeof(g_base.theme), "%s", text(theme, "id"));
    cJSON_Delete(theme);
    cJSON_Delete(theme_body);

    /* a Library piece, a collection holding it and a subject linking it and the item that is deleted */
    char path[600];
    snprintf(path, sizeof(path), "%s/a.musicxml", URSATZ_GUI_TEST_LINK_FIXTURES);
    char *data = NULL;
    size_t size = 0;
    assert(common_file_read(path, &data, &size, NULL) == URSATZ_OK);
    char *piece = NULL;
    assert(workshop_analysis_import_and_save(WORKSHOP_DIR_PIECES, (const unsigned char *)data, size, "a.musicxml", &piece) == URSATZ_OK);
    snprintf(g_base.piece, sizeof(g_base.piece), "%s", piece);
    free(piece);
    free(data);
    char *collection = NULL;
    assert(workshop_collection_create("Collection", "", &collection) == URSATZ_OK);
    snprintf(g_base.collection, sizeof(g_base.collection), "%s", collection);
    const char *members[] = {g_base.piece};
    size_t added = 0;
    size_t already = 0;
    assert(workshop_collection_add_pieces(collection, members, 1, &added, &already) == URSATZ_OK);
    free(collection);
    cJSON *subject = NULL;
    assert(workshop_subject_create("Subject", "idea", "", &subject) == URSATZ_OK);
    snprintf(g_base.subject, sizeof(g_base.subject), "%s", text(subject, "id"));
    cJSON_Delete(subject);
    bool linked = false;
    assert(workshop_subject_link_item(g_base.subject, g_base.del, &linked, NULL) == URSATZ_OK);
    assert(workshop_subject_link_piece(g_base.subject, g_base.piece, &linked, NULL) == URSATZ_OK);

    cJSON *items = NULL;
    assert(workshop_material_item_list(NULL, NULL, &items) == URSATZ_OK);
    g_base.items = cJSON_GetArraySize(items);
    cJSON_Delete(items);
    g_base.pieces = 1;
    WorkshopJournal journal;
    UrsatzError error2;
    memset(&error2, 0, sizeof(error2));
    assert(workshop_journal_read(WORKSHOP_JOURNAL_FILE, &journal, &error2) == URSATZ_OK);
    g_base.seq = journal.last_seq;
    workshop_journal_free(&journal);
    services_down();
    assert(chdir(g_home) == 0);
}

/* --- the commands ------------------------------------------------------------------------------------------------------------- */

typedef struct {
    const char *name;
    void (*run)(void);
    /* Counts what the folder holds after recovery: true when it is the new state, false for the old one; a mix is reported. */
    bool (*is_new)(void);
} Command;

static void run_item_create(void) {
    cJSON *body = motif_body("C4:q G4:q");
    cJSON *item = NULL;
    workshop_material_item_create_noted("motif", "Created", g_base.general, body, NULL, &item);
    cJSON_Delete(item);
    cJSON_Delete(body);
}

static int item_count(void) {
    cJSON *items = NULL;
    int count = workshop_material_item_list(NULL, NULL, &items) == URSATZ_OK ? cJSON_GetArraySize(items) : -1;
    cJSON_Delete(items);
    return count;
}

static bool is_new_item_create(void) {
    int count = item_count();
    CHECK(count == g_base.items || count == g_base.items + 1, "an item create leaves %d items (base %d)", count, g_base.items);
    return count == g_base.items + 1;
}

static void run_item_save(void) {
    cJSON *body = motif_body("C4:q F4:q E4:h");
    cJSON *item = NULL;
    workshop_material_item_add_version(g_base.save, body, "second", &item);
    cJSON_Delete(item);
    cJSON_Delete(body);
}

static bool is_new_item_save(void) {
    cJSON *meta = NULL;
    cJSON *body = NULL;
    CHECK(workshop_material_item_get(g_base.save, &meta, &body) == URSATZ_OK, "the saved item reads");
    int current = (int)(cJSON_IsNumber(cJSON_GetObjectItemCaseSensitive(meta, "current_version"))
                            ? cJSON_GetObjectItemCaseSensitive(meta, "current_version")->valuedouble
                            : -1);
    CHECK(current == 1 || current == 2, "the item is at version %d", current);
    cJSON *second = NULL;
    bool has_two = workshop_material_item_get_version(g_base.save, 2, &second) == URSATZ_OK;
    CHECK(has_two == (current == 2), "version 2 exists exactly when the header names it (current %d)", current);
    cJSON_Delete(second);
    cJSON_Delete(meta);
    cJSON_Delete(body);
    return current == 2;
}

static void run_item_rename(void) {
    cJSON *item = NULL;
    workshop_material_item_rename(g_base.rename, "Renamed", &item);
    cJSON_Delete(item);
}

static bool is_new_item_rename(void) {
    cJSON *meta = NULL;
    CHECK(workshop_material_item_get(g_base.rename, &meta, NULL) == URSATZ_OK, "the renamed item reads");
    const char *name = text(meta, "name");
    CHECK(strcmp(name, "Rename") == 0 || strcmp(name, "Renamed") == 0, "the name is %s", name);
    bool renamed = strcmp(name, "Renamed") == 0;
    cJSON_Delete(meta);
    return renamed;
}

/* Her knowledge file written as the knowledge commands write it (R-768): one staged file, in one transaction. */
static const char kKnowledgeFile[] = "{\"schema\": \"ursatz-knowledge\", \"schema_version\": 1, \"source\": \"user\", \"entries\": []}";

static void run_knowledge_add(void) {
    UrsatzError error = {{0}, {0}};
    common_dir_make("knowledge", NULL);
    WorkshopTxn *txn = workshop_txn_begin(NULL, "knowledge.add", "", 0);
    if (txn && workshop_txn_stage_file(txn, "knowledge/user.json", kKnowledgeFile, strlen(kKnowledgeFile)) == URSATZ_OK) {
        workshop_txn_commit(txn, &error);
    } else {
        workshop_txn_abort(txn);
    }
}

static bool is_new_knowledge_add(void) {
    return common_path_exists("knowledge/user.json");
}

static void run_item_move(void) {
    cJSON *item = NULL;
    workshop_material_item_move(g_base.move, g_base.other, &item);
    cJSON_Delete(item);
}

static bool is_new_item_move(void) {
    cJSON *meta = NULL;
    CHECK(workshop_material_item_get(g_base.move, &meta, NULL) == URSATZ_OK, "the moved item reads");
    const char *project = text(meta, "project_id");
    bool moved = strcmp(project, g_base.other) == 0;
    CHECK(moved || strcmp(project, g_base.general) == 0, "the item is in project %s", project);
    int folders = 0;
    char path[600];
    const char *projects[] = {g_base.general, g_base.other};
    for (int i = 0; i < 2; i++) {
        snprintf(path, sizeof(path), "projects/%s/items/%s", projects[i], g_base.move);
        folders += common_path_is_dir(path);
    }
    CHECK(folders == 1, "the item has one folder, not %d", folders);
    cJSON_Delete(meta);
    return moved;
}

static void run_item_delete(void) {
    workshop_material_item_delete(g_base.del, true);
}

static bool subject_links(const char *member, const char *id) {
    cJSON *subject = NULL;
    bool linked = false;
    if (workshop_subject_get(g_base.subject, &subject) == URSATZ_OK) {
        const cJSON *entry = NULL;
        cJSON_ArrayForEach(entry, cJSON_GetObjectItemCaseSensitive(subject, member)) {
            linked = linked || (cJSON_IsString(entry) && strcmp(entry->valuestring, id) == 0);
        }
    }
    cJSON_Delete(subject);
    return linked;
}

static bool is_new_item_delete(void) {
    cJSON *meta = NULL;
    bool exists = workshop_material_item_get(g_base.del, &meta, NULL) == URSATZ_OK;
    cJSON_Delete(meta);
    bool linked = subject_links("items", g_base.del);
    CHECK(exists == linked, "the item %s and the subject %s it (never a mix)", exists ? "exists" : "is gone", linked ? "links" : "does not link");
    return !exists;
}

static void run_library_import(void) {
    char path[600];
    snprintf(path, sizeof(path), "%s/b.musicxml", URSATZ_GUI_TEST_LINK_FIXTURES);
    char *data = NULL;
    size_t size = 0;
    if (common_file_read(path, &data, &size, NULL) == URSATZ_OK) {
        char *id = NULL;
        workshop_analysis_import_and_save(WORKSHOP_DIR_PIECES, (const unsigned char *)data, size, "b.musicxml", &id);
        free(id);
        free(data);
    }
}

static bool is_new_library_import(void) {
    CommonNames names = {0};
    size_t count = 0;
    CHECK(common_dir_list(WORKSHOP_DIR_PIECES, &names, NULL) == URSATZ_OK, "the Library folder lists");
    for (size_t i = 0; i < names.count; i++) {
        count += workshop_library_piece_exists(WORKSHOP_DIR_PIECES, names.items[i]);
    }
    common_names_free(&names);
    CHECK((int)count == g_base.pieces || (int)count == g_base.pieces + 1, "the Library holds %zu pieces (base %d)", count, g_base.pieces);
    return (int)count == g_base.pieces + 1;
}

static void run_library_delete(void) {
    workshop_library_delete(WORKSHOP_DIR_PIECES, g_base.piece);
}

static bool collection_holds(const char *id) {
    WorkshopCollection *collection = NULL;
    bool holds = false;
    if (workshop_collection_get(g_base.collection, &collection) == URSATZ_OK) {
        for (size_t i = 0; i < collection->piece_count; i++) {
            holds = holds || strcmp(collection->pieces[i], id) == 0;
        }
    }
    workshop_collection_destroy(collection);
    return holds;
}

static bool is_new_library_delete(void) {
    bool exists = workshop_library_piece_exists(WORKSHOP_DIR_PIECES, g_base.piece);
    bool in_collection = collection_holds(g_base.piece);
    bool linked = subject_links("pieces", g_base.piece);
    CHECK(exists == in_collection && exists == linked, "the piece %s, the collection %s it and the subject %s it (never a mix)",
          exists ? "exists" : "is gone", in_collection ? "holds" : "does not hold", linked ? "links" : "does not link");
    return !exists;
}

static void run_theme_compose(void) {
    cJSON *item = NULL;
    UrsatzError error;
    memset(&error, 0, sizeof(error));
    UrsatzStatus status = workshop_theme_compose(g_base.theme, true, 1, false, NULL, NULL, &item, &error);
    if (status != URSATZ_OK) {
        fprintf(stderr, "theme compose: status %d: %s\n", (int)status, error.message);
    }
    cJSON_Delete(item);
}

static bool is_new_theme_compose(void) {
    cJSON *meta = NULL;
    CHECK(workshop_material_item_get(g_base.theme, &meta, NULL) == URSATZ_OK, "the theme reads");
    int current = (int)(cJSON_IsNumber(cJSON_GetObjectItemCaseSensitive(meta, "current_version"))
                            ? cJSON_GetObjectItemCaseSensitive(meta, "current_version")->valuedouble
                            : -1);
    CHECK(current == 1 || current == 2, "the theme is at version %d", current);
    unsigned char *file = NULL;
    size_t size = 0;
    bool plan = workshop_material_version_file(g_base.theme, 2, "plan.json", &file, &size) == URSATZ_OK;
    free(file);
    file = NULL;
    bool midi = workshop_material_version_file(g_base.theme, 2, "theme.mid", &file, &size) == URSATZ_OK;
    free(file);
    CHECK(plan == (current == 2) && midi == (current == 2), "version 2 has its files exactly when it exists (current %d)", current);
    cJSON_Delete(meta);
    return current == 2;
}

/* --- the harness -------------------------------------------------------------------------------------------------------------- */

/* No pending, removed or staging entry is left anywhere under the folder. */
static int leftovers(const char *folder) {
    CommonNames names = {0};
    int found = 0;
    if (common_dir_list(folder, &names, NULL) != URSATZ_OK) {
        return 0;
    }
    for (size_t i = 0; i < names.count; i++) {
        char child[1024];
        snprintf(child, sizeof(child), "%s/%s", folder, names.items[i]);
        if (strncmp(names.items[i], ".pending-", 9) == 0 || strncmp(names.items[i], ".removed-", 9) == 0 ||
            strstr(names.items[i], ".stage-") != NULL || strstr(names.items[i], ".tmp-") != NULL) {
            found++;
        } else if (common_path_is_dir(child) && names.items[i][0] != '.') {
            found += leftovers(child);
        }
    }
    common_names_free(&names);
    return found;
}

static const struct {
    WorkshopCrashPoint point;
    const char *name;
    bool committed; /* the journal record is whole once the point is reached */
} kPoints[] = {{WORKSHOP_CRASH_AFTER_STAGE, "after stage", false},
               {WORKSHOP_CRASH_AFTER_VALIDATE, "after validate", false},
               {WORKSHOP_CRASH_BEFORE_JOURNAL_SYNC, "before journal sync", false},
               {WORKSHOP_CRASH_AFTER_JOURNAL_SYNC, "after journal sync", true},
               {WORKSHOP_CRASH_AFTER_RENAME, "after rename", true},
               {WORKSHOP_CRASH_AFTER_HEADERS, "after headers", true},
               {WORKSHOP_CRASH_AFTER_INDEX, "after index", true}};

static void check_after_stop(const Command *command, const char *point_name, unsigned nth, bool committed) {
    char label[256];
    snprintf(label, sizeof(label), "%s, %s #%u", command->name, point_name, nth);
    /* the open recovers the folder */
    WorkshopConfig config = {"case", NULL, 9402, 2, 2};
    WorkshopConfig resolved;
    char storage[512];
    UrsatzError error;
    memset(&error, 0, sizeof(error));
    CHECK(workshop_config_resolve(&config, &resolved, storage, sizeof(storage), &error) == URSATZ_OK, "%s: settings", label);
    Workshop *workshop = NULL;
    if (workshop_open(&resolved, &workshop, &error) != URSATZ_OK) {
        CHECK(false, "%s: open after the stop: %s", label, error.message);
        return;
    }
    workshop_close(workshop);
    CHECK(leftovers("case") == 0, "%s: pending, removed or staging entries are left", label);

    assert(chdir("case") == 0);
    services_up();
    WorkshopJournal journal;
    memset(&error, 0, sizeof(error));
    CHECK(workshop_journal_read(WORKSHOP_JOURNAL_FILE, &journal, &error) == URSATZ_OK && !journal.torn, "%s: the journal reads whole", label);
    bool is_new = command->is_new();
    CHECK(is_new == committed, "%s: the state is the %s one, expected the %s one", label, is_new ? "new" : "old", committed ? "new" : "old");
    CHECK(journal.last_seq == g_base.seq + (committed ? 1 : 0), "%s: the journal ends at record %lld, expected %lld", label, journal.last_seq,
          g_base.seq + (committed ? 1 : 0));
    if (committed && journal.count > 0) {
        CHECK(text(journal.records[journal.count - 1], "command")[0] != '\0', "%s: the last record names its command", label);
    }
    workshop_journal_free(&journal);
    services_down();
    assert(chdir(g_home) == 0);
}

static void run_command(const Command *command) {
    int stops = 0;
    for (size_t p = 0; p < sizeof(kPoints) / sizeof(kPoints[0]); p++) {
        for (unsigned nth = 1; nth <= 40; nth++) {
            shell("rm -rf '%s' && cp -r '%s' case", "case", "base");
            pid_t child = fork();
            assert(child >= 0);
            if (child == 0) {
                if (chdir("case") != 0) {
                    _exit(2);
                }
                services_up();
                workshop_test_set_crash_point(kPoints[p].point, nth);
                command->run();
                _exit(0); /* the point was not reached an nth time: the command is done */
            }
            int status = 0;
            waitpid(child, &status, 0);
            bool stopped = WIFEXITED(status) && WEXITSTATUS(status) == 70;
            if (!stopped) {
                CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0, "%s: the child ended oddly (status %d)", command->name, status);
                break; /* no more occurrences of this point */
            }
            check_after_stop(command, kPoints[p].name, nth, kPoints[p].committed);
            stops++;
        }
    }
    printf("   %d stops\n", stops);
    CHECK(stops >= 6, "%s: the command reaches every crash point at least once (%d stops)", command->name, stops);
}

int main(int argc, char **argv) {
    /* "smoke" runs the item commands alone (the whole test is slow: a Library piece is analyzed and a theme composed at each stop) */
    bool smoke = argc > 1 && strcmp(argv[1], "smoke") == 0;
    assert(getcwd(g_home, sizeof(g_home)));
    command_context_support_begin(); /* the services are called directly: a write needs a command (command_context_support.h) */
    make_base();
    static const Command kCommands[] = {
        {"item create", run_item_create, is_new_item_create},
        {"item save", run_item_save, is_new_item_save},
        {"item rename", run_item_rename, is_new_item_rename},
        {"item move", run_item_move, is_new_item_move},
        {"knowledge file", run_knowledge_add, is_new_knowledge_add},
        {"item delete with subject links", run_item_delete, is_new_item_delete},
        {"library import", run_library_import, is_new_library_import},
        {"library delete with collection and subject links", run_library_delete, is_new_library_delete},
        {"theme compose", run_theme_compose, is_new_theme_compose},
    };
    for (size_t i = 0; i < sizeof(kCommands) / sizeof(kCommands[0]); i++) {
        if (smoke && i >= 6) {
            break; /* the Library and the theme are in the whole test */
        }
        printf("-- %s\n", kCommands[i].name);
        run_command(&kCommands[i]);
    }
    shell("rm -rf '%s' '%s'", "base", "case");
    printf("workshop_crash_points: %d checks, %d failures\n", g_checked, g_failures);
    return g_failures == 0 ? 0 : 1;
}
