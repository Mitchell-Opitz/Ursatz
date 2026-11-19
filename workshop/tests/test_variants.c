#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <cJSON.h>

#include "composition/motifs.h"
#include "composition/variants.h"
#include "items/store.h"
#include "test_knowledge.h"
#include "ursatz_check.h"

/*
 * The variant engine through the workshop functions, without HTTP: the lineage root of a three-deep variant, the reroll seed search
 * moving to a new seed, the link-form loop refusal and a nested variant tree.
 */

static char g_dir[512];

static const char *kMotifBody =
    "{\"uses\":[],\"motif\":{\"role\":\"melodic\",\"form\":\"full\",\"notes\":\"C4:q Eb4:e G4:e C5:h\","
    "\"meter\":\"4/4\",\"key\":\"C minor\",\"pickup\":\"0\",\"features\":[{\"label\":\"a\",\"first\":0,\"last\":2}],"
    "\"analysis\":null,\"derived\":null}}";

static const char *str(const cJSON *object, const char *name) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);
    return cJSON_IsString(item) ? item->valuestring : "";
}

static const cJSON *member(const cJSON *object, const char *name) {
    return cJSON_GetObjectItemCaseSensitive(object, name);
}

/* Saves the analyzed motif body in a new project and returns its id (malloc'd). */
static char *new_source(const char *project_id) {
    cJSON *body = cJSON_Parse(kMotifBody);
    char analysis_error[WORKSHOP_MOTIF_ANALYSIS_ERROR_SIZE];
    char refusal[512] = "";
    char refusal_path[128] = "";
    cJSON *item = NULL;
    char *id = NULL;
    if (workshop_motif_analyze_body(body, NULL, analysis_error, refusal, sizeof(refusal), refusal_path, sizeof(refusal_path)) ==
            WORKSHOP_MOTIF_ANALYSIS_DONE &&
        workshop_material_item_create_noted("motif", "Source", project_id, body, "Created", &item) == URSATZ_OK) {
        id = strdup(str(item, "id"));
    }
    cJSON_Delete(item);
    cJSON_Delete(body);
    return id;
}

/* One variants run on the item with the chain {invert} and the seed; the Composer's candidate in *candidate (caller-owned). */
static bool run_candidate(const char *id, int seed, cJSON **candidate) {
    char request[128];
    snprintf(request, sizeof(request), "{\"chain\":{\"steps\":[{\"op\":\"invert\"}]},\"count\":1,\"seed\":%d}", seed);
    WorkshopVariantsJob *job = NULL;
    UrsatzError error = {{0}, {0}};
    if (workshop_variants_job_new(WORKSHOP_VARIANTS_VARIANTS, id, cJSON_Parse(request), &job, &error) != URSATZ_OK) {
        return false;
    }
    WorkshopJobResult result = {0, NULL};
    workshop_variants_job_run(job, NULL, &result);
    workshop_variants_job_free(job);
    cJSON *document = result.json ? cJSON_Parse(result.json) : NULL;
    free(result.json);
    const cJSON *first = cJSON_GetArrayItem(member(document, "variants"), 0);
    *candidate = first ? cJSON_Duplicate(first, 1) : NULL;
    bool ok = result.http_status == 200 && *candidate;
    cJSON_Delete(document);
    return ok;
}

/* A variant of id saved from the candidate of the seed; the new id (malloc'd). */
static char *save_variant(const char *id, int seed) {
    cJSON *candidate = NULL;
    char *new_id = NULL;
    if (run_candidate(id, seed, &candidate)) {
        cJSON *body = cJSON_CreateObject();
        cJSON_AddItemToObject(body, "variant", candidate);
        candidate = NULL;
        cJSON *item = NULL;
        UrsatzError error = {{0}, {0}};
        if (workshop_variants_save(id, body, &item, &error) == URSATZ_OK) {
            new_id = strdup(str(item, "id"));
        }
        cJSON_Delete(item);
        cJSON_Delete(body);
    }
    cJSON_Delete(candidate);
    return new_id;
}

static void remove_store(void) {
    cJSON *projects = NULL;
    if (workshop_material_project_list(&projects) == URSATZ_OK) {
        const cJSON *project = NULL;
        cJSON_ArrayForEach(project, projects) {
            workshop_material_project_delete(str(project, "id"));
        }
        cJSON_Delete(projects);
    }
    workshop_material_service_shutdown();
    char path[540];
    snprintf(path, sizeof(path), "%s/projects", g_dir);
    rmdir(path);
    snprintf(path, sizeof(path), "%s/items", g_dir);
    rmdir(path);
    rmdir(g_dir);
}

int main(void) {
    test_knowledge();
    snprintf(g_dir, sizeof(g_dir), "%s/materials_%d", URSATZ_GUI_TEST_STORE_DIR, (int)getpid());
    check_quiet(workshop_material_service_init(g_dir), "init");

    cJSON *projects = NULL;
    check_quiet(workshop_material_project_list(&projects) == URSATZ_OK && cJSON_GetArraySize(projects) > 0, "a project exists");
    char project[64];
    snprintf(project, sizeof(project), "%s", str(cJSON_GetArrayItem(projects, 0), "id"));
    cJSON_Delete(projects);

    char *source = new_source(project);
    char *child = source ? save_variant(source, 3) : NULL;
    char *grandchild = child ? save_variant(child, 4) : NULL;
    char *great = grandchild ? save_variant(grandchild, 5) : NULL;
    check(source && child && grandchild && great, "a motif and three generations of variants are saved");
    if (!great) {
        free(source);
        free(child);
        free(grandchild);
        remove_store();
        return test_finish("workshop_variants");
    }

    /* The lineage root of a three-deep variant is the motif it all comes from. */
    cJSON *leaf = NULL;
    check(workshop_material_item_get(great, NULL, &leaf) == URSATZ_OK, "the third variant is read");
    cJSON *root = workshop_variants_lineage_root(member(leaf, "motif"));
    cJSON *source_body = NULL;
    check(workshop_material_item_get(source, NULL, &source_body) == URSATZ_OK, "the source is read");
    check(root && cJSON_IsNull(member(member(root, "motif"), "derived")) &&
              strcmp(str(member(root, "motif"), "degrees"), str(member(source_body, "motif"), "degrees")) == 0,
          "the lineage root of a three-deep variant is the source's body");
    cJSON *root_of_source = workshop_variants_lineage_root(member(source_body, "motif"));
    check(root_of_source == NULL, "a motif that is not derived has no lineage root");
    cJSON_Delete(root);
    cJSON_Delete(leaf);
    cJSON_Delete(source_body);

    /* A reroll draws a new seed (the stored seed plus 1000003) and a different motif; it is a new version. */
    cJSON *before = NULL;
    check(workshop_material_item_get(child, NULL, &before) == URSATZ_OK, "the first variant is read");
    double seed = cJSON_IsNumber(member(member(member(before, "motif"), "derived"), "seed"))
               ? member(member(member(before, "motif"), "derived"), "seed")->valuedouble
                     : -1;
    char degrees[256];
    snprintf(degrees, sizeof(degrees), "%s", str(member(before, "motif"), "degrees"));
    cJSON_Delete(before);
    WorkshopVariantsJob *job = NULL;
    UrsatzError error = {{0}, {0}};
    check(workshop_variants_job_new(WORKSHOP_VARIANTS_REROLL, child, NULL, &job, &error) == URSATZ_OK, "a reroll job is made");
    WorkshopJobResult result = {0, NULL};
    workshop_variants_job_run(job, NULL, &result);
    workshop_variants_job_free(job);
    check(result.http_status == 200, "the reroll answers 200");
    free(result.json);
    cJSON *after = NULL;
    check(workshop_material_item_get(child, NULL, &after) == URSATZ_OK, "the first variant is read again");
    const cJSON *derived = member(member(after, "motif"), "derived");
    check(cJSON_IsNumber(member(derived, "seed")) && member(derived, "seed")->valuedouble == seed + 1000003 &&
              strcmp(str(member(after, "motif"), "degrees"), degrees) != 0,
          "a reroll moves the seed and changes the degrees");
    check(strcmp(str(derived, "flag"), "unflagged") == 0 && strcmp(str(derived, "source_item"), source) == 0,
          "a reroll resets the flag and keeps the source");
    cJSON_Delete(after);
    result = (WorkshopJobResult){0, NULL};
    check(workshop_variants_job_new(WORKSHOP_VARIANTS_REROLL, source, NULL, &job, &error) == URSATZ_OK, "a reroll job of the source is made");
    workshop_variants_job_run(job, NULL, &result);
    workshop_variants_job_free(job);
    check(result.http_status == 400, "a motif that is not derived cannot be rerolled");
    free(result.json);

    /* The link-form loop refusal: the source cannot be made a form of its own variant. */
    cJSON *link = cJSON_Parse("{\"source\":{\"item\":\"\",\"version\":1},\"recognition\":{\"found\":true}}");
    cJSON_ReplaceItemInObjectCaseSensitive(cJSON_GetObjectItemCaseSensitive(link, "source"), "item", cJSON_CreateString(great));
    cJSON *item = NULL;
    UrsatzStatus status = workshop_variants_link_form(source, link, &item, &error);
    check(status == URSATZ_ERR_CONFLICT && strstr(error.message, "cannot be a form of itself") != NULL,
          "a motif cannot be linked as a form of its own variant");
    cJSON_Delete(item);
    cJSON_Delete(link);

    /* The variant tree nests the variants of variants. */
    cJSON *tree = NULL;
    check(workshop_variants_tree(source, &tree, &error) == URSATZ_OK, "the tree of the source");
    check(strcmp(str(tree, "id"), source) == 0 && cJSON_IsNull(member(tree, "derived")) &&
              cJSON_GetArraySize(member(tree, "children")) == 1,
          "the tree starts at the source with one child");
    const cJSON *node = tree;
    const char *chain[] = {child, grandchild, great};
    bool nested = true;
    for (int depth = 0; depth < 3; depth++) {
        node = cJSON_GetArrayItem(member(node, "children"), 0);
        nested = nested && node && strcmp(str(node, "id"), chain[depth]) == 0;
    }
    check(nested && cJSON_GetArraySize(member(node, "children")) == 0, "the tree nests three generations and ends at a leaf");
    cJSON_Delete(tree);
    check(workshop_variants_tree("mot-0000000000000000", &tree, &error) == URSATZ_ERR_NOT_FOUND, "the tree of a missing item is not found");

    free(source);
    free(child);
    free(grandchild);
    free(great);
    remove_store();
    return test_finish("workshop_variants");
}
