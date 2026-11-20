#include <dirent.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cJSON.h>

#include "items/store.h"
#include "ursatz_check.h"
#include "test_knowledge.h"

#define OK URSATZ_OK

static char g_forgotten[64];
static int g_forgotten_count;

static void record_forgotten(WorkshopTxn *txn, const char *item_id) {
    (void)txn;
    snprintf(g_forgotten, sizeof(g_forgotten), "%s", item_id);
    g_forgotten_count++;
}

static char g_dir[512];

static const char *kMotifBody =
    "{\"uses\":[],\"motif\":{\"role\":\"melodic\",\"form\":\"full\",\"notes\":\"D4:q F4:e A4:e D5:h\","
    "\"meter\":\"4/4\",\"key\":\"D minor\",\"pickup\":\"0\",\"features\":[{\"label\":\"a\",\"first\":0,\"last\":2}],"
    "\"analysis\":null,\"derived\":null}}";

static const char *kThemeBody = "{\"uses\":[],\"theme\":{\"form\":null,\"slots\":[]}}";
static const char *kPieceBody = "{\"uses\":[],\"piece\":{\"structure\":null}}";

static const char *str(const cJSON *object, const char *name) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);
    return cJSON_IsString(item) ? item->valuestring : "";
}

static int num(const cJSON *object, const char *name) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);
    return cJSON_IsNumber(item) ? item->valueint : -1;
}

/* Creates a project and returns its id (malloc'd). */
static char *new_project(const char *name) {
    cJSON *project = NULL;
    char *id = NULL;
    if (workshop_material_project_create(name, "", &project) == OK) {
        id = strdup(str(project, "id"));
    }
    cJSON_Delete(project);
    return id;
}

/* Creates an item from a JSON body text and returns its id (malloc'd). */
static char *new_item(const char *type, const char *name, const char *project_id, const char *body_text) {
    cJSON *body = cJSON_Parse(body_text);
    cJSON *item = NULL;
    char *id = NULL;
    if (workshop_material_item_create_noted(type, name, project_id, body, NULL, &item) == OK) {
        id = strdup(str(item, "id"));
    }
    cJSON_Delete(item);
    cJSON_Delete(body);
    return id;
}

static const cJSON *find_by_id(const cJSON *array, const char *id) {
    const cJSON *entry = NULL;
    cJSON_ArrayForEach(entry, array) {
        if (strcmp(str(entry, "id"), id) == 0) {
            return entry;
        }
    }
    return NULL;
}

/* The folder of an item (projects/<prj>/items/<id>), found under the store; false when it is not there. */
static bool item_dir_of(const char *id, char *out, size_t size) {
    DIR *projects = opendir("projects");
    bool found = false;
    for (struct dirent *entry = projects ? readdir(projects) : NULL; entry && !found; entry = readdir(projects)) {
        if (entry->d_name[0] != '.') {
            snprintf(out, size, "projects/%s/items/%s", entry->d_name, id);
            struct stat st;
            found = stat(out, &st) == 0 && S_ISDIR(st.st_mode);
        }
    }
    if (projects) {
        closedir(projects);
    }
    return found;
}

/* A stored version body as it is on disk (caller deletes), NULL when it is not there. */
static cJSON *stored_body(const char *id, int number) {
    char dir[500];
    char path[700];
    if (!item_dir_of(id, dir, sizeof(dir))) {
        return NULL;
    }
    snprintf(path, sizeof(path), "%s/v%d/body.json", dir, number);
    char *text = test_read_file(path, NULL);
    cJSON *body = text ? cJSON_Parse(text) : NULL;
    free(text);
    return body;
}

static void test_general_project(void) {
    cJSON *projects = NULL;
    check_quiet(workshop_material_project_list(&projects) == OK, "list projects");
    check_quiet(cJSON_GetArraySize(projects) == 1, "one project after first init");
    const cJSON *general = cJSON_GetArrayItem(projects, 0);
    check_quiet(strcmp(str(general, "name"), "General") == 0, "the project is named General");
    check_quiet(strncmp(str(general, "id"), "prj-", 4) == 0, "project ids start with prj-");
    check_quiet(num(general, "item_count") == 0, "General holds no items");
    cJSON_Delete(projects);

    workshop_material_service_shutdown();
    check_quiet(workshop_material_service_init(), "init again");
    check_quiet(workshop_material_project_list(&projects) == OK && cJSON_GetArraySize(projects) == 1,
          "init does not add a second General");
    cJSON_Delete(projects);
}

static void test_project_crud(void) {
    cJSON *project = NULL;
    check_quiet(workshop_material_project_create("  beta  ", "  first notes ", &project) == OK, "create project");
    check_quiet(strcmp(str(project, "name"), "beta") == 0, "name is trimmed");
    check_quiet(strcmp(str(project, "description"), "first notes") == 0, "description is trimmed");
    char *beta = strdup(str(project, "id"));
    cJSON_Delete(project);
    char *alpha = new_project("Alpha");
    char *zed = new_project("zed");
    check_quiet(beta && alpha && zed, "three projects");

    cJSON *projects = NULL;
    check_quiet(workshop_material_project_list(&projects) == OK, "list");
    check_quiet(cJSON_GetArraySize(projects) == 4, "General plus three");
    check_quiet(strcmp(str(cJSON_GetArrayItem(projects, 0), "name"), "Alpha") == 0, "sorted by name, first");
    check_quiet(strcmp(str(cJSON_GetArrayItem(projects, 1), "name"), "beta") == 0, "sorted by name, case-insensitive");
    check_quiet(strcmp(str(cJSON_GetArrayItem(projects, 2), "name"), "General") == 0, "sorted by name, third");
    check_quiet(strcmp(str(cJSON_GetArrayItem(projects, 3), "name"), "zed") == 0, "sorted by name, last");
    cJSON_Delete(projects);

    check_quiet(workshop_material_project_get(beta, &project) == OK, "get");
    check_quiet(strcmp(str(project, "description"), "first notes") == 0, "get returns the description");
    cJSON_Delete(project);

    check_quiet(workshop_material_project_update(beta, "Gamma", NULL, &project) == OK, "update name");
    check_quiet(strcmp(str(project, "name"), "Gamma") == 0 && strcmp(str(project, "description"), "first notes") == 0,
          "update keeps the description");
    cJSON_Delete(project);
    check_quiet(workshop_material_project_update(beta, NULL, "", NULL) == OK, "clear the description");
    check_quiet(workshop_material_project_get(beta, &project) == OK && strcmp(str(project, "description"), "") == 0,
          "description cleared");
    cJSON_Delete(project);

    check_quiet(workshop_material_project_create("", NULL, NULL) == URSATZ_ERR_REQUEST, "empty name");
    check_quiet(workshop_material_project_create("   ", NULL, NULL) == URSATZ_ERR_REQUEST,
          "blank name");
    char long_name[260];
    memset(long_name, 'n', 201);
    long_name[201] = '\0';
    check_quiet(workshop_material_project_create(long_name, NULL, NULL) == URSATZ_ERR_REQUEST,
          "name of 201 characters");
    long_name[200] = '\0';
    check_quiet(workshop_material_project_create(long_name, NULL, NULL) == OK, "name of 200 characters");
    char *long_description = calloc(1, 2002);
    memset(long_description, 'd', 2001);
    check_quiet(workshop_material_project_create("x", long_description, NULL) == URSATZ_ERR_REQUEST,
          "description of 2001 characters");
    long_description[2000] = '\0';
    check_quiet(workshop_material_project_create("Described", long_description, NULL) == OK,
          "description of 2000 characters");
    free(long_description);
    check_quiet(workshop_material_project_update(beta, "", NULL, NULL) == URSATZ_ERR_REQUEST,
          "update to an empty name");
    check_quiet(workshop_material_project_get("../x", &project) == URSATZ_ERR_REQUEST, "bad id");
    check_quiet(workshop_material_project_get("prj-0000000000000000", &project) == URSATZ_ERR_NOT_FOUND,
          "unknown id");

    check_quiet(workshop_material_project_delete(zed) == OK, "delete");
    check_quiet(workshop_material_project_get(zed, &project) == URSATZ_ERR_NOT_FOUND, "gone after delete");
    check_quiet(workshop_material_project_delete(zed) == URSATZ_ERR_NOT_FOUND, "delete twice");
    free(beta);
    free(alpha);
    free(zed);
}

static void test_items_and_versions(void) {
    char *home = new_project("Items home");
    char *other = new_project("Items other");
    cJSON *body = cJSON_Parse(kMotifBody);
    cJSON *item = NULL;

    check_quiet(workshop_material_item_create_noted("motif", "  Opening  ", home, body, NULL, &item) == OK, "create motif");
    check_quiet(strncmp(str(item, "id"), "mot-", 4) == 0, "motif ids start with mot-");
    check_quiet(strcmp(str(item, "name"), "Opening") == 0 && strcmp(str(item, "type"), "motif") == 0, "name and type");
    check_quiet(num(item, "current_version") == 1, "current version is 1");
    check_quiet(cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(item, "versions")) == 1, "one version entry");
    char *motif = strdup(str(item, "id"));
    cJSON_Delete(item);
    char *theme = new_item("theme", "Theme one", home, kThemeBody);
    char *piece = new_item("piece", "Piece one", other, kPieceBody);
    check_quiet(theme && strncmp(theme, "thm-", 4) == 0, "theme ids start with thm-");
    check_quiet(piece && strncmp(piece, "pce-", 4) == 0, "piece ids start with pce-");

    check_quiet(workshop_material_item_create_noted("chord", "x", home, body, NULL, NULL) ==
              URSATZ_ERR_REQUEST,
          "unknown type");
    check_quiet(workshop_material_item_create_noted("motif", " ", home, body, NULL, NULL) ==
              URSATZ_ERR_REQUEST,
          "blank item name");
    check_quiet(workshop_material_item_create_noted("motif", "x", "prj-0000000000000000", body, NULL, NULL) ==
              URSATZ_ERR_NOT_FOUND,
          "unknown project");
    check_quiet(workshop_material_item_create_noted("motif", "x", "a/b", body, NULL, NULL) ==
              URSATZ_ERR_REQUEST,
          "bad project id");

    cJSON *meta = NULL;
    cJSON *current = NULL;
    check_quiet(workshop_material_item_get(motif, &meta, &current) == OK, "get");
    check_quiet(strcmp(str(cJSON_GetObjectItemCaseSensitive(current, "motif"), "notes"), "D4:q F4:e A4:e D5:h") == 0,
          "get returns the body");
    cJSON_Delete(meta);
    cJSON_Delete(current);

    /* A new version never rewrites an earlier one. */
    char v1_path[700];
    char motif_dir[500];
    check_quiet(item_dir_of(motif, motif_dir, sizeof(motif_dir)), "the motif has a folder");
    snprintf(v1_path, sizeof(v1_path), "%s/v1/body.json", motif_dir);
    char *v1_before = test_read_file(v1_path, NULL);
    cJSON *second = cJSON_Parse(kMotifBody);
    cJSON_ReplaceItemInObjectCaseSensitive(cJSON_GetObjectItemCaseSensitive(second, "motif"), "notes",
                                           cJSON_CreateString("D4:q E4:q"));
    check_quiet(workshop_material_item_add_version(motif, second, "  trimmed ending ", &item) == OK, "add version");
    check_quiet(num(item, "current_version") == 2, "current version advances");
    const cJSON *versions = cJSON_GetObjectItemCaseSensitive(item, "versions");
    check_quiet(cJSON_GetArraySize(versions) == 2 && strcmp(str(cJSON_GetArrayItem(versions, 1), "note"), "trimmed ending") == 0,
          "second entry carries the note");
    cJSON_Delete(item);
    char *v1_after = test_read_file(v1_path, NULL);
    check_quiet(v1_before && v1_after && strcmp(v1_before, v1_after) == 0, "the first version is untouched");
    free(v1_before);
    free(v1_after);

    cJSON *old_body = NULL;
    check_quiet(workshop_material_item_get_version(motif, 1, &old_body) == OK, "get version 1");
    check_quiet(strcmp(str(cJSON_GetObjectItemCaseSensitive(old_body, "motif"), "notes"), "D4:q F4:e A4:e D5:h") == 0,
          "version 1 keeps its notes");
    cJSON_Delete(old_body);
    check_quiet(workshop_material_item_get_version(motif, 2, &old_body) == OK &&
              strcmp(str(cJSON_GetObjectItemCaseSensitive(old_body, "motif"), "notes"), "D4:q E4:q") == 0,
          "version 2 has the new notes");
    cJSON_Delete(old_body);
    check_quiet(workshop_material_item_get_version(motif, 3, &old_body) == URSATZ_ERR_NOT_FOUND,
          "version 3 does not exist");
    check_quiet(workshop_material_item_get_version(motif, 0, &old_body) == URSATZ_ERR_NOT_FOUND,
          "version 0 does not exist");
    check_quiet(workshop_material_item_get(motif, NULL, &current) == OK &&
              strcmp(str(cJSON_GetObjectItemCaseSensitive(current, "motif"), "notes"), "D4:q E4:q") == 0,
          "get returns the current version");
    cJSON_Delete(current);

    /* Rename and move change item.json only. */
    check_quiet(workshop_material_item_rename(motif, "  Renamed ", &item) == OK && strcmp(str(item, "name"), "Renamed") == 0,
          "rename");
    cJSON_Delete(item);
    check_quiet(workshop_material_item_rename(motif, "", NULL) == URSATZ_ERR_REQUEST, "rename to empty");
    check_quiet(workshop_material_item_move(motif, other, &item) == OK && strcmp(str(item, "project_id"), other) == 0,
          "move");
    check_quiet(num(item, "current_version") == 2, "move adds no version");
    cJSON_Delete(item);
    check_quiet(workshop_material_item_move(motif, "prj-0000000000000000", NULL) == URSATZ_ERR_NOT_FOUND,
          "move to an unknown project");

    /* Filters and order. */
    cJSON *list = NULL;
    check_quiet(workshop_material_item_list(NULL, NULL, &list) == OK && cJSON_GetArraySize(list) == 3, "list all");
    const cJSON *previous = NULL;
    const cJSON *entry = NULL;
    int ordered = 1;
    cJSON_ArrayForEach(entry, list) {
        if (previous && strcmp(str(previous, "updated_at"), str(entry, "updated_at")) < 0) {
            ordered = 0;
        }
        previous = entry;
    }
    check_quiet(ordered, "newest updated_at first");
    cJSON_Delete(list);
    check_quiet(workshop_material_item_list(home, NULL, &list) == OK && cJSON_GetArraySize(list) == 1 &&
              find_by_id(list, theme),
          "project filter");
    cJSON_Delete(list);
    check_quiet(workshop_material_item_list(other, NULL, &list) == OK && cJSON_GetArraySize(list) == 2 &&
              find_by_id(list, motif) && find_by_id(list, piece),
          "project filter after the move");
    cJSON_Delete(list);
    check_quiet(workshop_material_item_list(NULL, "piece", &list) == OK && cJSON_GetArraySize(list) == 1 &&
              find_by_id(list, piece),
          "type filter");
    cJSON_Delete(list);
    check_quiet(workshop_material_item_list(other, "theme", &list) == OK && cJSON_GetArraySize(list) == 0,
          "both filters");
    cJSON_Delete(list);
    check_quiet(workshop_material_item_list(NULL, "chord", &list) == URSATZ_ERR_REQUEST,
          "list with an unknown type");

    /* A project that holds items cannot be deleted. */
    check_quiet(workshop_material_project_delete(home) == URSATZ_ERR_CONFLICT, "delete a project with items");
    cJSON *project = NULL;
    check_quiet(workshop_material_project_get(home, &project) == OK && num(project, "item_count") == 1, "item count");
    cJSON_Delete(project);

    /* Items survive a restart. */
    workshop_material_service_shutdown();
    check_quiet(workshop_material_item_list(NULL, NULL, &list) == URSATZ_ERR_INTERNAL,
          "not initialized after shutdown");
    check_quiet(workshop_material_service_init(), "init after shutdown");
    check_quiet(workshop_material_item_list(NULL, NULL, &list) == OK && cJSON_GetArraySize(list) == 3, "items persist");
    cJSON_Delete(list);

    char theme_dir[700];
    check_quiet(item_dir_of(theme, theme_dir, sizeof(theme_dir)), "the theme has a folder");
    workshop_store_set_forget_item(record_forgotten);
    check_quiet(workshop_material_item_delete(theme, false) == OK, "delete the theme");
    workshop_store_set_forget_item(NULL);
    check_quiet(g_forgotten_count == 1 && strcmp(g_forgotten, theme) == 0, "an item delete calls the forget hook once with the item id");
    check_quiet(workshop_material_item_get(theme, &meta, NULL) == URSATZ_ERR_NOT_FOUND, "theme is gone");
    check_quiet(access(theme_dir, F_OK) != 0, "the theme's folder is gone");
    check_quiet(workshop_material_item_delete(theme, false) == URSATZ_ERR_NOT_FOUND, "delete twice");
    check_quiet(workshop_material_project_delete(home) == OK, "delete the emptied project");
    check_quiet(workshop_material_item_delete(motif, false) == OK && workshop_material_item_delete(piece, false) == OK,
          "delete the rest");
    check_quiet(workshop_material_project_delete(other) == OK, "delete the other project");

    cJSON_Delete(body);
    cJSON_Delete(second);
    free(home);
    free(other);
    free(motif);
    free(theme);
    free(piece);
}

#define INVALID URSATZ_ERR_REQUEST

static UrsatzStatus validate(const char *type, const cJSON *body) {
    return workshop_material_body_validate(type, body, NULL);
}

/* A valid motif body with one member of its motif object replaced by the JSON text. */
static cJSON *motif_with(const char *member, const char *json) {
    cJSON *body = cJSON_Parse(kMotifBody);
    cJSON *motif = cJSON_GetObjectItemCaseSensitive(body, "motif");
    cJSON *value = cJSON_Parse(json);
    if (cJSON_GetObjectItemCaseSensitive(motif, member)) {
        cJSON_ReplaceItemInObjectCaseSensitive(motif, member, value);
    } else {
        cJSON_AddItemToObject(motif, member, value);
    }
    return body;
}

/* Expects the motif body with this member replaced to give result. */
static void expect_motif(const char *member, const char *json, UrsatzStatus result) {
    /* What a role, a form, a meter, a pickup, a feature or a tone override means is the Composer's (R-649): the store keeps the draft, and validate and
     * compose refuse it where the Composer reads it (the validate route test). The rows that expected a refusal are not the store's. */
    static const char *const kComposerWords[] = {"role", "form", "meter", "pickup", "features", "tone_overrides", NULL};
    bool composer_word = strcmp(member, "notes") == 0 && strcmp(json, "\"\"") == 0;
    for (const char *const *word = kComposerWords; *word; word++) {
        composer_word = composer_word || strcmp(member, *word) == 0;
    }
    if (result == INVALID && composer_word) {
        return;
    }
    cJSON *body = motif_with(member, json);
    char label[160];
    snprintf(label, sizeof(label), "motif %s = %s", member, json);
    check_quiet(validate("motif", body) == result, label);
    cJSON_Delete(body);
}

static const char *kDerived =
    "{\"source_item\":\"mot-0123456789abcdef\",\"source_version\":1,\"operations\":[],\"seed\":42,"
    "\"kind\":\"variant\",\"kept\":[\"rhythm\"],\"changed\":[\"intervals\",\"contour\"],\"harmony\":null,"
    "\"functions\":[\"continuation\"],\"flag\":\"unflagged\"}";

static void test_body_validation(void) {
    cJSON *body = cJSON_Parse(kMotifBody);
    UrsatzError error = {{0}};
    check_quiet(workshop_material_body_validate("motif", body, &error) == OK && error.message[0] == '\0',
          "a valid motif body");
    cJSON_Delete(body);
    body = cJSON_Parse(kThemeBody);
    check_quiet(validate("theme", body) == OK, "a valid theme body");
    cJSON_Delete(body);
    body = cJSON_Parse(kPieceBody);
    check_quiet(validate("piece", body) == OK, "a valid piece body");
    /* the piece's words are the Composer's: a save keeps the type and size, validate marks the value (R-276) */
    static const struct {
        const char *structure;
        UrsatzStatus result;
        const char *label;
    } pieces[] = {
        {"\"key\":\"D\",\"mode\":\"major\",\"meter\":\"4/4\"", OK, "a plain structure"},
        {"\"key\":\"H\",\"mode\":\"major\",\"meter\":\"4/4\"", OK, "a key the Composer does not read"},
        {"\"key\":\"C# major\",\"mode\":\"major\",\"meter\":\"4/4\"", OK, "a GUI key spelling"},
        {"\"key\":\"D\",\"mode\":\"dorian\",\"meter\":\"7/8\"", OK, "a mode and a meter the Composer does not read"},
        {"\"key\":5,\"mode\":\"major\",\"meter\":\"4/4\"", OK, "a key that is not text (the store keeps the draft; the Composer refuses it)"},
        {"\"key\":\"D\",\"mode\":null,\"meter\":7", OK, "a meter that is not text (the store keeps the draft; the Composer refuses it)"},
        {"\"key\":\"D\",\"material\":\"omt.material.natural-minor\",\"meter\":\"4/4\"", OK, "a structure with a material"},
        {"\"key\":\"D\",\"material\":\"style\",\"meter\":\"4/4\"", OK, "a structure with the material style"},
        {"\"key\":\"D\",\"material\":5,\"meter\":\"4/4\"", OK, "a material that is not text (the store keeps the draft; the Composer refuses it)"},
    };
    for (size_t i = 0; i < sizeof(pieces) / sizeof(pieces[0]); i++) {
        char text[512];
        snprintf(text, sizeof(text),
                 "{\"uses\":[],\"piece\":{\"structure\":{%s,\"sections\":[{\"id\":\"s0\",\"part\":\"body\",\"beats\":8,\"pickup\":\"0\",\"cadence\":null,\"fill\":null}]}}}",
                 pieces[i].structure);
        cJSON *piece_body = cJSON_Parse(text);
        check_quiet(piece_body != NULL, "the piece test body parses");
        check_quiet(validate("piece", piece_body) == pieces[i].result, pieces[i].label);
        cJSON_Delete(piece_body);
    }
    /* a piece names its form (R-430): absent is the game loop, a sized form has sizes and no sections */
    static const char *const kLoopSection =
        "\"sections\":[{\"id\":\"s0\",\"part\":\"body\",\"beats\":8,\"pickup\":\"0\",\"cadence\":null,\"fill\":null}]";
    static const struct {
        const char *structure;
        UrsatzStatus result;
        const char *label;
    } forms[] = {
        {"\"form\":\"game-loop\",\"key\":\"D\",\"mode\":\"major\",\"meter\":\"4/4\",SECTION", OK, "the game loop by name"},
        {"\"form\":null,\"key\":\"D\",\"mode\":\"major\",\"meter\":\"4/4\",SECTION", OK, "a null form is the game loop"},
        {"\"form\":\"waltz\",\"key\":\"D\",\"mode\":\"major\",\"meter\":\"4/4\",SECTION", OK, "a form the Composer does not build (the store keeps the draft; the Composer refuses it)"},
        {"\"form\":7,\"key\":\"D\",\"mode\":\"major\",\"meter\":\"4/4\",SECTION", OK, "a form that is not text (the store keeps the draft; the Composer refuses it)"},
        {"\"form\":\"game-loop\",\"key\":\"D\",\"mode\":\"major\",\"meter\":\"4/4\",\"modulate\":true,SECTION", OK, "modulate on the game loop (the store keeps the draft; the Composer refuses it)"},
        {"\"form\":\"game-loop\",\"key\":\"D\",\"mode\":\"major\",\"meter\":\"4/4\",\"bars\":16,SECTION", OK, "bars on the game loop (the store keeps the draft; the Composer refuses it)"},
        {"\"form\":\"game-loop\",\"key\":\"D\",\"mode\":\"major\",\"meter\":\"4/4\",\"bars\":0,\"count\":0,\"modulate\":false,SECTION", OK, "no size and no modulation on the game loop"},
        {"\"key\":\"D\",\"mode\":\"major\",\"meter\":\"4/4\",\"sections\":[{\"id\":\"s0\",\"part\":\"intro\",\"beats\":8,\"pickup\":\"0\",\"cadence\":null,\"fill\":null}]", OK, "a game loop with no body section (the store keeps the draft; the Composer refuses it)"},
        {"\"key\":\"D\",\"mode\":\"major\",\"meter\":\"4/4\",\"sections\":[{\"id\":\"s0\",\"part\":\"body\",\"beats\":4096,\"pickup\":\"0\",\"cadence\":null,\"fill\":null}]", OK, "4096 beats, the vocabulary's limit"},
        {"\"key\":\"D\",\"mode\":\"major\",\"meter\":\"4/4\",\"sections\":[{\"id\":\"s0\",\"part\":\"body\",\"beats\":4097,\"pickup\":\"0\",\"cadence\":null,\"fill\":null}]", OK, "4097 beats (the store keeps the draft; the Composer refuses it)"},
        {"\"form\":\"rounded-binary\",\"key\":\"C\",\"mode\":\"major\",\"meter\":\"4/4\"", OK, "a form with sizes and its default size"},
        {"\"form\":\"rounded-binary\",\"key\":\"C\",\"mode\":\"major\",\"meter\":\"4/4\",\"sections\":null,\"bars\":24,\"count\":0,\"modulate\":true", OK, "a size of the form, modulating"},
        {"\"form\":\"rounded-binary\",\"key\":\"C\",\"mode\":\"major\",\"meter\":\"4/4\",\"bars\":20", OK, "a size the form does not have (the store keeps the draft; the Composer refuses it)"},
        {"\"form\":\"rounded-binary\",\"key\":\"C\",\"mode\":\"major\",\"meter\":\"4/4\",\"bars\":16.5", OK, "bars that are not a whole number (the store keeps the draft; the Composer refuses it)"},
        {"\"form\":\"rounded-binary\",\"key\":\"C\",\"mode\":\"major\",\"meter\":\"4/4\",\"modulate\":\"yes\"", OK, "modulate that is not a boolean (the store keeps the draft; the Composer refuses it)"},
        {"\"form\":\"rounded-binary\",\"key\":\"C\",\"mode\":\"major\",\"meter\":\"4/4\",SECTION", OK, "sections on a form with sizes (the store keeps the draft; the Composer refuses it)"},
        {"\"form\":\"variations\",\"key\":\"C\",\"mode\":\"major\",\"meter\":\"4/4\",\"count\":3", OK, "a size picked by its count"},
        {"\"form\":\"variations\",\"key\":\"C\",\"mode\":\"major\",\"meter\":\"4/4\",\"count\":9", OK, "a count the form has no size of (the store keeps the draft; the Composer refuses it)"},
        {"\"form\":\"variations\",\"key\":\"C\",\"mode\":\"major\",\"meter\":\"4/4\",\"bars\":48,\"count\":5", OK, "a size picked by bars and count"},
        {"\"form\":\"variations\",\"key\":\"C\",\"mode\":\"major\",\"meter\":\"4/4\",\"bars\":48,\"count\":4", OK, "bars and count of two different sizes (the store keeps the draft; the Composer refuses it)"},
        {"\"form\":\"loop\",\"key\":\"C\",\"mode\":\"major\",\"meter\":\"4/4\",\"modulate\":true", OK, "modulate on a form that does not modulate (the store keeps the draft; the Composer refuses it)"},
        {"\"form\":\"loop\",\"key\":\"C\",\"mode\":\"major\",\"meter\":\"4/4\",\"modulate\":false", OK, "no modulation on a form that does not modulate"},
    };
    for (size_t i = 0; i < sizeof(forms) / sizeof(forms[0]); i++) {
        char structure[1024];
        const char *marker = strstr(forms[i].structure, "SECTION");
        if (marker) {
            snprintf(structure, sizeof(structure), "%.*s%s", (int)(marker - forms[i].structure), forms[i].structure, kLoopSection);
        } else {
            snprintf(structure, sizeof(structure), "%s", forms[i].structure);
        }
        char text[1400];
        snprintf(text, sizeof(text), "{\"uses\":[],\"piece\":{\"structure\":{%s}}}", structure);
        cJSON *form_body = cJSON_Parse(text);
        check_quiet(form_body != NULL, "the form test body parses");
        check_quiet(validate("piece", form_body) == forms[i].result, forms[i].label);
        cJSON_Delete(form_body);
    }
    static const struct {
        const char *section;
        UrsatzStatus result;
        const char *label;
    } sections[] = {
        {"\"pickup\":\"5\",\"cadence\":\"XX\",\"fill\":null", OK, "a pickup longer than a bar and a cadence the Composer does not read"},
        {"\"pickup\":\"0\",\"cadence\":null,\"fill\":{\"kind\":\"progression\",\"numerals\":\"I $ V\"}", OK, "numerals the Composer does not read"},
        {"\"pickup\":\"0\",\"cadence\":null,\"fill\":{\"kind\":\"progression\",\"numerals\":5}", OK, "numerals that are not text (the store keeps the draft; the Composer refuses it)"},
        {"\"pickup\":\"0\",\"cadence\":7,\"fill\":null", OK, "a cadence that is not text (the store keeps the draft; the Composer refuses it)"},
        {"\"pickup\":\"x\",\"cadence\":null,\"fill\":null", OK, "a pickup that is not beats (the store keeps the draft; the Composer refuses it)"},
        {"\"pickup\":\"0\",\"cadence\":null,\"function\":\"transition\",\"fill\":null", OK, "a section function that is text"},
        {"\"pickup\":\"0\",\"cadence\":null,\"function\":null,\"fill\":null", OK, "a section function that is null"},
        {"\"pickup\":\"0\",\"cadence\":null,\"function\":7,\"fill\":null", OK, "a section function that is not text (the store keeps the draft; the Composer refuses it)"},
    };
    for (size_t i = 0; i < sizeof(sections) / sizeof(sections[0]); i++) {
        char text[512];
        snprintf(text, sizeof(text),
                 "{\"uses\":[],\"piece\":{\"structure\":{\"key\":\"D\",\"mode\":\"major\",\"meter\":\"4/4\",\"sections\":[{\"id\":\"s0\",\"part\":\"body\",\"beats\":8,%s}]}}}",
                 sections[i].section);
        cJSON *section_body = cJSON_Parse(text);
        check_quiet(section_body != NULL, "the section test body parses");
        check_quiet(validate("piece", section_body) == sections[i].result, sections[i].label);
        cJSON_Delete(section_body);
    }
    cJSON_Delete(body);

    body = cJSON_Parse("[]");
    check_quiet(validate("motif", body) == INVALID, "a body must be an object");
    cJSON_Delete(body);
    body = cJSON_Parse(kMotifBody);
    check_quiet(validate("chord", body) == URSATZ_ERR_REQUEST, "an unknown type");
    check_quiet(validate(NULL, body) == URSATZ_ERR_REQUEST, "no type");
    check_quiet(validate("motif", NULL) == INVALID, "no body");
    cJSON_Delete(body);

    /* uses */
    body = cJSON_Parse("{\"motif\":{}}");
    check_quiet(validate("motif", body) == INVALID, "uses is required");
    cJSON_Delete(body);
    body = cJSON_Parse("{\"uses\":{},\"theme\":{\"form\":null,\"slots\":[]}}");
    check_quiet(validate("theme", body) == INVALID, "uses must be an array");
    cJSON_Delete(body);
    body = cJSON_Parse("{\"uses\":[{\"item_id\":\"mot-0123456789abcdef\",\"version\":1,\"copy\":{}}],"
                       "\"theme\":{\"form\":null,\"slots\":[]}}");
    check_quiet(validate("theme", body) == OK, "a use with a copy");
    cJSON_Delete(body);
    body = cJSON_Parse("{\"uses\":[{\"item_id\":\"mot-0123456789abcdef\",\"version\":0}],"
                       "\"theme\":{\"form\":null,\"slots\":[]}}");
    check_quiet(validate("theme", body) == INVALID, "a use needs a version of 1 or more");
    cJSON_Delete(body);
    body = cJSON_Parse("{\"uses\":[{\"item_id\":\"../x\",\"version\":1}],\"theme\":{\"form\":null,\"slots\":[]}}");
    check_quiet(validate("theme", body) == INVALID, "a use needs a valid item id");
    cJSON_Delete(body);

    /* one type's member in another type's body */
    body = cJSON_Parse("{\"uses\":[],\"theme\":{\"form\":null,\"slots\":[]},\"piece\":{\"structure\":null}}");
    check_quiet(validate("theme", body) == INVALID, "a theme body must not carry a piece member");
    cJSON_Delete(body);
    body = cJSON_Parse("{\"uses\":[],\"motif\":{},\"piece\":{\"structure\":null}}");
    check_quiet(validate("piece", body) == INVALID, "a piece body must not carry a motif member");
    cJSON_Delete(body);
    body = cJSON_Parse(kThemeBody);
    check_quiet(validate("motif", body) == INVALID, "a motif body needs a motif member");
    check_quiet(validate("piece", body) == INVALID, "a piece body needs a piece member");
    cJSON_Delete(body);
    body = cJSON_Parse("{\"uses\":[],\"theme\":{\"form\":null}}");
    check_quiet(validate("theme", body) == INVALID, "a theme needs slots");
    cJSON_Delete(body);

    /* the v2 theme body: no key or pool, repeated slot items, ideas and chords */
    static const char *const kRef = "\"item_id\":\"mot-0123456789abcdef\",\"version\":1";
    struct {
        const char *theme;
        UrsatzStatus result;
        const char *label;
    } v2[] = {
        {"\"form\":{\"into\":\"sentence\"},\"slots\":[{\"slot\":\"basic\",REF}]", OK, "a form with no key"},
        {"\"form\":{\"into\":\"sentence\",\"key\":\"D\"},\"slots\":[{\"slot\":\"basic\",REF}]", OK, "a form key is refused (the store keeps the draft; the Composer refuses it)"},
        {"\"form\":{\"into\":\"sentence\",\"key\":null},\"slots\":[{\"slot\":\"basic\",REF}]", OK, "a null form key is refused (the store keeps the draft; the Composer refuses it)"},
        {"\"form\":null,\"slots\":[{\"slot\":\"basic\",REF},{\"slot\":\"cadential\",REF}]", OK, "one item in two slots"},
        {"\"form\":null,\"slots\":[{\"slot\":\"basic\",REF},{\"slot\":\"basic\",REF}]", OK, "a slot named twice (the store keeps the draft; the Composer refuses it)"},
        {"\"form\":null,\"slots\":[{\"slot\":\"basic\",REF}],\"pool\":\"approved\"", OK, "a pool is refused (the store keeps the draft; the Composer refuses it)"},
        {"\"form\":null,\"slots\":[],\"pool\":\"none\"", OK, "a pool is refused on a placeholder too (the store keeps the draft; the Composer refuses it)"},
        /* the Composer's words are not judged on a save: validate marks them, compose refuses them (R-276) */
        {"\"form\":{\"into\":\"sentence\",\"meter\":\"5/4\"},\"slots\":[{\"slot\":\"basic\",REF}]", OK, "a meter the Composer does not read"},
        {"\"form\":{\"into\":\"hybrid\",\"hybrid\":\"fugue\"},\"slots\":[{\"slot\":\"basic\",REF}]", OK, "a hybrid the Composer does not read"},
        {"\"form\":{\"into\":\"rondo\",\"mode\":\"dorian\",\"cadence\":\"DC\",\"first_cadence\":\"PAC\",\"bars\":6},\"slots\":[{\"slot\":\"basic\",REF}]",
         OK, "an into, mode, cadences and bars the Composer does not read"},
        {"\"form\":{\"into\":\"sentence\",\"material\":\"omt.material.major\"},\"slots\":[{\"slot\":\"basic\",REF}]", OK, "a form with a material"},
        {"\"form\":{\"into\":\"sentence\",\"material\":\"style\"},\"slots\":[{\"slot\":\"basic\",REF}]", OK, "a form with the material style"},
        {"\"form\":{\"into\":\"sentence\",\"material\":7},\"slots\":[{\"slot\":\"basic\",REF}]", OK, "a form material that is not text (the store keeps the draft; the Composer refuses it)"},
        {"\"form\":{\"into\":\"sentence\",\"meter\":4},\"slots\":[{\"slot\":\"basic\",REF}]", OK, "a meter that is not text (the store keeps the draft; the Composer refuses it)"},
        {"\"form\":{\"into\":\"sentence\",\"bars\":\"8\"},\"slots\":[{\"slot\":\"basic\",REF}]", OK, "bars that are not a number (the store keeps the draft; the Composer refuses it)"},
        {"\"form\":null,\"slots\":[{\"slot\":\"basic\",REF}],\"tempo\":5", OK, "a tempo under 20"},
        {"\"form\":null,\"slots\":[{\"slot\":\"basic\",REF}],\"tempo\":1.5", OK, "a tempo that is not whole (the store keeps the draft; the Composer refuses it)"},
        {"\"form\":null,\"slots\":[{\"slot\":\"basic\",REF}],\"ideas\":[]", OK, "no ideas"},
        {"\"form\":null,\"slots\":[{\"slot\":\"basic\",REF}],\"ideas\":[{\"idea\":\"repetition\",REF},{\"idea\":\"cadential 2\",REF}]", OK,
         "ideas of a motif"},
        {"\"form\":null,\"slots\":[{\"slot\":\"basic\",REF}],\"ideas\":[{\"idea\":\"repetition\",REF},{\"idea\":\"repetition\",REF}]",
         OK, "an idea named twice (the store keeps the draft; the Composer refuses it)"},
        {"\"form\":null,\"slots\":[{\"slot\":\"basic\",REF}],\"ideas\":[{\"idea\":\"coda\",REF}]", OK, "an unknown idea (the store keeps the draft; the Composer refuses it)"},
        {"\"form\":null,\"slots\":[{\"slot\":\"basic\",REF}],\"ideas\":[{\"idea\":\"repetition\"}]", INVALID, "an idea with no motif"},
        {"\"form\":null,\"slots\":[{\"slot\":\"basic\",REF}],\"ideas\":{}", INVALID, "ideas that are not a list"},
        {"\"form\":null,\"slots\":[{\"slot\":\"basic\",REF}],\"chords\":[{\"bar\":0,\"text\":\"I\"},{\"bar\":7,\"text\":\"V\"}]", OK,
         "chords in bars 0 and 7"},
        {"\"form\":null,\"slots\":[{\"slot\":\"basic\",REF}],\"chords\":[{\"bar\":8,\"text\":\"I\"}]", OK, "a chord bar of 8 (the store keeps the draft; the Composer refuses it)"},
        {"\"form\":null,\"slots\":[{\"slot\":\"basic\",REF}],\"chords\":[{\"bar\":-1,\"text\":\"I\"}]", OK, "a negative chord bar (the store keeps the draft; the Composer refuses it)"},
        {"\"form\":null,\"slots\":[{\"slot\":\"basic\",REF}],\"chords\":[{\"bar\":0.5,\"text\":\"I\"}]", OK, "a fractional chord bar (the store keeps the draft; the Composer refuses it)"},
        {"\"form\":null,\"slots\":[{\"slot\":\"basic\",REF}],\"chords\":[{\"bar\":1,\"text\":\"I\"},{\"bar\":1,\"text\":\"V\"}]", OK,
         "a chord bar named twice (the store keeps the draft; the Composer refuses it)"},
        {"\"form\":null,\"slots\":[{\"slot\":\"basic\",REF}],\"chords\":[{\"bar\":1,\"text\":5}]", OK, "a chord text that is not a string (the store keeps the draft; the Composer refuses it)"},
        {"\"form\":null,\"slots\":[{\"slot\":\"basic\",REF}],\"chords\":[{\"bar\":1,\"text\":\"" "00000000001111111111222222222233333333334444444444555555555566666\"}]",
         OK, "a chord text of 65 characters (the store keeps the draft; the Composer refuses it)"},
    };
    for (size_t i = 0; i < sizeof(v2) / sizeof(v2[0]); i++) {
        char text[900] = "{\"uses\":[],\"theme\":{";
        for (const char *c = v2[i].theme; *c && strlen(text) + 80 < sizeof(text); c++) {
            if (strncmp(c, "REF", 3) == 0) {
                strcat(text, kRef);
                c += 2;
            } else {
                strncat(text, c, 1);
            }
        }
        strcat(text, "}}");
        body = cJSON_Parse(text);
        check_quiet(body != NULL, "the v2 test body parses");
        check_quiet(validate("theme", body) == v2[i].result, v2[i].label);
        cJSON_Delete(body);
    }

    /* motif members */
    expect_motif("role", "\"accompaniment\"", OK);
    expect_motif("role", "\"bass\"", INVALID);
    expect_motif("role", "null", INVALID);
    expect_motif("form", "\"rhythm\"", OK);
    expect_motif("form", "\"contour\"", OK);
    expect_motif("form", "\"partial\"", INVALID);
    expect_motif("notes", "\"\"", INVALID);
    expect_motif("notes", "5", INVALID);

    char *long_notes = calloc(1, 4010);
    memset(long_notes, 'x', 4001);
    long_notes[0] = '"';
    long_notes[4000] = '"';
    long_notes[4001] = '\0';
    expect_motif("notes", long_notes, OK); /* 3999 characters between the quotes */
    memset(long_notes, 'x', 4003);
    long_notes[0] = '"';
    long_notes[4002] = '"';
    long_notes[4003] = '\0';
    expect_motif("notes", long_notes, INVALID); /* 4001 characters */
    free(long_notes);

    expect_motif("meter", "\"6/8\"", OK);
    expect_motif("meter", "\"12/8\"", OK);
    expect_motif("meter", "\"4\"", INVALID);
    expect_motif("meter", "\"4/0\"", INVALID);
    expect_motif("meter", "\"100/4\"", INVALID);
    expect_motif("meter", "\"a/b\"", INVALID);
    expect_motif("meter", "44", INVALID);
    expect_motif("key", "null", OK);
    expect_motif("key", "\"A major\"", OK);
    expect_motif("key", "7", INVALID);
    expect_motif("material", "null", OK);
    expect_motif("material", "\"omt.material.natural-minor\"", OK);
    expect_motif("material", "7", INVALID);
    expect_motif("pickup", "\"1/2\"", OK);
    expect_motif("pickup", "\"3\"", OK);
    expect_motif("pickup", "\"-1\"", INVALID);
    expect_motif("pickup", "\"1/0\"", INVALID);
    expect_motif("pickup", "\"0.5\"", INVALID);
    expect_motif("pickup", "0", INVALID);
    expect_motif("features", "[]", OK);
    expect_motif("features", "{}", INVALID);
    expect_motif("features", "[{\"label\":\"abcdefgh\",\"first\":1,\"last\":1}]", OK);
    expect_motif("features", "[{\"label\":\"abcdefghi\",\"first\":1,\"last\":1}]", INVALID);
    expect_motif("features", "[{\"label\":\"\",\"first\":0,\"last\":1}]", INVALID);
    expect_motif("features", "[{\"label\":\"a\",\"first\":-1,\"last\":1}]", INVALID);
    expect_motif("features", "[{\"label\":\"a\",\"first\":3,\"last\":2}]", INVALID);
    expect_motif("features", "[{\"label\":\"a\",\"first\":0.5,\"last\":2}]", INVALID);
    expect_motif("features", "[{\"label\":\"a\",\"first\":0}]", INVALID);
    expect_motif("harmony", "null", OK);
    expect_motif("harmony", "\"I V6@2 I@3\"", OK);
    expect_motif("harmony", "5", INVALID);
    expect_motif("harmony", "[]", INVALID);
    expect_motif("tone_overrides", "null", OK);
    expect_motif("tone_overrides", "{}", OK);
    expect_motif("tone_overrides", "{\"2\":\"passing\",\"3\":\"chord\",\"64\":\"escape\"}", OK);
    expect_motif("tone_overrides", "{\"0\":\"passing\"}", INVALID);
    expect_motif("tone_overrides", "{\"65\":\"passing\"}", INVALID);
    expect_motif("tone_overrides", "{\"-1\":\"passing\"}", INVALID);
    expect_motif("tone_overrides", "{\"x\":\"chord\"}", INVALID);
    expect_motif("tone_overrides", "{\"2\":\"bogus\"}", OK); /* a role the Composer does not read: validate marks it */
    expect_motif("tone_overrides", "{\"2\":5}", INVALID);
    expect_motif("tone_overrides", "[]", INVALID);
    expect_motif("analysis", "{\"degrees\":[]}", OK);
    expect_motif("analysis", "[]", INVALID);

    /* derived */
    expect_motif("derived", kDerived, OK);
    expect_motif("derived", "[]", INVALID);
    const char *edits[][2] = {
        {"\"kind\":\"variant\"", "\"kind\":\"copy\""},
        {"\"flag\":\"unflagged\"", "\"flag\":\"maybe\""},
        {"\"kept\":[\"rhythm\"]", "\"kept\":[\"tempo\"]"},
        {"\"changed\":[\"intervals\",\"contour\"]", "\"changed\":\"intervals\""},
        {"\"functions\":[\"continuation\"]", "\"functions\":[\"ending\"]"},
        {"\"source_version\":1", "\"source_version\":0"},
        {"\"source_item\":\"mot-0123456789abcdef\"", "\"source_item\":\"a b\""},
        {"\"operations\":[]", "\"operations\":null"},
        {"\"seed\":42", "\"seed\":\"x\""},
        {"\"seed\":42", "\"seed\":42,\"remoteness\":2"},
        {"\"seed\":42", "\"seed\":42,\"harmony\":7"},
    };
    for (size_t i = 0; i < sizeof(edits) / sizeof(edits[0]); i++) {
        char *text = strdup(kDerived);
        char *at = strstr(text, edits[i][0]);
        check_quiet(at != NULL, "derived edit applies");
        char *edited = calloc(1, strlen(text) + strlen(edits[i][1]) + 1);
        if (at) {
            memcpy(edited, text, (size_t)(at - text));
            strcat(edited, edits[i][1]);
            strcat(edited, at + strlen(edits[i][0]));
            expect_motif("derived", edited, INVALID);
        }
        free(edited);
        free(text);
    }

    /* A malformed body is refused on create and add_version, and writes nothing. */
    char *home = new_project("Validation home");
    cJSON *bad = motif_with("derived", "5"); /* the lineage of a motif is the store's */
    cJSON *item = NULL;
    check_quiet(workshop_material_item_create_noted("motif", "Bad", home, bad, NULL, &item) == INVALID && item == NULL,
          "create refuses a bad body");
    cJSON *list = NULL;
    check_quiet(workshop_material_item_list(home, NULL, &list) == OK && cJSON_GetArraySize(list) == 0, "no item written");
    cJSON_Delete(list);
    char *motif = new_item("motif", "Good", home, kMotifBody);
    check_quiet(motif != NULL, "create a good motif");
    check_quiet(workshop_material_item_add_version(motif, bad, NULL, &item) == INVALID && item == NULL,
          "add_version refuses a bad body");
    check_quiet(workshop_material_item_get(motif, &item, NULL) == OK && num(item, "current_version") == 1,
          "no version written");
    cJSON_Delete(item);
    cJSON *theme = cJSON_Parse(kThemeBody);
    check_quiet(workshop_material_item_add_version(motif, theme, NULL, NULL) == INVALID,
          "add_version refuses another type's body");
    cJSON_Delete(theme);
    check_quiet(workshop_material_item_delete(motif, false) == OK, "delete the motif");
    check_quiet(workshop_material_project_delete(home) == OK, "delete the project");
    cJSON_Delete(bad);
    free(motif);
    free(home);
}

/* A motif body with the given notes. */
static cJSON *motif_body_with_notes(const char *notes) {
    cJSON *body = cJSON_Parse(kMotifBody);
    cJSON *motif = cJSON_GetObjectItemCaseSensitive(body, "motif");
    cJSON_ReplaceItemInObjectCaseSensitive(motif, "notes", cJSON_CreateString(notes));
    return body;
}

/* A theme body whose single use names item_id at version, with a bogus client copy. */
static cJSON *theme_body_using(const char *item_id, int version) {
    cJSON *body = cJSON_Parse(kThemeBody);
    cJSON *use = cJSON_CreateObject();
    cJSON_AddStringToObject(use, "item_id", item_id);
    cJSON_AddNumberToObject(use, "version", version);
    cJSON *copy = cJSON_AddObjectToObject(use, "copy");
    cJSON_AddStringToObject(copy, "bogus", "client");
    cJSON_AddItemToArray(cJSON_GetObjectItemCaseSensitive(body, "uses"), use);
    return body;
}

static const cJSON *first_use(const cJSON *body) {
    return cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(body, "uses"), 0);
}

static const char *copied_notes(const cJSON *use) {
    const cJSON *copy = cJSON_GetObjectItemCaseSensitive(use, "copy");
    return str(cJSON_GetObjectItemCaseSensitive(copy, "motif"), "notes");
}

/* Creates a theme in project_id that uses source_id at version 1 and returns its id (malloc'd). */
static char *new_theme_using(const char *name, const char *project_id, const char *source_id) {
    cJSON *body = theme_body_using(source_id, 1);
    cJSON *item = NULL;
    char *id = NULL;
    if (workshop_material_item_create_noted("theme", name, project_id, body, NULL, &item) == OK) {
        id = strdup(str(item, "id"));
    }
    cJSON_Delete(item);
    cJSON_Delete(body);
    return id;
}

static void test_references(void) {
    char *home = new_project("Refs source");
    char *other = new_project("Refs user");
    char *source = new_item("motif", "Source motif", home, kMotifBody);
    check_quiet(source != NULL, "create the source motif");

    /* A use is copied from the version it names, in any project; a client copy is replaced. */
    char *user = new_theme_using("User theme", other, source);
    check_quiet(user != NULL, "create a theme that uses the motif");
    cJSON *body = NULL;
    check_quiet(workshop_material_item_get(user, NULL, &body) == OK, "get the user's body");
    check_quiet(strcmp(copied_notes(first_use(body)), "D4:q F4:e A4:e D5:h") == 0, "the copy holds the source's notes");
    check_quiet(!cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(first_use(body), "copy"), "bogus"),
          "the client copy is replaced");
    cJSON_Delete(body);

    /* Unknown items and versions, and an item using itself, are refused. */
    cJSON *bad = theme_body_using("mot-0000000000000000", 1);
    check_quiet(workshop_material_item_create_noted("theme", "Ghost use", other, bad, NULL, NULL) == INVALID,
          "unknown source item");
    cJSON_Delete(bad);
    bad = theme_body_using(source, 2);
    check_quiet(workshop_material_item_create_noted("theme", "Future use", other, bad, NULL, NULL) == INVALID,
          "unknown source version");
    check_quiet(workshop_material_item_add_version(user, bad, NULL, NULL) == INVALID, "add_version refuses unknown version");
    cJSON_Delete(bad);
    bad = theme_body_using(user, 1);
    check_quiet(workshop_material_item_add_version(user, bad, NULL, NULL) == INVALID, "an item may not use itself");
    cJSON_Delete(bad);
    cJSON *meta = NULL;
    check_quiet(workshop_material_item_get(user, &meta, NULL) == OK && num(meta, "current_version") == 1,
          "refused versions are not written");
    cJSON_Delete(meta);

    /* uses_status: current while the source is on the used version. */
    cJSON *status = NULL;
    check_quiet(workshop_material_uses_status(user, &status) == OK && cJSON_GetArraySize(status) == 1, "one use reported");
    const cJSON *entry = cJSON_GetArrayItem(status, 0);
    check_quiet(strcmp(str(entry, "item_id"), source) == 0 && strcmp(str(entry, "name"), "Source motif") == 0 &&
              strcmp(str(entry, "type"), "motif") == 0 && strcmp(str(entry, "project_id"), home) == 0,
          "status names the source");
    check_quiet(num(entry, "version") == 1 && num(entry, "current_version") == 1, "status versions");
    check_quiet(cJSON_IsFalse(cJSON_GetObjectItemCaseSensitive(entry, "updated")), "not updated yet");
    cJSON_Delete(status);
    check_quiet(workshop_material_uses_status("thm-0000000000000000", &status) == URSATZ_ERR_NOT_FOUND,
          "status of an unknown item");

    /* The source moves on: the use reports it and used_by lists the user. */
    cJSON *next = motif_body_with_notes("D4:q E4:q F4:h");
    check_quiet(workshop_material_item_add_version(source, next, "second", NULL) == OK, "source gets version 2");
    cJSON_Delete(next);
    check_quiet(workshop_material_uses_status(user, &status) == OK, "status after the source moved");
    entry = cJSON_GetArrayItem(status, 0);
    check_quiet(cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(entry, "updated")) && num(entry, "current_version") == 2 &&
              num(entry, "version") == 1,
          "the use is flagged updated");
    cJSON_Delete(status);

    cJSON *users = NULL;
    check_quiet(workshop_material_used_by(source, &users) == OK && cJSON_GetArraySize(users) == 1, "one user");
    entry = cJSON_GetArrayItem(users, 0);
    check_quiet(strcmp(str(entry, "id"), user) == 0 && strcmp(str(entry, "name"), "User theme") == 0 &&
              strcmp(str(entry, "type"), "theme") == 0 && strcmp(str(entry, "project_id"), other) == 0 &&
              num(entry, "version") == 1,
          "used_by names the user across projects");
    cJSON_Delete(users);
    check_quiet(workshop_material_used_by(user, &users) == OK && cJSON_GetArraySize(users) == 0, "nothing uses the theme");
    cJSON_Delete(users);

    /* update_use writes a new version that points at the source's latest. */
    check_quiet(workshop_material_update_use(user, "mot-0000000000000000", NULL) == URSATZ_ERR_NOT_FOUND,
          "update_use of a source the theme does not use");
    check_quiet(workshop_material_update_use(user, source, &meta) == OK, "update_use");
    check_quiet(num(meta, "current_version") == 2, "update_use advances the version");
    const cJSON *versions = cJSON_GetObjectItemCaseSensitive(meta, "versions");
    check_quiet(strcmp(str(cJSON_GetArrayItem(versions, 1), "note"), "updated Source motif to v2") == 0, "update_use note");
    cJSON_Delete(meta);
    check_quiet(workshop_material_item_get(user, NULL, &body) == OK && num(first_use(body), "version") == 2 &&
              strcmp(copied_notes(first_use(body)), "D4:q E4:q F4:h") == 0,
          "the new version uses the latest copy");
    cJSON_Delete(body);
    check_quiet(workshop_material_item_get_version(user, 1, &body) == OK && num(first_use(body), "version") == 1 &&
              strcmp(copied_notes(first_use(body)), "D4:q F4:e A4:e D5:h") == 0,
          "version 1 keeps its old copy");
    cJSON_Delete(body);
    check_quiet(workshop_material_uses_status(user, &status) == OK &&
              cJSON_IsFalse(cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(status, 0), "updated")),
          "no longer updated");
    cJSON_Delete(status);

    /* A used item is refused a plain delete. */
    check_quiet(workshop_material_item_delete(source, false) == URSATZ_ERR_CONFLICT, "used item is protected");
    check_quiet(workshop_material_item_get(source, NULL, NULL) == OK, "the used item is still there");

    check_quiet(workshop_material_item_delete(user, false) == OK, "delete the user");
    check_quiet(workshop_material_item_delete(source, false) == OK, "delete the source once unused");
    check_quiet(workshop_material_project_delete(home) == OK && workshop_material_project_delete(other) == OK,
          "delete the projects");
    free(user);
    free(source);
    free(home);
    free(other);
}

static void test_forced_delete(void) {
    char *home = new_project("Forced home");
    char *source = new_item("motif", "Forced source", home, kMotifBody);
    char *user = new_theme_using("Forced user", home, source);
    check_quiet(source && user, "create the source and its user");

    check_quiet(workshop_material_item_delete(source, false) == URSATZ_ERR_CONFLICT, "refused without force");
    check_quiet(workshop_material_item_delete(source, true) == OK, "forced delete");
    check_quiet(workshop_material_item_get(source, NULL, NULL) == URSATZ_ERR_NOT_FOUND, "the source is gone");

    cJSON *body = NULL;
    check_quiet(workshop_material_item_get(user, NULL, &body) == OK, "the user still loads");
    check_quiet(strcmp(copied_notes(first_use(body)), "D4:q F4:e A4:e D5:h") == 0, "the copy is untouched");
    cJSON_Delete(body);

    cJSON *status = NULL;
    check_quiet(workshop_material_uses_status(user, &status) == OK && cJSON_GetArraySize(status) == 1, "status still lists the use");
    const cJSON *entry = cJSON_GetArrayItem(status, 0);
    check_quiet(cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(entry, "missing")) &&
              strcmp(str(entry, "item_id"), source) == 0 && num(entry, "version") == 1,
          "the use reports a missing source");
    cJSON_Delete(status);
    check_quiet(workshop_material_update_use(user, source, NULL) == URSATZ_ERR_NOT_FOUND,
          "a missing source cannot be updated to");

    /* The copy keeps being copied into later versions of the user. */
    check_quiet(workshop_material_item_get(user, NULL, &body) == OK, "get the user's body");
    check_quiet(workshop_material_item_add_version(user, body, "again", NULL) == URSATZ_ERR_REQUEST,
          "a new version cannot name a deleted source");
    cJSON_Delete(body);

    check_quiet(workshop_material_item_delete(user, false) == OK, "delete the user");
    check_quiet(workshop_material_project_delete(home) == OK, "delete the project");
    free(user);
    free(source);
    free(home);
}

/* Creates a motif whose current version is derived from source_id; standalone is "" or ",\"standalone\":true" and so on. */
static char *new_variant(const char *name, const char *project_id, const char *source_id, const char *standalone) {
    char derived[600];
    snprintf(derived, sizeof(derived),
             "{\"source_item\":\"%s\",\"source_version\":1,\"operations\":[],\"seed\":42,\"kind\":\"variant\","
             "\"kept\":[\"rhythm\"],\"changed\":[\"intervals\"],\"harmony\":null,\"functions\":[\"continuation\"],"
             "\"flag\":\"unflagged\"%s}",
             source_id, standalone);
    cJSON *body = motif_with("derived", derived);
    cJSON *item = NULL;
    char *id = NULL;
    if (workshop_material_item_create_noted("motif", name, project_id, body, NULL, &item) == OK) {
        id = strdup(str(item, "id"));
    }
    cJSON_Delete(item);
    cJSON_Delete(body);
    return id;
}

static bool exists(const char *id) {
    return workshop_material_item_get(id, NULL, NULL) == OK;
}

/* R-187: a derived motif is a variant unless standalone, it reports variant_of, and it goes with its motif. */
static void test_variant_ownership(void) {
    char *home = new_project("Variant home");
    char *parent = new_item("motif", "Parent", home, kMotifBody);
    char *variant = new_variant("Parent v1", home, parent, "");
    char *grandchild = new_variant("Parent v1 v1", home, variant, "");
    char *promoted = new_variant("Promoted", home, parent, ",\"standalone\":true");
    check_quiet(parent && variant && grandchild && promoted, "create a motif, a variant, its variant and a standalone copy");

    /* Validation and the helper. */
    char text[700];
    snprintf(text, sizeof(text), "%s", kDerived);
    text[strlen(text) - 1] = '\0';
    char with_flag[760];
    snprintf(with_flag, sizeof(with_flag), "%s,\"standalone\":false}", text);
    expect_motif("derived", with_flag, OK);
    snprintf(with_flag, sizeof(with_flag), "%s,\"standalone\":\"yes\"}", text);
    expect_motif("derived", with_flag, URSATZ_ERR_REQUEST);
    cJSON *plain = cJSON_Parse(kMotifBody);
    cJSON *derived_body = motif_with("derived", kDerived);
    snprintf(with_flag, sizeof(with_flag), "%s,\"standalone\":true}", text);
    cJSON *standalone_body = motif_with("derived", with_flag);
    cJSON *theme = cJSON_Parse(kThemeBody);
    check_quiet(!workshop_material_item_is_variant(plain), "a motif without derived is not a variant");
    check_quiet(workshop_material_item_is_variant(derived_body), "a derived motif is a variant");
    check_quiet(!workshop_material_item_is_variant(standalone_body), "a standalone derived motif is not a variant");
    check_quiet(!workshop_material_item_is_variant(theme) && !workshop_material_item_is_variant(NULL),
          "a theme body or no body is not a variant");
    cJSON_Delete(plain);
    cJSON_Delete(derived_body);
    cJSON_Delete(standalone_body);
    cJSON_Delete(theme);

    /* variant_of in the list and in the get. */
    cJSON *items = NULL;
    check_quiet(workshop_material_item_list(home, NULL, &items) == OK, "list the project");
    const cJSON *entry = find_by_id(items, parent);
    check_quiet(entry && cJSON_IsNull(cJSON_GetObjectItemCaseSensitive(entry, "variant_of")), "a motif has variant_of null");
    entry = find_by_id(items, variant);
    check_quiet(entry && strcmp(str(entry, "variant_of"), parent) == 0, "a variant names its motif in variant_of");
    entry = find_by_id(items, grandchild);
    check_quiet(entry && strcmp(str(entry, "variant_of"), variant) == 0, "a variant of a variant names the variant");
    entry = find_by_id(items, promoted);
    check_quiet(entry && cJSON_IsNull(cJSON_GetObjectItemCaseSensitive(entry, "variant_of")), "a standalone copy has variant_of null");
    cJSON_Delete(items);
    cJSON *meta = NULL;
    check_quiet(workshop_material_item_get(variant, &meta, NULL) == OK && strcmp(str(meta, "variant_of"), parent) == 0,
          "the get answers variant_of too");
    cJSON_Delete(meta);
    check_quiet(workshop_material_item_get(parent, &meta, NULL) == OK &&
              cJSON_IsNull(cJSON_GetObjectItemCaseSensitive(meta, "variant_of")),
          "the get answers variant_of null for a motif");
    cJSON_Delete(meta);

    /* A theme using a variant blocks the delete of the motif until forced; nothing is deleted meanwhile. */
    char *user = new_theme_using("Variant user", home, grandchild);
    check_quiet(user != NULL, "create a theme using the variant of a variant");
    check_quiet(workshop_material_item_delete(parent, false) == URSATZ_ERR_CONFLICT,
          "a use of a variant blocks the delete of its motif");
    check_quiet(exists(parent) && exists(variant) && exists(grandchild), "nothing was deleted");
    cJSON *blockers = NULL;
    check_quiet(workshop_material_delete_blockers(parent, &blockers) == OK && cJSON_GetArraySize(blockers) == 1 &&
              strcmp(str(cJSON_GetArrayItem(blockers, 0), "id"), user) == 0,
          "the blockers name the theme");
    cJSON_Delete(blockers);
    check_quiet(workshop_material_item_delete(parent, true) == OK, "a forced delete covers the variants");
    check_quiet(!exists(parent) && !exists(variant) && !exists(grandchild), "the motif, its variant and the variant's variant are gone");
    check_quiet(exists(promoted) && exists(user), "a standalone copy and the theme stay");

    /* Without a user the delete takes the variants along. */
    char *motif2 = new_item("motif", "Second", home, kMotifBody);
    char *kid = new_variant("Second v1", home, motif2, "");
    check_quiet(motif2 && kid, "create a second motif with a variant");
    check_quiet(workshop_material_delete_blockers(motif2, &blockers) == OK && cJSON_GetArraySize(blockers) == 0,
          "nothing blocks the second motif");
    cJSON_Delete(blockers);
    check_quiet(workshop_material_item_delete(motif2, false) == OK && !exists(motif2) && !exists(kid),
          "deleting a motif deletes its variants");
    check_quiet(workshop_material_item_delete(kid, false) == URSATZ_ERR_NOT_FOUND, "the variant is gone");

    /* A variant deleted on its own leaves its motif. */
    char *motif3 = new_item("motif", "Third", home, kMotifBody);
    char *kid3 = new_variant("Third v1", home, motif3, "");
    check_quiet(workshop_material_item_delete(kid3, false) == OK && exists(motif3), "deleting a variant leaves its motif");

    workshop_material_item_delete(user, true);
    workshop_material_item_delete(promoted, true);
    workshop_material_item_delete(motif3, true);
    check_quiet(workshop_material_project_delete(home) == OK, "delete the project");
    free(home);
    free(parent);
    free(variant);
    free(grandchild);
    free(promoted);
    free(user);
    free(motif2);
    free(kid);
    free(motif3);
    free(kid3);
}

static bool exists_file(const char *path) {
    FILE *file = fopen(path, "r");
    if (file) {
        fclose(file);
    }
    return file != NULL;
}

/* The marks of Try every operation: validated, kept in op-checks.json beside the versions, deleted with the item. */
static void test_op_checks(void) {
    char *home = new_project("Marks home");
    char *motif = new_item("motif", "Marked", home, kMotifBody);
    char *theme = new_item("theme", "Not a motif", home, kThemeBody);
    check_quiet(motif && theme, "create a motif and a theme for the marks");
    char path[700];
    char marks_dir[500];
    check_quiet(item_dir_of(motif, marks_dir, sizeof(marks_dir)), "the motif has a folder");
    snprintf(path, sizeof(path), "%s/op-checks.json", marks_dir);
    UrsatzError error = {{0}};

    cJSON *checks = NULL;
    check_quiet(workshop_material_op_checks_get(motif, &checks) == OK &&
              cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(checks, "checks")) == 0,
          "a motif without marks has an empty set");
    cJSON_Delete(checks);
    check_quiet(workshop_material_op_checks_get(theme, &checks) == URSATZ_ERR_NOT_FOUND,
          "only a motif has marks");
    check_quiet(workshop_material_op_checks_get("mot-0000000000000000", &checks) == URSATZ_ERR_NOT_FOUND,
          "an item that is not there has none");

    const char *good =
        "{\"checks\":{\"invert\":{\"verdict\":\"wrong\",\"comment\":\"flips the wrong way\",\"seed\":4,"
        "\"notes_text\":\"C4:q E4:q\",\"date\":\"2026-10-23\",\"extra\":1},\"augment\":{\"verdict\":\"works\","
        "\"comment\":\"\",\"seed\":0,\"notes_text\":\"C4:h\",\"date\":\"2026-10-23\"}}}";
    cJSON *body = cJSON_Parse(good);
    cJSON *stored = NULL;
    check_quiet(workshop_material_op_checks_put(motif, body, &error, &stored) == OK &&
              cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(stored, "checks")) == 2,
          "valid marks are stored");
    check_quiet(!cJSON_GetObjectItemCaseSensitive(
              cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(stored, "checks"), "invert"), "extra"),
          "a stray member is not kept");
    cJSON_Delete(stored);
    cJSON_Delete(body);
    check_quiet(workshop_material_op_checks_get(motif, &checks) == OK &&
              strcmp(str(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(checks, "checks"), "invert"),
                         "verdict"),
                     "wrong") == 0,
          "the marks read back");
    cJSON_Delete(checks);
    cJSON *item = NULL;
    check_quiet(workshop_material_item_get(motif, &item, NULL) == OK &&
              cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(item, "versions")) == 1,
          "marking adds no version");
    cJSON_Delete(item);
    check_quiet(exists_file(path), "the marks are a file in the item folder");

    static const char *const bad[] = {
        "{}",
        "{\"checks\":[]}",
        "{\"checks\":{\"Invert\":{\"verdict\":\"works\",\"comment\":\"\",\"seed\":1,\"notes_text\":\"\",\"date\":\"2026-10-23\"}}}",
        "{\"checks\":{\"invert\":{\"verdict\":\"fine\",\"comment\":\"\",\"seed\":1,\"notes_text\":\"\",\"date\":\"2026-10-23\"}}}",
        "{\"checks\":{\"invert\":{\"verdict\":\"works\",\"comment\":5,\"seed\":1,\"notes_text\":\"\",\"date\":\"2026-10-23\"}}}",
        "{\"checks\":{\"invert\":{\"verdict\":\"works\",\"comment\":\"\",\"seed\":-1,\"notes_text\":\"\",\"date\":\"2026-10-23\"}}}",
        "{\"checks\":{\"invert\":{\"verdict\":\"works\",\"comment\":\"\",\"seed\":1.5,\"notes_text\":\"\",\"date\":\"2026-10-23\"}}}",
        "{\"checks\":{\"invert\":{\"verdict\":\"works\",\"comment\":\"\",\"seed\":1,\"date\":\"2026-10-23\"}}}",
        "{\"checks\":{\"invert\":{\"verdict\":\"works\",\"comment\":\"\",\"seed\":1,\"notes_text\":\"\",\"date\":\"23/10/2026\"}}}",
        "{\"checks\":{\"invert\":{\"verdict\":\"works\",\"comment\":\"\",\"seed\":1,\"notes_text\":\"\",\"date\":\"2026-13-01\"}}}",
    };
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
        cJSON *bad_body = cJSON_Parse(bad[i]);
        check_quiet(workshop_material_op_checks_put(motif, bad_body, &error, &stored) ==
                  URSATZ_ERR_REQUEST,
              "a malformed mark is refused");
        cJSON_Delete(bad_body);
    }
    char comment[600];
    memset(comment, 'x', 501);
    comment[501] = '\0';
    char long_text[900];
    snprintf(long_text, sizeof(long_text),
             "{\"checks\":{\"invert\":{\"verdict\":\"works\",\"comment\":\"%s\",\"seed\":1,\"notes_text\":\"\",\"date\":\"2026-10-23\"}}}",
             comment);
    cJSON *long_body = cJSON_Parse(long_text);
    check_quiet(workshop_material_op_checks_put(motif, long_body, &error, &stored) ==
              URSATZ_ERR_REQUEST,
          "a comment over 500 characters is refused");
    cJSON_Delete(long_body);
    cJSON *good_body = cJSON_Parse(good);
    check_quiet(workshop_material_op_checks_put(theme, good_body, &error, &stored) ==
              URSATZ_ERR_NOT_FOUND,
          "marks cannot be put on a theme");
    cJSON_Delete(good_body);
    check_quiet(workshop_material_op_checks_get(motif, &checks) == OK &&
              cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(checks, "checks")) == 2,
          "a refused put leaves the marks as they were");
    cJSON_Delete(checks);

    check_quiet(workshop_material_item_delete(motif, false) == OK && !exists_file(path), "deleting the motif deletes its marks");
    free(motif);
    free(theme);
    free(home);
}


static cJSON *member_of(const cJSON *object, const char *name) {
    return cJSON_GetObjectItemCaseSensitive(object, name);
}

/* Bodies with the old members (a motif's key text, a theme form's and a piece structure's mode) are read with the new
   ones (R-597): the stored file keeps what was written, a read gives `material`, a new version writes it, and a body that
   has both keeps the new one. */
static void test_old_and_new_bodies(void) {
    static const char *const kOldMotif =
        "{\"uses\":[],\"motif\":{\"role\":\"melodic\",\"form\":\"full\",\"notes\":\"C4:q Eb4:e G4:e C5:h\","
        "\"meter\":\"4/4\",\"key\":\"C minor\",\"pickup\":\"0\",\"features\":[],\"analysis\":null,\"derived\":null}}";
    static const char *const kOldTheme =
        "{\"uses\":[],\"theme\":{\"form\":{\"into\":\"sentence\",\"mode\":\"minor\",\"meter\":\"4/4\"},"
        "\"slots\":[{\"slot\":\"basic\",\"item_id\":\"%s\",\"version\":1}]}}";
    static const char *const kOldPiece =
        "{\"uses\":[],\"piece\":{\"structure\":{\"key\":\"D\",\"mode\":\"%s\",\"meter\":\"4/4\",\"sections\":["
        "{\"id\":\"s0\",\"part\":\"body\",\"beats\":8,\"pickup\":\"0\",\"cadence\":null,\"fill\":null}]}}}";
    char *home = new_project("Old and new");
    char *motif = new_item("motif", "Old motif", home, kOldMotif);
    check_quiet(motif != NULL, "an old motif body is saved");
    cJSON *stored = motif ? stored_body(motif, 1) : NULL;
    check_quiet(stored && strcmp(str(member_of(stored, "motif"), "material"), "omt.material.natural-minor") == 0 &&
                    !member_of(member_of(stored, "motif"), "key"),
                "a save of an old motif body writes the material and no key");
    cJSON_Delete(stored);
    cJSON *body = NULL;
    check_quiet(motif && workshop_material_item_get_version(motif, 1, &body) == OK, "the saved motif is read");
    const cJSON *read = member_of(body, "motif");
    check_quiet(strcmp(str(read, "material"), "omt.material.natural-minor") == 0 && !member_of(read, "key"),
                "a motif saved with the key C minor is read as the material natural minor, with no key");
    cJSON *added = NULL;
    check_quiet(motif && workshop_material_item_add_version(motif, body, "Saved", &added) == OK && num(added, "current_version") == 2,
                "the read body is saved as a new version");
    cJSON_Delete(added);
    stored = motif ? stored_body(motif, 2) : NULL;
    check_quiet(stored && strcmp(str(member_of(stored, "motif"), "material"), "omt.material.natural-minor") == 0 &&
                    !member_of(member_of(stored, "motif"), "key"),
                "a save writes the material and no key");
    cJSON_Delete(stored);
    cJSON_Delete(body);

    /* C major, a null key and a body with both */
    static const struct {
        const char *key;
        const char *material_json;
        const char *expect;
    } motifs[] = {
        {"\"C major\"", NULL, "omt.material.major"},
        {"null", NULL, NULL},
        {"\"C major\"", "\"omt.material.natural-minor\"", "omt.material.natural-minor"},
    };
    for (size_t i = 0; i < sizeof(motifs) / sizeof(motifs[0]); i++) {
        cJSON *raw = cJSON_Parse(kOldMotif);
        cJSON *motif_member = member_of(raw, "motif");
        cJSON_ReplaceItemInObjectCaseSensitive(motif_member, "key", cJSON_Parse(motifs[i].key));
        if (motifs[i].material_json) {
            cJSON_AddItemToObject(motif_member, "material", cJSON_Parse(motifs[i].material_json));
        }
        workshop_material_body_migrate(raw);
        motif_member = member_of(raw, "motif");
        const cJSON *material = member_of(motif_member, "material");
        check_quiet(!member_of(motif_member, "key") && (motifs[i].expect ? strcmp(str(motif_member, "material"), motifs[i].expect) == 0
                                                                          : cJSON_IsNull(material)),
                    "a motif key becomes the material, the new member wins, a null key gives a null material");
        cJSON_Delete(raw);
    }
    cJSON *off_c = cJSON_Parse(kOldMotif);
    cJSON_ReplaceItemInObjectCaseSensitive(member_of(off_c, "motif"), "key", cJSON_CreateString("D minor"));
    workshop_material_body_migrate(off_c);
    check_quiet(strcmp(str(member_of(off_c, "motif"), "key"), "D minor") == 0 && !member_of(member_of(off_c, "motif"), "material"),
                "a key on another tonic stays as it was");
    cJSON_Delete(off_c);

    /* a theme form's mode and its copies of the motifs */
    char old_theme[900];
    snprintf(old_theme, sizeof(old_theme), kOldTheme, motif);
    char *theme = new_item("theme", "Old theme", home, old_theme);
    check_quiet(theme != NULL, "an old theme body is saved");
    stored = theme ? stored_body(theme, 1) : NULL;
    check_quiet(stored && strcmp(str(member_of(member_of(stored, "theme"), "form"), "material"), "omt.material.natural-minor") == 0 &&
                    !member_of(member_of(member_of(stored, "theme"), "form"), "mode"),
                "a save of an old theme body writes the form material and no mode");
    cJSON_Delete(stored);
    body = NULL;
    check_quiet(theme && workshop_material_item_get_version(theme, 1, &body) == OK, "the saved theme is read");
    const cJSON *form = member_of(member_of(body, "theme"), "form");
    check_quiet(strcmp(str(form, "material"), "omt.material.natural-minor") == 0 && !member_of(form, "mode") &&
                    strcmp(str(form, "into"), "sentence") == 0,
                "a theme form mode minor is saved as the material natural minor");
    cJSON_Delete(body);

    /* a piece structure's mode: major and minor by word, style by itself, a word with no material stays */
    static const struct {
        const char *mode;
        const char *expect;
    } pieces[] = {
        {"major", "omt.material.major"},
        {"minor", "omt.material.natural-minor"},
        {"style", "style"},
        {"dorian", NULL},
    };
    for (size_t i = 0; i < sizeof(pieces) / sizeof(pieces[0]); i++) {
        char old_piece[700];
        snprintf(old_piece, sizeof(old_piece), kOldPiece, pieces[i].mode);
        char *piece = new_item("piece", "Old piece", home, old_piece);
        check_quiet(piece != NULL, "an old piece body is saved");
        body = NULL;
        check_quiet(piece && workshop_material_item_get_version(piece, 1, &body) == OK, "the old piece is read");
        const cJSON *structure = member_of(member_of(body, "piece"), "structure");
        if (pieces[i].expect) {
            check_quiet(strcmp(str(structure, "material"), pieces[i].expect) == 0 && !member_of(structure, "mode") &&
                            strcmp(str(structure, "key"), "D") == 0,
                        "a piece structure mode is read as its material, the key stays");
        } else {
            check_quiet(strcmp(str(structure, "mode"), "dorian") == 0 && !member_of(structure, "material"),
                        "a piece structure mode that names no material stays as it was");
        }
        cJSON_Delete(body);
        free(piece);
    }
    /* a piece and a theme saved with the material, as the pages send them, are read as they are */
    static const char *const kNewPiece =
        "{\"uses\":[],\"piece\":{\"structure\":{\"key\":\"D\",\"material\":\"omt.material.major\",\"meter\":\"4/4\",\"sections\":["
        "{\"id\":\"s0\",\"part\":\"body\",\"beats\":8,\"pickup\":\"0\",\"cadence\":null,\"fill\":null}]}}}";
    char *fresh = new_item("piece", "New piece", home, kNewPiece);
    body = NULL;
    check_quiet(fresh && workshop_material_item_get_version(fresh, 1, &body) == OK &&
                    strcmp(str(member_of(member_of(body, "piece"), "structure"), "material"), "omt.material.major") == 0,
                "a new piece body is read with its material");
    cJSON_Delete(body);
    stored = fresh ? stored_body(fresh, 1) : NULL;
    check_quiet(stored && strcmp(str(member_of(member_of(stored, "piece"), "structure"), "material"), "omt.material.major") == 0 &&
                    !member_of(member_of(stored, "piece"), "mode") && !member_of(member_of(member_of(stored, "piece"), "structure"), "mode"),
                "a new piece body is stored with its material and no mode");
    cJSON_Delete(stored);
    free(fresh);
    free(theme);
    free(motif);
    free(home);
}

/* Deletes every project, then the store's directories. */
static void remove_store(void) {
    cJSON *projects = NULL;
    if (workshop_material_project_list(&projects) == OK) {
        const cJSON *project = NULL;
        cJSON_ArrayForEach(project, projects) {
            workshop_material_project_delete(str(project, "id"));
        }
        cJSON_Delete(projects);
    }
    workshop_material_service_shutdown();
    char command[700];
    if (chdir(URSATZ_GUI_TEST_STORE_DIR) == 0) {
        snprintf(command, sizeof(command), "rm -rf '%s'", g_dir);
        system(command);
    }
}

int main(void) {
    test_knowledge();
    snprintf(g_dir, sizeof(g_dir), "%s/materials_%d", URSATZ_GUI_TEST_STORE_DIR, (int)getpid());
    check_quiet(mkdir(g_dir, 0755) == 0 && chdir(g_dir) == 0, "enter a folder of the test's own");
    check_quiet(workshop_material_service_init(), "init");

    test_general_project();
    test_project_crud();
    test_items_and_versions();
    test_body_validation();
    test_references();
    test_forced_delete();
    test_variant_ownership();
    test_op_checks();
    test_old_and_new_bodies();

    remove_store();
    return test_finish("material_service");
}
