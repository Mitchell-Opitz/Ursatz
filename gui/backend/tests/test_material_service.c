#include <stdbool.h>
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

    CHECK(ursatz_gui_material_item_delete(theme, false) == OK, "delete the theme");
    CHECK(ursatz_gui_material_item_get(theme, &meta, NULL) == URSATZ_GUI_MATERIAL_ERR_NOT_FOUND, "theme is gone");
    char theme_dir[200];
    snprintf(theme_dir, sizeof(theme_dir), "%s/items/%s/v1.json", g_dir, theme);
    CHECK(access(theme_dir, F_OK) != 0, "theme versions are gone");
    CHECK(ursatz_gui_material_item_delete(theme, false) == URSATZ_GUI_MATERIAL_ERR_NOT_FOUND, "delete twice");
    CHECK(ursatz_gui_material_project_delete(home) == OK, "delete the emptied project");
    CHECK(ursatz_gui_material_item_delete(motif, false) == OK && ursatz_gui_material_item_delete(piece, false) == OK,
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

#define INVALID URSATZ_GUI_MATERIAL_ERR_INVALID_BODY

static UrsatzGuiMaterialResult validate(const char *type, const cJSON *body) {
    return ursatz_gui_material_body_validate(type, body, NULL, 0);
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
static void expect_motif(const char *member, const char *json, UrsatzGuiMaterialResult result) {
    cJSON *body = motif_with(member, json);
    char label[160];
    snprintf(label, sizeof(label), "motif %s = %s", member, json);
    CHECK(validate("motif", body) == result, label);
    cJSON_Delete(body);
}

static const char *kDerived =
    "{\"source_item\":\"mot-0123456789abcdef\",\"source_version\":1,\"operations\":[],\"seed\":42,"
    "\"kind\":\"variant\",\"kept\":[\"rhythm\"],\"changed\":[\"intervals\",\"contour\"],\"harmony\":null,"
    "\"functions\":[\"continuation\"],\"flag\":\"unflagged\"}";

static void test_body_validation(void) {
    cJSON *body = cJSON_Parse(kMotifBody);
    char error[200];
    CHECK(ursatz_gui_material_body_validate("motif", body, error, sizeof(error)) == OK && error[0] == '\0',
          "a valid motif body");
    cJSON_Delete(body);
    body = cJSON_Parse(kThemeBody);
    CHECK(validate("theme", body) == OK, "a valid theme body");
    cJSON_Delete(body);
    body = cJSON_Parse(kPieceBody);
    CHECK(validate("piece", body) == OK, "a valid piece body");
    cJSON_Delete(body);

    body = cJSON_Parse("[]");
    CHECK(validate("motif", body) == INVALID, "a body must be an object");
    cJSON_Delete(body);
    body = cJSON_Parse(kMotifBody);
    CHECK(validate("chord", body) == URSATZ_GUI_MATERIAL_ERR_INVALID_TYPE, "an unknown type");
    CHECK(validate(NULL, body) == URSATZ_GUI_MATERIAL_ERR_INVALID_TYPE, "no type");
    CHECK(validate("motif", NULL) == INVALID, "no body");
    cJSON_Delete(body);

    /* uses */
    body = cJSON_Parse("{\"motif\":{}}");
    CHECK(validate("motif", body) == INVALID, "uses is required");
    cJSON_Delete(body);
    body = cJSON_Parse("{\"uses\":{},\"theme\":{\"form\":null,\"slots\":[]}}");
    CHECK(validate("theme", body) == INVALID, "uses must be an array");
    cJSON_Delete(body);
    body = cJSON_Parse("{\"uses\":[{\"item_id\":\"mot-0123456789abcdef\",\"version\":1,\"copy\":{}}],"
                       "\"theme\":{\"form\":null,\"slots\":[]}}");
    CHECK(validate("theme", body) == OK, "a use with a copy");
    cJSON_Delete(body);
    body = cJSON_Parse("{\"uses\":[{\"item_id\":\"mot-0123456789abcdef\",\"version\":0}],"
                       "\"theme\":{\"form\":null,\"slots\":[]}}");
    CHECK(validate("theme", body) == INVALID, "a use needs a version of 1 or more");
    cJSON_Delete(body);
    body = cJSON_Parse("{\"uses\":[{\"item_id\":\"../x\",\"version\":1}],\"theme\":{\"form\":null,\"slots\":[]}}");
    CHECK(validate("theme", body) == INVALID, "a use needs a valid item id");
    cJSON_Delete(body);

    /* one type's member in another type's body */
    body = cJSON_Parse("{\"uses\":[],\"theme\":{\"form\":null,\"slots\":[]},\"piece\":{\"structure\":null}}");
    CHECK(validate("theme", body) == INVALID, "a theme body must not carry a piece member");
    cJSON_Delete(body);
    body = cJSON_Parse("{\"uses\":[],\"motif\":{},\"piece\":{\"structure\":null}}");
    CHECK(validate("piece", body) == INVALID, "a piece body must not carry a motif member");
    cJSON_Delete(body);
    body = cJSON_Parse(kThemeBody);
    CHECK(validate("motif", body) == INVALID, "a motif body needs a motif member");
    CHECK(validate("piece", body) == INVALID, "a piece body needs a piece member");
    cJSON_Delete(body);
    body = cJSON_Parse("{\"uses\":[],\"theme\":{\"form\":null}}");
    CHECK(validate("theme", body) == INVALID, "a theme needs slots");
    cJSON_Delete(body);

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
        CHECK(at != NULL, "derived edit applies");
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
    cJSON *bad = motif_with("role", "\"bass\"");
    cJSON *item = NULL;
    CHECK(ursatz_gui_material_item_create("motif", "Bad", home, bad, &item) == INVALID && item == NULL,
          "create refuses a bad body");
    cJSON *list = NULL;
    CHECK(ursatz_gui_material_item_list(home, NULL, &list) == OK && cJSON_GetArraySize(list) == 0, "no item written");
    cJSON_Delete(list);
    char *motif = new_item("motif", "Good", home, kMotifBody);
    CHECK(motif != NULL, "create a good motif");
    CHECK(ursatz_gui_material_item_add_version(motif, bad, NULL, &item) == INVALID && item == NULL,
          "add_version refuses a bad body");
    CHECK(ursatz_gui_material_item_get(motif, &item, NULL) == OK && num(item, "current_version") == 1,
          "no version written");
    cJSON_Delete(item);
    cJSON *theme = cJSON_Parse(kThemeBody);
    CHECK(ursatz_gui_material_item_add_version(motif, theme, NULL, NULL) == INVALID,
          "add_version refuses another type's body");
    cJSON_Delete(theme);
    CHECK(ursatz_gui_material_item_delete(motif, false) == OK, "delete the motif");
    CHECK(ursatz_gui_material_project_delete(home) == OK, "delete the project");
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
    if (ursatz_gui_material_item_create("theme", name, project_id, body, &item) == OK) {
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
    CHECK(source != NULL, "create the source motif");

    /* A use is copied from the version it names, in any project; a client copy is replaced. */
    char *user = new_theme_using("User theme", other, source);
    CHECK(user != NULL, "create a theme that uses the motif");
    cJSON *body = NULL;
    CHECK(ursatz_gui_material_item_get(user, NULL, &body) == OK, "get the user's body");
    CHECK(strcmp(copied_notes(first_use(body)), "D4:q F4:e A4:e D5:h") == 0, "the copy holds the source's notes");
    CHECK(!cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(first_use(body), "copy"), "bogus"),
          "the client copy is replaced");
    cJSON_Delete(body);

    /* Unknown items and versions, and an item using itself, are refused. */
    cJSON *bad = theme_body_using("mot-0000000000000000", 1);
    CHECK(ursatz_gui_material_item_create("theme", "Ghost use", other, bad, NULL) == INVALID, "unknown source item");
    cJSON_Delete(bad);
    bad = theme_body_using(source, 2);
    CHECK(ursatz_gui_material_item_create("theme", "Future use", other, bad, NULL) == INVALID, "unknown source version");
    CHECK(ursatz_gui_material_item_add_version(user, bad, NULL, NULL) == INVALID, "add_version refuses unknown version");
    cJSON_Delete(bad);
    bad = theme_body_using(user, 1);
    CHECK(ursatz_gui_material_item_add_version(user, bad, NULL, NULL) == INVALID, "an item may not use itself");
    cJSON_Delete(bad);
    cJSON *meta = NULL;
    CHECK(ursatz_gui_material_item_get(user, &meta, NULL) == OK && num(meta, "current_version") == 1,
          "refused versions are not written");
    cJSON_Delete(meta);

    /* uses_status: current while the source is on the used version. */
    cJSON *status = NULL;
    CHECK(ursatz_gui_material_uses_status(user, &status) == OK && cJSON_GetArraySize(status) == 1, "one use reported");
    const cJSON *entry = cJSON_GetArrayItem(status, 0);
    CHECK(strcmp(str(entry, "item_id"), source) == 0 && strcmp(str(entry, "name"), "Source motif") == 0 &&
              strcmp(str(entry, "type"), "motif") == 0 && strcmp(str(entry, "project_id"), home) == 0,
          "status names the source");
    CHECK(num(entry, "version") == 1 && num(entry, "current_version") == 1, "status versions");
    CHECK(cJSON_IsFalse(cJSON_GetObjectItemCaseSensitive(entry, "updated")), "not updated yet");
    cJSON_Delete(status);
    CHECK(ursatz_gui_material_uses_status("thm-0000000000000000", &status) == URSATZ_GUI_MATERIAL_ERR_NOT_FOUND,
          "status of an unknown item");

    /* The source moves on: the use reports it and used_by lists the user. */
    cJSON *next = motif_body_with_notes("D4:q E4:q F4:h");
    CHECK(ursatz_gui_material_item_add_version(source, next, "second", NULL) == OK, "source gets version 2");
    cJSON_Delete(next);
    CHECK(ursatz_gui_material_uses_status(user, &status) == OK, "status after the source moved");
    entry = cJSON_GetArrayItem(status, 0);
    CHECK(cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(entry, "updated")) && num(entry, "current_version") == 2 &&
              num(entry, "version") == 1,
          "the use is flagged updated");
    cJSON_Delete(status);

    cJSON *users = NULL;
    CHECK(ursatz_gui_material_used_by(source, &users) == OK && cJSON_GetArraySize(users) == 1, "one user");
    entry = cJSON_GetArrayItem(users, 0);
    CHECK(strcmp(str(entry, "id"), user) == 0 && strcmp(str(entry, "name"), "User theme") == 0 &&
              strcmp(str(entry, "type"), "theme") == 0 && strcmp(str(entry, "project_id"), other) == 0 &&
              num(entry, "version") == 1,
          "used_by names the user across projects");
    cJSON_Delete(users);
    CHECK(ursatz_gui_material_used_by(user, &users) == OK && cJSON_GetArraySize(users) == 0, "nothing uses the theme");
    cJSON_Delete(users);

    /* update_use writes a new version that points at the source's latest. */
    CHECK(ursatz_gui_material_update_use(user, "mot-0000000000000000", NULL) == URSATZ_GUI_MATERIAL_ERR_NOT_FOUND,
          "update_use of a source the theme does not use");
    CHECK(ursatz_gui_material_update_use(user, source, &meta) == OK, "update_use");
    CHECK(num(meta, "current_version") == 2, "update_use advances the version");
    const cJSON *versions = cJSON_GetObjectItemCaseSensitive(meta, "versions");
    CHECK(strcmp(str(cJSON_GetArrayItem(versions, 1), "note"), "updated Source motif to v2") == 0, "update_use note");
    cJSON_Delete(meta);
    CHECK(ursatz_gui_material_item_get(user, NULL, &body) == OK && num(first_use(body), "version") == 2 &&
              strcmp(copied_notes(first_use(body)), "D4:q E4:q F4:h") == 0,
          "the new version uses the latest copy");
    cJSON_Delete(body);
    CHECK(ursatz_gui_material_item_get_version(user, 1, &body) == OK && num(first_use(body), "version") == 1 &&
              strcmp(copied_notes(first_use(body)), "D4:q F4:e A4:e D5:h") == 0,
          "version 1 keeps its old copy");
    cJSON_Delete(body);
    CHECK(ursatz_gui_material_uses_status(user, &status) == OK &&
              cJSON_IsFalse(cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(status, 0), "updated")),
          "no longer updated");
    cJSON_Delete(status);

    /* A used item is refused a plain delete. */
    CHECK(ursatz_gui_material_item_delete(source, false) == URSATZ_GUI_MATERIAL_ERR_REFERENCED, "used item is protected");
    CHECK(ursatz_gui_material_item_get(source, NULL, NULL) == OK, "the used item is still there");

    CHECK(ursatz_gui_material_item_delete(user, false) == OK, "delete the user");
    CHECK(ursatz_gui_material_item_delete(source, false) == OK, "delete the source once unused");
    CHECK(ursatz_gui_material_project_delete(home) == OK && ursatz_gui_material_project_delete(other) == OK,
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
    CHECK(source && user, "create the source and its user");

    CHECK(ursatz_gui_material_item_delete(source, false) == URSATZ_GUI_MATERIAL_ERR_REFERENCED, "refused without force");
    CHECK(ursatz_gui_material_item_delete(source, true) == OK, "forced delete");
    CHECK(ursatz_gui_material_item_get(source, NULL, NULL) == URSATZ_GUI_MATERIAL_ERR_NOT_FOUND, "the source is gone");

    cJSON *body = NULL;
    CHECK(ursatz_gui_material_item_get(user, NULL, &body) == OK, "the user still loads");
    CHECK(strcmp(copied_notes(first_use(body)), "D4:q F4:e A4:e D5:h") == 0, "the copy is untouched");
    cJSON_Delete(body);

    cJSON *status = NULL;
    CHECK(ursatz_gui_material_uses_status(user, &status) == OK && cJSON_GetArraySize(status) == 1, "status still lists the use");
    const cJSON *entry = cJSON_GetArrayItem(status, 0);
    CHECK(cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(entry, "missing")) &&
              strcmp(str(entry, "item_id"), source) == 0 && num(entry, "version") == 1,
          "the use reports a missing source");
    cJSON_Delete(status);
    CHECK(ursatz_gui_material_update_use(user, source, NULL) == URSATZ_GUI_MATERIAL_ERR_NOT_FOUND,
          "a missing source cannot be updated to");

    /* The copy keeps being copied into later versions of the user. */
    CHECK(ursatz_gui_material_item_get(user, NULL, &body) == OK, "get the user's body");
    CHECK(ursatz_gui_material_item_add_version(user, body, "again", NULL) == URSATZ_GUI_MATERIAL_ERR_INVALID_BODY,
          "a new version cannot name a deleted source");
    cJSON_Delete(body);

    CHECK(ursatz_gui_material_item_delete(user, false) == OK, "delete the user");
    CHECK(ursatz_gui_material_project_delete(home) == OK, "delete the project");
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
    if (ursatz_gui_material_item_create("motif", name, project_id, body, &item) == OK) {
        id = strdup(str(item, "id"));
    }
    cJSON_Delete(item);
    cJSON_Delete(body);
    return id;
}

static bool exists(const char *id) {
    return ursatz_gui_material_item_get(id, NULL, NULL) == OK;
}

/* R-187: a derived motif is a variant unless standalone, it reports variant_of, and it goes with its motif. */
static void test_variant_ownership(void) {
    char *home = new_project("Variant home");
    char *parent = new_item("motif", "Parent", home, kMotifBody);
    char *variant = new_variant("Parent v1", home, parent, "");
    char *grandchild = new_variant("Parent v1 v1", home, variant, "");
    char *promoted = new_variant("Promoted", home, parent, ",\"standalone\":true");
    CHECK(parent && variant && grandchild && promoted, "create a motif, a variant, its variant and a standalone copy");

    /* Validation and the helper. */
    char text[700];
    snprintf(text, sizeof(text), "%s", kDerived);
    text[strlen(text) - 1] = '\0';
    char with_flag[760];
    snprintf(with_flag, sizeof(with_flag), "%s,\"standalone\":false}", text);
    expect_motif("derived", with_flag, OK);
    snprintf(with_flag, sizeof(with_flag), "%s,\"standalone\":\"yes\"}", text);
    expect_motif("derived", with_flag, URSATZ_GUI_MATERIAL_ERR_INVALID_BODY);
    cJSON *plain = cJSON_Parse(kMotifBody);
    cJSON *derived_body = motif_with("derived", kDerived);
    snprintf(with_flag, sizeof(with_flag), "%s,\"standalone\":true}", text);
    cJSON *standalone_body = motif_with("derived", with_flag);
    cJSON *theme = cJSON_Parse(kThemeBody);
    CHECK(!ursatz_gui_material_item_is_variant(plain), "a motif without derived is not a variant");
    CHECK(ursatz_gui_material_item_is_variant(derived_body), "a derived motif is a variant");
    CHECK(!ursatz_gui_material_item_is_variant(standalone_body), "a standalone derived motif is not a variant");
    CHECK(!ursatz_gui_material_item_is_variant(theme) && !ursatz_gui_material_item_is_variant(NULL),
          "a theme body or no body is not a variant");
    cJSON_Delete(plain);
    cJSON_Delete(derived_body);
    cJSON_Delete(standalone_body);
    cJSON_Delete(theme);

    /* variant_of in the list and in the get. */
    cJSON *items = NULL;
    CHECK(ursatz_gui_material_item_list(home, NULL, &items) == OK, "list the project");
    const cJSON *entry = find_by_id(items, parent);
    CHECK(entry && cJSON_IsNull(cJSON_GetObjectItemCaseSensitive(entry, "variant_of")), "a motif has variant_of null");
    entry = find_by_id(items, variant);
    CHECK(entry && strcmp(str(entry, "variant_of"), parent) == 0, "a variant names its motif in variant_of");
    entry = find_by_id(items, grandchild);
    CHECK(entry && strcmp(str(entry, "variant_of"), variant) == 0, "a variant of a variant names the variant");
    entry = find_by_id(items, promoted);
    CHECK(entry && cJSON_IsNull(cJSON_GetObjectItemCaseSensitive(entry, "variant_of")), "a standalone copy has variant_of null");
    cJSON_Delete(items);
    cJSON *meta = NULL;
    CHECK(ursatz_gui_material_item_get(variant, &meta, NULL) == OK && strcmp(str(meta, "variant_of"), parent) == 0,
          "the get answers variant_of too");
    cJSON_Delete(meta);
    CHECK(ursatz_gui_material_item_get(parent, &meta, NULL) == OK &&
              cJSON_IsNull(cJSON_GetObjectItemCaseSensitive(meta, "variant_of")),
          "the get answers variant_of null for a motif");
    cJSON_Delete(meta);

    /* A theme using a variant blocks the delete of the motif until forced; nothing is deleted meanwhile. */
    char *user = new_theme_using("Variant user", home, grandchild);
    CHECK(user != NULL, "create a theme using the variant of a variant");
    CHECK(ursatz_gui_material_item_delete(parent, false) == URSATZ_GUI_MATERIAL_ERR_REFERENCED,
          "a use of a variant blocks the delete of its motif");
    CHECK(exists(parent) && exists(variant) && exists(grandchild), "nothing was deleted");
    cJSON *blockers = NULL;
    CHECK(ursatz_gui_material_delete_blockers(parent, &blockers) == OK && cJSON_GetArraySize(blockers) == 1 &&
              strcmp(str(cJSON_GetArrayItem(blockers, 0), "id"), user) == 0,
          "the blockers name the theme");
    cJSON_Delete(blockers);
    CHECK(ursatz_gui_material_item_delete(parent, true) == OK, "a forced delete covers the variants");
    CHECK(!exists(parent) && !exists(variant) && !exists(grandchild), "the motif, its variant and the variant's variant are gone");
    CHECK(exists(promoted) && exists(user), "a standalone copy and the theme stay");

    /* Without a user the delete takes the variants along. */
    char *motif2 = new_item("motif", "Second", home, kMotifBody);
    char *kid = new_variant("Second v1", home, motif2, "");
    CHECK(motif2 && kid, "create a second motif with a variant");
    CHECK(ursatz_gui_material_delete_blockers(motif2, &blockers) == OK && cJSON_GetArraySize(blockers) == 0,
          "nothing blocks the second motif");
    cJSON_Delete(blockers);
    CHECK(ursatz_gui_material_item_delete(motif2, false) == OK && !exists(motif2) && !exists(kid),
          "deleting a motif deletes its variants");
    CHECK(ursatz_gui_material_item_delete(kid, false) == URSATZ_GUI_MATERIAL_ERR_NOT_FOUND, "the variant is gone");

    /* A variant deleted on its own leaves its motif. */
    char *motif3 = new_item("motif", "Third", home, kMotifBody);
    char *kid3 = new_variant("Third v1", home, motif3, "");
    CHECK(ursatz_gui_material_item_delete(kid3, false) == OK && exists(motif3), "deleting a variant leaves its motif");

    ursatz_gui_material_item_delete(user, true);
    ursatz_gui_material_item_delete(promoted, true);
    ursatz_gui_material_item_delete(motif3, true);
    CHECK(ursatz_gui_material_project_delete(home) == OK, "delete the project");
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
    test_body_validation();
    test_references();
    test_forced_delete();
    test_variant_ownership();

    remove_store();
    if (failures) {
        fprintf(stderr, "%d check(s) failed\n", failures);
        return 1;
    }
    printf("material service tests passed\n");
    return 0;
}
