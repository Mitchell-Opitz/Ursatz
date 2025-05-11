#ifndef URSATZ_GUI_STYLE_SERVICE_H
#define URSATZ_GUI_STYLE_SERVICE_H

#include <stdbool.h>
#include <stddef.h>

/*
 * Style service: builds a StyleModel file from a set of pieces through the
 * Analyzer's style profile (R-106).
 *
 * A style is a directory styles/<id>/ holding:
 *   style-model.json   written by style_model_file_write; the Composer reads it with --style
 *   meta.json          {"id", "name", "created_at", "updated_at",
 *                       "source": {"kind": "filter" | "collection" | "pieces",
 *                                  "filter": {"composer", "source", "tag"} | null,
 *                                  "collection_id": "..." | null,
 *                                  "piece_ids": [...] | null},
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
    URSATZ_GUI_STYLE_ERR_FAILED,
    URSATZ_GUI_STYLE_ERR_ALLOC
} UrsatzGuiStyleResult;

typedef enum {
    URSATZ_GUI_STYLE_SOURCE_FILTER = 0,
    URSATZ_GUI_STYLE_SOURCE_COLLECTION,
    URSATZ_GUI_STYLE_SOURCE_PIECES
} UrsatzGuiStyleSourceKind;

/*
 * Where the pieces of a style come from.
 *   FILTER      the pieces that match composer, source and tag (a NULL or empty
 *               field matches any; at least one must be given), as the Library
 *               filters them
 *   COLLECTION  the pieces of the collection collection_id
 *   PIECES      the piece ids as given (repeated ids count once)
 */
typedef struct {
    UrsatzGuiStyleSourceKind kind;
    const char *composer;
    const char *source;
    const char *tag;
    const char *collection_id;
    const char *const *piece_ids;
    size_t piece_count;
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
 * filter, "Collection: <name>" for a collection, "Selected pieces" for ids.
 *
 * On OK, *out_id is newly allocated and caller-owned, and *out_skipped
 * (may be NULL) receives the skipped pieces (free with
 * ursatz_gui_style_skipped_free). URSATZ_GUI_STYLE_ERR_NO_PIECES when no piece
 * could be added: nothing is written.
 */
UrsatzGuiStyleResult ursatz_gui_style_build(const char *name, const UrsatzGuiStyleSource *source, char **out_id,
                                            UrsatzGuiStyleSkipped **out_skipped, size_t *out_skipped_count);

#endif /* URSATZ_GUI_STYLE_SERVICE_H */
