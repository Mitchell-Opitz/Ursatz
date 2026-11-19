#include "store_internal.h"
#include "layout.h"
#include "composition/composer_client.h"

/*
 * Returns in *out a copy of body whose uses each carry a fresh "copy" of the
 * version they name (a client-sent copy is replaced). A use of an item or
 * version that does not exist, or of self_id (may be NULL), is INVALID_BODY.
 */
static UrsatzStatus fill_uses_locked(const char *self_id, const cJSON *body, cJSON **out) {
    cJSON *filled = cJSON_Duplicate(body, 1);
    if (!filled) {
        return workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
    }
    workshop_material_body_migrate(filled);
    UrsatzStatus result = URSATZ_OK;
    cJSON *use = NULL;
    cJSON_ArrayForEach(use, cJSON_GetObjectItemCaseSensitive(filled, "uses")) {
        const char *source_id = common_json_string(use, "item_id");
        int number = cJSON_GetObjectItemCaseSensitive(use, "version")->valueint;
        cJSON *source = NULL;
        cJSON *copy = NULL;
        if (self_id && strcmp(source_id, self_id) == 0) {
            result = workshop_fail(URSATZ_ERR_REQUEST, "invalid body");
        } else {
            result = store_load_item_locked(source_id, &source);
        }
        if (result == URSATZ_OK) {
            result = number <= store_current_version_of(source) ? store_read_version_locked(source_id, number, &copy)
                                                           : workshop_fail(URSATZ_ERR_NOT_FOUND, "not found");
        }
        cJSON_Delete(source);
        if (result == URSATZ_ERR_NOT_FOUND || result == URSATZ_ERR_REQUEST) {
            result = workshop_fail(URSATZ_ERR_REQUEST, "invalid body");
        }
        if (result == URSATZ_OK) {
            cJSON_DeleteItemFromObjectCaseSensitive(use, "copy");
            if (!cJSON_AddItemToObject(use, "copy", copy)) {
                cJSON_Delete(copy);
                result = workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
            }
        }
        if (result != URSATZ_OK) {
            break;
        }
    }
    if (result != URSATZ_OK) {
        cJSON_Delete(filled);
        return result;
    }
    *out = filled;
    return URSATZ_OK;
}

#define BODY_REFERENCES_MAX 40

typedef enum { REF_MELODIC_MOTIF, REF_ACCOMPANIMENT_MOTIF, REF_THEME } ReferenceKind;

/*
 * The references a body makes to versions of other items: a theme's slots in order, its ideas, then its accompaniment when its
 * pattern is "motive"; a piece's motive, then the motif and theme fills of its sections in order. Placeholders make
 * none. refs receives up to BODY_REFERENCES_MAX pointers into body, kinds the kind of each; the count is returned.
 */
static size_t body_references(const cJSON *body, const cJSON *refs[BODY_REFERENCES_MAX], ReferenceKind kinds[BODY_REFERENCES_MAX]) {
    size_t count = 0;
    const cJSON *theme = cJSON_GetObjectItemCaseSensitive(body, "theme");
    if (cJSON_IsObject(theme) && !store_theme_is_placeholder(theme)) {
        const cJSON *slot = NULL;
        int slot_cap = cJSON_GetArraySize(workshop_composer_vocabulary_list("theme_slots"));
        cJSON_ArrayForEach(slot, cJSON_GetObjectItemCaseSensitive(theme, "slots")) {
            if (count < (size_t)slot_cap) {
                kinds[count] = REF_MELODIC_MOTIF;
                refs[count++] = slot;
            }
        }
        const cJSON *idea = NULL;
        cJSON_ArrayForEach(idea, cJSON_GetObjectItemCaseSensitive(theme, "ideas")) {
            if (count < BODY_REFERENCES_MAX) {
                kinds[count] = REF_MELODIC_MOTIF;
                refs[count++] = idea;
            }
        }
        const cJSON *accompaniment = cJSON_GetObjectItemCaseSensitive(theme, "accompaniment");
        const cJSON *pattern = cJSON_GetObjectItemCaseSensitive(accompaniment, "pattern");
        if (cJSON_IsObject(accompaniment) && cJSON_IsString(pattern) && strcmp(pattern->valuestring, "motive") == 0) {
            kinds[count] = REF_ACCOMPANIMENT_MOTIF;
            refs[count++] = accompaniment;
        }
    }
    const cJSON *piece = cJSON_GetObjectItemCaseSensitive(body, "piece");
    if (cJSON_IsObject(piece) && !store_piece_is_placeholder(piece)) {
        const cJSON *motive = cJSON_GetObjectItemCaseSensitive(piece, "motive");
        if (cJSON_IsObject(motive)) {
            kinds[count] = REF_MELODIC_MOTIF;
            refs[count++] = motive;
        }
        const cJSON *section = NULL;
        cJSON_ArrayForEach(section, cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(piece, "structure"), "sections")) {
            const cJSON *fill = cJSON_GetObjectItemCaseSensitive(section, "fill");
            const char *kind = common_json_string(fill, "kind");
            bool theme_fill = strcmp(kind, "theme") == 0;
            if (count < BODY_REFERENCES_MAX && (theme_fill || strcmp(kind, "motif") == 0)) {
                kinds[count] = theme_fill ? REF_THEME : REF_MELODIC_MOTIF;
                refs[count++] = fill;
            }
        }
    }
    return count;
}

/*
 * What a reference names must exist at that version: a motif with form full and the role the use needs, or a theme
 * version that was composed (a result and a plan.json beside the version).
 */
static UrsatzStatus check_reference_locked(const cJSON *reference, ReferenceKind kind) {
    const char *source_id = common_json_string(reference, "item_id");
    int number = cJSON_GetObjectItemCaseSensitive(reference, "version")->valueint;
    bool theme = kind == REF_THEME;
    cJSON *source = NULL;
    cJSON *version = NULL;
    UrsatzStatus result = store_load_item_locked(source_id, &source);
    if (result == URSATZ_OK) {
        result = strcmp(common_json_string(source, "type"), theme ? "theme" : "motif") == 0 && number <= store_current_version_of(source)
                     ? store_read_version_locked(source_id, number, &version)
                     : workshop_fail(URSATZ_ERR_REQUEST, "invalid body");
    }
    if (result == URSATZ_OK && theme) {
        char plan[PATH_BYTES];
        const cJSON *composed = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(version, "theme"), "result");
        if (!cJSON_IsObject(composed) ||
            !workshop_layout_version_file(common_json_string(source, "project_id"), source_id, number, "plan.json", plan, sizeof(plan)) ||
            !common_path_exists(plan)) {
            result = workshop_fail(URSATZ_ERR_REQUEST, "invalid body");
        }
    } else if (result == URSATZ_OK) {
        const cJSON *motif = cJSON_GetObjectItemCaseSensitive(version, "motif");
        const char *form = common_json_string(motif, "form");
        const char *role = common_json_string(motif, "role");
        if (!form || !role || strcmp(form, "full") != 0 ||
            strcmp(role, kind == REF_ACCOMPANIMENT_MOTIF ? "accompaniment" : "melodic") != 0) {
            result = workshop_fail(URSATZ_ERR_REQUEST, "invalid body");
        }
    }
    if (result == URSATZ_ERR_NOT_FOUND || result == URSATZ_ERR_REQUEST) {
        result = workshop_fail(URSATZ_ERR_REQUEST, "invalid body");
    }
    cJSON_Delete(source);
    cJSON_Delete(version);
    return result;
}

/* Adds {item_id, version} to uses unless an entry with both is there. false on allocation failure. */
static bool add_use(cJSON *uses, const char *item_id, int version) {
    const cJSON *use = NULL;
    cJSON_ArrayForEach(use, uses) {
        if (strcmp(common_json_string(use, "item_id"), item_id) == 0 &&
            cJSON_GetObjectItemCaseSensitive(use, "version")->valueint == version) {
            return true;
        }
    }
    cJSON *entry = cJSON_CreateObject();
    if (!entry || !cJSON_AddStringToObject(entry, "item_id", item_id) || !cJSON_AddNumberToObject(entry, "version", version)) {
        cJSON_Delete(entry);
        return false;
    }
    cJSON_AddItemToArray(uses, entry);
    return true;
}

/*
 * Returns in *out a copy of body. For a theme or piece that is not a placeholder, the motifs and themes it names
 * are checked, and its uses of those items are replaced by one use for each version named (a client use of them is
 * dropped; its other uses stay). Any other body is copied as it is.
 */
static UrsatzStatus prepare_uses_locked(const cJSON *body, cJSON **out) {
    cJSON *prepared = cJSON_Duplicate(body, 1);
    if (!prepared) {
        return workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
    }
    const cJSON *refs[BODY_REFERENCES_MAX];
    ReferenceKind kinds[BODY_REFERENCES_MAX];
    size_t count = body_references(prepared, refs, kinds);
    if (count == 0) {
        *out = prepared;
        return URSATZ_OK;
    }
    UrsatzStatus result = URSATZ_OK;
    for (size_t i = 0; result == URSATZ_OK && i < count; i++) {
        result = check_reference_locked(refs[i], kinds[i]);
    }
    cJSON *uses = cJSON_CreateArray();
    if (result == URSATZ_OK && !uses) {
        result = workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
    }
    const cJSON *use = NULL;
    cJSON_ArrayForEach(use, cJSON_GetObjectItemCaseSensitive(prepared, "uses")) {
        bool replaced = false;
        for (size_t i = 0; i < count; i++) {
            replaced = replaced || strcmp(common_json_string(refs[i], "item_id"), common_json_string(use, "item_id")) == 0;
        }
        if (result == URSATZ_OK && !replaced) {
            cJSON *kept = cJSON_Duplicate(use, 1);
            if (kept) {
                cJSON_AddItemToArray(uses, kept);
            } else {
                result = workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
            }
        }
    }
    for (size_t i = 0; result == URSATZ_OK && i < count; i++) {
        if (!add_use(uses, common_json_string(refs[i], "item_id"),
                           cJSON_GetObjectItemCaseSensitive(refs[i], "version")->valueint)) {
            result = workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
        }
    }
    if (result == URSATZ_OK) {
        cJSON_DeleteItemFromObjectCaseSensitive(prepared, "uses");
        if (!cJSON_AddItemToObject(prepared, "uses", uses)) {
            result = workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
        } else {
            uses = NULL;
        }
    }
    cJSON_Delete(uses);
    if (result != URSATZ_OK) {
        cJSON_Delete(prepared);
        return result;
    }
    *out = prepared;
    return URSATZ_OK;
}

/* prepare_uses_locked, then fill_uses_locked: *out is body with the uses a theme's slots imply and a fresh copy in each. */
UrsatzStatus store_fill_body_uses_locked(const char *self_id, const cJSON *body, cJSON **out) {
    cJSON *prepared = NULL;
    UrsatzStatus result = prepare_uses_locked(body, &prepared);
    if (result == URSATZ_OK) {
        result = fill_uses_locked(self_id, prepared, out);
    }
    cJSON_Delete(prepared);
    return result;
}

UrsatzStatus workshop_material_body_with_uses(const cJSON *body, cJSON **out) {
    *out = NULL;
    LOCK();
    UrsatzStatus result = store_fill_body_uses_locked(NULL, body, out);
    UNLOCK();
    return result;
}

/* Points every reference of a theme or piece body that names source_id at version latest. false on allocation failure. */
static bool bump_versions(cJSON *body, const char *source_id, int latest) {
    const cJSON *refs[BODY_REFERENCES_MAX];
    ReferenceKind kinds[BODY_REFERENCES_MAX];
    size_t count = body_references(body, refs, kinds);
    bool ok = true;
    for (size_t i = 0; i < count; i++) {
        if (strcmp(common_json_string(refs[i], "item_id"), source_id) == 0) {
            cJSON_DeleteItemFromObjectCaseSensitive((cJSON *)refs[i], "version");
            ok = ok && cJSON_AddNumberToObject((cJSON *)refs[i], "version", latest) != NULL;
        }
    }
    return ok;
}

bool workshop_material_item_is_variant(const cJSON *body) {
    const cJSON *motif = cJSON_GetObjectItemCaseSensitive(body, "motif");
    const cJSON *derived = cJSON_GetObjectItemCaseSensitive(motif, "derived");
    return cJSON_IsObject(derived) && !cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(derived, "standalone"));
}

/* Sets "variant_of" on an item object: the source item of a variant, else null. The member is added to answers only;
 * item.json never carries it. known_body is the item's current version when the caller has it, else NULL and a motif
 * reads it. */
bool store_annotate_variant_of_locked(cJSON *item, const cJSON *known_body) {
    cJSON *own_body = NULL;
    const cJSON *body = known_body;
    if (!body && strcmp(common_json_string(item, "type"), "motif") == 0 &&
        store_read_version_locked(common_json_string(item, "id"), store_current_version_of(item), &own_body) == URSATZ_OK) {
        body = own_body;
    }
    cJSON_DeleteItemFromObjectCaseSensitive(item, "variant_of");
    bool ok;
    if (body && workshop_material_item_is_variant(body)) {
        const cJSON *derived = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(body, "motif"), "derived");
        ok = cJSON_AddStringToObject(item, "variant_of", common_json_string(derived, "source_item")) != NULL;
    } else {
        ok = cJSON_AddNullToObject(item, "variant_of") != NULL;
    }
    cJSON_Delete(own_body);
    return ok;
}

UrsatzStatus workshop_material_uses_status(const char *id, cJSON **out_status) {
    *out_status = NULL;
    LOCK();
    if (!store_ready) {
        UNLOCK();
        return workshop_fail(URSATZ_ERR_INTERNAL, "material store failure");
    }
    cJSON *item = NULL;
    cJSON *body = NULL;
    cJSON *status = NULL;
    UrsatzStatus result = store_load_item_locked(id, &item);
    if (result == URSATZ_OK) {
        result = store_read_version_locked(id, store_current_version_of(item), &body);
    }
    if (result == URSATZ_OK) {
        status = cJSON_CreateArray();
        result = status ? URSATZ_OK : workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
    }
    const cJSON *uses = result == URSATZ_OK ? cJSON_GetObjectItemCaseSensitive(body, "uses") : NULL;
    const cJSON *use = NULL;
    cJSON_ArrayForEach(use, uses) {
        const char *source_id = common_json_string(use, "item_id");
        const cJSON *version = cJSON_GetObjectItemCaseSensitive(use, "version");
        int used = cJSON_IsNumber(version) ? version->valueint : 0;
        cJSON *source = NULL;
        cJSON *entry = cJSON_CreateObject();
        bool ok = entry && cJSON_AddStringToObject(entry, "item_id", source_id) &&
                  cJSON_AddNumberToObject(entry, "version", used);
        if (ok && store_load_item_locked(source_id, &source) == URSATZ_OK) {
            int latest = store_current_version_of(source);
            ok = cJSON_AddNumberToObject(entry, "current_version", latest) &&
                 cJSON_AddStringToObject(entry, "name", common_json_string(source, "name")) &&
                 cJSON_AddStringToObject(entry, "type", common_json_string(source, "type")) &&
                 cJSON_AddStringToObject(entry, "project_id", common_json_string(source, "project_id")) &&
                 cJSON_AddBoolToObject(entry, "updated", latest > used) && cJSON_AddBoolToObject(entry, "missing", false);
        } else if (ok) {
            ok = cJSON_AddBoolToObject(entry, "updated", false) && cJSON_AddBoolToObject(entry, "missing", true);
        }
        cJSON_Delete(source);
        if (!ok) {
            cJSON_Delete(entry);
            result = workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
            break;
        }
        cJSON_AddItemToArray(status, entry);
    }
    UNLOCK();
    cJSON_Delete(item);
    cJSON_Delete(body);
    if (result == URSATZ_OK) {
        *out_status = status;
    } else {
        cJSON_Delete(status);
    }
    return result;
}

/*
 * Builds the array of items whose current version uses id: {id, name, type,
 * project_id, version}. The item need not exist (a deleted source still has users).
 */
static UrsatzStatus used_by_locked(const char *id, cJSON **out) {
    CommonNames names = {0};
    if (!store_is_valid_id(id)) {
        return workshop_fail(URSATZ_ERR_REQUEST, "invalid id");
    }
    if (store_item_ids_locked(&names) != URSATZ_OK) {
        common_names_free(&names);
        return workshop_fail(URSATZ_ERR_INTERNAL, "material store failure");
    }
    cJSON *users = cJSON_CreateArray();
    UrsatzStatus result = users ? URSATZ_OK : workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
    for (size_t i = 0; result == URSATZ_OK && i < names.count; i++) {
        cJSON *item = NULL;
        cJSON *body = NULL;
        if (strcmp(names.items[i], id) == 0 || store_load_item_locked(names.items[i], &item) != URSATZ_OK) {
            continue;
        }
        if (store_read_version_locked(names.items[i], store_current_version_of(item), &body) == URSATZ_OK) {
            const cJSON *use = NULL;
            cJSON_ArrayForEach(use, cJSON_GetObjectItemCaseSensitive(body, "uses")) {
                const cJSON *version = cJSON_GetObjectItemCaseSensitive(use, "version");
                if (strcmp(common_json_string(use, "item_id"), id) != 0) {
                    continue;
                }
                cJSON *entry = cJSON_CreateObject();
                bool ok = entry && cJSON_AddStringToObject(entry, "id", names.items[i]) &&
                          cJSON_AddStringToObject(entry, "name", common_json_string(item, "name")) &&
                          cJSON_AddStringToObject(entry, "type", common_json_string(item, "type")) &&
                          cJSON_AddStringToObject(entry, "project_id", common_json_string(item, "project_id")) &&
                          cJSON_AddNumberToObject(entry, "version", cJSON_IsNumber(version) ? version->valueint : 0);
                if (!ok) {
                    cJSON_Delete(entry);
                    result = workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
                } else {
                    cJSON_AddItemToArray(users, entry);
                }
                break;
            }
        }
        cJSON_Delete(item);
        cJSON_Delete(body);
    }
    common_names_free(&names);
    if (result == URSATZ_OK) {
        *out = users;
    } else {
        cJSON_Delete(users);
    }
    return result;
}

static UrsatzStatus derived_motifs_locked(cJSON **out) {
    CommonNames names = {0};
    if (store_item_ids_locked(&names) != URSATZ_OK) {
        common_names_free(&names);
        return workshop_fail(URSATZ_ERR_INTERNAL, "material store failure");
    }
    cJSON *found = cJSON_CreateArray();
    UrsatzStatus result = found ? URSATZ_OK : workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
    for (size_t i = 0; result == URSATZ_OK && i < names.count; i++) {
        cJSON *item = NULL;
        cJSON *body = NULL;
        if (store_load_item_locked(names.items[i], &item) != URSATZ_OK) {
            continue;
        }
        if (strcmp(common_json_string(item, "type"), "motif") == 0 &&
            store_read_version_locked(names.items[i], store_current_version_of(item), &body) == URSATZ_OK) {
            cJSON *derived = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(body, "motif"), "derived");
            if (cJSON_IsObject(derived)) {
                cJSON *entry = cJSON_CreateObject();
                cJSON *copy = cJSON_Duplicate(derived, 1);
                bool ok = entry && copy && cJSON_AddStringToObject(entry, "id", names.items[i]) &&
                          cJSON_AddStringToObject(entry, "name", common_json_string(item, "name")) &&
                          cJSON_AddNumberToObject(entry, "current_version", store_current_version_of(item)) &&
                          cJSON_AddItemToObject(entry, "derived", copy);
                if (!ok) {
                    if (entry && !cJSON_GetObjectItemCaseSensitive(entry, "derived")) {
                        cJSON_Delete(copy);
                    }
                    cJSON_Delete(entry);
                    result = workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
                } else {
                    cJSON_AddItemToArray(found, entry);
                }
            }
        }
        cJSON_Delete(item);
        cJSON_Delete(body);
    }
    common_names_free(&names);
    if (result == URSATZ_OK) {
        *out = found;
    } else {
        cJSON_Delete(found);
    }
    return result;
}

UrsatzStatus workshop_material_derived_motifs(cJSON **out_motifs) {
    *out_motifs = NULL;
    LOCK();
    UrsatzStatus result = store_ready ? derived_motifs_locked(out_motifs) : workshop_fail(URSATZ_ERR_INTERNAL, "material store failure");
    UNLOCK();
    return result;
}

UrsatzStatus workshop_material_used_by(const char *id, cJSON **out_users) {
    *out_users = NULL;
    LOCK();
    UrsatzStatus result = store_ready ? used_by_locked(id, out_users) : workshop_fail(URSATZ_ERR_INTERNAL, "material store failure");
    UNLOCK();
    return result;
}

UrsatzStatus workshop_material_update_use(const char *id, const char *source_id, cJSON **out_item) {
    if (out_item) {
        *out_item = NULL;
    }
    WorkshopTxn *txn = store_txn_begin("item.update-use", id ? id : "");
    if (!txn) {
        return workshop_fail(URSATZ_ERR_INTERNAL, "material store failure");
    }
    LOCK();
    if (!store_ready) {
        UNLOCK();
        workshop_txn_abort(txn);
        return workshop_fail(URSATZ_ERR_INTERNAL, "material store failure");
    }
    cJSON *item = NULL;
    cJSON *source = NULL;
    cJSON *body = NULL;
    cJSON *stored = NULL;
    char note[NAME_MAX_CHARS + 40];
    UrsatzStatus result = store_is_valid_id(source_id) ? store_load_item_locked(id, &item)
                                                            : workshop_fail(URSATZ_ERR_REQUEST, "invalid id");
    if (result == URSATZ_OK) {
        result = store_read_version_locked(id, store_current_version_of(item), &body);
    }
    if (result == URSATZ_OK) {
        result = store_load_item_locked(source_id, &source);
    }
    if (result == URSATZ_OK) {
        /* Point every use of the source at its current version; fill_uses_locked refreshes the copies. */
        int latest = store_current_version_of(source);
        bool found = false;
        bool ok = true;
        cJSON *use = NULL;
        cJSON_ArrayForEach(use, cJSON_GetObjectItemCaseSensitive(body, "uses")) {
            if (strcmp(common_json_string(use, "item_id"), source_id) == 0) {
                found = true;
                cJSON_DeleteItemFromObjectCaseSensitive(use, "version");
                ok = ok && cJSON_AddNumberToObject(use, "version", latest) != NULL;
            }
        }
        if (!ok || !bump_versions(body, source_id, latest)) {
            result = workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
        } else if (!found) {
            result = workshop_fail(URSATZ_ERR_NOT_FOUND, "not found");
        } else {
            snprintf(note, sizeof(note), "updated %s to v%d", common_json_string(source, "name"), latest);
            result = store_fill_body_uses_locked(id, body, &stored);
        }
    }
    if (result == URSATZ_OK) {
        result = store_commit_version_locked(txn, id, item, stored, note, NULL);
    }
    UNLOCK();
    result = store_txn_end(txn, result);
    if (result == URSATZ_OK && out_item) {
        *out_item = item;
    } else {
        cJSON_Delete(item);
    }
    cJSON_Delete(source);
    cJSON_Delete(body);
    cJSON_Delete(stored);
    return result;
}

/* A set of item ids. */
typedef struct {
    char **ids;
    size_t count;
    size_t capacity;
} IdSet;

static bool id_set_has(const IdSet *set, const char *id) {
    for (size_t i = 0; i < set->count; i++) {
        if (strcmp(set->ids[i], id) == 0) {
            return true;
        }
    }
    return false;
}

static bool id_set_add(IdSet *set, const char *id) {
    if (set->count == set->capacity) {
        size_t capacity = set->capacity ? set->capacity * 2 : 8;
        char **grown = realloc(set->ids, capacity * sizeof(char *));
        if (!grown) {
            return false;
        }
        set->ids = grown;
        set->capacity = capacity;
    }
    set->ids[set->count] = strdup(id);
    if (!set->ids[set->count]) {
        return false;
    }
    set->count++;
    return true;
}

static void id_set_free(IdSet *set) {
    for (size_t i = 0; i < set->count; i++) {
        free(set->ids[i]);
    }
    free(set->ids);
    set->ids = NULL;
    set->count = 0;
    set->capacity = 0;
}

/* Fills out with the ids of the variants of id, recursively (a variant of a variant too), in one scan of the store.
 * id itself is not in the set. */
static UrsatzStatus variant_closure_locked(const char *id, IdSet *out) {
    CommonNames names = {0};
    if (store_item_ids_locked(&names) != URSATZ_OK) {
        common_names_free(&names);
        return workshop_fail(URSATZ_ERR_INTERNAL, "material store failure");
    }
    IdSet children = {0};
    IdSet parents = {0};
    UrsatzStatus result = URSATZ_OK;
    for (size_t i = 0; result == URSATZ_OK && i < names.count; i++) {
        cJSON *item = NULL;
        cJSON *body = NULL;
        if (store_load_item_locked(names.items[i], &item) == URSATZ_OK && strcmp(common_json_string(item, "type"), "motif") == 0 &&
            store_read_version_locked(names.items[i], store_current_version_of(item), &body) == URSATZ_OK &&
            workshop_material_item_is_variant(body)) {
            const cJSON *derived = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(body, "motif"), "derived");
            if (!id_set_add(&children, names.items[i]) || !id_set_add(&parents, common_json_string(derived, "source_item"))) {
                result = workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
            }
        }
        cJSON_Delete(item);
        cJSON_Delete(body);
    }
    common_names_free(&names);
    /* Walk down from id: the descendants found so far are also parents to look for. */
    for (size_t level = 0; result == URSATZ_OK && level <= out->count; level++) {
        const char *parent = level == 0 ? id : out->ids[level - 1];
        for (size_t i = 0; result == URSATZ_OK && i < children.count; i++) {
            if (strcmp(parents.ids[i], parent) == 0 && strcmp(children.ids[i], id) != 0 && !id_set_has(out, children.ids[i]) &&
                !id_set_add(out, children.ids[i])) {
                result = workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
            }
        }
    }
    id_set_free(&children);
    id_set_free(&parents);
    return result;
}

/* The items outside the delete set (id and its variants) that use any member of it, as used_by_locked entries once each. */
static UrsatzStatus delete_blockers_locked(const char *id, const IdSet *variants, cJSON **out) {
    cJSON *blockers = cJSON_CreateArray();
    if (!blockers) {
        return workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
    }
    UrsatzStatus result = URSATZ_OK;
    for (size_t member = 0; result == URSATZ_OK && member <= variants->count; member++) {
        cJSON *users = NULL;
        result = used_by_locked(member == 0 ? id : variants->ids[member - 1], &users);
        const cJSON *user = NULL;
        cJSON_ArrayForEach(user, users) {
            const char *user_id = common_json_string(user, "id");
            bool seen = false;
            const cJSON *known = NULL;
            cJSON_ArrayForEach(known, blockers) {
                seen = seen || strcmp(common_json_string(known, "id"), user_id) == 0;
            }
            if (strcmp(user_id, id) == 0 || id_set_has(variants, user_id) || seen) {
                continue;
            }
            cJSON *copy = cJSON_Duplicate(user, 1);
            if (!copy) {
                result = workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
                break;
            }
            cJSON_AddItemToArray(blockers, copy);
        }
        cJSON_Delete(users);
    }
    if (result == URSATZ_OK) {
        *out = blockers;
    } else {
        cJSON_Delete(blockers);
    }
    return result;
}

UrsatzStatus workshop_material_delete_blockers(const char *id, cJSON **out_users) {
    *out_users = NULL;
    LOCK();
    if (!store_ready) {
        UNLOCK();
        return workshop_fail(URSATZ_ERR_INTERNAL, "material store failure");
    }
    IdSet variants = {0};
    UrsatzStatus result = variant_closure_locked(id, &variants);
    if (result == URSATZ_OK) {
        result = delete_blockers_locked(id, &variants, out_users);
    }
    UNLOCK();
    id_set_free(&variants);
    return result;
}

UrsatzStatus workshop_material_item_delete(const char *id, bool force) {
    WorkshopTxn *txn = store_txn_begin("item.delete", id ? id : "");
    if (!txn) {
        return workshop_fail(URSATZ_ERR_INTERNAL, "material store failure");
    }
    LOCK();
    if (!store_ready) {
        UNLOCK();
        workshop_txn_abort(txn);
        return workshop_fail(URSATZ_ERR_INTERNAL, "material store failure");
    }
    char dir[PATH_BYTES];
    char meta[PATH_BYTES];
    cJSON *users = NULL;
    IdSet variants = {0};
    IdSet removed = {0};
    UrsatzStatus result = URSATZ_OK;
    if (!store_is_valid_id(id)) {
        result = workshop_fail(URSATZ_ERR_REQUEST, "invalid id");
    } else if (!store_item_dir_path(id, dir, sizeof(dir)) || !store_item_file_path(id, WORKSHOP_FILE_ITEM, meta, sizeof(meta))) {
        result = workshop_fail(URSATZ_ERR_NOT_FOUND, "not found");
    } else if ((result = variant_closure_locked(id, &variants)) != URSATZ_OK) {
        /* nothing is deleted when the variants cannot be found */
    } else if (!force && (result = delete_blockers_locked(id, &variants, &users)) == URSATZ_OK && users->child) {
        result = workshop_fail(URSATZ_ERR_CONFLICT, "the item is used by other items");
    } else if (result == URSATZ_OK) {
        /* The variants go with their motif (R-187), then the motif: each folder is removed in the one transaction. */
        for (size_t member = 0; result == URSATZ_OK && member <= variants.count; member++) {
            const char *member_id = member < variants.count ? variants.ids[member] : id;
            char member_dir[PATH_BYTES];
            if (!store_item_dir_path(member_id, member_dir, sizeof(member_dir)) || workshop_txn_remove(txn, member_dir) != URSATZ_OK) {
                result = workshop_fail(URSATZ_ERR_INTERNAL, "material store failure");
            } else {
                char render[PATH_BYTES];
                if (snprintf(render, sizeof(render), WORKSHOP_DIR_RENDERS "/%s", member_id) < (int)sizeof(render)) {
                    (void)workshop_txn_remove(txn, render);
                }
                if (!id_set_add(&removed, member_id)) {
                    result = workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
                }
            }
        }
    }
    UNLOCK();
    cJSON_Delete(users);
    /* The subjects drop their links to the items in the same transaction; they take their own lock, so this runs with the store's lock released. */
    for (size_t i = 0; result == URSATZ_OK && i < removed.count; i++) {
        store_forget_item(txn, removed.ids[i]);
    }
    result = store_txn_end(txn, result);
    id_set_free(&variants);
    id_set_free(&removed);
    return result;
}
