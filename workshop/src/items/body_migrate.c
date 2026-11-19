#include "store_internal.h"
#include "composition/composer_client.h"

/* ---- old members ---------------------------------------------------------- */

/* Renames a member in place (its position stays). false on allocation failure. */
static bool rename_member(cJSON *object, const char *from, const char *to) {
    cJSON *item = cJSON_GetObjectItemCaseSensitive(object, from);
    char *name = item ? cJSON_malloc(strlen(to) + 1) : NULL;
    if (!name) {
        return item == NULL;
    }
    memcpy(name, to, strlen(to) + 1);
    if (!(item->type & cJSON_StringIsConst)) {
        cJSON_free(item->string);
    }
    item->type &= ~cJSON_StringIsConst;
    item->string = name;
    return true;
}

/* An old mode member (a key word, or "style") of object becomes the member `material` holding the id of the material; one
 * that is already there wins and the old member goes. A word that names no material stays as it was. */
static void migrate_mode_member(cJSON *object) {
    cJSON *mode = cJSON_GetObjectItemCaseSensitive(object, "mode");
    if (!mode) {
        return;
    }
    if (cJSON_GetObjectItemCaseSensitive(object, "material")) {
        cJSON_DeleteItemFromObjectCaseSensitive(object, "mode");
        return;
    }
    char id[64];
    if (cJSON_IsNull(mode)) {
        rename_member(object, "mode", "material");
    } else if (cJSON_IsString(mode) && workshop_composer_material_of_word(mode->valuestring, id, sizeof(id))) {
        cJSON *replacement = cJSON_CreateString(id);
        if (replacement && cJSON_ReplaceItemInObjectCaseSensitive(object, "mode", replacement)) {
            rename_member(object, "mode", "material");
        } else {
            cJSON_Delete(replacement);
        }
    }
}

/* A motif's old key text ("C major" or "C minor", motifs are in C) becomes the member `material`; a key on another tonic
 * stays as it was (the data check refuses it). */
static void migrate_motif_key(cJSON *motif) {
    if (!cJSON_IsObject(motif)) {
        return;
    }
    cJSON *key = cJSON_GetObjectItemCaseSensitive(motif, "key");
    if (!key) {
        return;
    }
    if (cJSON_GetObjectItemCaseSensitive(motif, "material")) {
        cJSON_DeleteItemFromObjectCaseSensitive(motif, "key");
        return;
    }
    char id[64];
    if (cJSON_IsNull(key)) {
        rename_member(motif, "key", "material");
    } else if (cJSON_IsString(key) && strncmp(key->valuestring, "C ", 2) == 0 &&
               workshop_composer_material_of_word(key->valuestring + 2, id, sizeof(id)) && strcmp(id, "style") != 0) {
        cJSON *replacement = cJSON_CreateString(id);
        if (replacement && cJSON_ReplaceItemInObjectCaseSensitive(motif, "key", replacement)) {
            rename_member(motif, "key", "material");
        } else {
            cJSON_Delete(replacement);
        }
    }
}

/* A stored progression fill is the section's harmony (R-610): {"progression", "cells"} on the section and an empty fill. A section that
 * has a harmony already keeps it and loses the fill's numerals. */
static void migrate_progression_fill(cJSON *section) {
    cJSON *fill = cJSON_GetObjectItemCaseSensitive(section, "fill");
    if (!cJSON_IsObject(fill) || strcmp(common_json_string(fill, "kind"), "progression") != 0) {
        return;
    }
    const cJSON *numerals = cJSON_GetObjectItemCaseSensitive(fill, "numerals");
    const cJSON *cells = cJSON_GetObjectItemCaseSensitive(fill, "cells");
    cJSON *harmony = store_is_absent_or_null(section, "harmony") && cJSON_IsString(numerals) ? cJSON_CreateObject() : NULL;
    /* The harmony is made from the fill before the fill goes. */
    if (harmony && cJSON_AddStringToObject(harmony, "progression", numerals->valuestring) &&
        (!cJSON_IsNumber(cells) || cJSON_AddNumberToObject(harmony, "cells", cells->valuedouble)) &&
        cJSON_AddItemToObject(section, "harmony", harmony)) {
        harmony = NULL;
    }
    cJSON_Delete(harmony);
    cJSON *empty = cJSON_CreateObject();
    if (!empty || !cJSON_AddStringToObject(empty, "kind", "empty") || !cJSON_ReplaceItemInObjectCaseSensitive(section, "fill", empty)) {
        cJSON_Delete(empty);
    }
}

void workshop_material_body_migrate(cJSON *body) {
    if (!cJSON_IsObject(body)) {
        return;
    }
    migrate_motif_key(cJSON_GetObjectItemCaseSensitive(body, "motif"));
    const cJSON *theme = cJSON_GetObjectItemCaseSensitive(body, "theme");
    if (cJSON_IsObject(cJSON_GetObjectItemCaseSensitive(theme, "form"))) {
        migrate_mode_member(cJSON_GetObjectItemCaseSensitive(theme, "form"));
    }
    const cJSON *piece = cJSON_GetObjectItemCaseSensitive(body, "piece");
    if (cJSON_IsObject(cJSON_GetObjectItemCaseSensitive(piece, "structure"))) {
        migrate_mode_member(cJSON_GetObjectItemCaseSensitive(piece, "structure"));
        cJSON *section = NULL;
        cJSON_ArrayForEach(section, cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(piece, "structure"), "sections")) {
            if (cJSON_IsObject(section)) {
                migrate_progression_fill(section);
            }
        }
    }
    cJSON *use = NULL;
    cJSON_ArrayForEach(use, cJSON_GetObjectItemCaseSensitive(body, "uses")) {
        workshop_material_body_migrate(cJSON_GetObjectItemCaseSensitive(use, "copy"));
    }
}
