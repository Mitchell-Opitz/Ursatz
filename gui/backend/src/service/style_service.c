#include "style_service.h"
#include "util/fs_util.h"
#include "util/store_lock.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#else
#include <dirent.h>
#include <pthread.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include <cJSON.h>

#include <ursatz_analyzer/analysis_document.h>
#include <ursatz_analyzer/style_profile.h>

#include "analysis_service.h"
#include "collection_service.h"
#include "id_service.h"
#include "piece_metadata.h"

static const char *kPiecesDir = "pieces";
static const char *kStylesDir = "styles";

#define PATH_BYTES 400
#define NAME_MAX_BYTES URSATZ_GUI_META_MAX_VALUE_BYTES
#define ID_MAX_TRIES 8

static UrsatzGuiLock g_lock = URSATZ_GUI_LOCK_INIT;
#define LOCK() ursatz_gui_lock_acquire(&g_lock)
#define UNLOCK() ursatz_gui_lock_release(&g_lock)

/* Renames from over to, replacing it (Windows cannot rename over a file). */
static bool replace_file(const char *from, const char *to) {
#ifdef _WIN32
    remove(to);
#endif
    if (rename(from, to) != 0) {
        remove(from);
        return false;
    }
    return true;
}

/* --- skipped pieces ------------------------------------------------------- */

void ursatz_gui_style_skipped_free(UrsatzGuiStyleSkipped *skipped, size_t count) {
    for (size_t i = 0; skipped && i < count; i++) {
        free(skipped[i].id);
        free(skipped[i].reason);
    }
    free(skipped);
}

typedef struct {
    UrsatzGuiStyleSkipped *items;
    size_t count;
    size_t capacity;
} SkippedList;

static bool skipped_add(SkippedList *list, const char *id, const char *reason) {
    if (list->count == list->capacity) {
        size_t capacity = list->capacity ? list->capacity * 2 : 8;
        UrsatzGuiStyleSkipped *grown = realloc(list->items, capacity * sizeof(*grown));
        if (!grown) {
            return false;
        }
        list->items = grown;
        list->capacity = capacity;
    }
    list->items[list->count].id = ursatz_gui_dup_string(id);
    list->items[list->count].reason = ursatz_gui_dup_string(reason);
    if (!list->items[list->count].id || !list->items[list->count].reason) {
        free(list->items[list->count].id);
        free(list->items[list->count].reason);
        return false;
    }
    list->count++;
    return true;
}

/* --- id lists ------------------------------------------------------------- */

typedef struct {
    char **items;
    size_t count;
    size_t capacity;
} IdList;

static void id_list_free(IdList *list) {
    for (size_t i = 0; i < list->count; i++) {
        free(list->items[i]);
    }
    free(list->items);
    memset(list, 0, sizeof(*list));
}

static bool id_list_contains(const IdList *list, const char *id) {
    for (size_t i = 0; i < list->count; i++) {
        if (strcmp(list->items[i], id) == 0) {
            return true;
        }
    }
    return false;
}

static bool id_list_add(IdList *list, const char *id) {
    if (list->count == list->capacity) {
        size_t capacity = list->capacity ? list->capacity * 2 : 16;
        char **grown = realloc(list->items, capacity * sizeof(*grown));
        if (!grown) {
            return false;
        }
        list->items = grown;
        list->capacity = capacity;
    }
    list->items[list->count] = ursatz_gui_dup_string(id);
    if (!list->items[list->count]) {
        return false;
    }
    list->count++;
    return true;
}

/* --- briefs --------------------------------------------------------------- */

static void free_strings(char **items, size_t count);

static char *dup_or_null(const char *value) {
    return value && value[0] != '\0' ? ursatz_gui_dup_string(value) : NULL;
}

void ursatz_gui_style_brief_free(UrsatzGuiStyleBrief *brief) {
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
        copy[i] = ursatz_gui_dup_string(items[i]);
        if (!copy[i]) {
            free_strings(copy, i);
            return false;
        }
    }
    *out = copy;
    return true;
}

/* A deep copy of source into out (zeroed first); out is freed with ursatz_gui_style_brief_free. */
static bool brief_copy(const UrsatzGuiStyleBrief *source, UrsatzGuiStyleBrief *out) {
    memset(out, 0, sizeof(*out));
    if (source->scope_count > 0) {
        out->scopes = calloc(source->scope_count, sizeof(*out->scopes));
        if (!out->scopes) {
            return false;
        }
        out->scope_count = source->scope_count;
        for (size_t i = 0; i < source->scope_count; i++) {
            const UrsatzGuiLinkScope *from = &source->scopes[i];
            UrsatzGuiLinkScope *to = &out->scopes[i];
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
                ursatz_gui_style_brief_free(out);
                return false;
            }
        }
    }
    if (!strings_copy(source->piece_ids, source->piece_count, &out->piece_ids) ||
        !strings_copy(source->style_ids, source->style_count, &out->style_ids)) {
        ursatz_gui_style_brief_free(out);
        return false;
    }
    out->piece_count = source->piece_count;
    out->style_count = source->style_count;
    return true;
}

static const char *const kScopeKinds[] = {"all", "collection", "filter", "subject", "project"};

static const char *scope_kind_label(UrsatzGuiLinkScopeKind kind) {
    return (int)kind >= 0 && (size_t)kind < sizeof(kScopeKinds) / sizeof(kScopeKinds[0]) ? kScopeKinds[kind] : "all";
}

static bool scope_kind_from_label(const char *label, UrsatzGuiLinkScopeKind *kind) {
    for (size_t i = 0; label && i < sizeof(kScopeKinds) / sizeof(kScopeKinds[0]); i++) {
        if (strcmp(label, kScopeKinds[i]) == 0) {
            *kind = (UrsatzGuiLinkScopeKind)i;
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

static const char *source_kind_label(UrsatzGuiStyleSourceKind kind) {
    switch (kind) {
        case URSATZ_GUI_STYLE_SOURCE_FILTER:
            return "filter";
        case URSATZ_GUI_STYLE_SOURCE_COLLECTION:
            return "collection";
        case URSATZ_GUI_STYLE_SOURCE_BRIEF:
            return "brief";
        case URSATZ_GUI_STYLE_SOURCE_PIECES:
        default:
            return "pieces";
    }
}

/* A source with its filter values normalized, ready to resolve and to store. */
typedef struct {
    UrsatzGuiStyleSourceKind kind;
    char *composer; /* "" when unset */
    char *source;
    char *tag;
    char *collection_id;
    IdList piece_ids;
    UrsatzGuiStyleBrief brief;
} NormalizedSource;

static void normalized_source_free(NormalizedSource *source) {
    free(source->composer);
    free(source->source);
    free(source->tag);
    free(source->collection_id);
    id_list_free(&source->piece_ids);
    ursatz_gui_style_brief_free(&source->brief);
    memset(source, 0, sizeof(*source));
}

/* Normalizes the filter values of a request; INVALID_SOURCE when none is given or a tag is invalid. */
static UrsatzGuiStyleResult normalized_source_make(const UrsatzGuiStyleSource *input, NormalizedSource *out) {
    memset(out, 0, sizeof(*out));
    out->kind = input->kind;

    if (input->kind == URSATZ_GUI_STYLE_SOURCE_FILTER) {
        bool too_long = false;
        bool invalid = false;
        out->composer = ursatz_gui_meta_normalize_value(input->composer, &too_long);
        out->source = out->composer ? ursatz_gui_meta_normalize_value(input->source, &too_long) : NULL;
        if (!out->composer || !out->source) {
            normalized_source_free(out);
            return too_long ? URSATZ_GUI_STYLE_ERR_INVALID_SOURCE : URSATZ_GUI_STYLE_ERR_ALLOC;
        }
        char *tag_text = ursatz_gui_meta_normalize_value(input->tag, &too_long);
        if (!tag_text) {
            normalized_source_free(out);
            return too_long ? URSATZ_GUI_STYLE_ERR_INVALID_SOURCE : URSATZ_GUI_STYLE_ERR_ALLOC;
        }
        out->tag = tag_text[0] != '\0' ? ursatz_gui_meta_normalize_tag(tag_text, &invalid) : ursatz_gui_dup_string("");
        free(tag_text);
        if (!out->tag) {
            normalized_source_free(out);
            return invalid ? URSATZ_GUI_STYLE_ERR_INVALID_SOURCE : URSATZ_GUI_STYLE_ERR_ALLOC;
        }
        if (out->composer[0] == '\0' && out->source[0] == '\0' && out->tag[0] == '\0') {
            normalized_source_free(out);
            return URSATZ_GUI_STYLE_ERR_INVALID_SOURCE;
        }
        return URSATZ_GUI_STYLE_OK;
    }

    if (input->kind == URSATZ_GUI_STYLE_SOURCE_COLLECTION) {
        if (!ursatz_gui_collection_is_valid_id(input->collection_id)) {
            return URSATZ_GUI_STYLE_ERR_INVALID_SOURCE;
        }
        out->collection_id = ursatz_gui_dup_string(input->collection_id);
        return out->collection_id ? URSATZ_GUI_STYLE_OK : URSATZ_GUI_STYLE_ERR_ALLOC;
    }

    if (input->kind == URSATZ_GUI_STYLE_SOURCE_BRIEF) {
        const UrsatzGuiStyleBrief *brief = input->brief;
        if (!brief || brief->scope_count > URSATZ_GUI_STYLE_BRIEF_MAX_SCOPES ||
            brief->piece_count > URSATZ_GUI_STYLE_BRIEF_MAX_PIECES ||
            brief->style_count > URSATZ_GUI_STYLE_BRIEF_MAX_STYLES ||
            brief->scope_count + brief->piece_count + brief->style_count == 0 ||
            (brief->scope_count > 0 && !brief->scopes) || (brief->piece_count > 0 && !brief->piece_ids) ||
            (brief->style_count > 0 && !brief->style_ids)) {
            return URSATZ_GUI_STYLE_ERR_INVALID_SOURCE;
        }
        for (size_t i = 0; i < brief->scope_count; i++) {
            if ((int)brief->scopes[i].kind < 0 ||
                (size_t)brief->scopes[i].kind >= sizeof(kScopeKinds) / sizeof(kScopeKinds[0])) {
                return URSATZ_GUI_STYLE_ERR_INVALID_SOURCE;
            }
        }
        for (size_t i = 0; i < brief->piece_count; i++) {
            if (!ursatz_gui_collection_is_valid_id(brief->piece_ids[i])) {
                return URSATZ_GUI_STYLE_ERR_INVALID_SOURCE;
            }
        }
        for (size_t i = 0; i < brief->style_count; i++) {
            if (!ursatz_gui_collection_is_valid_id(brief->style_ids[i])) {
                return URSATZ_GUI_STYLE_ERR_INVALID_SOURCE;
            }
        }
        return brief_copy(brief, &out->brief) ? URSATZ_GUI_STYLE_OK : URSATZ_GUI_STYLE_ERR_ALLOC;
    }

    if (input->piece_count == 0 || !input->piece_ids) {
        return URSATZ_GUI_STYLE_ERR_INVALID_SOURCE;
    }
    for (size_t i = 0; i < input->piece_count; i++) {
        if (!ursatz_gui_collection_is_valid_id(input->piece_ids[i])) {
            normalized_source_free(out);
            return URSATZ_GUI_STYLE_ERR_INVALID_SOURCE;
        }
        if (!id_list_contains(&out->piece_ids, input->piece_ids[i]) &&
            !id_list_add(&out->piece_ids, input->piece_ids[i])) {
            normalized_source_free(out);
            return URSATZ_GUI_STYLE_ERR_ALLOC;
        }
    }
    return URSATZ_GUI_STYLE_OK;
}

static void free_summaries(UrsatzGuiPieceSummary *pieces, size_t count) {
    for (size_t i = 0; i < count; i++) {
        free(pieces[i].id);
        free(pieces[i].filename);
        free(pieces[i].format);
        free(pieces[i].uploaded_at);
        free(pieces[i].composer);
        free(pieces[i].source);
        ursatz_gui_meta_tags_free(pieces[i].tags, pieces[i].tag_count);
    }
    free(pieces);
}

static UrsatzGuiStyleResult load_style(const char *id, UrsatzGuiStyle *style);
static void style_destroy_contents(UrsatzGuiStyle *style);

static int compare_ids(const void *left, const void *right) {
    return strcmp(*(char *const *)left, *(char *const *)right);
}

/* The pieces of a brief: scopes, ids and styles' last builds, in id order without repeats. */
static UrsatzGuiStyleResult resolve_brief(const UrsatzGuiStyleBrief *brief, IdList *ids) {
    IdList all = {0};
    UrsatzGuiStyleResult result = URSATZ_GUI_STYLE_OK;

    for (size_t i = 0; result == URSATZ_GUI_STYLE_OK && i < brief->scope_count; i++) {
        char **scope_ids = NULL;
        size_t scope_count = 0;
        UrsatzGuiLinkResult resolved = ursatz_gui_link_scope_resolve(&brief->scopes[i], &scope_ids, &scope_count);
        if (resolved == URSATZ_GUI_LINK_ERR_EMPTY) {
            continue;
        }
        if (resolved != URSATZ_GUI_LINK_OK) {
            result = resolved == URSATZ_GUI_LINK_ERR_INVALID_SCOPE ? URSATZ_GUI_STYLE_ERR_INVALID_SOURCE
                     : resolved == URSATZ_GUI_LINK_ERR_NOT_FOUND   ? URSATZ_GUI_STYLE_ERR_NOT_FOUND
                     : resolved == URSATZ_GUI_LINK_ERR_ALLOC       ? URSATZ_GUI_STYLE_ERR_ALLOC
                                                                   : URSATZ_GUI_STYLE_ERR_FAILED;
            continue;
        }
        for (size_t j = 0; result == URSATZ_GUI_STYLE_OK && j < scope_count; j++) {
            result = id_list_add(&all, scope_ids[j]) ? URSATZ_GUI_STYLE_OK : URSATZ_GUI_STYLE_ERR_ALLOC;
        }
        ursatz_gui_link_ids_free(scope_ids, scope_count);
    }
    for (size_t i = 0; result == URSATZ_GUI_STYLE_OK && i < brief->piece_count; i++) {
        result = id_list_add(&all, brief->piece_ids[i]) ? URSATZ_GUI_STYLE_OK : URSATZ_GUI_STYLE_ERR_ALLOC;
    }
    for (size_t i = 0; result == URSATZ_GUI_STYLE_OK && i < brief->style_count; i++) {
        UrsatzGuiStyle style;
        memset(&style, 0, sizeof(style));
        result = load_style(brief->style_ids[i], &style);
        for (size_t j = 0; result == URSATZ_GUI_STYLE_OK && j < style.piece_count; j++) {
            result = id_list_add(&all, style.pieces[j]) ? URSATZ_GUI_STYLE_OK : URSATZ_GUI_STYLE_ERR_ALLOC;
        }
        style_destroy_contents(&style);
    }

    if (result == URSATZ_GUI_STYLE_OK && all.count > 1) {
        qsort(all.items, all.count, sizeof(*all.items), compare_ids);
    }
    for (size_t i = 0; result == URSATZ_GUI_STYLE_OK && i < all.count; i++) {
        if (i > 0 && strcmp(all.items[i], all.items[i - 1]) == 0) {
            continue;
        }
        result = id_list_add(ids, all.items[i]) ? URSATZ_GUI_STYLE_OK : URSATZ_GUI_STYLE_ERR_ALLOC;
    }
    id_list_free(&all);
    return result;
}

/*
 * The piece ids a source names, in the order the Library lists them (filter),
 * the collection's add order, or as given. ids that name no piece are not
 * dropped here: collect_pieces skips them with the reason "not found".
 * default_name receives the style's default name.
 */
static UrsatzGuiStyleResult resolve_piece_ids(const NormalizedSource *source, IdList *ids, char *default_name,
                                              size_t default_name_size) {
    if (source->kind == URSATZ_GUI_STYLE_SOURCE_FILTER) {
        UrsatzGuiMetaFilter filter = {0};
        filter.composer = source->composer[0] ? source->composer : NULL;
        filter.source = source->source[0] ? source->source : NULL;
        filter.tag = source->tag[0] ? source->tag : NULL;

        UrsatzGuiPieceSummary *pieces = NULL;
        size_t count = 0;
        if (ursatz_gui_analysis_list(kPiecesDir, &pieces, &count) != URSATZ_GUI_ANALYSIS_OK) {
            return URSATZ_GUI_STYLE_ERR_FAILED;
        }
        for (size_t i = 0; i < count; i++) {
            if (ursatz_gui_meta_matches(&pieces[i], &filter) && !id_list_add(ids, pieces[i].id)) {
                free_summaries(pieces, count);
                return URSATZ_GUI_STYLE_ERR_ALLOC;
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
                return URSATZ_GUI_STYLE_ERR_FAILED;
            }
            used += (size_t)n;
        }
        clip_utf8(name, NAME_MAX_BYTES);
        snprintf(default_name, default_name_size, "%s", name);
        return URSATZ_GUI_STYLE_OK;
    }

    if (source->kind == URSATZ_GUI_STYLE_SOURCE_COLLECTION) {
        UrsatzGuiCollection *collection = NULL;
        UrsatzGuiCollectionResult result = ursatz_gui_collection_get(source->collection_id, &collection);
        if (result == URSATZ_GUI_COLLECTION_ERR_NOT_FOUND) {
            return URSATZ_GUI_STYLE_ERR_NOT_FOUND;
        }
        if (result != URSATZ_GUI_COLLECTION_OK) {
            return result == URSATZ_GUI_COLLECTION_ERR_ALLOC ? URSATZ_GUI_STYLE_ERR_ALLOC : URSATZ_GUI_STYLE_ERR_FAILED;
        }
        bool ok = true;
        for (size_t i = 0; ok && i < collection->piece_count; i++) {
            ok = id_list_add(ids, collection->pieces[i]);
        }
        char name[NAME_MAX_BYTES * 2 + 32];
        snprintf(name, sizeof(name), "Collection: %s", collection->name);
        clip_utf8(name, NAME_MAX_BYTES);
        snprintf(default_name, default_name_size, "%s", name);
        ursatz_gui_collection_destroy(collection);
        return ok ? URSATZ_GUI_STYLE_OK : URSATZ_GUI_STYLE_ERR_ALLOC;
    }

    if (source->kind == URSATZ_GUI_STYLE_SOURCE_BRIEF) {
        snprintf(default_name, default_name_size, "Brief");
        return resolve_brief(&source->brief, ids);
    }

    for (size_t i = 0; i < source->piece_ids.count; i++) {
        if (!id_list_add(ids, source->piece_ids.items[i])) {
            return URSATZ_GUI_STYLE_ERR_ALLOC;
        }
    }
    snprintf(default_name, default_name_size, "Selected pieces");
    return URSATZ_GUI_STYLE_OK;
}

/* --- reading pieces into a profile ---------------------------------------- */

/* The pieces a build used and left out, with the profile they made. */
typedef struct {
    StyleProfile *profile;
    IdList used;
    SkippedList skipped;
    char default_name[NAME_MAX_BYTES + 1];
} Collected;

static void collected_free(Collected *collected) {
    style_profile_destroy(collected->profile);
    id_list_free(&collected->used);
    ursatz_gui_style_skipped_free(collected->skipped.items, collected->skipped.count);
    memset(collected, 0, sizeof(*collected));
}

/*
 * Adds one piece's analysis document to the profile. false with *reason set
 * when the piece is skipped; *alloc_failed set when memory ran out.
 */
static bool add_piece(StyleProfile *profile, const char *id, const char **reason) {
    char dir_path[PATH_BYTES];
    char file_path[PATH_BYTES + 16];
    if (!ursatz_gui_collection_is_valid_id(id) || snprintf(dir_path, sizeof(dir_path), "%s/%s", kPiecesDir, id) >=
                                                     (int)sizeof(dir_path)) {
        *reason = "not found";
        return false;
    }
    snprintf(file_path, sizeof(file_path), "%s/meta.json", dir_path);
    cJSON *meta = ursatz_gui_read_json_object(file_path);
    if (!meta) {
        *reason = "not found";
        return false;
    }
    cJSON_Delete(meta);

    /* Another schema, Analyzer version or Ursatz commit: analyze the piece again first. */
    if (ursatz_gui_analysis_ensure_current(kPiecesDir, id) != URSATZ_GUI_ANALYSIS_OK) {
        *reason = "re-analysis failed";
        return false;
    }

    snprintf(file_path, sizeof(file_path), "%s/analysis.json", dir_path);
    char *document = ursatz_gui_read_file(file_path, NULL);
    if (!document) {
        *reason = "unreadable analysis";
        return false;
    }
    StyleProfileResult result = style_profile_add_document_json(profile, document);
    free(document);
    if (result != STYLE_PROFILE_OK) {
        *reason = "unreadable analysis";
        return false;
    }
    return true;
}

/* Resolves the source and adds every piece it names. The caller holds the lock. */
static UrsatzGuiStyleResult collect_pieces(const NormalizedSource *source, Collected *collected) {
    memset(collected, 0, sizeof(*collected));

    IdList ids = {0};
    char default_name[NAME_MAX_BYTES + 1] = "";
    UrsatzGuiStyleResult resolved = resolve_piece_ids(source, &ids, default_name, sizeof(default_name));
    if (resolved != URSATZ_GUI_STYLE_OK) {
        id_list_free(&ids);
        return resolved;
    }
    snprintf(collected->default_name, sizeof(collected->default_name), "%s", default_name);

    collected->profile = style_profile_create();
    if (!collected->profile) {
        id_list_free(&ids);
        return URSATZ_GUI_STYLE_ERR_ALLOC;
    }

    UrsatzGuiStyleResult result = URSATZ_GUI_STYLE_OK;
    for (size_t i = 0; result == URSATZ_GUI_STYLE_OK && i < ids.count; i++) {
        const char *reason = NULL;
        if (add_piece(collected->profile, ids.items[i], &reason)) {
            if (!id_list_add(&collected->used, ids.items[i])) {
                result = URSATZ_GUI_STYLE_ERR_ALLOC;
            }
        } else if (!skipped_add(&collected->skipped, ids.items[i], reason)) {
            result = URSATZ_GUI_STYLE_ERR_ALLOC;
        }
    }
    id_list_free(&ids);

    if (result == URSATZ_GUI_STYLE_OK && collected->used.count == 0) {
        result = URSATZ_GUI_STYLE_ERR_NO_PIECES;
    }
    if (result != URSATZ_GUI_STYLE_OK) {
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

static cJSON *scope_json(const UrsatzGuiLinkScope *scope) {
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
static cJSON *brief_json(const UrsatzGuiStyleBrief *brief) {
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
    if (source->kind == URSATZ_GUI_STYLE_SOURCE_FILTER) {
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
    if (source->kind == URSATZ_GUI_STYLE_SOURCE_PIECES) {
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
    cJSON *brief = source->kind == URSATZ_GUI_STYLE_SOURCE_BRIEF ? brief_json(&source->brief) : cJSON_CreateNull();
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
    if (ok) {
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
        snprintf(meta_path, sizeof(meta_path), "%s/meta.json", dir) >= (int)sizeof(meta_path)) {
        return false;
    }
    char *text = meta_text(id, name, created_at, updated_at, source, collected);
    if (!text) {
        return false;
    }
    bool ok = style_model_file_write(collected->profile, name, tmp_path) == STYLE_MODEL_FILE_OK &&
              replace_file(tmp_path, model_path) && ursatz_gui_write_file_atomic(meta_path, text, strlen(text));
    free(text);
    if (!ok) {
        remove(tmp_path);
    }
    return ok;
}

/* Removes a style directory this module made: its two files and temp files. */
static void remove_style_dir(const char *dir) {
    const char *files[] = {"style-model.json", "style-model.json.tmp", "meta.json", "meta.json.tmp"};
    for (size_t i = 0; i < sizeof(files) / sizeof(files[0]); i++) {
        char path[PATH_BYTES + 32];
        snprintf(path, sizeof(path), "%s/%s", dir, files[i]);
        remove(path);
    }
#ifdef _WIN32
    _rmdir(dir);
#else
    rmdir(dir);
#endif
}

/* --- build ---------------------------------------------------------------- */

/* The style's name: the given one normalized, or the default when it is empty. */
static UrsatzGuiStyleResult style_name(const char *requested, const char *default_name, char *out, size_t out_size) {
    bool too_long = false;
    char *normalized = ursatz_gui_meta_normalize_value(requested, &too_long);
    if (!normalized) {
        return too_long ? URSATZ_GUI_STYLE_ERR_INVALID_NAME : URSATZ_GUI_STYLE_ERR_ALLOC;
    }
    snprintf(out, out_size, "%s", normalized[0] != '\0' ? normalized : default_name);
    free(normalized);
    return URSATZ_GUI_STYLE_OK;
}

/* Normalizes the source, adds its pieces and settles the name. The caller holds the lock; on OK it frees *normalized
 * with normalized_source_free and *collected with collected_free. */
static UrsatzGuiStyleResult prepare_build(const char *name, const UrsatzGuiStyleSource *source,
                                          NormalizedSource *normalized, Collected *collected, char *final_name,
                                          size_t final_name_size) {
    memset(collected, 0, sizeof(*collected));
    UrsatzGuiStyleResult result = normalized_source_make(source, normalized);
    if (result != URSATZ_GUI_STYLE_OK) {
        return result;
    }
    result = collect_pieces(normalized, collected);
    if (result == URSATZ_GUI_STYLE_OK) {
        result = style_name(name, collected->default_name, final_name, final_name_size);
        if (result != URSATZ_GUI_STYLE_OK) {
            collected_free(collected);
        }
    }
    if (result != URSATZ_GUI_STYLE_OK) {
        normalized_source_free(normalized);
    }
    return result;
}

/* Hands the skipped pieces of a finished build to the caller. */
static void give_skipped(Collected *collected, UrsatzGuiStyleSkipped **out_skipped, size_t *out_skipped_count) {
    if (out_skipped) {
        *out_skipped = collected->skipped.items;
        collected->skipped.items = NULL;
        if (out_skipped_count) {
            *out_skipped_count = collected->skipped.count;
        }
    }
}

static void reset_outputs(UrsatzGuiStyleSkipped **out_skipped, size_t *out_skipped_count) {
    if (out_skipped) {
        *out_skipped = NULL;
    }
    if (out_skipped_count) {
        *out_skipped_count = 0;
    }
}

UrsatzGuiStyleResult ursatz_gui_style_build(const char *name, const UrsatzGuiStyleSource *source, char **out_id,
                                            UrsatzGuiStyleSkipped **out_skipped, size_t *out_skipped_count) {
    if (out_id) {
        *out_id = NULL;
    }
    reset_outputs(out_skipped, out_skipped_count);
    if (!source || !out_id) {
        return URSATZ_GUI_STYLE_ERR_INVALID_SOURCE;
    }

    LOCK();
    NormalizedSource normalized;
    Collected collected;
    char final_name[NAME_MAX_BYTES + 1];
    UrsatzGuiStyleResult result = prepare_build(name, source, &normalized, &collected, final_name, sizeof(final_name));
    if (result != URSATZ_GUI_STYLE_OK) {
        UNLOCK();
        return result;
    }

    char *id = NULL;
    char style_dir[PATH_BYTES];
    bool dir_made = false;
    result = URSATZ_GUI_STYLE_ERR_FAILED;
    if (ursatz_gui_ensure_dir(kStylesDir)) {
        for (int attempt = 0; attempt < ID_MAX_TRIES && !dir_made; attempt++) {
            id = ursatz_gui_new_id("sty");
            if (!id) {
                result = URSATZ_GUI_STYLE_ERR_ALLOC;
                break;
            }
            if (snprintf(style_dir, sizeof(style_dir), "%s/%s", kStylesDir, id) < (int)sizeof(style_dir)) {
                dir_made = ursatz_gui_make_dir(style_dir);
            }
            if (!dir_made) {
                free(id);
                id = NULL;
            }
        }
    }
    if (dir_made) {
        char now[32];
        ursatz_gui_iso_utc_now(now, sizeof(now));
        if (write_style_files(style_dir, id, final_name, now, now, &normalized, &collected)) {
            result = URSATZ_GUI_STYLE_OK;
        } else {
            remove_style_dir(style_dir);
        }
    }
    if (result == URSATZ_GUI_STYLE_OK) {
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

UrsatzGuiStyleResult ursatz_gui_style_build_file(const UrsatzGuiStyleSource *source, const char *name, const char *path,
                                                 char ***out_pieces, size_t *out_piece_count,
                                                 UrsatzGuiStyleSkipped **out_skipped, size_t *out_skipped_count) {
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
        return URSATZ_GUI_STYLE_ERR_INVALID_SOURCE;
    }

    LOCK();
    NormalizedSource normalized;
    Collected collected;
    char final_name[NAME_MAX_BYTES + 1];
    UrsatzGuiStyleResult result = prepare_build(name, source, &normalized, &collected, final_name, sizeof(final_name));
    if (result != URSATZ_GUI_STYLE_OK) {
        UNLOCK();
        return result;
    }

    if (style_model_file_write(collected.profile, final_name, tmp_path) != STYLE_MODEL_FILE_OK ||
        !replace_file(tmp_path, path)) {
        remove(tmp_path);
        result = URSATZ_GUI_STYLE_ERR_FAILED;
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

static void style_destroy_contents(UrsatzGuiStyle *style) {
    free(style->id);
    free(style->name);
    free(style->created_at);
    free(style->updated_at);
    free(style->filter_composer);
    free(style->filter_source);
    free(style->filter_tag);
    free(style->collection_id);
    free_strings(style->source_piece_ids, style->source_piece_count);
    ursatz_gui_style_brief_free(&style->brief);
    free_strings(style->pieces, style->piece_count);
    ursatz_gui_style_skipped_free(style->skipped, style->skipped_count);
    for (size_t i = 0; i < style->category_count; i++) {
        UrsatzGuiStyleCategory *category = &style->categories[i];
        free(category->dimension);
        free(category->category);
        for (size_t j = 0; j < category->label_count; j++) {
            free(category->labels[j].label);
        }
        free(category->labels);
    }
    free(style->categories);
    memset(style, 0, sizeof(*style));
}

void ursatz_gui_style_destroy(UrsatzGuiStyle *style) {
    if (style) {
        style_destroy_contents(style);
        free(style);
    }
}

void ursatz_gui_style_free_list(UrsatzGuiStyle *styles, size_t count) {
    for (size_t i = 0; styles && i < count; i++) {
        style_destroy_contents(&styles[i]);
    }
    free(styles);
}

static bool style_dir_path(const char *id, char *out, size_t out_size) {
    return ursatz_gui_collection_is_valid_id(id) && snprintf(out, out_size, "%s/%s", kStylesDir, id) < (int)out_size;
}

/* A copy of a string member, "" when it is absent or not a string. */
static char *string_member(const cJSON *object, const char *key) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, key);
    return ursatz_gui_dup_string(cJSON_IsString(item) && item->valuestring ? item->valuestring : "");
}

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
        items[count] = ursatz_gui_dup_string(item->valuestring);
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

static UrsatzGuiStyleSourceKind source_kind_from_label(const char *label) {
    if (label && strcmp(label, "collection") == 0) {
        return URSATZ_GUI_STYLE_SOURCE_COLLECTION;
    }
    if (label && strcmp(label, "pieces") == 0) {
        return URSATZ_GUI_STYLE_SOURCE_PIECES;
    }
    if (label && strcmp(label, "brief") == 0) {
        return URSATZ_GUI_STYLE_SOURCE_BRIEF;
    }
    return URSATZ_GUI_STYLE_SOURCE_FILTER;
}

/* Reads the stored brief into out (owned strings). false on allocation failure. */
static bool load_brief(const cJSON *json, UrsatzGuiStyleBrief *out) {
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
            UrsatzGuiLinkScope *scope = &out->scopes[out->scope_count];
            if (!cJSON_IsString(kind) || !scope_kind_from_label(kind->valuestring, &scope->kind)) {
                continue;
            }
            char *fields[6];
            fields[0] = string_member(entry, "collection_id");
            fields[1] = string_member(filter, "composer");
            fields[2] = string_member(filter, "source");
            fields[3] = string_member(filter, "tag");
            fields[4] = string_member(entry, "subject_id");
            fields[5] = string_member(entry, "project_id");
            bool filled = true;
            for (int i = 0; i < 6; i++) {
                filled = filled && fields[i] != NULL;
            }
            if (!filled) {
                for (int i = 0; i < 6; i++) {
                    free(fields[i]);
                }
                ursatz_gui_style_brief_free(out);
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
        ursatz_gui_style_brief_free(out);
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
    UrsatzGuiLinkScopeKind kind;
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
        if (!cJSON_IsString(item) || !ursatz_gui_collection_is_valid_id(item->valuestring)) {
            return "invalid brief";
        }
    }
    return NULL;
}

const char *ursatz_gui_style_brief_parse(const cJSON *json, UrsatzGuiStyleBrief *out) {
    memset(out, 0, sizeof(*out));
    if (!cJSON_IsObject(json)) {
        return "invalid brief";
    }
    const cJSON *scopes = cJSON_GetObjectItemCaseSensitive(json, "scopes");
    if (scopes) {
        if (!cJSON_IsArray(scopes)) {
            return "invalid brief";
        }
        if ((size_t)cJSON_GetArraySize(scopes) > URSATZ_GUI_STYLE_BRIEF_MAX_SCOPES) {
            return "too many entries in the brief";
        }
        const cJSON *entry = NULL;
        cJSON_ArrayForEach(entry, scopes) {
            if (!scope_json_valid(entry)) {
                return "invalid scope";
            }
        }
    }
    const char *failure = brief_ids_failure(json, "piece_ids", URSATZ_GUI_STYLE_BRIEF_MAX_PIECES);
    if (!failure) {
        failure = brief_ids_failure(json, "style_ids", URSATZ_GUI_STYLE_BRIEF_MAX_STYLES);
    }
    if (failure) {
        return failure;
    }
    if (!load_brief(json, out)) {
        return "out of memory";
    }
    if (out->scope_count + out->piece_count + out->style_count == 0) {
        ursatz_gui_style_brief_free(out);
        return "invalid brief";
    }
    return NULL;
}

/* Loads styles/<id>/meta.json into style (zeroed by the caller). */
static UrsatzGuiStyleResult load_style(const char *id, UrsatzGuiStyle *style) {
    char dir[PATH_BYTES];
    char path[PATH_BYTES + 16];
    if (!style_dir_path(id, dir, sizeof(dir))) {
        return URSATZ_GUI_STYLE_ERR_NOT_FOUND;
    }
    snprintf(path, sizeof(path), "%s/meta.json", dir);
    cJSON *meta = ursatz_gui_read_json_object(path);
    if (!meta) {
        return URSATZ_GUI_STYLE_ERR_NOT_FOUND;
    }

    /* The directory name is the id; a copy of it inside the file is not trusted. */
    style->id = ursatz_gui_dup_string(id);
    style->name = string_member(meta, "name");
    style->created_at = string_member(meta, "created_at");
    style->updated_at = string_member(meta, "updated_at");

    const cJSON *source = cJSON_GetObjectItemCaseSensitive(meta, "source");
    const cJSON *kind = cJSON_GetObjectItemCaseSensitive(source, "kind");
    const cJSON *filter = cJSON_GetObjectItemCaseSensitive(source, "filter");
    style->source_kind = source_kind_from_label(cJSON_IsString(kind) ? kind->valuestring : NULL);
    style->filter_composer = string_member(filter, "composer");
    style->filter_source = string_member(filter, "source");
    style->filter_tag = string_member(filter, "tag");
    style->collection_id = string_member(source, "collection_id");
    bool ok = style->id && style->name && style->created_at && style->updated_at && style->filter_composer &&
              style->filter_source && style->filter_tag && style->collection_id &&
              string_array_copy(cJSON_GetObjectItemCaseSensitive(source, "piece_ids"), &style->source_piece_ids,
                                &style->source_piece_count) &&
              string_array_copy(cJSON_GetObjectItemCaseSensitive(meta, "pieces"), &style->pieces,
                                &style->piece_count);

    if (ok && style->source_kind == URSATZ_GUI_STYLE_SOURCE_BRIEF) {
        ok = load_brief(cJSON_GetObjectItemCaseSensitive(source, "brief"), &style->brief);
    }

    const cJSON *skipped = cJSON_GetObjectItemCaseSensitive(meta, "skipped");
    int skipped_length = cJSON_IsArray(skipped) ? cJSON_GetArraySize(skipped) : 0;
    if (ok && skipped_length > 0) {
        style->skipped = calloc((size_t)skipped_length, sizeof(*style->skipped));
        ok = style->skipped != NULL;
        const cJSON *entry = NULL;
        cJSON_ArrayForEach(entry, skipped) {
            if (!ok) {
                break;
            }
            style->skipped[style->skipped_count].id = string_member(entry, "id");
            style->skipped[style->skipped_count].reason = string_member(entry, "reason");
            if (!style->skipped[style->skipped_count].id || !style->skipped[style->skipped_count].reason) {
                free(style->skipped[style->skipped_count].id);
                free(style->skipped[style->skipped_count].reason);
                ok = false;
            } else {
                style->skipped_count++;
            }
        }
    }
    cJSON_Delete(meta);

    if (!ok) {
        style_destroy_contents(style);
        return URSATZ_GUI_STYLE_ERR_ALLOC;
    }
    return URSATZ_GUI_STYLE_OK;
}

/* --- list ----------------------------------------------------------------- */

/* The names in styles/; *out_names is NULL when the directory is absent or empty. */
static bool list_style_dirs(char ***out_names, size_t *out_count) {
    IdList names = {0};
    *out_names = NULL;
    *out_count = 0;

#ifdef _WIN32
    char pattern[MAX_PATH];
    snprintf(pattern, sizeof(pattern), "%s\\*", kStylesDir);
    WIN32_FIND_DATAA find_data;
    HANDLE find_handle = FindFirstFileA(pattern, &find_data);
    if (find_handle == INVALID_HANDLE_VALUE) {
        return true;
    }
    bool more = true;
    bool ok = true;
    while (more && ok) {
        const char *name = find_data.cFileName;
        if (strcmp(name, ".") != 0 && strcmp(name, "..") != 0) {
            ok = id_list_add(&names, name);
        }
        more = FindNextFileA(find_handle, &find_data) != 0;
    }
    FindClose(find_handle);
#else
    DIR *dir = opendir(kStylesDir);
    if (!dir) {
        return true;
    }
    bool ok = true;
    struct dirent *entry;
    while (ok && (entry = readdir(dir)) != NULL) {
        const char *name = entry->d_name;
        if (strcmp(name, ".") != 0 && strcmp(name, "..") != 0) {
            ok = id_list_add(&names, name);
        }
    }
    closedir(dir);
#endif
    if (!ok) {
        id_list_free(&names);
        return false;
    }
    *out_names = names.items;
    *out_count = names.count;
    return true;
}

/* Newest update first, then id. */
static int compare_styles(const void *left, const void *right) {
    const UrsatzGuiStyle *a = left;
    const UrsatzGuiStyle *b = right;
    int by_time = strcmp(b->updated_at, a->updated_at);
    return by_time != 0 ? by_time : strcmp(a->id, b->id);
}

UrsatzGuiStyleResult ursatz_gui_style_list(UrsatzGuiStyle **out_styles, size_t *out_count) {
    *out_styles = NULL;
    *out_count = 0;

    LOCK();
    char **names = NULL;
    size_t name_count = 0;
    if (!list_style_dirs(&names, &name_count)) {
        UNLOCK();
        return URSATZ_GUI_STYLE_ERR_ALLOC;
    }

    UrsatzGuiStyle *styles = name_count > 0 ? calloc(name_count, sizeof(*styles)) : NULL;
    UrsatzGuiStyleResult result = name_count > 0 && !styles ? URSATZ_GUI_STYLE_ERR_ALLOC : URSATZ_GUI_STYLE_OK;
    size_t count = 0;
    for (size_t i = 0; result == URSATZ_GUI_STYLE_OK && i < name_count; i++) {
        UrsatzGuiStyleResult loaded = load_style(names[i], &styles[count]);
        if (loaded == URSATZ_GUI_STYLE_OK) {
            count++;
        } else if (loaded != URSATZ_GUI_STYLE_ERR_NOT_FOUND) {
            result = loaded;
        }
    }
    free_strings(names, name_count);
    UNLOCK();

    if (result != URSATZ_GUI_STYLE_OK || count == 0) {
        ursatz_gui_style_free_list(styles, count);
        return result;
    }
    qsort(styles, count, sizeof(*styles), compare_styles);
    *out_styles = styles;
    *out_count = count;
    return URSATZ_GUI_STYLE_OK;
}

/* --- get ------------------------------------------------------------------ */

#define MAX_LABELS_PER_CATEGORY 12

/* Most frequent first, then label. */
static int compare_labels(const void *left, const void *right) {
    const UrsatzGuiStyleLabel *a = left;
    const UrsatzGuiStyleLabel *b = right;
    if (a->count != b->count) {
        return a->count > b->count ? -1 : 1;
    }
    return strcmp(a->label, b->label);
}

static int dimension_rank(const char *name) {
    StyleDimension dimension;
    return style_dimension_from_name(name, &dimension) ? (int)dimension : STYLE_DIMENSION_COUNT;
}

/* Harmony, rhythm, melody, form, then category. */
static int compare_categories(const void *left, const void *right) {
    const UrsatzGuiStyleCategory *a = left;
    const UrsatzGuiStyleCategory *b = right;
    int by_dimension = dimension_rank(a->dimension) - dimension_rank(b->dimension);
    return by_dimension != 0 ? by_dimension : strcmp(a->category, b->category);
}

/* Fills style->categories from the model file; the statistics come sorted by category then label. */
static UrsatzGuiStyleResult load_statistics(const char *model_path, UrsatzGuiStyle *style) {
    StyleModel *model = NULL;
    if (style_model_file_read(model_path, &model, NULL) != STYLE_MODEL_FILE_OK) {
        return URSATZ_GUI_STYLE_ERR_FAILED;
    }
    StyleStatistic *statistics = NULL;
    size_t count = 0;
    StyleModelFileResult read = style_model_statistics(model, &statistics, &count);
    style_model_destroy(model);
    if (read != STYLE_MODEL_FILE_OK) {
        return URSATZ_GUI_STYLE_ERR_FAILED;
    }

    UrsatzGuiStyleResult result = URSATZ_GUI_STYLE_OK;
    size_t start = 0;
    while (result == URSATZ_GUI_STYLE_OK && start < count) {
        size_t end = start;
        while (end < count && strcmp(statistics[end].category, statistics[start].category) == 0) {
            end++;
        }
        StyleDimension dimension;
        if (!style_category_dimension(statistics[start].category, &dimension)) {
            start = end;
            continue;
        }

        UrsatzGuiStyleCategory *grown =
            realloc(style->categories, (style->category_count + 1) * sizeof(*style->categories));
        if (!grown) {
            result = URSATZ_GUI_STYLE_ERR_ALLOC;
            break;
        }
        style->categories = grown;
        UrsatzGuiStyleCategory *category = &style->categories[style->category_count];
        memset(category, 0, sizeof(*category));
        style->category_count++;

        size_t total_labels = end - start;
        UrsatzGuiStyleLabel *labels = calloc(total_labels, sizeof(*labels));
        category->dimension = ursatz_gui_dup_string(style_dimension_name(dimension));
        category->category = ursatz_gui_dup_string(statistics[start].category);
        if (!labels || !category->dimension || !category->category) {
            free(labels);
            result = URSATZ_GUI_STYLE_ERR_ALLOC;
            break;
        }
        for (size_t i = 0; i < total_labels; i++) {
            labels[i].label = (char *)statistics[start + i].label; /* borrowed until the cut below */
            labels[i].count = statistics[start + i].count;
            labels[i].total = statistics[start + i].total;
        }
        qsort(labels, total_labels, sizeof(*labels), compare_labels);

        size_t kept = total_labels < MAX_LABELS_PER_CATEGORY ? total_labels : MAX_LABELS_PER_CATEGORY;
        for (size_t i = 0; result == URSATZ_GUI_STYLE_OK && i < kept; i++) {
            labels[i].label = ursatz_gui_dup_string(labels[i].label);
            if (!labels[i].label) {
                for (size_t j = 0; j < i; j++) {
                    free(labels[j].label);
                }
                free(labels);
                result = URSATZ_GUI_STYLE_ERR_ALLOC;
            }
        }
        if (result != URSATZ_GUI_STYLE_OK) {
            break;
        }
        category->labels = labels;
        category->label_count = kept;
        category->more = total_labels - kept;
        start = end;
    }
    style_statistics_free(statistics, count);

    if (result == URSATZ_GUI_STYLE_OK && style->category_count > 1) {
        qsort(style->categories, style->category_count, sizeof(*style->categories), compare_categories);
    }
    return result;
}

UrsatzGuiStyleResult ursatz_gui_style_get(const char *id, UrsatzGuiStyle **out_style) {
    *out_style = NULL;
    UrsatzGuiStyle *style = calloc(1, sizeof(*style));
    if (!style) {
        return URSATZ_GUI_STYLE_ERR_ALLOC;
    }
    LOCK();
    UrsatzGuiStyleResult result = load_style(id, style);
    if (result == URSATZ_GUI_STYLE_OK) {
        char dir[PATH_BYTES];
        char model_path[PATH_BYTES + 32];
        style_dir_path(id, dir, sizeof(dir));
        snprintf(model_path, sizeof(model_path), "%s/style-model.json", dir);
        result = load_statistics(model_path, style);
    }
    UNLOCK();
    if (result != URSATZ_GUI_STYLE_OK) {
        ursatz_gui_style_destroy(style);
        return result;
    }
    *out_style = style;
    return URSATZ_GUI_STYLE_OK;
}

/* --- rebuild, delete, model path ------------------------------------------ */

UrsatzGuiStyleResult ursatz_gui_style_rebuild(const char *id) {
    UrsatzGuiStyle style;
    memset(&style, 0, sizeof(style));

    LOCK();
    UrsatzGuiStyleResult result = load_style(id, &style);
    if (result != URSATZ_GUI_STYLE_OK) {
        UNLOCK();
        return result;
    }

    UrsatzGuiStyleSource input = {0};
    input.kind = style.source_kind;
    input.composer = style.filter_composer;
    input.source = style.filter_source;
    input.tag = style.filter_tag;
    input.collection_id = style.collection_id;
    input.piece_ids = (const char *const *)style.source_piece_ids;
    input.piece_count = style.source_piece_count;
    input.brief = &style.brief;

    NormalizedSource source;
    result = normalized_source_make(&input, &source);
    if (result == URSATZ_GUI_STYLE_OK) {
        Collected collected;
        result = collect_pieces(&source, &collected);
        if (result == URSATZ_GUI_STYLE_ERR_NOT_FOUND) {
            result = URSATZ_GUI_STYLE_ERR_SOURCE_MISSING;
        }
        if (result == URSATZ_GUI_STYLE_OK) {
            char dir[PATH_BYTES];
            char now[32];
            ursatz_gui_iso_utc_now(now, sizeof(now));
            style_dir_path(id, dir, sizeof(dir));
            if (!write_style_files(dir, style.id, style.name, style.created_at, now, &source, &collected)) {
                result = URSATZ_GUI_STYLE_ERR_FAILED;
            }
            collected_free(&collected);
        }
        normalized_source_free(&source);
    }
    UNLOCK();
    style_destroy_contents(&style);
    return result;
}

UrsatzGuiStyleResult ursatz_gui_style_delete(const char *id) {
    char dir[PATH_BYTES];
    if (!style_dir_path(id, dir, sizeof(dir))) {
        return URSATZ_GUI_STYLE_ERR_NOT_FOUND;
    }
    LOCK();
    UrsatzGuiStyleResult result = URSATZ_GUI_STYLE_OK;
    if (!ursatz_gui_path_is_dir(dir)) {
        result = URSATZ_GUI_STYLE_ERR_NOT_FOUND;
    } else {
        remove_style_dir(dir);
        if (ursatz_gui_path_is_dir(dir)) {
            result = URSATZ_GUI_STYLE_ERR_FAILED;
        }
    }
    UNLOCK();
    return result;
}

UrsatzGuiStyleResult ursatz_gui_style_model_path(const char *id, char *buffer, size_t buffer_size) {
    char path[PATH_BYTES + 32];
    char dir[PATH_BYTES];
    if (!buffer || buffer_size == 0 || !style_dir_path(id, dir, sizeof(dir))) {
        return URSATZ_GUI_STYLE_ERR_NOT_FOUND;
    }
    snprintf(path, sizeof(path), "%s/style-model.json", dir);

    LOCK();
#ifdef _WIN32
    char resolved[PATH_BYTES * 4];
    bool found = _fullpath(resolved, path, sizeof(resolved)) != NULL && GetFileAttributesA(resolved) != INVALID_FILE_ATTRIBUTES;
#else
    char resolved[4096]; /* realpath writes up to PATH_MAX bytes */
    bool found = realpath(path, resolved) != NULL;
#endif
    UNLOCK();
    if (!found) {
        return URSATZ_GUI_STYLE_ERR_NOT_FOUND;
    }
    if (strlen(resolved) >= buffer_size) {
        return URSATZ_GUI_STYLE_ERR_FAILED;
    }
    memcpy(buffer, resolved, strlen(resolved) + 1);
    return URSATZ_GUI_STYLE_OK;
}
