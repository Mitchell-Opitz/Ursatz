#include "styles.h"
#include "items/formats.h"
#include "items/layout.h"
#include "items/txn.h"
#include "base/last_error.h"
#include "items/lock.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include <cJSON.h>
#include <ursatz_analyzer/analyzer_api.h>
#include <theory/style/style_compare.h>
#include <theory/style/style_features.h>
#include <theory/style/style_labels.h>
#include <theory/style/style_model.h>
#include <theory/style/style_model_file.h>
#include <theory/style/style_profile.h>
#include <common/files.h>
#include <common/json.h>
#include <common/strings.h>
#include "analysis.h"
#include "items/collections.h"
#include "items/ids.h"
#include "items/library_metadata.h"
#include "analysis/style_model_io.h"
#include "analysis/styles_internal.h"

/* Moves the file from over to: the bytes are written atomically (fsynced) and from is removed. */
static bool replace_file(const char *from, const char *to) {
    bool ok = common_file_copy(from, to, NULL) == URSATZ_OK;
    remove(from);
    return ok;
}

/* --- skipped pieces ------------------------------------------------------- */

void workshop_style_skipped_free(WorkshopStyleSkipped *skipped, size_t count) {
    for (size_t i = 0; skipped && i < count; i++) {
        free(skipped[i].id);
        free(skipped[i].reason);
    }
    free(skipped);
}

bool styles_skipped_add(SkippedList *list, const char *id, const char *reason) {
    if (list->count == list->capacity) {
        size_t capacity = list->capacity ? list->capacity * 2 : 8;
        WorkshopStyleSkipped *grown = realloc(list->items, capacity * sizeof(*grown));
        if (!grown) {
            return false;
        }
        list->items = grown;
        list->capacity = capacity;
    }
    list->items[list->count].id = common_strdup(id);
    list->items[list->count].reason = common_strdup(reason);
    if (!list->items[list->count].id || !list->items[list->count].reason) {
        free(list->items[list->count].id);
        free(list->items[list->count].reason);
        return false;
    }
    list->count++;
    return true;
}

/* --- briefs --------------------------------------------------------------- */

void styles_free_strings(char **items, size_t count);

static char *dup_or_null(const char *value) {
    return value && value[0] != '\0' ? common_strdup(value) : NULL;
}

void workshop_style_brief_free(WorkshopStyleBrief *brief) {
    if (!brief) {
        return;
    }
    for (size_t i = 0; brief->scopes && i < brief->scope_count; i++) {
        free((char *)brief->scopes[i].collection_id);
        free((char *)brief->scopes[i].composer);
        free((char *)brief->scopes[i].source);
        free((char *)brief->scopes[i].tag);
        free((char *)brief->scopes[i].subject_id);
        free((char *)brief->scopes[i].project_id);
    }
    free(brief->scopes);
    styles_free_strings(brief->piece_ids, brief->piece_count);
    styles_free_strings(brief->style_ids, brief->style_count);
    memset(brief, 0, sizeof(*brief));
}

static bool strings_copy(char *const *items, size_t count, char ***out) {
    *out = NULL;
    if (count == 0) {
        return true;
    }
    char **copy = calloc(count, sizeof(*copy));
    if (!copy) {
        return false;
    }
    for (size_t i = 0; i < count; i++) {
        copy[i] = common_strdup(items[i]);
        if (!copy[i]) {
            styles_free_strings(copy, i);
            return false;
        }
    }
    *out = copy;
    return true;
}

/* A deep copy of source into out (zeroed first); out is freed with workshop_style_brief_free. */
static bool brief_copy(const WorkshopStyleBrief *source, WorkshopStyleBrief *out) {
    memset(out, 0, sizeof(*out));
    if (source->scope_count > 0) {
        out->scopes = calloc(source->scope_count, sizeof(*out->scopes));
        if (!out->scopes) {
            return false;
        }
        out->scope_count = source->scope_count;
        for (size_t i = 0; i < source->scope_count; i++) {
            const WorkshopLinkScope *from = &source->scopes[i];
            WorkshopLinkScope *to = &out->scopes[i];
            to->kind = from->kind;
            to->collection_id = dup_or_null(from->collection_id);
            to->composer = dup_or_null(from->composer);
            to->source = dup_or_null(from->source);
            to->tag = dup_or_null(from->tag);
            to->subject_id = dup_or_null(from->subject_id);
            to->project_id = dup_or_null(from->project_id);
            if ((from->collection_id && from->collection_id[0] && !to->collection_id) ||
                (from->composer && from->composer[0] && !to->composer) ||
                (from->source && from->source[0] && !to->source) || (from->tag && from->tag[0] && !to->tag) ||
                (from->subject_id && from->subject_id[0] && !to->subject_id) ||
                (from->project_id && from->project_id[0] && !to->project_id)) {
                workshop_style_brief_free(out);
                return false;
            }
        }
    }
    if (!strings_copy(source->piece_ids, source->piece_count, &out->piece_ids) ||
        !strings_copy(source->style_ids, source->style_count, &out->style_ids)) {
        workshop_style_brief_free(out);
        return false;
    }
    out->piece_count = source->piece_count;
    out->style_count = source->style_count;
    return true;
}

static const char *const kScopeKinds[] = {"all", "collection", "filter", "subject", "project"};

static const char *scope_kind_label(WorkshopLinkScopeKind kind) {
    return (int)kind >= 0 && (size_t)kind < sizeof(kScopeKinds) / sizeof(kScopeKinds[0]) ? kScopeKinds[kind] : "all";
}

bool styles_scope_kind_from_label(const char *label, WorkshopLinkScopeKind *kind) {
    for (size_t i = 0; label && i < sizeof(kScopeKinds) / sizeof(kScopeKinds[0]); i++) {
        if (strcmp(label, kScopeKinds[i]) == 0) {
            *kind = (WorkshopLinkScopeKind)i;
            return true;
        }
    }
    return false;
}

/* --- names ---------------------------------------------------------------- */

/* Cuts text to at most max_bytes, never inside a UTF-8 sequence. */
void styles_clip_utf8(char *text, size_t max_bytes) {
    size_t length = strlen(text);
    if (length <= max_bytes) {
        return;
    }
    size_t cut = max_bytes;
    while (cut > 0 && ((unsigned char)text[cut] & 0xC0) == 0x80) {
        cut--;
    }
    text[cut] = '\0';
}

/* --- the source of a style ------------------------------------------------ */

static const char *source_kind_label(WorkshopStyleSourceKind kind) {
    switch (kind) {
        case WORKSHOP_STYLE_SOURCE_FILTER:
            return "filter";
        case WORKSHOP_STYLE_SOURCE_COLLECTION:
            return "collection";
        case WORKSHOP_STYLE_SOURCE_BRIEF:
            return "brief";
        case WORKSHOP_STYLE_SOURCE_PIECES:
        default:
            return "pieces";
    }
}

void styles_normalized_source_free(NormalizedSource *source) {
    free(source->composer);
    free(source->source);
    free(source->tag);
    free(source->collection_id);
    common_names_free(&source->piece_ids);
    workshop_style_brief_free(&source->brief);
    memset(source, 0, sizeof(*source));
}

/* Normalizes the filter values of a request; INVALID_SOURCE when none is given or a tag is invalid. */
UrsatzStatus styles_normalized_source_make(const WorkshopStyleSource *input, NormalizedSource *out) {
    memset(out, 0, sizeof(*out));
    out->kind = input->kind;

    if (input->kind == WORKSHOP_STYLE_SOURCE_FILTER) {
        bool too_long = false;
        bool invalid = false;
        out->composer = workshop_meta_normalize_value(input->composer, &too_long);
        out->source = out->composer ? workshop_meta_normalize_value(input->source, &too_long) : NULL;
        if (!out->composer || !out->source) {
            styles_normalized_source_free(out);
            return too_long ? workshop_fail(URSATZ_ERR_REQUEST, "invalid source") : workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
        }
        char *tag_text = workshop_meta_normalize_value(input->tag, &too_long);
        if (!tag_text) {
            styles_normalized_source_free(out);
            return too_long ? workshop_fail(URSATZ_ERR_REQUEST, "invalid source") : workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
        }
        out->tag = tag_text[0] != '\0' ? workshop_meta_normalize_tag(tag_text, &invalid) : common_strdup("");
        free(tag_text);
        if (!out->tag) {
            styles_normalized_source_free(out);
            return invalid ? workshop_fail(URSATZ_ERR_REQUEST, "invalid source") : workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
        }
        if (out->composer[0] == '\0' && out->source[0] == '\0' && out->tag[0] == '\0') {
            styles_normalized_source_free(out);
            return workshop_fail(URSATZ_ERR_REQUEST, "invalid source");
        }
        return URSATZ_OK;
    }

    if (input->kind == WORKSHOP_STYLE_SOURCE_COLLECTION) {
        if (!workshop_collection_is_valid_id(input->collection_id)) {
            return workshop_fail(URSATZ_ERR_REQUEST, "invalid source");
        }
        out->collection_id = common_strdup(input->collection_id);
        return out->collection_id ? URSATZ_OK : workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
    }

    if (input->kind == WORKSHOP_STYLE_SOURCE_BRIEF) {
        const WorkshopStyleBrief *brief = input->brief;
        if (!brief || brief->scope_count > WORKSHOP_STYLE_BRIEF_MAX_SCOPES ||
            brief->piece_count > WORKSHOP_STYLE_BRIEF_MAX_PIECES ||
            brief->style_count > WORKSHOP_STYLE_BRIEF_MAX_STYLES ||
            brief->scope_count + brief->piece_count + brief->style_count == 0 ||
            (brief->scope_count > 0 && !brief->scopes) || (brief->piece_count > 0 && !brief->piece_ids) ||
            (brief->style_count > 0 && !brief->style_ids)) {
            return workshop_fail(URSATZ_ERR_REQUEST, "invalid source");
        }
        for (size_t i = 0; i < brief->scope_count; i++) {
            if ((int)brief->scopes[i].kind < 0 ||
                (size_t)brief->scopes[i].kind >= sizeof(kScopeKinds) / sizeof(kScopeKinds[0])) {
                return workshop_fail(URSATZ_ERR_REQUEST, "invalid source");
            }
        }
        for (size_t i = 0; i < brief->piece_count; i++) {
            if (!workshop_collection_is_valid_id(brief->piece_ids[i])) {
                return workshop_fail(URSATZ_ERR_REQUEST, "invalid source");
            }
        }
        for (size_t i = 0; i < brief->style_count; i++) {
            if (!workshop_collection_is_valid_id(brief->style_ids[i])) {
                return workshop_fail(URSATZ_ERR_REQUEST, "invalid source");
            }
        }
        return brief_copy(brief, &out->brief) ? URSATZ_OK : workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
    }

    if (input->piece_count == 0 || !input->piece_ids) {
        return workshop_fail(URSATZ_ERR_REQUEST, "invalid source");
    }
    for (size_t i = 0; i < input->piece_count; i++) {
        if (!workshop_collection_is_valid_id(input->piece_ids[i])) {
            styles_normalized_source_free(out);
            return workshop_fail(URSATZ_ERR_REQUEST, "invalid source");
        }
        if (!common_names_contains(&out->piece_ids, input->piece_ids[i]) &&
            !common_names_add(&out->piece_ids, input->piece_ids[i])) {
            styles_normalized_source_free(out);
            return workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
        }
    }
    return URSATZ_OK;
}

/* --- writing a style ------------------------------------------------------ */

static cJSON *string_or_null(const char *text) {
    return text && text[0] != '\0' ? cJSON_CreateString(text) : cJSON_CreateNull();
}

static cJSON *string_array_json(char *const *items, size_t count) {
    cJSON *array = cJSON_CreateArray();
    for (size_t i = 0; array && i < count; i++) {
        cJSON *item = cJSON_CreateString(items[i]);
        if (!item) {
            cJSON_Delete(array);
            return NULL;
        }
        cJSON_AddItemToArray(array, item);
    }
    return array;
}

static cJSON *scope_json(const WorkshopLinkScope *scope) {
    cJSON *object = cJSON_CreateObject();
    cJSON *filter = cJSON_CreateObject();
    bool ok = object && filter && cJSON_AddStringToObject(object, "kind", scope_kind_label(scope->kind)) &&
              cJSON_AddItemToObject(filter, "composer", string_or_null(scope->composer)) &&
              cJSON_AddItemToObject(filter, "source", string_or_null(scope->source)) &&
              cJSON_AddItemToObject(filter, "tag", string_or_null(scope->tag));
    if (ok) {
        cJSON_AddItemToObject(object, "collection_id", string_or_null(scope->collection_id));
        cJSON_AddItemToObject(object, "filter", filter);
        filter = NULL;
        cJSON_AddItemToObject(object, "subject_id", string_or_null(scope->subject_id));
        cJSON_AddItemToObject(object, "project_id", string_or_null(scope->project_id));
    }
    cJSON_Delete(filter);
    if (!ok) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

/* {"scopes": [...], "piece_ids": [...], "style_ids": [...]} */
static cJSON *brief_json(const WorkshopStyleBrief *brief) {
    cJSON *object = cJSON_CreateObject();
    cJSON *scopes = cJSON_CreateArray();
    cJSON *piece_ids = string_array_json(brief->piece_ids, brief->piece_count);
    cJSON *style_ids = string_array_json(brief->style_ids, brief->style_count);
    bool ok = object && scopes && piece_ids && style_ids;
    for (size_t i = 0; ok && i < brief->scope_count; i++) {
        cJSON *scope = scope_json(&brief->scopes[i]);
        ok = scope != NULL;
        if (ok) {
            cJSON_AddItemToArray(scopes, scope);
        }
    }
    if (ok) {
        cJSON_AddItemToObject(object, "scopes", scopes);
        cJSON_AddItemToObject(object, "piece_ids", piece_ids);
        cJSON_AddItemToObject(object, "style_ids", style_ids);
        return object;
    }
    cJSON_Delete(object);
    cJSON_Delete(scopes);
    cJSON_Delete(piece_ids);
    cJSON_Delete(style_ids);
    return NULL;
}

static cJSON *source_json(const NormalizedSource *source) {
    cJSON *object = cJSON_CreateObject();
    if (!object || !cJSON_AddStringToObject(object, "kind", source_kind_label(source->kind))) {
        cJSON_Delete(object);
        return NULL;
    }
    cJSON *filter = NULL;
    if (source->kind == WORKSHOP_STYLE_SOURCE_FILTER) {
        filter = cJSON_CreateObject();
        if (!filter || !cJSON_AddItemToObject(filter, "composer", string_or_null(source->composer)) ||
            !cJSON_AddItemToObject(filter, "source", string_or_null(source->source)) ||
            !cJSON_AddItemToObject(filter, "tag", string_or_null(source->tag))) {
            cJSON_Delete(filter);
            cJSON_Delete(object);
            return NULL;
        }
    } else {
        filter = cJSON_CreateNull();
    }
    cJSON *piece_ids = NULL;
    if (source->kind == WORKSHOP_STYLE_SOURCE_PIECES) {
        piece_ids = cJSON_CreateArray();
        for (size_t i = 0; piece_ids && i < source->piece_ids.count; i++) {
            cJSON *id = cJSON_CreateString(source->piece_ids.items[i]);
            if (!id) {
                cJSON_Delete(piece_ids);
                piece_ids = NULL;
            } else {
                cJSON_AddItemToArray(piece_ids, id);
            }
        }
    } else {
        piece_ids = cJSON_CreateNull();
    }
    cJSON *brief = source->kind == WORKSHOP_STYLE_SOURCE_BRIEF ? brief_json(&source->brief) : cJSON_CreateNull();
    if (!filter || !piece_ids || !brief || !cJSON_AddItemToObject(object, "filter", filter) ||
        !cJSON_AddItemToObject(object, "collection_id", string_or_null(source->collection_id)) ||
        !cJSON_AddItemToObject(object, "piece_ids", piece_ids) || !cJSON_AddItemToObject(object, "brief", brief)) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

/* The meta.json text of a style (caller frees), or NULL. */
static char *meta_text(const char *id, const char *name, const char *created_at, const char *updated_at,
                       const NormalizedSource *source, const Collected *collected) {
    cJSON *meta = cJSON_CreateObject();
    cJSON *source_object = source_json(source);
    cJSON *used = cJSON_CreateArray();
    cJSON *skipped = cJSON_CreateArray();
    bool ok = meta && source_object && used && skipped && cJSON_AddStringToObject(meta, "id", id) &&
              cJSON_AddStringToObject(meta, "name", name) && cJSON_AddStringToObject(meta, "created_at", created_at) &&
              cJSON_AddStringToObject(meta, "updated_at", updated_at);
    if (ok) {
        cJSON_AddItemToObject(meta, "source", source_object);
        source_object = NULL;
    }
    for (size_t i = 0; ok && i < collected->used.count; i++) {
        cJSON *piece = cJSON_CreateString(collected->used.items[i]);
        ok = piece != NULL;
        if (ok) {
            cJSON_AddItemToArray(used, piece);
        }
    }
    for (size_t i = 0; ok && i < collected->skipped.count; i++) {
        cJSON *entry = cJSON_CreateObject();
        ok = entry && cJSON_AddStringToObject(entry, "id", collected->skipped.items[i].id) &&
             cJSON_AddStringToObject(entry, "reason", collected->skipped.items[i].reason);
        if (ok) {
            cJSON_AddItemToArray(skipped, entry);
        } else {
            cJSON_Delete(entry);
        }
    }
    char *text = NULL;
    if (ok) {
        cJSON_AddItemToObject(meta, "pieces", used);
        cJSON_AddItemToObject(meta, "skipped", skipped);
        used = skipped = NULL;
        ok = cJSON_AddNumberToObject(meta, "piece_count", (double)collected->used.count) != NULL;
    }
    if (ok && workshop_format_stamp(meta, WORKSHOP_FORMAT_STYLE)) {
        text = cJSON_Print(meta);
    }
    cJSON_Delete(source_object);
    cJSON_Delete(used);
    cJSON_Delete(skipped);
    cJSON_Delete(meta);
    return text;
}

/* Writes style-model.json, then meta.json, into dir by temp file and rename. */
bool styles_write_style_files(const char *dir, const char *id, const char *name, const char *created_at,
                              const char *updated_at, const NormalizedSource *source, const Collected *collected) {
    char model_path[PATH_BYTES];
    char tmp_path[PATH_BYTES + 8];
    char meta_path[PATH_BYTES];
    if (snprintf(model_path, sizeof(model_path), "%s/style-model.json", dir) >= (int)sizeof(model_path) ||
        snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", model_path) >= (int)sizeof(tmp_path) ||
        snprintf(meta_path, sizeof(meta_path), "%s/" WORKSHOP_FILE_STYLE, dir) >= (int)sizeof(meta_path)) {
        return false;
    }
    char *text = meta_text(id, name, created_at, updated_at, source, collected);
    if (!text) {
        return false;
    }
    bool ok = workshop_style_model_write(collected->profile, name, analyzer_version(), tmp_path) == STYLE_MODEL_FILE_OK &&
              replace_file(tmp_path, model_path) && (common_file_write_atomic(meta_path, text, strlen(text), NULL) == URSATZ_OK);
    free(text);
    if (!ok) {
        remove(tmp_path);
    }
    return ok;
}

/* --- build ---------------------------------------------------------------- */

/* The style's name: the given one normalized, or the default when it is empty. */
static UrsatzStatus style_name(const char *requested, const char *default_name, char *out, size_t out_size) {
    bool too_long = false;
    char *normalized = workshop_meta_normalize_value(requested, &too_long);
    if (!normalized) {
        return too_long ? workshop_fail(URSATZ_ERR_REQUEST, "invalid name") : workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
    }
    snprintf(out, out_size, "%s", normalized[0] != '\0' ? normalized : default_name);
    free(normalized);
    return URSATZ_OK;
}

/* Normalizes the source, adds its pieces and settles the name, holding no lock; on OK it frees *normalized with
 * styles_normalized_source_free and *collected with styles_collected_free. */
static UrsatzStatus prepare_build(const char *name, const WorkshopStyleSource *source, const UrsatzJob *job,
                                          NormalizedSource *normalized, Collected *collected, char *final_name,
                                          size_t final_name_size) {
    memset(collected, 0, sizeof(*collected));
    UrsatzStatus result = styles_normalized_source_make(source, normalized);
    if (result != URSATZ_OK) {
        return result;
    }
    result = styles_collect_pieces(normalized, job, collected);
    if (result == URSATZ_OK) {
        result = style_name(name, collected->default_name, final_name, final_name_size);
        if (result != URSATZ_OK) {
            styles_collected_free(collected);
        }
    }
    if (result != URSATZ_OK) {
        styles_normalized_source_free(normalized);
    }
    return result;
}

/* Hands the skipped pieces of a finished build to the caller. */
static void give_skipped(Collected *collected, WorkshopStyleSkipped **out_skipped, size_t *out_skipped_count) {
    if (out_skipped) {
        *out_skipped = collected->skipped.items;
        collected->skipped.items = NULL;
        if (out_skipped_count) {
            *out_skipped_count = collected->skipped.count;
        }
    }
}

static void reset_outputs(WorkshopStyleSkipped **out_skipped, size_t *out_skipped_count) {
    if (out_skipped) {
        *out_skipped = NULL;
    }
    if (out_skipped_count) {
        *out_skipped_count = 0;
    }
}

UrsatzStatus workshop_style_build(const char *name, const WorkshopStyleSource *source, const UrsatzJob *job, char **out_id,
                                            WorkshopStyleSkipped **out_skipped, size_t *out_skipped_count) {
    if (out_id) {
        *out_id = NULL;
    }
    reset_outputs(out_skipped, out_skipped_count);
    if (!source || !out_id) {
        return workshop_fail(URSATZ_ERR_REQUEST, "invalid source");
    }

    NormalizedSource normalized;
    Collected collected;
    char final_name[NAME_MAX_BYTES + 1];
    UrsatzStatus result = prepare_build(name, source, job, &normalized, &collected, final_name, sizeof(final_name));
    if (result != URSATZ_OK) {
        return result;
    }
    if (ursatz_job_cancelled(job)) {
        styles_collected_free(&collected);
        styles_normalized_source_free(&normalized);
        return URSATZ_ERR_CANCELLED;
    }

    char *id = NULL;
    char style_dir[PATH_BYTES];
    bool id_found = false;
    result = workshop_fail(URSATZ_ERR_INTERNAL, "style operation failed");
    if (common_dir_make(styles_kStylesDir, NULL) == URSATZ_OK) {
        for (int attempt = 0; attempt < ID_MAX_TRIES && !id_found; attempt++) {
            id = workshop_new_id("sty");
            if (!id) {
                result = workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
                break;
            }
            id_found = workshop_layout_style_dir(id, style_dir, sizeof(style_dir)) && !common_path_exists(style_dir);
            if (!id_found) {
                free(id);
                id = NULL;
            }
        }
    }
    if (id_found) {
        /* The style is a folder written whole (the model and its meta) and put in place by one transaction: the transaction is the
         * outermost lock, then the styles lock (the pieces were read above, holding neither). */
        WorkshopTxn *txn = workshop_txn_begin(NULL, "style.build", id, 0);
        LOCK();
        char now[32];
        const char *pending = NULL;
        common_utc_now(now);
        if (txn && workshop_txn_stage_folder(txn, style_dir, &pending) == URSATZ_OK &&
            styles_write_style_files(pending, id, final_name, now, now, &normalized, &collected)) {
            UrsatzError error;
            memset(&error, 0, sizeof(error));
            result = workshop_txn_commit(txn, &error) == URSATZ_OK ? URSATZ_OK : workshop_fail(URSATZ_ERR_INTERNAL, "style operation failed");
            txn = NULL;
        }
        UNLOCK();
        if (txn) {
            workshop_txn_abort(txn);
        }
    }
    if (result == URSATZ_OK) {
        *out_id = id;
        id = NULL;
        give_skipped(&collected, out_skipped, out_skipped_count);
    }
    free(id);
    styles_collected_free(&collected);
    styles_normalized_source_free(&normalized);
    return result;
}

UrsatzStatus workshop_style_build_file(const WorkshopStyleSource *source, const char *name, const char *path,
                                                 char ***out_pieces, size_t *out_piece_count,
                                                 WorkshopStyleSkipped **out_skipped, size_t *out_skipped_count) {
    if (out_pieces) {
        *out_pieces = NULL;
    }
    if (out_piece_count) {
        *out_piece_count = 0;
    }
    reset_outputs(out_skipped, out_skipped_count);
    char tmp_path[PATH_BYTES + 8];
    if (!source || !path || path[0] == '\0' || strlen(path) >= PATH_BYTES ||
        snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path) >= (int)sizeof(tmp_path)) {
        return workshop_fail(URSATZ_ERR_REQUEST, "invalid source");
    }

    NormalizedSource normalized;
    Collected collected;
    char final_name[NAME_MAX_BYTES + 1];
    UrsatzStatus result = prepare_build(name, source, NULL, &normalized, &collected, final_name, sizeof(final_name));
    if (result != URSATZ_OK) {
        return result;
    }

    if (workshop_style_model_write(collected.profile, final_name, analyzer_version(), tmp_path) != STYLE_MODEL_FILE_OK ||
        !replace_file(tmp_path, path)) {
        remove(tmp_path);
        result = workshop_fail(URSATZ_ERR_INTERNAL, "style operation failed");
    } else {
        if (out_pieces) {
            *out_pieces = collected.used.items;
            collected.used.items = NULL;
            if (out_piece_count) {
                *out_piece_count = collected.used.count;
            }
            collected.used.count = 0;
        }
        give_skipped(&collected, out_skipped, out_skipped_count);
    }
    styles_collected_free(&collected);
    styles_normalized_source_free(&normalized);
    return result;
}
