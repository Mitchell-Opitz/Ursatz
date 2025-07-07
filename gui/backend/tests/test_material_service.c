#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <cJSON.h>

#include "service/material_service.h"

static int failures = 0;

#define CHECK(cond, msg)                                            \
    do {                                                            \
        if (!(cond)) {                                              \
            fprintf(stderr, "FAIL: %s (line %d)\n", msg, __LINE__); \
            failures++;                                             \
        }                                                           \
    } while (0)

#define OK URSATZ_GUI_MATERIAL_OK

static char g_dir[64];

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
    if (ursatz_gui_material_project_create(name, "", &project) == OK) {
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
    if (ursatz_gui_material_item_create(type, name, project_id, body, &item) == OK) {
        id = strdup(str(item, "id"));
    }
    cJSON_Delete(item);
    cJSON_Delete(body);
    return id;
}

static char *read_all(const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        return NULL;
    }
    char *text = calloc(1, 65536);
    size_t n = text ? fread(text, 1, 65535, file) : 0;
    if (text) {
        text[n] = '\0';
    }
    fclose(file);
    return text;
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

static void test_general_project(void) {
    cJSON *projects = NULL;
    CHECK(ursatz_gui_material_project_list(&projects) == OK, "list projects");
    CHECK(cJSON_GetArraySize(projects) == 1, "one project after first init");
    const cJSON *general = cJSON_GetArrayItem(projects, 0);
    CHECK(strcmp(str(general, "name"), "General") == 0, "the project is named General");
    CHECK(strncmp(str(general, "id"), "prj-", 4) == 0, "project ids start with prj-");
    CHECK(num(general, "item_count") == 0, "General holds no items");
    cJSON_Delete(projects);

    ursatz_gui_material_service_shutdown();
    CHECK(ursatz_gui_material_service_init(g_dir), "init again");
    CHECK(ursatz_gui_material_project_list(&projects) == OK && cJSON_GetArraySize(projects) == 1,
          "init does not add a second General");
    cJSON_Delete(projects);
}

static void test_project_crud(void) {
    cJSON *project = NULL;
    CHECK(ursatz_gui_material_project_create("  beta  ", "  first notes ", &project) == OK, "create project");
    CHECK(strcmp(str(project, "name"), "beta") == 0, "name is trimmed");
    CHECK(strcmp(str(project, "description"), "first notes") == 0, "description is trimmed");
    char *beta = strdup(str(project, "id"));
    cJSON_Delete(project);
    char *alpha = new_project("Alpha");
    char *zed = new_project("zed");
    CHECK(beta && alpha && zed, "three projects");

    cJSON *projects = NULL;
    CHECK(ursatz_gui_material_project_list(&projects) == OK, "list");
    CHECK(cJSON_GetArraySize(projects) == 4, "General plus three");
    CHECK(strcmp(str(cJSON_GetArrayItem(projects, 0), "name"), "Alpha") == 0, "sorted by name, first");
    CHECK(strcmp(str(cJSON_GetArrayItem(projects, 1), "name"), "beta") == 0, "sorted by name, case-insensitive");
    CHECK(strcmp(str(cJSON_GetArrayItem(projects, 2), "name"), "General") == 0, "sorted by name, third");
    CHECK(strcmp(str(cJSON_GetArrayItem(projects, 3), "name"), "zed") == 0, "sorted by name, last");
    cJSON_Delete(projects);

    CHECK(ursatz_gui_material_project_get(beta, &project) == OK, "get");
    CHECK(strcmp(str(project, "description"), "first notes") == 0, "get returns the description");
    cJSON_Delete(project);

    CHECK(ursatz_gui_material_project_update(beta, "Gamma", NULL, &project) == OK, "update name");
    CHECK(strcmp(str(project, "name"), "Gamma") == 0 && strcmp(str(project, "description"), "first notes") == 0,
          "update keeps the description");
    cJSON_Delete(project);
    CHECK(ursatz_gui_material_project_update(beta, NULL, "", NULL) == OK, "clear the description");
    CHECK(ursatz_gui_material_project_get(beta, &project) == OK && strcmp(str(project, "description"), "") == 0,
          "description cleared");
    cJSON_Delete(project);

    CHECK(ursatz_gui_material_project_create("", NULL, NULL) == URSATZ_GUI_MATERIAL_ERR_INVALID_NAME, "empty name");
    CHECK(ursatz_gui_material_project_create("   ", NULL, NULL) == URSATZ_GUI_MATERIAL_ERR_INVALID_NAME,
          "blank name");
    char long_name[260];
    memset(long_name, 'n', 201);
    long_name[201] = '\0';
    CHECK(ursatz_gui_material_project_create(long_name, NULL, NULL) == URSATZ_GUI_MATERIAL_ERR_INVALID_NAME,
          "name of 201 characters");
    long_name[200] = '\0';
    CHECK(ursatz_gui_material_project_create(long_name, NULL, NULL) == OK, "name of 200 characters");
    char *long_description = calloc(1, 2002);
    memset(long_description, 'd', 2001);
    CHECK(ursatz_gui_material_project_create("x", long_description, NULL) == URSATZ_GUI_MATERIAL_ERR_INVALID_NAME,
          "description of 2001 characters");
    long_description[2000] = '\0';
    CHECK(ursatz_gui_material_project_create("Described", long_description, NULL) == OK,
          "description of 2000 characters");
    free(long_description);
    CHECK(ursatz_gui_material_project_update(beta, "", NULL, NULL) == URSATZ_GUI_MATERIAL_ERR_INVALID_NAME,
          "update to an empty name");
    CHECK(ursatz_gui_material_project_get("../x", &project) == URSATZ_GUI_MATERIAL_ERR_INVALID_ID, "bad id");
    CHECK(ursatz_gui_material_project_get("prj-0000000000000000", &project) == URSATZ_GUI_MATERIAL_ERR_NOT_FOUND,
          "unknown id");

    CHECK(ursatz_gui_material_project_delete(zed) == OK, "delete");
    CHECK(ursatz_gui_material_project_get(zed, &project) == URSATZ_GUI_MATERIAL_ERR_NOT_FOUND, "gone after delete");
    CHECK(ursatz_gui_material_project_delete(zed) == URSATZ_GUI_MATERIAL_ERR_NOT_FOUND, "delete twice");
    free(beta);
    free(alpha);
    free(zed);
}

static void test_items_and_versions(void) {
    char *home = new_project("Items home");
    char *other = new_project("Items other");
    cJSON *body = cJSON_Parse(kMotifBody);
    cJSON *item = NULL;

    CHECK(ursatz_gui_material_item_create("motif", "  Opening  ", home, body, &item) == OK, "create motif");
    CHECK(strncmp(str(item, "id"), "mot-", 4) == 0, "motif ids start with mot-");
    CHECK(strcmp(str(item, "name"), "Opening") == 0 && strcmp(str(item, "type"), "motif") == 0, "name and type");
    CHECK(num(item, "current_version") == 1, "current version is 1");
    CHECK(cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(item, "versions")) == 1, "one version entry");
    char *motif = strdup(str(item, "id"));
    cJSON_Delete(item);
    char *theme = new_item("theme", "Theme one", home, kThemeBody);
    char *piece = new_item("piece", "Piece one", other, kPieceBody);
    CHECK(theme && strncmp(theme, "thm-", 4) == 0, "theme ids start with thm-");
    CHECK(piece && strncmp(piece, "pce-", 4) == 0, "piece ids start with pce-");

    CHECK(ursatz_gui_material_item_create("chord", "x", home, body, NULL) == URSATZ_GUI_MATERIAL_ERR_INVALID_TYPE,
          "unknown type");
    CHECK(ursatz_gui_material_item_create("motif", " ", home, body, NULL) == URSATZ_GUI_MATERIAL_ERR_INVALID_NAME,
          "blank item name");
    CHECK(ursatz_gui_material_item_create("motif", "x", "prj-0000000000000000", body, NULL) ==
              URSATZ_GUI_MATERIAL_ERR_NOT_FOUND,
          "unknown project");
    CHECK(ursatz_gui_material_item_create("motif", "x", "a/b", body, NULL) == URSATZ_GUI_MATERIAL_ERR_INVALID_ID,
          "bad project id");

    cJSON *meta = NULL;
    cJSON *current = NULL;
    CHECK(ursatz_gui_material_item_get(motif, &meta, &current) == OK, "get");
    CHECK(strcmp(str(cJSON_GetObjectItemCaseSensitive(current, "motif"), "notes"), "D4:q F4:e A4:e D5:h") == 0,
          "get returns the body");
    cJSON_Delete(meta);
    cJSON_Delete(current);

    /* A new version never rewrites an earlier one. */
    char v1_path[200];
    snprintf(v1_path, sizeof(v1_path), "%s/items/%s/v1.json", g_dir, motif);
    char *v1_before = read_all(v1_path);
    cJSON *second = cJSON_Parse(kMotifBody);
    cJSON_ReplaceItemInObjectCaseSensitive(cJSON_GetObjectItemCaseSensitive(second, "motif"), "notes",
                                           cJSON_CreateString("D4:q E4:q"));
    CHECK(ursatz_gui_material_item_add_version(motif, second, "  trimmed ending ", &item) == OK, "add version");
    CHECK(num(item, "current_version") == 2, "current version advances");
    const cJSON *versions = cJSON_GetObjectItemCaseSensitive(item, "versions");
    CHECK(cJSON_GetArraySize(versions) == 2 && strcmp(str(cJSON_GetArrayItem(versions, 1), "note"), "trimmed ending") == 0,
          "second entry carries the note");
    cJSON_Delete(item);
    char *v1_after = read_all(v1_path);
    CHECK(v1_before && v1_after && strcmp(v1_before, v1_after) == 0, "v1.json is untouched");
    free(v1_before);
    free(v1_after);

    cJSON *old_body = NULL;
    CHECK(ursatz_gui_material_item_get_version(motif, 1, &old_body) == OK, "get version 1");
    CHECK(strcmp(str(cJSON_GetObjectItemCaseSensitive(old_body, "motif"), "notes"), "D4:q F4:e A4:e D5:h") == 0,
          "version 1 keeps its notes");
    cJSON_Delete(old_body);
    CHECK(ursatz_gui_material_item_get_version(motif, 2, &old_body) == OK &&
              strcmp(str(cJSON_GetObjectItemCaseSensitive(old_body, "motif"), "notes"), "D4:q E4:q") == 0,
          "version 2 has the new notes");
    cJSON_Delete(old_body);
    CHECK(ursatz_gui_material_item_get_version(motif, 3, &old_body) == URSATZ_GUI_MATERIAL_ERR_NOT_FOUND,
          "version 3 does not exist");
    CHECK(ursatz_gui_material_item_get_version(motif, 0, &old_body) == URSATZ_GUI_MATERIAL_ERR_NOT_FOUND,
          "version 0 does not exist");
    CHECK(ursatz_gui_material_item_get(motif, NULL, &current) == OK &&
              strcmp(str(cJSON_GetObjectItemCaseSensitive(current, "motif"), "notes"), "D4:q E4:q") == 0,
          "get returns the current version");
    cJSON_Delete(current);

    /* Rename and move change item.json only. */
    CHECK(ursatz_gui_material_item_rename(motif, "  Renamed ", &item) == OK && strcmp(str(item, "name"), "Renamed") == 0,
          "rename");
    cJSON_Delete(item);
    CHECK(ursatz_gui_material_item_rename(motif, "", NULL) == URSATZ_GUI_MATERIAL_ERR_INVALID_NAME, "rename to empty");
    CHECK(ursatz_gui_material_item_move(motif, other, &item) == OK && strcmp(str(item, "project_id"), other) == 0,
          "move");
    CHECK(num(item, "current_version") == 2, "move adds no version");
    cJSON_Delete(item);
    CHECK(ursatz_gui_material_item_move(motif, "prj-0000000000000000", NULL) == URSATZ_GUI_MATERIAL_ERR_NOT_FOUND,
          "move to an unknown project");

    /* Filters and order. */
    cJSON *list = NULL;
    CHECK(ursatz_gui_material_item_list(NULL, NULL, &list) == OK && cJSON_GetArraySize(list) == 3, "list all");
    const cJSON *previous = NULL;
    const cJSON *entry = NULL;
    int ordered = 1;
    cJSON_ArrayForEach(entry, list) {
        if (previous && strcmp(str(previous, "updated_at"), str(entry, "updated_at")) < 0) {
            ordered = 0;
        }
        previous = entry;
    }
    CHECK(ordered, "newest updated_at first");
    cJSON_Delete(list);
    CHECK(ursatz_gui_material_item_list(home, NULL, &list) == OK && cJSON_GetArraySize(list) == 1 &&
              find_by_id(list, theme),
          "project filter");
    cJSON_Delete(list);
    CHECK(ursatz_gui_material_item_list(other, NULL, &list) == OK && cJSON_GetArraySize(list) == 2 &&
              find_by_id(list, motif) && find_by_id(list, piece),
          "project filter after the move");
    cJSON_Delete(list);
    CHECK(ursatz_gui_material_item_list(NULL, "piece", &list) == OK && cJSON_GetArraySize(list) == 1 &&
              find_by_id(list, piece),
          "type filter");
    cJSON_Delete(list);
    CHECK(ursatz_gui_material_item_list(other, "theme", &list) == OK && cJSON_GetArraySize(list) == 0,
          "both filters");
    cJSON_Delete(list);
    CHECK(ursatz_gui_material_item_list(NULL, "chord", &list) == URSATZ_GUI_MATERIAL_ERR_INVALID_TYPE,
          "list with an unknown type");

    /* A project that holds items cannot be deleted. */
    CHECK(ursatz_gui_material_project_delete(home) == URSATZ_GUI_MATERIAL_ERR_NOT_EMPTY, "delete a project with items");
    cJSON *project = NULL;
    CHECK(ursatz_gui_material_project_get(home, &project) == OK && num(project, "item_count") == 1, "item count");
    cJSON_Delete(project);

    /* Items survive a restart. */
    ursatz_gui_material_service_shutdown();
    CHECK(ursatz_gui_material_item_list(NULL, NULL, &list) == URSATZ_GUI_MATERIAL_ERR_NOT_INITIALIZED,
          "not initialized after shutdown");
    CHECK(ursatz_gui_material_service_init(g_dir), "init after shutdown");
    CHECK(ursatz_gui_material_item_list(NULL, NULL, &list) == OK && cJSON_GetArraySize(list) == 3, "items persist");
    cJSON_Delete(list);

    CHECK(ursatz_gui_material_item_delete(theme) == OK, "delete the theme");
    CHECK(ursatz_gui_material_item_get(theme, &meta, NULL) == URSATZ_GUI_MATERIAL_ERR_NOT_FOUND, "theme is gone");
    char theme_dir[200];
    snprintf(theme_dir, sizeof(theme_dir), "%s/items/%s/v1.json", g_dir, theme);
    CHECK(access(theme_dir, F_OK) != 0, "theme versions are gone");
    CHECK(ursatz_gui_material_item_delete(theme) == URSATZ_GUI_MATERIAL_ERR_NOT_FOUND, "delete twice");
    CHECK(ursatz_gui_material_project_delete(home) == OK, "delete the emptied project");
    CHECK(ursatz_gui_material_item_delete(motif) == OK && ursatz_gui_material_item_delete(piece) == OK,
          "delete the rest");
    CHECK(ursatz_gui_material_project_delete(other) == OK, "delete the other project");

    cJSON_Delete(body);
    cJSON_Delete(second);
    free(home);
    free(other);
    free(motif);
    free(theme);
    free(piece);
}

/* Deletes every project, then the store's directories. */
static void remove_store(void) {
    cJSON *projects = NULL;
    if (ursatz_gui_material_project_list(&projects) == OK) {
        const cJSON *project = NULL;
        cJSON_ArrayForEach(project, projects) {
            ursatz_gui_material_project_delete(str(project, "id"));
        }
        cJSON_Delete(projects);
    }
    ursatz_gui_material_service_shutdown();
    char path[100];
    snprintf(path, sizeof(path), "%s/projects", g_dir);
    rmdir(path);
    snprintf(path, sizeof(path), "%s/items", g_dir);
    rmdir(path);
    rmdir(g_dir);
}

int main(void) {
    snprintf(g_dir, sizeof(g_dir), "materials_%d", (int)getpid());
    CHECK(ursatz_gui_material_service_init(g_dir), "init");

    test_general_project();
    test_project_crud();
    test_items_and_versions();

    remove_store();
    if (failures) {
        fprintf(stderr, "%d check(s) failed\n", failures);
        return 1;
    }
    printf("material service tests passed\n");
    return 0;
}
