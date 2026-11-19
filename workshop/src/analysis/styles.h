#ifndef WORKSHOP_STYLE_SERVICE_H
#define WORKSHOP_STYLE_SERVICE_H

#include <common/job.h>
#include <common/status.h>

#include <stdbool.h>
#include <stddef.h>

#include <cJSON.h>

#include "links.h"
#include "base/last_error.h"

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
 * "sty-<hex>". civetweb serves requests on several threads, so the reads and the commit of a style run under one mutex; a build
 * runs on the caller's thread (the job queue's, for the GUI).
 *
 * Pieces are read from pieces/<id>/ and collections from the collection
 * service. This module reads only analysis documents, never the scores.
 */

typedef enum {
    WORKSHOP_STYLE_SOURCE_FILTER = 0,
    WORKSHOP_STYLE_SOURCE_COLLECTION,
    WORKSHOP_STYLE_SOURCE_PIECES,
    WORKSHOP_STYLE_SOURCE_BRIEF
} WorkshopStyleSourceKind;

#define WORKSHOP_STYLE_BRIEF_MAX_SCOPES 16
#define WORKSHOP_STYLE_BRIEF_MAX_PIECES 5000
#define WORKSHOP_STYLE_BRIEF_MAX_STYLES 16

/*
 * A brief (R-167): the union of link scopes, piece ids and styles. The pieces are, in id order without repeats,
 * every scope's pieces (workshop_link_scope_resolve; a scope that names no piece adds none), the piece ids, and
 * each style's pieces (its last build). At most 16 scopes, 5000 piece ids and 16 styles; at least one entry in all.
 */
typedef struct {
    WorkshopLinkScope *scopes;
    size_t scope_count;
    char **piece_ids;
    size_t piece_count;
    char **style_ids;
    size_t style_count;
} WorkshopStyleBrief;

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
    WorkshopStyleSourceKind kind;
    const char *composer;
    const char *source;
    const char *tag;
    const char *collection_id;
    const char *const *piece_ids;
    size_t piece_count;
    const WorkshopStyleBrief *brief;
} WorkshopStyleSource;

/* A piece that was left out, with the reason: "not found", "re-analysis failed" or "unreadable analysis". */
typedef struct {
    char *id;
    char *reason;
} WorkshopStyleSkipped;

void workshop_style_skipped_free(WorkshopStyleSkipped *skipped, size_t count);

/*
 * Builds a style from the source's pieces. For each piece the stored
 * analysis.json is added to a style profile; when the stored analysis is not
 * current (analyzer_document_status), the piece is re-analyzed first
 * (workshop_analysis_ensure_current). A piece that cannot be
 * added is skipped with a reason and the build goes on.
 *
 * name is normalized like piece metadata values; when empty it becomes a
 * default: "Composer: X", "Source: X", "Tag: x" joined with " · " for a
 * filter, "Collection: <name>" for a collection, "Selected pieces" for ids, "Brief" for a brief.
 *
 * job (NULL for none) is the progress and cancel handle (R-645): it is told "pieces d/t" after each piece and asked between pieces,
 * and the build stops with URSATZ_ERR_CANCELLED, having written nothing. The pieces are read holding no store lock; the style is put in
 * place by one transaction.
 *
 * On OK, *out_id is newly allocated and caller-owned, and *out_skipped
 * (may be NULL) receives the skipped pieces (free with
 * workshop_style_skipped_free). URSATZ_ERR_UNSATISFIABLE when no piece
 * could be added: nothing is written.
 */
UrsatzStatus workshop_style_build(const char *name, const WorkshopStyleSource *source, const UrsatzJob *job, char **out_id,
                                            WorkshopStyleSkipped **out_skipped, size_t *out_skipped_count);

/*
 * The same build into one model file at path (written to "<path>.tmp", then renamed), with no store entry (B8 builds
 * a theme version's brief with it). name is the model's name, normalized and defaulted as above. On OK, *out_pieces
 * (may be NULL) receives the ids used (free each with free, then the array) and *out_skipped the pieces left out.
 * The errors are those of workshop_style_build; nothing is written unless it returns OK.
 */
UrsatzStatus workshop_style_build_file(const WorkshopStyleSource *source, const char *name, const char *path,
                                                 char ***out_pieces, size_t *out_piece_count,
                                                 WorkshopStyleSkipped **out_skipped, size_t *out_skipped_count);

/*
 * Lists every style, newest update first (then id), as JSON entries {"id", "name", "piece_count", "source", "updated_at"} in *out_styles
 * (an array, empty when none), made from each style's meta.json. A directory in styles/ without a readable meta.json is not a style.
 */
UrsatzStatus workshop_style_list(cJSON **out_styles);

/*
 * One style with its statistics: the stored meta.json tree (id, name, created_at, updated_at, source, pieces, skipped, piece_count) plus
 * "statistics": {dimension: {category: {"labels": [{"label", "count", "total"}], "more": n}}} (the labels by count, at most 12 a
 * category; style_model_file_read, then style_model_statistics). NOT_FOUND if absent; INTERNAL if the model file cannot be read.
 */
UrsatzStatus workshop_style_get(const char *id, cJSON **out_style);

/* The list entry of a style (as workshop_style_list gives it) in *out_entry and its skipped pieces ([{"id", "reason"}]) in *out_skipped:
 * the answer of a build or a rebuild. NOT_FOUND if absent. */
UrsatzStatus workshop_style_get_entry(const char *id, cJSON **out_entry, cJSON **out_skipped);

/*
 * A style build request as the styles route takes it: {name?, and exactly one of filter, collection_id, piece_ids, brief}. The source
 * points into the request body and holds the strings the parse made (release with workshop_style_request_free). REQUEST with the message
 * ("invalid name", "give exactly one of ...", "invalid filter", "invalid collection_id", "invalid piece_ids", "too many pieces", the brief
 * messages) when it is not one; ALLOC on no memory.
 */
typedef struct {
    WorkshopStyleSource source;
    WorkshopStyleBrief brief;
    const char **ids;
    const char *name; /* NULL when not given */
} WorkshopStyleRequest;

UrsatzStatus workshop_style_build_request_parse(const cJSON *body, WorkshopStyleRequest *request, UrsatzError *error);
void workshop_style_request_free(WorkshopStyleRequest *request);

/*
 * Reads a brief object {"scopes": [...], "piece_ids": [...], "style_ids": [...]} as the styles route takes it, with
 * the limits above, into out, which owns its strings (free with workshop_style_brief_free). Returns NULL, or a failure
 * message ("invalid brief", "invalid scope", "too many entries in the brief", "out of memory") with out left empty.
 */
const char *workshop_style_brief_parse(const cJSON *json, WorkshopStyleBrief *out);

/* Frees the scopes, ids and style ids of a brief (the strings are owned only for a brief a style holds). */
void workshop_style_brief_free(WorkshopStyleBrief *brief);

/*
 * Builds the style again from its stored source over today's pieces and
 * replaces its two files in place; id and name stay, updated_at changes, so a
 * plan that names the file uses the new counts. NOT_FOUND if the style is
 * absent, SOURCE_MISSING if its collection, subject, project or style is gone, NO_PIECES if no piece
 * could be added (the style is left as it was). job as for workshop_style_build.
 */
UrsatzStatus workshop_style_rebuild(const char *id, const UrsatzJob *job);

/* Deletes the style's directory. NOT_FOUND if absent. */
UrsatzStatus workshop_style_delete(const char *id);

/*
 * Writes the absolute path of the style's style-model.json to buffer (for
 * the Composer's --style). NOT_FOUND if the style or the file is absent;
 * FAILED if the path does not fit.
 */
UrsatzStatus workshop_style_model_path(const char *id, char *buffer, size_t buffer_size);

#endif /* WORKSHOP_STYLE_SERVICE_H */
