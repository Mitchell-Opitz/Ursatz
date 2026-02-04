#ifndef URSATZ_GUI_STYLE_SERVICE_H
#define URSATZ_GUI_STYLE_SERVICE_H

#include <stdbool.h>
#include <stddef.h>

#include <cJSON.h>

#include "link_service.h"

/*
 * Style service: builds a StyleModel file from a set of pieces through the
 * Analyzer's style profile (R-106).
 *
 * A style is a directory styles/<id>/ holding:
 *   style-model.json   written by style_model_file_write; the Composer reads it with --style
 *   meta.json          {"id", "name", "created_at", "updated_at",
 *                       "source": {"kind": "filter" | "collection" | "pieces" | "brief",
 *                                  "filter": {"composer", "source", "tag"} | null,
 *                                  "collection_id": "..." | null,
 *                                  "piece_ids": [...] | null,
 *                                  "brief": {"scopes": [...], "piece_ids": [...], "style_ids": [...]} | null},
 *                       "pieces": [ids used], "skipped": [{"id", "reason"}],
 *                       "piece_count": n}
 * Both files are written to "<file>.tmp" and renamed, the model first. Ids are
 * "sty-<hex>". civetweb serves requests on several threads, so every
 * operation here runs under one mutex; a build runs on the caller's thread.
 *
 * Pieces are read from pieces/<id>/ and collections from the collection
 * service. This module reads only analysis documents, never the scores.
 */

typedef enum {
    URSATZ_GUI_STYLE_OK = 0,
    URSATZ_GUI_STYLE_ERR_INVALID_NAME,   /* longer than 120 bytes after normalizing */
    URSATZ_GUI_STYLE_ERR_INVALID_SOURCE, /* no filter value, a bad tag, a bad id, or no ids */
    URSATZ_GUI_STYLE_ERR_NOT_FOUND,      /* no such style, or no such collection */
    URSATZ_GUI_STYLE_ERR_NO_PIECES,      /* none of the pieces could be added; nothing is written */
    URSATZ_GUI_STYLE_ERR_SOURCE_MISSING, /* rebuild: the style's collection no longer exists */
    URSATZ_GUI_STYLE_ERR_FAILED,
    URSATZ_GUI_STYLE_ERR_ALLOC
} UrsatzGuiStyleResult;

typedef enum {
    URSATZ_GUI_STYLE_SOURCE_FILTER = 0,
    URSATZ_GUI_STYLE_SOURCE_COLLECTION,
    URSATZ_GUI_STYLE_SOURCE_PIECES,
    URSATZ_GUI_STYLE_SOURCE_BRIEF
} UrsatzGuiStyleSourceKind;

#define URSATZ_GUI_STYLE_BRIEF_MAX_SCOPES 16
#define URSATZ_GUI_STYLE_BRIEF_MAX_PIECES 5000
#define URSATZ_GUI_STYLE_BRIEF_MAX_STYLES 16

/*
 * A brief (R-167): the union of link scopes, piece ids and styles. The pieces are, in id order without repeats,
 * every scope's pieces (ursatz_gui_link_scope_resolve; a scope that names no piece adds none), the piece ids, and
 * each style's pieces (its last build). At most 16 scopes, 5000 piece ids and 16 styles; at least one entry in all.
 */
typedef struct {
    UrsatzGuiLinkScope *scopes;
    size_t scope_count;
    char **piece_ids;
    size_t piece_count;
    char **style_ids;
    size_t style_count;
} UrsatzGuiStyleBrief;

/*
 * Where the pieces of a style come from.
 *   FILTER      the pieces that match composer, source and tag (a NULL or empty
 *               field matches any; at least one must be given), as the Library
 *               filters them
 *   COLLECTION  the pieces of the collection collection_id
 *   PIECES      the piece ids as given (repeated ids count once)
 *   BRIEF       the pieces of the brief; an unknown collection, subject, project or style is NOT_FOUND
 */
typedef struct {
    UrsatzGuiStyleSourceKind kind;
    const char *composer;
    const char *source;
    const char *tag;
    const char *collection_id;
    const char *const *piece_ids;
    size_t piece_count;
    const UrsatzGuiStyleBrief *brief;
} UrsatzGuiStyleSource;

/* A piece that was left out, with the reason: "not found", "re-analysis failed" or "unreadable analysis". */
typedef struct {
    char *id;
    char *reason;
} UrsatzGuiStyleSkipped;

void ursatz_gui_style_skipped_free(UrsatzGuiStyleSkipped *skipped, size_t count);

/*
 * Builds a style from the source's pieces. For each piece the stored
 * analysis.json is added to a style profile; when the piece's meta.json
 * document_schema_version is not the Analyzer's current one, the piece is
 * re-analyzed first (ursatz_gui_analysis_reanalyze). A piece that cannot be
 * added is skipped with a reason and the build goes on.
 *
 * name is normalized like piece metadata values; when empty it becomes a
 * default: "Composer: X", "Source: X", "Tag: x" joined with " · " for a
 * filter, "Collection: <name>" for a collection, "Selected pieces" for ids, "Brief" for a brief.
 *
 * On OK, *out_id is newly allocated and caller-owned, and *out_skipped
 * (may be NULL) receives the skipped pieces (free with
 * ursatz_gui_style_skipped_free). URSATZ_GUI_STYLE_ERR_NO_PIECES when no piece
 * could be added: nothing is written.
 */
UrsatzGuiStyleResult ursatz_gui_style_build(const char *name, const UrsatzGuiStyleSource *source, char **out_id,
                                            UrsatzGuiStyleSkipped **out_skipped, size_t *out_skipped_count);

/*
 * The same build into one model file at path (written to "<path>.tmp", then renamed), with no store entry (B8 builds
 * a theme version's brief with it). name is the model's name, normalized and defaulted as above. On OK, *out_pieces
 * (may be NULL) receives the ids used (free each with free, then the array) and *out_skipped the pieces left out.
 * The errors are those of ursatz_gui_style_build; nothing is written unless it returns OK.
 */
UrsatzGuiStyleResult ursatz_gui_style_build_file(const UrsatzGuiStyleSource *source, const char *name, const char *path,
                                                 char ***out_pieces, size_t *out_piece_count,
                                                 UrsatzGuiStyleSkipped **out_skipped, size_t *out_skipped_count);

/* One label of a category: how often it was seen and how many things the category counted. */
typedef struct {
    char *label;
    unsigned long count;
    unsigned long total;
} UrsatzGuiStyleLabel;

/* A category of a style's statistics: its labels by count descending (then label), at most 12, and how many more there are. */
typedef struct {
    char *dimension; /* "harmony", "rhythm", "melody" or "form" */
    char *category;
    UrsatzGuiStyleLabel *labels;
    size_t label_count;
    size_t more;
} UrsatzGuiStyleCategory;

/*
 * A stored style. Strings are "" when unset. filter_* are set for a filter
 * source, collection_id for a collection source, source_piece_ids for a
 * pieces source. pieces are the ids used by the last build, skipped the ones
 * left out. categories is only filled by ursatz_gui_style_get, sorted by
 * dimension (harmony, rhythm, melody, form) then category.
 */
typedef struct {
    char *id;
    char *name;
    char *created_at;
    char *updated_at;
    UrsatzGuiStyleSourceKind source_kind;
    char *filter_composer;
    char *filter_source;
    char *filter_tag;
    char *collection_id;
    char **source_piece_ids;
    size_t source_piece_count;
    UrsatzGuiStyleBrief brief; /* for a brief source; owns its strings */
    char **pieces;
    size_t piece_count;
    UrsatzGuiStyleSkipped *skipped;
    size_t skipped_count;
    UrsatzGuiStyleCategory *categories;
    size_t category_count;
} UrsatzGuiStyle;

/*
 * Lists every style, newest update first (then id). *out_styles has *out_count
 * entries (NULL when none) without statistics; free with
 * ursatz_gui_style_free_list. A directory in styles/ without a readable
 * meta.json is not a style.
 */
UrsatzGuiStyleResult ursatz_gui_style_list(UrsatzGuiStyle **out_styles, size_t *out_count);

void ursatz_gui_style_free_list(UrsatzGuiStyle *styles, size_t count);

/*
 * Loads one style with its statistics (style_model_file_read, then
 * style_model_statistics). NOT_FOUND if absent; FAILED if the model file
 * cannot be read. Free with ursatz_gui_style_destroy.
 */
UrsatzGuiStyleResult ursatz_gui_style_get(const char *id, UrsatzGuiStyle **out_style);

void ursatz_gui_style_destroy(UrsatzGuiStyle *style);

/*
 * Reads a brief object {"scopes": [...], "piece_ids": [...], "style_ids": [...]} as the styles route takes it, with
 * the limits above, into out, which owns its strings (free with ursatz_gui_style_brief_free). Returns NULL, or a failure
 * message ("invalid brief", "invalid scope", "too many entries in the brief", "out of memory") with out left empty.
 */
const char *ursatz_gui_style_brief_parse(const cJSON *json, UrsatzGuiStyleBrief *out);

/* Frees the scopes, ids and style ids of a brief (the strings are owned only for a brief a style holds). */
void ursatz_gui_style_brief_free(UrsatzGuiStyleBrief *brief);

/*
 * Builds the style again from its stored source over today's pieces and
 * replaces its two files in place; id and name stay, updated_at changes, so a
 * plan that names the file uses the new counts. NOT_FOUND if the style is
 * absent, SOURCE_MISSING if its collection, subject, project or style is gone, NO_PIECES if no piece
 * could be added (the style is left as it was).
 */
UrsatzGuiStyleResult ursatz_gui_style_rebuild(const char *id);

/* Deletes the style's directory. NOT_FOUND if absent. */
UrsatzGuiStyleResult ursatz_gui_style_delete(const char *id);

/*
 * Writes the absolute path of the style's style-model.json to buffer (for
 * the Composer's --style). NOT_FOUND if the style or the file is absent;
 * FAILED if the path does not fit.
 */
UrsatzGuiStyleResult ursatz_gui_style_model_path(const char *id, char *buffer, size_t buffer_size);

#endif /* URSATZ_GUI_STYLE_SERVICE_H */
