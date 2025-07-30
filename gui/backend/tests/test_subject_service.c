#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cJSON.h>

#include "service/material_service.h"
#include "service/subject_service.h"

static int failures = 0;

#define CHECK(cond, msg)                                            \
    do {                                                            \
        if (!(cond)) {                                              \
            fprintf(stderr, "FAIL: %s (line %d)\n", msg, __LINE__); \
            failures++;                                             \
        }                                                           \
    } while (0)

#define OK URSATZ_GUI_SUBJECT_OK

static char g_materials[64];
static char g_subjects[64];
static char g_piece[64];
static char g_piece_dir[100];
static char g_piece_meta[120];
static char g_project[64];

static const char *kPieceBody = "{\"uses\":[],\"piece\":{\"structure\":null}}";

static const char *str(const cJSON *object, const char *name) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);
    return cJSON_IsString(item) ? item->valuestring : "";
}

static int num(const cJSON *object, const char *name) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);
    return cJSON_IsNumber(item) ? item->valueint : -1;
}

static int length(const cJSON *object, const char *name) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);
    return cJSON_IsArray(item) ? cJSON_GetArraySize(item) : -1;
}

static bool lists(const cJSON *object, const char *name, const char *value) {
    const cJSON *entry;
    cJSON_ArrayForEach(entry, cJSON_GetObjectItemCaseSensitive(object, name)) {
        if (cJSON_IsString(entry) && strcmp(entry->valuestring, value) == 0) {
            return true;
        }
    }
    return false;
}

/* Creates a subject and returns its id (malloc'd). */
static char *new_subject(const char *name, const char *kind) {
    cJSON *subject = NULL;
    char *id = NULL;
    if (ursatz_gui_subject_create(name, kind, "", &subject) == OK) {
        id = strdup(str(subject, "id"));
    }
    cJSON_Delete(subject);
    return id;
}

/* Creates a piece item in the test project and returns its id (malloc'd). */
static char *new_item(const char *name) {
    cJSON *body = cJSON_Parse(kPieceBody);
    cJSON *item = NULL;
    char *id = NULL;
    if (ursatz_gui_material_item_create("piece", name, g_project, body, &item) == URSATZ_GUI_MATERIAL_OK) {
        id = strdup(str(item, "id"));
    }
    cJSON_Delete(body);
    cJSON_Delete(item);
    return id;
}

static cJSON *get(const char *id) {
    cJSON *subject = NULL;
    if (ursatz_gui_subject_get(id, &subject) != OK) {
        return NULL;
    }
    return subject;
}

static void test_crud(void) {
    cJSON *subject = NULL;
    CHECK(ursatz_gui_subject_create("  Anna  ", "character", "  the cellist ", &subject) == OK, "create");
    CHECK(strncmp(str(subject, "id"), "sub-", 4) == 0, "id prefix");
    CHECK(strcmp(str(subject, "name"), "Anna") == 0, "name trimmed");
    CHECK(strcmp(str(subject, "note"), "the cellist") == 0, "note trimmed");
    CHECK(strcmp(str(subject, "kind"), "character") == 0, "kind kept");
    CHECK(length(subject, "items") == 0 && length(subject, "pieces") == 0 && length(subject, "related") == 0,
          "empty link lists");
    char *id = strdup(str(subject, "id"));
    cJSON_Delete(subject);

    CHECK(ursatz_gui_subject_create("", "idea", "", NULL) == URSATZ_GUI_SUBJECT_ERR_INVALID_NAME, "empty name");
    char long_name[260];
    memset(long_name, 'a', 201);
    long_name[201] = '\0';
    CHECK(ursatz_gui_subject_create(long_name, "idea", "", NULL) == URSATZ_GUI_SUBJECT_ERR_INVALID_NAME,
          "name over 200");
    CHECK(ursatz_gui_subject_create("X", "person", "", NULL) == URSATZ_GUI_SUBJECT_ERR_INVALID_KIND, "bad kind");
    CHECK(ursatz_gui_subject_create("X", NULL, "", NULL) == URSATZ_GUI_SUBJECT_ERR_INVALID_KIND, "null kind");
    char *long_note = malloc(4002);
    memset(long_note, 'n', 4001);
    long_note[4001] = '\0';
    CHECK(ursatz_gui_subject_create("X", "note", long_note, NULL) == URSATZ_GUI_SUBJECT_ERR_INVALID_NAME,
          "note over 4000");
    free(long_note);

    subject = get(id);
    CHECK(subject && strcmp(str(subject, "name"), "Anna") == 0, "get");
    cJSON_Delete(subject);
    CHECK(ursatz_gui_subject_get("sub-missing", &subject) == URSATZ_GUI_SUBJECT_ERR_NOT_FOUND, "get missing");
    CHECK(ursatz_gui_subject_get("../x", &subject) == URSATZ_GUI_SUBJECT_ERR_INVALID_ID, "get bad id");

    CHECK(ursatz_gui_subject_update(id, "Anna K", "place", NULL, &subject) == OK, "update");
    CHECK(strcmp(str(subject, "name"), "Anna K") == 0 && strcmp(str(subject, "kind"), "place") == 0 &&
              strcmp(str(subject, "note"), "the cellist") == 0,
          "update changes only what is given");
    cJSON_Delete(subject);
    CHECK(ursatz_gui_subject_update(id, NULL, "robot", NULL, NULL) == URSATZ_GUI_SUBJECT_ERR_INVALID_KIND,
          "update bad kind");
    CHECK(ursatz_gui_subject_update("sub-missing", "A", NULL, NULL, NULL) == URSATZ_GUI_SUBJECT_ERR_NOT_FOUND,
          "update missing");

    char *second = new_subject("anna b", "idea");
    char *third = new_subject("Zed", "idea");
    cJSON *all = NULL;
    CHECK(ursatz_gui_subject_list(&all) == OK && cJSON_GetArraySize(all) == 3, "list size");
    CHECK(strcmp(str(cJSON_GetArrayItem(all, 0), "name"), "anna b") == 0 &&
              strcmp(str(cJSON_GetArrayItem(all, 1), "name"), "Anna K") == 0 &&
              strcmp(str(cJSON_GetArrayItem(all, 2), "name"), "Zed") == 0,
          "list in name order");
    CHECK(num(cJSON_GetArrayItem(all, 0), "item_count") == 0 && num(cJSON_GetArrayItem(all, 0), "piece_count") == 0 &&
              num(cJSON_GetArrayItem(all, 0), "related_count") == 0,
          "list carries counts");
    cJSON_Delete(all);

    CHECK(ursatz_gui_subject_delete(id) == OK, "delete");
    CHECK(ursatz_gui_subject_delete(id) == URSATZ_GUI_SUBJECT_ERR_NOT_FOUND, "delete twice");
    CHECK(ursatz_gui_subject_delete(second) == OK && ursatz_gui_subject_delete(third) == OK, "delete others");
    free(id);
    free(second);
    free(third);
}

static void test_links(void) {
    char *id = new_subject("Anna", "character");
    char *item = new_item("Opening");
    bool already = true;
    cJSON *subject = NULL;

    CHECK(ursatz_gui_subject_link_item(id, item, &already, &subject) == OK && !already, "link item");
    CHECK(lists(subject, "items", item), "item listed");
    cJSON_Delete(subject);
    CHECK(ursatz_gui_subject_link_item(id, item, &already, &subject) == OK && already, "link twice reports already");
    CHECK(length(subject, "items") == 1, "no duplicate item");
    cJSON_Delete(subject);
    CHECK(ursatz_gui_subject_link_item(id, "mot-0000000000000000", NULL, NULL) == URSATZ_GUI_SUBJECT_ERR_ITEM_NOT_FOUND,
          "link missing item");
    CHECK(ursatz_gui_subject_link_item(id, "../x", NULL, NULL) == URSATZ_GUI_SUBJECT_ERR_INVALID_ID,
          "link bad item id");
    CHECK(ursatz_gui_subject_link_item("sub-missing", item, NULL, NULL) == URSATZ_GUI_SUBJECT_ERR_NOT_FOUND,
          "link on missing subject");

    CHECK(ursatz_gui_subject_link_piece(id, g_piece, &already, &subject) == OK && !already, "link piece");
    CHECK(lists(subject, "pieces", g_piece), "piece listed");
    cJSON_Delete(subject);
    CHECK(ursatz_gui_subject_link_piece(id, g_piece, &already, NULL) == OK && already, "link piece twice");
    CHECK(ursatz_gui_subject_link_piece(id, "no-such-piece", NULL, NULL) == URSATZ_GUI_SUBJECT_ERR_PIECE_NOT_FOUND,
          "link missing piece");
    CHECK(ursatz_gui_subject_link_piece(id, "a/b", NULL, NULL) == URSATZ_GUI_SUBJECT_ERR_INVALID_ID,
          "link bad piece id");

    subject = get(id);
    CHECK(num(subject, "item_count") == -1 && length(subject, "items") == 1 && length(subject, "pieces") == 1,
          "get holds the links");
    cJSON_Delete(subject);

    CHECK(ursatz_gui_subject_unlink_item(id, item, &subject) == OK && length(subject, "items") == 0, "unlink item");
    cJSON_Delete(subject);
    CHECK(ursatz_gui_subject_unlink_item(id, item, NULL) == URSATZ_GUI_SUBJECT_ERR_NOT_LINKED, "unlink item twice");
    CHECK(ursatz_gui_subject_unlink_piece(id, g_piece, &subject) == OK && length(subject, "pieces") == 0,
          "unlink piece");
    cJSON_Delete(subject);
    CHECK(ursatz_gui_subject_unlink_piece(id, g_piece, NULL) == URSATZ_GUI_SUBJECT_ERR_NOT_LINKED,
          "unlink piece twice");

    CHECK(ursatz_gui_subject_delete(id) == OK, "delete subject");
    CHECK(ursatz_gui_material_item_delete(item, true) == URSATZ_GUI_MATERIAL_OK, "item survives subject delete");
    free(id);
    free(item);
}

static void test_relations(void) {
    char *a = new_subject("A", "character");
    char *b = new_subject("B", "place");
    char *c = new_subject("C", "idea");
    bool already = true;
    cJSON *subject = NULL;

    CHECK(ursatz_gui_subject_relate(a, b, &already, &subject) == OK && !already, "relate");
    CHECK(lists(subject, "related", b), "a lists b");
    cJSON_Delete(subject);
    subject = get(b);
    CHECK(lists(subject, "related", a), "b lists a");
    cJSON_Delete(subject);
    CHECK(ursatz_gui_subject_relate(b, a, &already, NULL) == OK && already, "relate again from the other side");
    subject = get(a);
    CHECK(length(subject, "related") == 1, "no duplicate relation");
    cJSON_Delete(subject);

    CHECK(ursatz_gui_subject_relate(a, a, NULL, NULL) == URSATZ_GUI_SUBJECT_ERR_SELF, "self refused");
    CHECK(ursatz_gui_subject_relate(a, "sub-missing", NULL, NULL) == URSATZ_GUI_SUBJECT_ERR_NOT_FOUND,
          "relate to missing");
    CHECK(ursatz_gui_subject_relate("sub-missing", a, NULL, NULL) == URSATZ_GUI_SUBJECT_ERR_NOT_FOUND,
          "relate from missing");
    subject = get(a);
    CHECK(length(subject, "related") == 1, "failed relations change nothing");
    cJSON_Delete(subject);

    CHECK(ursatz_gui_subject_relate(a, c, NULL, NULL) == OK, "relate a and c");
    CHECK(ursatz_gui_subject_unrelate(a, b, &subject) == OK && length(subject, "related") == 1 &&
              lists(subject, "related", c),
          "unrelate");
    cJSON_Delete(subject);
    subject = get(b);
    CHECK(length(subject, "related") == 0, "other side cleared");
    cJSON_Delete(subject);
    CHECK(ursatz_gui_subject_unrelate(a, b, NULL) == URSATZ_GUI_SUBJECT_ERR_NOT_RELATED, "unrelate twice");

    CHECK(ursatz_gui_subject_delete(c) == OK, "delete related subject");
    subject = get(a);
    CHECK(length(subject, "related") == 0, "delete cleans related lists");
    cJSON_Delete(subject);

    CHECK(ursatz_gui_subject_delete(a) == OK && ursatz_gui_subject_delete(b) == OK, "cleanup");
    free(a);
    free(b);
    free(c);
}

static void test_forget(void) {
    char *a = new_subject("A", "character");
    char *b = new_subject("B", "idea");
    char *item = new_item("Motif");
    cJSON *subject = NULL;

    CHECK(ursatz_gui_subject_link_item(a, item, NULL, NULL) == OK && ursatz_gui_subject_link_item(b, item, NULL, NULL) == OK,
          "link item twice");
    CHECK(ursatz_gui_subject_link_piece(a, g_piece, NULL, NULL) == OK, "link piece");

    CHECK(ursatz_gui_subject_forget_piece(g_piece) == OK, "forget piece");
    subject = get(a);
    CHECK(length(subject, "pieces") == 0 && length(subject, "items") == 1, "piece link gone, item link kept");
    cJSON_Delete(subject);
    CHECK(ursatz_gui_subject_forget_piece(g_piece) == OK, "forget absent piece");
    CHECK(ursatz_gui_subject_forget_piece("a/b") == URSATZ_GUI_SUBJECT_ERR_INVALID_ID, "forget bad piece id");

    CHECK(ursatz_gui_material_item_delete(item, false) == URSATZ_GUI_MATERIAL_OK, "delete item");
    subject = get(a);
    CHECK(length(subject, "items") == 0, "item delete clears the first subject");
    cJSON_Delete(subject);
    subject = get(b);
    CHECK(length(subject, "items") == 0, "item delete clears the second subject");
    cJSON_Delete(subject);

    CHECK(ursatz_gui_subject_delete(a) == OK && ursatz_gui_subject_delete(b) == OK, "cleanup");
    free(a);
    free(b);
    free(item);
}

static void test_groups(void) {
    char *anna = new_subject("Anna", "character");
    char *ben = new_subject("ben", "character");
    char *attic = new_subject("Attic", "place");
    char *item = new_item("Opening");
    CHECK(ursatz_gui_subject_link_item(anna, item, NULL, NULL) == OK, "link item");
    CHECK(ursatz_gui_subject_link_piece(anna, g_piece, NULL, NULL) == OK, "link piece");
    CHECK(ursatz_gui_subject_link_piece(attic, g_piece, NULL, NULL) == OK, "link piece to a place");

    cJSON *groups = NULL;
    CHECK(ursatz_gui_subject_groups(&groups) == OK && cJSON_GetArraySize(groups) == 4, "four groups");
    const char *kinds[] = {"character", "place", "idea", "note"};
    for (int i = 0; i < 4; i++) {
        CHECK(strcmp(str(cJSON_GetArrayItem(groups, i), "kind"), kinds[i]) == 0, "group order");
    }
    const cJSON *characters = cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(groups, 0), "subjects");
    CHECK(cJSON_GetArraySize(characters) == 2, "two characters");
    CHECK(strcmp(str(cJSON_GetArrayItem(characters, 0), "name"), "Anna") == 0 &&
              strcmp(str(cJSON_GetArrayItem(characters, 1), "name"), "ben") == 0,
          "characters in name order");
    CHECK(num(cJSON_GetArrayItem(characters, 0), "item_count") == 1 &&
              num(cJSON_GetArrayItem(characters, 0), "piece_count") == 1 &&
              num(cJSON_GetArrayItem(characters, 0), "link_count") == 2,
          "link counts add items and pieces");
    CHECK(num(cJSON_GetArrayItem(characters, 1), "link_count") == 0, "unlinked subject counts zero");
    const cJSON *places = cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(groups, 1), "subjects");
    CHECK(cJSON_GetArraySize(places) == 1 && num(cJSON_GetArrayItem(places, 0), "link_count") == 1, "place group");
    CHECK(cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(groups, 2), "subjects")) == 0,
          "empty kind has an empty array");
    cJSON_Delete(groups);

    ursatz_gui_subject_delete(anna);
    ursatz_gui_subject_delete(ben);
    ursatz_gui_subject_delete(attic);
    ursatz_gui_material_item_delete(item, true);
    free(anna);
    free(ben);
    free(attic);
    free(item);
}

static void remove_stores(void) {
    cJSON *subjects = NULL;
    if (ursatz_gui_subject_list(&subjects) == OK) {
        const cJSON *subject;
        cJSON_ArrayForEach(subject, subjects) {
            ursatz_gui_subject_delete(str(subject, "id"));
        }
        cJSON_Delete(subjects);
    }
    ursatz_gui_subject_service_shutdown();
    rmdir(g_subjects);

    cJSON *items = NULL;
    if (ursatz_gui_material_item_list(NULL, NULL, &items) == URSATZ_GUI_MATERIAL_OK) {
        const cJSON *item;
        cJSON_ArrayForEach(item, items) {
            ursatz_gui_material_item_delete(str(item, "id"), true);
        }
        cJSON_Delete(items);
    }
    cJSON *projects = NULL;
    if (ursatz_gui_material_project_list(&projects) == URSATZ_GUI_MATERIAL_OK) {
        const cJSON *project;
        cJSON_ArrayForEach(project, projects) {
            ursatz_gui_material_project_delete(str(project, "id"));
        }
        cJSON_Delete(projects);
    }
    ursatz_gui_material_service_shutdown();
    char path[100];
    snprintf(path, sizeof(path), "%s/projects", g_materials);
    rmdir(path);
    snprintf(path, sizeof(path), "%s/items", g_materials);
    rmdir(path);
    rmdir(g_materials);

    remove(g_piece_meta);
    rmdir(g_piece_dir);
    rmdir("pieces");
}

int main(void) {
    snprintf(g_materials, sizeof(g_materials), "materials_%d", (int)getpid());
    snprintf(g_subjects, sizeof(g_subjects), "subjects_%d", (int)getpid());
    snprintf(g_piece, sizeof(g_piece), "piece-%d", (int)getpid());
    snprintf(g_piece_dir, sizeof(g_piece_dir), "pieces/%s", g_piece);
    snprintf(g_piece_meta, sizeof(g_piece_meta), "%s/meta.json", g_piece_dir);

    CHECK(ursatz_gui_material_service_init(g_materials), "init materials");
    CHECK(ursatz_gui_subject_service_init(g_subjects), "init subjects");

    /* A Library piece is a directory with a meta.json; the subject service only asks whether it exists. */
    mkdir("pieces", 0755);
    mkdir(g_piece_dir, 0755);
    FILE *meta = fopen(g_piece_meta, "wb");
    CHECK(meta != NULL, "create piece");
    if (meta) {
        fputs("{}", meta);
        fclose(meta);
    }

    cJSON *project = NULL;
    CHECK(ursatz_gui_material_project_create("Subjects", "", &project) == URSATZ_GUI_MATERIAL_OK, "project");
    snprintf(g_project, sizeof(g_project), "%s", str(project, "id"));
    cJSON_Delete(project);

    test_crud();
    test_links();
    test_relations();
    test_forget();
    test_groups();

    remove_stores();
    if (failures) {
        fprintf(stderr, "%d check(s) failed\n", failures);
        return 1;
    }
    printf("subject service tests passed\n");
    return 0;
}
