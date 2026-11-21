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
        LOCK();
        result = styles_load_meta(brief->style_ids[i], &meta);
        UNLOCK();
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
 * dropped here: styles_collect_pieces skips them with the reason "not found".
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
        if (workshop_library_list(styles_kPiecesDir, &pieces, &count) != URSATZ_OK) {
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
        styles_clip_utf8(name, NAME_MAX_BYTES);
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
        styles_clip_utf8(name, NAME_MAX_BYTES);
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

void styles_collected_free(Collected *collected) {
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
    UrsatzStatus ensured = workshop_analysis_ensure_current(styles_kPiecesDir, id);
    if (ensured != URSATZ_OK) {
        *reason = ensured == URSATZ_ERR_NOT_FOUND ? "not found" : "re-analysis failed";
        return false;
    }

    char *document = NULL;
    if (workshop_analysis_get_document(styles_kPiecesDir, id, NULL, 0, &document) != URSATZ_OK) {
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

/* Resolves the source and adds every piece it names, reporting "pieces d/t" and stopping between pieces when the job is cancelled
 * (URSATZ_ERR_CANCELLED). Holds no lock: a stale piece is analyzed again here, which takes the piece's lock and its own transaction. */
UrsatzStatus styles_collect_pieces(const NormalizedSource *source, const UrsatzJob *job, Collected *collected) {
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
    ursatz_job_progress(job, "pieces", 0, (unsigned)ids.count);
    for (size_t i = 0; result == URSATZ_OK && i < ids.count; i++) {
        if (ursatz_job_cancelled(job)) {
            result = URSATZ_ERR_CANCELLED;
            break;
        }
        const char *reason = NULL;
        if (add_piece(collected->profile, ids.items[i], &reason)) {
            if (!common_names_add(&collected->used, ids.items[i])) {
                result = workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
            }
        } else if (!styles_skipped_add(&collected->skipped, ids.items[i], reason)) {
            result = workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
        }
        ursatz_job_progress(job, "pieces", (unsigned)(i + 1), (unsigned)ids.count);
    }
    common_names_free(&ids);

    if (result == URSATZ_OK && collected->used.count == 0) {
        result = workshop_fail(URSATZ_ERR_UNSATISFIABLE, "no analyzable pieces");
    }
    if (result != URSATZ_OK) {
        styles_collected_free(collected);
    }
    return result;
}
