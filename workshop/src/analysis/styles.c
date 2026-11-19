#include "styles.h"
#include "items/formats.h"
#include "items/layout.h"
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

static const char *kPiecesDir = WORKSHOP_DIR_PIECES;
static const char *kStylesDir = WORKSHOP_DIR_STYLES;

#define PATH_BYTES 400
#define NAME_MAX_BYTES WORKSHOP_META_MAX_VALUE_BYTES
#define ID_MAX_TRIES 8

static WorkshopLock g_lock = WORKSHOP_LOCK_INIT;
#define LOCK() workshop_lock_acquire(&g_lock)
#define UNLOCK() workshop_lock_release(&g_lock)

/* Moves the file from over to: the bytes are written atomically (fsynced) and from is removed. */
static bool replace_file(const char *from, const char *to) {
    char *data = NULL;
    size_t size = 0;
    bool ok = common_file_read(from, &data, &size, NULL) == URSATZ_OK &&
              common_file_write_atomic(to, data, size, NULL) == URSATZ_OK;
    free(data);
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

typedef struct {
    WorkshopStyleSkipped *items;
    size_t count;
    size_t capacity;
} SkippedList;

static bool skipped_add(SkippedList *list, const char *id, const char *reason) {
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

static void free_strings(char **items, size_t count);

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
    free_strings(brief->piece_ids, brief->piece_count);
    free_strings(brief->style_ids, brief->style_count);
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
            free_strings(copy, i);
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

static bool scope_kind_from_label(const char *label, WorkshopLinkScopeKind *kind) {
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
static void clip_utf8(char *text, size_t max_bytes) {
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

/* A source with its filter values normalized, ready to resolve and to store. */
typedef struct {
    WorkshopStyleSourceKind kind;
    char *composer; /* "" when unset */
    char *source;
    char *tag;
    char *collection_id;
    CommonNames piece_ids;
    WorkshopStyleBrief brief;
} NormalizedSource;

static void normalized_source_free(NormalizedSource *source) {
    free(source->composer);
    free(source->source);
    free(source->tag);
    free(source->collection_id);
    common_names_free(&source->piece_ids);
    workshop_style_brief_free(&source->brief);
    memset(source, 0, sizeof(*source));
}

/* Normalizes the filter values of a request; INVALID_SOURCE when none is given or a tag is invalid. */
static UrsatzStatus normalized_source_make(const WorkshopStyleSource *input, NormalizedSource *out) {
    memset(out, 0, sizeof(*out));
    out->kind = input->kind;

    if (input->kind == WORKSHOP_STYLE_SOURCE_FILTER) {
        bool too_long = false;
        bool invalid = false;
        out->composer = workshop_meta_normalize_value(input->composer, &too_long);
        out->source = out->composer ? workshop_meta_normalize_value(input->source, &too_long) : NULL;
        if (!out->composer || !out->source) {
            normalized_source_free(out);
            return too_long ? workshop_fail(URSATZ_ERR_REQUEST, "invalid source") : workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
        }
        char *tag_text = workshop_meta_normalize_value(input->tag, &too_long);
        if (!tag_text) {
            normalized_source_free(out);
            return too_long ? workshop_fail(URSATZ_ERR_REQUEST, "invalid source") : workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
        }
        out->tag = tag_text[0] != '\0' ? workshop_meta_normalize_tag(tag_text, &invalid) : common_strdup("");
        free(tag_text);
        if (!out->tag) {
            normalized_source_free(out);
            return invalid ? workshop_fail(URSATZ_ERR_REQUEST, "invalid source") : workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
        }
        if (out->composer[0] == '\0' && out->source[0] == '\0' && out->tag[0] == '\0') {
            normalized_source_free(out);
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
            normalized_source_free(out);
            return workshop_fail(URSATZ_ERR_REQUEST, "invalid source");
        }
        if (!common_names_contains(&out->piece_ids, input->piece_ids[i]) &&
            !common_names_add(&out->piece_ids, input->piece_ids[i])) {
            normalized_source_free(out);
            return workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
        }
    }
    return URSATZ_OK;
}

static void free_summaries(WorkshopPieceSummary *pieces, size_t count) {
    for (size_t i = 0; i < count; i++) {
        free(pieces[i].id);
        free(pieces[i].filename);
        free(pieces[i].format);
        free(pieces[i].uploaded_at);
        free(pieces[i].composer);
        free(pieces[i].source);
        workshop_meta_tags_free(pieces[i].tags, pieces[i].tag_count);
    }
    free(pieces);
}

static UrsatzStatus load_meta(const char *id, cJSON **meta);

/* The pieces of a brief: scopes, ids and styles' last builds, in id order without repeats. */
static UrsatzStatus resolve_brief(const WorkshopStyleBrief *brief, CommonNames *ids) {
    CommonNames all = {0};
    UrsatzStatus result = URSATZ_OK;

    for (size_t i = 0; result == URSATZ_OK && i < brief->scope_count; i++) {
        char **scope_ids = NULL;
        size_t scope_count = 0;
        UrsatzStatus resolved = workshop_link_scope_resolve(&brief->scopes[i], &scope_ids, &scope_count);
        if (resolved == URSATZ_ERR_UNSATISFIABLE) {
            continue;
        }
        if (resolved != URSATZ_OK) {
            result = resolved == URSATZ_ERR_REQUEST ? workshop_fail(URSATZ_ERR_REQUEST, "invalid source")
                     : resolved == URSATZ_ERR_NOT_FOUND   ? workshop_fail(URSATZ_ERR_NOT_FOUND, "style not found")
                     : resolved == URSATZ_ERR_ALLOC       ? workshop_fail(URSATZ_ERR_ALLOC, "out of memory")
                                                                   : workshop_fail(URSATZ_ERR_INTERNAL, "style operation failed");
            continue;
        }
        for (size_t j = 0; result == URSATZ_OK && j < scope_count; j++) {
            result = common_names_add(&all, scope_ids[j]) ? URSATZ_OK : workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
        }
        workshop_link_ids_free(scope_ids, scope_count);
    }
    for (size_t i = 0; result == URSATZ_OK && i < brief->piece_count; i++) {
        result = common_names_add(&all, brief->piece_ids[i]) ? URSATZ_OK : workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
    }
    for (size_t i = 0; result == URSATZ_OK && i < brief->style_count; i++) {
        cJSON *meta = NULL;
        result = load_meta(brief->style_ids[i], &meta);
        const cJSON *piece = NULL;
        cJSON_ArrayForEach(piece, cJSON_GetObjectItemCaseSensitive(meta, "pieces")) {
            if (result == URSATZ_OK && cJSON_IsString(piece) && !common_names_add(&all, piece->valuestring)) {
                result = workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
            }
        }
        cJSON_Delete(meta);
    }

    if (result == URSATZ_OK && all.count > 1) {
        common_names_sort(&all);
    }
    for (size_t i = 0; result == URSATZ_OK && i < all.count; i++) {
        if (i > 0 && strcmp(all.items[i], all.items[i - 1]) == 0) {
            continue;
        }
        result = common_names_add(ids, all.items[i]) ? URSATZ_OK : workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
    }
    common_names_free(&all);
    return result;
}

/*
 * The piece ids a source names, in the order the Library lists them (filter),
 * the collection's add order, or as given. ids that name no piece are not
 * dropped here: collect_pieces skips them with the reason "not found".
 * default_name receives the style's default name.
 */
static UrsatzStatus resolve_piece_ids(const NormalizedSource *source, CommonNames *ids, char *default_name,
                                              size_t default_name_size) {
    if (source->kind == WORKSHOP_STYLE_SOURCE_FILTER) {
        WorkshopMetaFilter filter = {0};
        filter.composer = source->composer[0] ? source->composer : NULL;
        filter.source = source->source[0] ? source->source : NULL;
        filter.tag = source->tag[0] ? source->tag : NULL;

        WorkshopPieceSummary *pieces = NULL;
        size_t count = 0;
        if (workshop_library_list(kPiecesDir, &pieces, &count) != URSATZ_OK) {
            return workshop_fail(URSATZ_ERR_INTERNAL, "style operation failed");
        }
        for (size_t i = 0; i < count; i++) {
            if (workshop_meta_matches(&pieces[i], &filter) && !common_names_add(ids, pieces[i].id)) {
                free_summaries(pieces, count);
                return workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
            }
        }
        free_summaries(pieces, count);

        char name[NAME_MAX_BYTES * 3 + 32] = "";
        size_t used = 0;
        const char *labels[3] = {"Composer: ", "Source: ", "Tag: "};
        const char *values[3] = {source->composer, source->source, source->tag};
        for (int i = 0; i < 3; i++) {
            if (values[i][0] == '\0') {
                continue;
            }
            int n = snprintf(name + used, sizeof(name) - used, "%s%s%s", used > 0 ? " \xC2\xB7 " : "", labels[i],
                             values[i]);
            if (n < 0 || (size_t)n >= sizeof(name) - used) {
                return workshop_fail(URSATZ_ERR_INTERNAL, "style operation failed");
            }
            used += (size_t)n;
        }
        clip_utf8(name, NAME_MAX_BYTES);
        snprintf(default_name, default_name_size, "%s", name);
        return URSATZ_OK;
    }

    if (source->kind == WORKSHOP_STYLE_SOURCE_COLLECTION) {
        WorkshopCollection *collection = NULL;
        UrsatzStatus result = workshop_collection_get(source->collection_id, &collection);
        if (result == URSATZ_ERR_NOT_FOUND) {
            return workshop_fail(URSATZ_ERR_NOT_FOUND, "style not found");
        }
        if (result != URSATZ_OK) {
            return result == URSATZ_ERR_ALLOC ? workshop_fail(URSATZ_ERR_ALLOC, "out of memory") : workshop_fail(URSATZ_ERR_INTERNAL, "style operation failed");
        }
        bool ok = true;
        for (size_t i = 0; ok && i < collection->piece_count; i++) {
            ok = common_names_add(ids, collection->pieces[i]);
        }
        char name[NAME_MAX_BYTES * 2 + 32];
        snprintf(name, sizeof(name), "Collection: %s", collection->name);
        clip_utf8(name, NAME_MAX_BYTES);
        snprintf(default_name, default_name_size, "%s", name);
        workshop_collection_destroy(collection);
        return ok ? URSATZ_OK : workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
    }

    if (source->kind == WORKSHOP_STYLE_SOURCE_BRIEF) {
        snprintf(default_name, default_name_size, "Brief");
        return resolve_brief(&source->brief, ids);
    }

    for (size_t i = 0; i < source->piece_ids.count; i++) {
        if (!common_names_add(ids, source->piece_ids.items[i])) {
            return workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
        }
    }
    snprintf(default_name, default_name_size, "Selected pieces");
    return URSATZ_OK;
}

/* --- reading pieces into a profile ---------------------------------------- */

/* The pieces a build used and left out, with the profile they made. */
typedef struct {
    StyleProfile *profile;
    CommonNames used;
    SkippedList skipped;
    char default_name[NAME_MAX_BYTES + 1];
} Collected;

static void collected_free(Collected *collected) {
    style_profile_destroy(collected->profile);
    common_names_free(&collected->used);
    workshop_style_skipped_free(collected->skipped.items, collected->skipped.count);
    memset(collected, 0, sizeof(*collected));
}

/*
 * Adds one piece's analysis document to the profile. false with *reason set
 * when the piece is skipped; *alloc_failed set when memory ran out.
 */
static bool add_piece(StyleProfile *profile, const char *id, const char **reason) {
    if (!workshop_collection_is_valid_id(id)) {
        *reason = "not found";
        return false;
    }

    /* Another schema, Analyzer version or Ursatz commit: analyze the piece again first. */
    UrsatzStatus ensured = workshop_analysis_ensure_current(kPiecesDir, id);
    if (ensured != URSATZ_OK) {
        *reason = ensured == URSATZ_ERR_NOT_FOUND ? "not found" : "re-analysis failed";
        return false;
    }

    char *document = NULL;
    if (workshop_analysis_get_document(kPiecesDir, id, NULL, 0, &document) != URSATZ_OK) {
        *reason = "unreadable analysis";
        return false;
    }
    StyleProfileResult result = analyzer_style_add_document_json(profile, document);
    free(document);
    if (result != STYLE_PROFILE_OK) {
        *reason = "unreadable analysis";
        return false;
    }
    return true;
}

/* Resolves the source and adds every piece it names. The caller holds the lock. */
static UrsatzStatus collect_pieces(const NormalizedSource *source, Collected *collected) {
    memset(collected, 0, sizeof(*collected));

    CommonNames ids = {0};
    char default_name[NAME_MAX_BYTES + 1] = "";
    UrsatzStatus resolved = resolve_piece_ids(source, &ids, default_name, sizeof(default_name));
    if (resolved != URSATZ_OK) {
        common_names_free(&ids);
        return resolved;
    }
    snprintf(collected->default_name, sizeof(collected->default_name), "%s", default_name);

    collected->profile = style_profile_create();
    if (!collected->profile) {
        common_names_free(&ids);
        return workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
    }

    UrsatzStatus result = URSATZ_OK;
    for (size_t i = 0; result == URSATZ_OK && i < ids.count; i++) {
        const char *reason = NULL;
        if (add_piece(collected->profile, ids.items[i], &reason)) {
            if (!common_names_add(&collected->used, ids.items[i])) {
                result = workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
            }
        } else if (!skipped_add(&collected->skipped, ids.items[i], reason)) {
            result = workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
        }
    }
    common_names_free(&ids);

    if (result == URSATZ_OK && collected->used.count == 0) {
        result = workshop_fail(URSATZ_ERR_UNSATISFIABLE, "no analyzable pieces");
    }
    if (result != URSATZ_OK) {
        collected_free(collected);
    }
    return result;
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
static bool write_style_files(const char *dir, const char *id, const char *name, const char *created_at,
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
    bool ok = style_model_file_write(collected->profile, name, analyzer_version(), tmp_path) == STYLE_MODEL_FILE_OK &&
              replace_file(tmp_path, model_path) && (common_file_write_atomic(meta_path, text, strlen(text), NULL) == URSATZ_OK);
    free(text);
    if (!ok) {
        remove(tmp_path);
    }
    return ok;
}

/* Removes a style directory this module made: its files and temp files. */
static void remove_style_dir(const char *dir) {
    common_dir_remove(dir, NULL);
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

/* Normalizes the source, adds its pieces and settles the name. The caller holds the lock; on OK it frees *normalized
 * with normalized_source_free and *collected with collected_free. */
static UrsatzStatus prepare_build(const char *name, const WorkshopStyleSource *source,
                                          NormalizedSource *normalized, Collected *collected, char *final_name,
                                          size_t final_name_size) {
    memset(collected, 0, sizeof(*collected));
    UrsatzStatus result = normalized_source_make(source, normalized);
    if (result != URSATZ_OK) {
        return result;
    }
    result = collect_pieces(normalized, collected);
    if (result == URSATZ_OK) {
        result = style_name(name, collected->default_name, final_name, final_name_size);
        if (result != URSATZ_OK) {
            collected_free(collected);
        }
    }
    if (result != URSATZ_OK) {
        normalized_source_free(normalized);
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

UrsatzStatus workshop_style_build(const char *name, const WorkshopStyleSource *source, char **out_id,
                                            WorkshopStyleSkipped **out_skipped, size_t *out_skipped_count) {
    if (out_id) {
        *out_id = NULL;
    }
    reset_outputs(out_skipped, out_skipped_count);
    if (!source || !out_id) {
        return workshop_fail(URSATZ_ERR_REQUEST, "invalid source");
    }

    LOCK();
    NormalizedSource normalized;
    Collected collected;
    char final_name[NAME_MAX_BYTES + 1];
    UrsatzStatus result = prepare_build(name, source, &normalized, &collected, final_name, sizeof(final_name));
    if (result != URSATZ_OK) {
        UNLOCK();
        return result;
    }

    char *id = NULL;
    char style_dir[PATH_BYTES];
    bool dir_made = false;
    result = workshop_fail(URSATZ_ERR_INTERNAL, "style operation failed");
    if ((common_dir_make(kStylesDir, NULL) == URSATZ_OK)) {
        for (int attempt = 0; attempt < ID_MAX_TRIES && !dir_made; attempt++) {
            id = workshop_new_id("sty");
            if (!id) {
                result = workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
                break;
            }
            if (workshop_layout_style_dir(id, style_dir, sizeof(style_dir))) {
                dir_made = (common_dir_create(style_dir, NULL) == URSATZ_OK);
            }
            if (!dir_made) {
                free(id);
                id = NULL;
            }
        }
    }
    if (dir_made) {
        char now[32];
        common_utc_now(now);
        if (write_style_files(style_dir, id, final_name, now, now, &normalized, &collected)) {
            result = URSATZ_OK;
        } else {
            remove_style_dir(style_dir);
        }
    }
    if (result == URSATZ_OK) {
        *out_id = id;
        id = NULL;
        give_skipped(&collected, out_skipped, out_skipped_count);
    }
    free(id);
    collected_free(&collected);
    UNLOCK();
    normalized_source_free(&normalized);
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

    LOCK();
    NormalizedSource normalized;
    Collected collected;
    char final_name[NAME_MAX_BYTES + 1];
    UrsatzStatus result = prepare_build(name, source, &normalized, &collected, final_name, sizeof(final_name));
    if (result != URSATZ_OK) {
        UNLOCK();
        return result;
    }

    if (style_model_file_write(collected.profile, final_name, analyzer_version(), tmp_path) != STYLE_MODEL_FILE_OK ||
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
    collected_free(&collected);
    UNLOCK();
    normalized_source_free(&normalized);
    return result;
}

/* --- stored styles -------------------------------------------------------- */

static void free_strings(char **items, size_t count) {
    for (size_t i = 0; items && i < count; i++) {
        free(items[i]);
    }
    free(items);
}

static bool style_dir_path(const char *id, char *out, size_t out_size) {
    return workshop_collection_is_valid_id(id) && workshop_layout_style_dir(id, out, out_size);
}

/* A copy of a string member, "" when it is absent or not a string. */

/* Copies the string members of an array; *out is NULL when there are none. */
static bool string_array_copy(const cJSON *array, char ***out, size_t *out_count) {
    *out = NULL;
    *out_count = 0;
    int length = cJSON_IsArray(array) ? cJSON_GetArraySize(array) : 0;
    if (length <= 0) {
        return true;
    }
    char **items = calloc((size_t)length, sizeof(char *));
    if (!items) {
        return false;
    }
    size_t count = 0;
    const cJSON *item = NULL;
    cJSON_ArrayForEach(item, array) {
        if (!cJSON_IsString(item) || !item->valuestring) {
            continue;
        }
        items[count] = common_strdup(item->valuestring);
        if (!items[count]) {
            free_strings(items, count);
            return false;
        }
        count++;
    }
    if (count == 0) {
        free(items);
        return true;
    }
    *out = items;
    *out_count = count;
    return true;
}

static WorkshopStyleSourceKind source_kind_from_label(const char *label) {
    if (label && strcmp(label, "collection") == 0) {
        return WORKSHOP_STYLE_SOURCE_COLLECTION;
    }
    if (label && strcmp(label, "pieces") == 0) {
        return WORKSHOP_STYLE_SOURCE_PIECES;
    }
    if (label && strcmp(label, "brief") == 0) {
        return WORKSHOP_STYLE_SOURCE_BRIEF;
    }
    return WORKSHOP_STYLE_SOURCE_FILTER;
}

/* Reads the stored brief into out (owned strings). false on allocation failure. */
static bool load_brief(const cJSON *json, WorkshopStyleBrief *out) {
    memset(out, 0, sizeof(*out));
    const cJSON *scopes = cJSON_GetObjectItemCaseSensitive(json, "scopes");
    int length = cJSON_IsArray(scopes) ? cJSON_GetArraySize(scopes) : 0;
    if (length > 0) {
        out->scopes = calloc((size_t)length, sizeof(*out->scopes));
        if (!out->scopes) {
            return false;
        }
        const cJSON *entry = NULL;
        cJSON_ArrayForEach(entry, scopes) {
            const cJSON *kind = cJSON_GetObjectItemCaseSensitive(entry, "kind");
            const cJSON *filter = cJSON_GetObjectItemCaseSensitive(entry, "filter");
            WorkshopLinkScope *scope = &out->scopes[out->scope_count];
            if (!cJSON_IsString(kind) || !scope_kind_from_label(kind->valuestring, &scope->kind)) {
                continue;
            }
            char *fields[6];
            fields[0] = common_json_string_copy(entry, "collection_id");
            fields[1] = common_json_string_copy(filter, "composer");
            fields[2] = common_json_string_copy(filter, "source");
            fields[3] = common_json_string_copy(filter, "tag");
            fields[4] = common_json_string_copy(entry, "subject_id");
            fields[5] = common_json_string_copy(entry, "project_id");
            bool filled = true;
            for (int i = 0; i < 6; i++) {
                filled = filled && fields[i] != NULL;
            }
            if (!filled) {
                for (int i = 0; i < 6; i++) {
                    free(fields[i]);
                }
                workshop_style_brief_free(out);
                return false;
            }
            scope->collection_id = fields[0];
            scope->composer = fields[1];
            scope->source = fields[2];
            scope->tag = fields[3];
            scope->subject_id = fields[4];
            scope->project_id = fields[5];
            out->scope_count++;
        }
    }
    if (!string_array_copy(cJSON_GetObjectItemCaseSensitive(json, "piece_ids"), &out->piece_ids, &out->piece_count) ||
        !string_array_copy(cJSON_GetObjectItemCaseSensitive(json, "style_ids"), &out->style_ids, &out->style_count)) {
        workshop_style_brief_free(out);
        return false;
    }
    return true;
}

static bool json_string_or_null(const cJSON *object, const char *key) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, key);
    return !item || cJSON_IsNull(item) || cJSON_IsString(item);
}

/* A scope as the links routes take it: a known kind and string or null members. */
static bool scope_json_valid(const cJSON *json) {
    WorkshopLinkScopeKind kind;
    const cJSON *label = cJSON_GetObjectItemCaseSensitive(json, "kind");
    const cJSON *filter = cJSON_GetObjectItemCaseSensitive(json, "filter");
    return cJSON_IsObject(json) && cJSON_IsString(label) && scope_kind_from_label(label->valuestring, &kind) &&
           json_string_or_null(json, "collection_id") && json_string_or_null(json, "subject_id") &&
           json_string_or_null(json, "project_id") &&
           (!filter || cJSON_IsNull(filter) ||
            (cJSON_IsObject(filter) && json_string_or_null(filter, "composer") && json_string_or_null(filter, "source") &&
             json_string_or_null(filter, "tag")));
}

/* An optional array of at most max valid ids. NULL when it holds, else the failure message. */
static const char *brief_ids_failure(const cJSON *json, const char *key, size_t max) {
    const cJSON *array = cJSON_GetObjectItemCaseSensitive(json, key);
    if (!array) {
        return NULL;
    }
    if (!cJSON_IsArray(array)) {
        return "invalid brief";
    }
    if ((size_t)cJSON_GetArraySize(array) > max) {
        return "too many entries in the brief";
    }
    const cJSON *item = NULL;
    cJSON_ArrayForEach(item, array) {
        if (!cJSON_IsString(item) || !workshop_collection_is_valid_id(item->valuestring)) {
            return "invalid brief";
        }
    }
    return NULL;
}

const char *workshop_style_brief_parse(const cJSON *json, WorkshopStyleBrief *out) {
    memset(out, 0, sizeof(*out));
    if (!cJSON_IsObject(json)) {
        return "invalid brief";
    }
    const cJSON *scopes = cJSON_GetObjectItemCaseSensitive(json, "scopes");
    if (scopes) {
        if (!cJSON_IsArray(scopes)) {
            return "invalid brief";
        }
        if ((size_t)cJSON_GetArraySize(scopes) > WORKSHOP_STYLE_BRIEF_MAX_SCOPES) {
            return "too many entries in the brief";
        }
        const cJSON *entry = NULL;
        cJSON_ArrayForEach(entry, scopes) {
            if (!scope_json_valid(entry)) {
                return "invalid scope";
            }
        }
    }
    const char *failure = brief_ids_failure(json, "piece_ids", WORKSHOP_STYLE_BRIEF_MAX_PIECES);
    if (!failure) {
        failure = brief_ids_failure(json, "style_ids", WORKSHOP_STYLE_BRIEF_MAX_STYLES);
    }
    if (failure) {
        return failure;
    }
    if (!load_brief(json, out)) {
        return "out of memory";
    }
    if (out->scope_count + out->piece_count + out->style_count == 0) {
        workshop_style_brief_free(out);
        return "invalid brief";
    }
    return NULL;
}

/* Reads styles/<id>/meta.json: the stored tree, whose "id" is set from the directory name (a copy of it inside the file is not
   trusted). NOT_FOUND when the style is absent. */
static UrsatzStatus load_meta(const char *id, cJSON **out) {
    char dir[PATH_BYTES];
    char path[PATH_BYTES + 16];
    *out = NULL;
    if (!style_dir_path(id, dir, sizeof(dir))) {
        return workshop_fail(URSATZ_ERR_NOT_FOUND, "style not found");
    }
    snprintf(path, sizeof(path), "%s/" WORKSHOP_FILE_STYLE, dir);
    cJSON *meta = NULL;
    workshop_format_read(path, WORKSHOP_FORMAT_STYLE, true, &meta, NULL);
    if (!meta) {
        return workshop_fail(URSATZ_ERR_NOT_FOUND, "style not found");
    }
    cJSON_DeleteItemFromObjectCaseSensitive(meta, "id");
    if (!cJSON_AddStringToObject(meta, "id", id)) {
        cJSON_Delete(meta);
        return workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
    }
    *out = meta;
    return URSATZ_OK;
}

/* The list entry of a stored style: {"id","name","piece_count","source","updated_at"}; takes members of meta. */
static cJSON *entry_of(cJSON *meta) {
    cJSON *entry = cJSON_CreateObject();
    static const char *const kMembers[] = {"id", "name", "piece_count", "source", "updated_at"};
    for (size_t i = 0; entry && i < sizeof(kMembers) / sizeof(kMembers[0]); i++) {
        cJSON *member = cJSON_DetachItemFromObjectCaseSensitive(meta, kMembers[i]);
        if (!member && strcmp(kMembers[i], "piece_count") == 0) {
            member = cJSON_CreateNumber((double)cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(meta, "pieces")));
        }
        if (!member) {
            member = cJSON_CreateString("");
        }
        if (!member || !cJSON_AddItemToObject(entry, kMembers[i], member)) {
            cJSON_Delete(member);
            cJSON_Delete(entry);
            return NULL;
        }
    }
    return entry;
}

/* --- list ----------------------------------------------------------------- */

/* The names in styles/; empty when the directory is absent. false only when memory ran out. */
static bool list_style_dirs(CommonNames *names) {
    return common_dir_list(kStylesDir, names, NULL) != URSATZ_ERR_ALLOC;
}

static const char *entry_text(const cJSON *entry, const char *name) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(entry, name);
    return cJSON_IsString(item) ? item->valuestring : "";
}

/* Newest update first, then id. */
static int compare_entries(const void *left, const void *right) {
    const cJSON *a = *(const cJSON *const *)left;
    const cJSON *b = *(const cJSON *const *)right;
    int by_time = strcmp(entry_text(b, "updated_at"), entry_text(a, "updated_at"));
    return by_time != 0 ? by_time : strcmp(entry_text(a, "id"), entry_text(b, "id"));
}

UrsatzStatus workshop_style_list(cJSON **out_styles) {
    *out_styles = NULL;

    LOCK();
    CommonNames names = {0};
    if (!list_style_dirs(&names)) {
        UNLOCK();
        return workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
    }
    cJSON **entries = names.count > 0 ? calloc(names.count, sizeof(*entries)) : NULL;
    UrsatzStatus result = names.count > 0 && !entries ? workshop_fail(URSATZ_ERR_ALLOC, "out of memory") : URSATZ_OK;
    size_t count = 0;
    for (size_t i = 0; result == URSATZ_OK && i < names.count; i++) {
        cJSON *meta = NULL;
        UrsatzStatus loaded = load_meta(names.items[i], &meta);
        if (loaded == URSATZ_OK) {
            entries[count] = entry_of(meta);
            cJSON_Delete(meta);
            if (entries[count]) {
                count++;
            } else {
                result = workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
            }
        } else if (loaded != URSATZ_ERR_NOT_FOUND) {
            result = loaded;
        }
    }
    common_names_free(&names);
    UNLOCK();

    cJSON *array = result == URSATZ_OK ? cJSON_CreateArray() : NULL;
    if (array && count > 1) {
        qsort(entries, count, sizeof(*entries), compare_entries);
    }
    for (size_t i = 0; i < count; i++) {
        if (array) {
            cJSON_AddItemToArray(array, entries[i]);
        } else {
            cJSON_Delete(entries[i]);
        }
    }
    free(entries);
    if (result == URSATZ_OK && !array) {
        result = workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
    }
    *out_styles = array;
    return result;
}

UrsatzStatus workshop_style_get_entry(const char *id, cJSON **out_entry, cJSON **out_skipped) {
    *out_entry = NULL;
    *out_skipped = NULL;
    LOCK();
    cJSON *meta = NULL;
    UrsatzStatus result = load_meta(id, &meta);
    UNLOCK();
    if (result != URSATZ_OK) {
        return result;
    }
    cJSON *skipped = cJSON_DetachItemFromObjectCaseSensitive(meta, "skipped");
    cJSON *entry = entry_of(meta);
    cJSON_Delete(meta);
    if (!entry || !(skipped = skipped ? skipped : cJSON_CreateArray())) {
        cJSON_Delete(entry);
        cJSON_Delete(skipped);
        return workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
    }
    *out_entry = entry;
    *out_skipped = skipped;
    return URSATZ_OK;
}

/* --- get ------------------------------------------------------------------ */

#define MAX_LABELS_PER_CATEGORY 12

/* Most frequent first, then label. */
static int compare_labels(const void *left, const void *right) {
    const StyleStatistic *a = *(const StyleStatistic *const *)left;
    const StyleStatistic *b = *(const StyleStatistic *const *)right;
    if (a->count != b->count) {
        return a->count > b->count ? -1 : 1;
    }
    return strcmp(a->label, b->label);
}

/* One category of the statistics: statistics[first, end) share its name. */
typedef struct {
    size_t first;
    size_t end;
    StyleDimension dimension;
} CategoryRange;

/* Harmony, rhythm, melody, form, then category. */
static const StyleStatistic *g_statistics;

static int compare_categories(const void *left, const void *right) {
    const CategoryRange *a = left;
    const CategoryRange *b = right;
    int by_dimension = (int)a->dimension - (int)b->dimension;
    return by_dimension != 0 ? by_dimension : strcmp(g_statistics[a->first].category, g_statistics[b->first].category);
}

/* {dimension: {category: {"labels": [{"label","count","total"}], "more": n}}} from the model file; the labels come by count
   descending (then label), at most 12, and "more" is how many were cut. */
static UrsatzStatus statistics_json(const char *model_path, cJSON **out) {
    *out = NULL;
    StyleModel *model = NULL;
    if (style_model_file_read(model_path, &model, NULL) != STYLE_MODEL_FILE_OK) {
        return workshop_fail(URSATZ_ERR_INTERNAL, "style operation failed");
    }
    StyleStatistic *statistics = NULL;
    size_t count = 0;
    StyleModelFileResult read = style_model_statistics(model, &statistics, &count);
    style_model_destroy(model);
    if (read != STYLE_MODEL_FILE_OK) {
        return workshop_fail(URSATZ_ERR_INTERNAL, "style operation failed");
    }

    CategoryRange *ranges = count > 0 ? calloc(count, sizeof(*ranges)) : NULL;
    size_t range_count = 0;
    UrsatzStatus result = count > 0 && !ranges ? workshop_fail(URSATZ_ERR_ALLOC, "out of memory") : URSATZ_OK;
    for (size_t start = 0, end; result == URSATZ_OK && start < count; start = end) {
        end = start;
        while (end < count && strcmp(statistics[end].category, statistics[start].category) == 0) {
            end++;
        }
        StyleDimension dimension;
        if (style_category_dimension(statistics[start].category, &dimension)) {
            ranges[range_count++] = (CategoryRange){start, end, dimension};
        }
    }
    g_statistics = statistics;
    if (range_count > 1) {
        qsort(ranges, range_count, sizeof(*ranges), compare_categories);
    }
    g_statistics = NULL;

    cJSON *object = result == URSATZ_OK ? cJSON_CreateObject() : NULL;
    for (size_t i = 0; object && i < range_count; i++) {
        const CategoryRange *range = &ranges[i];
        const char *dimension_name = style_dimension_name(range->dimension);
        cJSON *dimension = cJSON_GetObjectItemCaseSensitive(object, dimension_name);
        if (!dimension) {
            dimension = cJSON_AddObjectToObject(object, dimension_name);
        }
        cJSON *entry = dimension ? cJSON_AddObjectToObject(dimension, statistics[range->first].category) : NULL;
        cJSON *labels = entry ? cJSON_AddArrayToObject(entry, "labels") : NULL;
        size_t total_labels = range->end - range->first;
        const StyleStatistic **sorted = labels ? calloc(total_labels, sizeof(*sorted)) : NULL;
        bool ok = sorted != NULL;
        for (size_t j = 0; ok && j < total_labels; j++) {
            sorted[j] = &statistics[range->first + j];
        }
        if (ok) {
            qsort(sorted, total_labels, sizeof(*sorted), compare_labels);
        }
        size_t kept = total_labels < MAX_LABELS_PER_CATEGORY ? total_labels : MAX_LABELS_PER_CATEGORY;
        for (size_t j = 0; ok && j < kept; j++) {
            cJSON *label = cJSON_CreateObject();
            ok = label && cJSON_AddStringToObject(label, "label", sorted[j]->label) &&
                 cJSON_AddNumberToObject(label, "count", (double)sorted[j]->count) &&
                 cJSON_AddNumberToObject(label, "total", (double)sorted[j]->total);
            if (ok) {
                cJSON_AddItemToArray(labels, label);
            } else {
                cJSON_Delete(label);
            }
        }
        free(sorted);
        if (!ok || !cJSON_AddNumberToObject(entry, "more", (double)(total_labels - kept))) {
            cJSON_Delete(object);
            object = NULL;
        }
    }
    free(ranges);
    style_statistics_free(statistics, count);
    if (result == URSATZ_OK && !object) {
        result = workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
    }
    *out = object;
    return result;
}

UrsatzStatus workshop_style_get(const char *id, cJSON **out_style) {
    *out_style = NULL;
    LOCK();
    cJSON *meta = NULL;
    UrsatzStatus result = load_meta(id, &meta);
    cJSON *statistics = NULL;
    if (result == URSATZ_OK) {
        char dir[PATH_BYTES];
        char model_path[PATH_BYTES + 32];
        style_dir_path(id, dir, sizeof(dir));
        snprintf(model_path, sizeof(model_path), "%s/style-model.json", dir);
        result = statistics_json(model_path, &statistics);
    }
    UNLOCK();
    if (result == URSATZ_OK && !cJSON_AddItemToObject(meta, "statistics", statistics)) {
        cJSON_Delete(statistics);
        result = workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
    }
    if (result != URSATZ_OK) {
        cJSON_Delete(meta);
        return result;
    }
    *out_style = meta;
    return URSATZ_OK;
}

/* --- rebuild, delete, model path ------------------------------------------ */

/* The source a stored style was built from, read from its meta.json; the strings point into meta. */
typedef struct {
    cJSON *meta;
    WorkshopStyleBrief brief;
    const char **piece_ids;
    WorkshopStyleSource input;
} StoredSource;

static void stored_source_free(StoredSource *stored) {
    workshop_style_brief_free(&stored->brief);
    free(stored->piece_ids);
    cJSON_Delete(stored->meta);
    memset(stored, 0, sizeof(*stored));
}

static const char *json_text(const cJSON *object, const char *name) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);
    return cJSON_IsString(item) ? item->valuestring : NULL;
}

static UrsatzStatus stored_source_load(const char *id, StoredSource *stored) {
    memset(stored, 0, sizeof(*stored));
    UrsatzStatus result = load_meta(id, &stored->meta);
    if (result != URSATZ_OK) {
        return result;
    }
    const cJSON *source = cJSON_GetObjectItemCaseSensitive(stored->meta, "source");
    const cJSON *filter = cJSON_GetObjectItemCaseSensitive(source, "filter");
    WorkshopStyleSource *input = &stored->input;
    input->kind = source_kind_from_label(json_text(source, "kind"));
    input->composer = json_text(filter, "composer");
    input->source = json_text(filter, "source");
    input->tag = json_text(filter, "tag");
    input->collection_id = json_text(source, "collection_id");
    const cJSON *ids = cJSON_GetObjectItemCaseSensitive(source, "piece_ids");
    int length = cJSON_IsArray(ids) ? cJSON_GetArraySize(ids) : 0;
    stored->piece_ids = length > 0 ? calloc((size_t)length, sizeof(*stored->piece_ids)) : NULL;
    bool ok = length == 0 || stored->piece_ids != NULL;
    const cJSON *piece = NULL;
    cJSON_ArrayForEach(piece, ids) {
        if (ok && cJSON_IsString(piece)) {
            stored->piece_ids[input->piece_count++] = piece->valuestring;
        }
    }
    input->piece_ids = stored->piece_ids;
    if (ok && input->kind == WORKSHOP_STYLE_SOURCE_BRIEF) {
        ok = load_brief(cJSON_GetObjectItemCaseSensitive(source, "brief"), &stored->brief);
    }
    input->brief = &stored->brief;
    if (!ok) {
        stored_source_free(stored);
        return workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
    }
    return URSATZ_OK;
}

UrsatzStatus workshop_style_rebuild(const char *id) {
    LOCK();
    StoredSource stored;
    UrsatzStatus result = stored_source_load(id, &stored);
    if (result != URSATZ_OK) {
        UNLOCK();
        return result;
    }

    NormalizedSource source;
    result = normalized_source_make(&stored.input, &source);
    if (result == URSATZ_OK) {
        Collected collected;
        result = collect_pieces(&source, &collected);
        if (result == URSATZ_ERR_NOT_FOUND) {
            result = workshop_fail(URSATZ_ERR_UNSATISFIABLE, "source not found");
        }
        if (result == URSATZ_OK) {
            char dir[PATH_BYTES];
            char now[32];
            common_utc_now(now);
            style_dir_path(id, dir, sizeof(dir));
            const char *name = json_text(stored.meta, "name");
            const char *created_at = json_text(stored.meta, "created_at");
            if (!write_style_files(dir, id, name ? name : "", created_at ? created_at : "", now, &source, &collected)) {
                result = workshop_fail(URSATZ_ERR_INTERNAL, "style operation failed");
            }
            collected_free(&collected);
        }
        normalized_source_free(&source);
    }
    UNLOCK();
    stored_source_free(&stored);
    return result;
}

UrsatzStatus workshop_style_delete(const char *id) {
    char dir[PATH_BYTES];
    if (!style_dir_path(id, dir, sizeof(dir))) {
        return workshop_fail(URSATZ_ERR_NOT_FOUND, "style not found");
    }
    LOCK();
    UrsatzStatus result = URSATZ_OK;
    if (!common_path_is_dir(dir)) {
        result = workshop_fail(URSATZ_ERR_NOT_FOUND, "style not found");
    } else {
        remove_style_dir(dir);
        if (common_path_is_dir(dir)) {
            result = workshop_fail(URSATZ_ERR_INTERNAL, "style operation failed");
        }
    }
    UNLOCK();
    return result;
}

UrsatzStatus workshop_style_model_path(const char *id, char *buffer, size_t buffer_size) {
    char path[PATH_BYTES + 32];
    char dir[PATH_BYTES];
    if (!buffer || buffer_size == 0 || !style_dir_path(id, dir, sizeof(dir))) {
        return workshop_fail(URSATZ_ERR_NOT_FOUND, "style not found");
    }
    snprintf(path, sizeof(path), "%s/style-model.json", dir);

    LOCK();
    char resolved[4096]; /* realpath writes up to PATH_MAX bytes */
    bool found = realpath(path, resolved) != NULL;
    UNLOCK();
    if (!found) {
        return workshop_fail(URSATZ_ERR_NOT_FOUND, "style not found");
    }
    if (strlen(resolved) >= buffer_size) {
        return workshop_fail(URSATZ_ERR_INTERNAL, "style operation failed");
    }
    memcpy(buffer, resolved, strlen(resolved) + 1);
    return URSATZ_OK;
}

/* --- the build request ---------------------------------------------------- */

void workshop_style_request_free(WorkshopStyleRequest *request) {
    free(request->ids);
    workshop_style_brief_free(&request->brief);
    memset(request, 0, sizeof(*request));
}

UrsatzStatus workshop_style_build_request_parse(const cJSON *body, WorkshopStyleRequest *request, UrsatzError *error) {
    memset(request, 0, sizeof(*request));
    const cJSON *name_item = cJSON_GetObjectItemCaseSensitive(body, "name");
    const cJSON *filter_item = cJSON_GetObjectItemCaseSensitive(body, "filter");
    const cJSON *collection_item = cJSON_GetObjectItemCaseSensitive(body, "collection_id");
    const cJSON *ids_item = cJSON_GetObjectItemCaseSensitive(body, "piece_ids");
    const cJSON *brief_item = cJSON_GetObjectItemCaseSensitive(body, "brief");
    if (name_item && !cJSON_IsString(name_item)) {
        return ursatz_error_set(error, URSATZ_ERR_REQUEST, NULL, "invalid name");
    }
    int sources = (filter_item != NULL) + (collection_item != NULL) + (ids_item != NULL) + (brief_item != NULL);
    if (sources != 1) {
        return ursatz_error_set(error, URSATZ_ERR_REQUEST, NULL, "give exactly one of filter, collection_id, piece_ids and brief");
    }
    request->name = name_item ? name_item->valuestring : NULL;
    WorkshopStyleSource *source = &request->source;
    const char *failure = NULL;

    if (brief_item) {
        source->kind = WORKSHOP_STYLE_SOURCE_BRIEF;
        failure = workshop_style_brief_parse(brief_item, &request->brief);
        source->brief = &request->brief;
    } else if (filter_item) {
        source->kind = WORKSHOP_STYLE_SOURCE_FILTER;
        const cJSON *composer = cJSON_GetObjectItemCaseSensitive(filter_item, "composer");
        const cJSON *filter_source = cJSON_GetObjectItemCaseSensitive(filter_item, "source");
        const cJSON *tag = cJSON_GetObjectItemCaseSensitive(filter_item, "tag");
        if (!cJSON_IsObject(filter_item) || (composer && !cJSON_IsString(composer) && !cJSON_IsNull(composer)) ||
            (filter_source && !cJSON_IsString(filter_source) && !cJSON_IsNull(filter_source)) ||
            (tag && !cJSON_IsString(tag) && !cJSON_IsNull(tag))) {
            failure = "invalid filter";
        } else {
            source->composer = cJSON_IsString(composer) ? composer->valuestring : NULL;
            source->source = cJSON_IsString(filter_source) ? filter_source->valuestring : NULL;
            source->tag = cJSON_IsString(tag) ? tag->valuestring : NULL;
        }
    } else if (collection_item) {
        source->kind = WORKSHOP_STYLE_SOURCE_COLLECTION;
        if (!cJSON_IsString(collection_item) || !workshop_collection_is_valid_id(collection_item->valuestring)) {
            failure = "invalid collection_id";
        } else {
            source->collection_id = collection_item->valuestring;
        }
    } else {
        source->kind = WORKSHOP_STYLE_SOURCE_PIECES;
        int length = cJSON_IsArray(ids_item) ? cJSON_GetArraySize(ids_item) : 0;
        if (length <= 0) {
            failure = "invalid piece_ids";
        } else if ((size_t)length > WORKSHOP_STYLE_BRIEF_MAX_PIECES) {
            failure = "too many pieces";
        } else {
            request->ids = calloc((size_t)length, sizeof(*request->ids));
            if (!request->ids) {
                return ursatz_error_set(error, URSATZ_ERR_ALLOC, NULL, "out of memory");
            }
            const cJSON *item = NULL;
            cJSON_ArrayForEach(item, ids_item) {
                if (!cJSON_IsString(item) || !workshop_collection_is_valid_id(item->valuestring)) {
                    failure = "invalid piece_ids";
                    break;
                }
                request->ids[source->piece_count++] = item->valuestring;
            }
            source->piece_ids = request->ids;
        }
    }
    if (failure) {
        UrsatzStatus status = strcmp(failure, "out of memory") == 0 ? URSATZ_ERR_ALLOC : URSATZ_ERR_REQUEST;
        workshop_style_request_free(request);
        return ursatz_error_set(error, status, NULL, "%s", failure);
    }
    return URSATZ_OK;
}
