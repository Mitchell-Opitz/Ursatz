#ifndef WORKSHOP_ITEMS_LIBRARY_PIECES_H
#define WORKSHOP_ITEMS_LIBRARY_PIECES_H

#include <common/status.h>

#include <stdbool.h>
#include <stddef.h>

#include <cJSON.h>

#include "items/txn.h"

#include <ursatz_analyzer/analyzer_api.h>

/*
 * The Library piece store: the pieces a person uploaded, each a directory pieces_dir/<id>/ holding
 *   source.mid | source.musicxml | source.xml | source.mxl   the upload, unchanged
 *   analysis.json                                            the Analyzer's document (written by analysis.c)
 *   meta.json                                                filename, format, uploaded_at,
 *                                                            analyzer_version, document_schema_version,
 *                                                            ursatz_commit,
 *                                                            composer, source, tags (R-103; absent = unset),
 *                                                            notation (R-110; absent = no override)
 * This file reads and writes the piece folder and meta.json and lists the pieces; it never runs the Analyzer on a source (the
 * analysis service does, and keeps the import cache of a piece it changes). pieces_dir is created if it does not already exist.
 */

/*
 * A piece's notation override (R-110): how its notes are to be read as notation.
 * Each member is the text the Analyzer's imported_reading_parse_* reads, or ""
 * for "not set": meter "3/4", value_scale "2" (1/4, 1/2, 1, 2 or 4), pickup
 * "1/4" (whole notes, after scaling).
 */
typedef struct {
    char meter[16];
    char value_scale[8];
    char pickup[16];
} WorkshopNotation;

/* One entry in workshop_library_list's result. */
typedef struct {
    char *id;
    char *filename;
    char *format;
    char *uploaded_at;
    char *composer;    /* "" when unset */
    char *source;      /* "" when unset */
    char **tags;       /* tag_count normalized tags in insertion order; NULL when none */
    size_t tag_count;
    bool has_notation; /* true when the piece has a notation override */
    WorkshopNotation notation;
    const char *analysis_status; /* from meta.json alone: "current", "stale_schema", "stale_analyzer",
                                    "stale_ursatz" or "unreadable"; static text */
} WorkshopPieceSummary;


/*
 * Lists the pieces in pieces_dir: every pieces_dir/<id>/ directory with a
 * readable meta.json, newest upload first, then by id. Anything else in
 * pieces_dir (old "<id>.db" pieces, stray files) is ignored. On
 * URSATZ_OK, *out_pieces is a newly allocated array of
 * *out_count entries (caller frees each entry's strings, then the array).
 * A missing or empty pieces_dir yields *out_count == 0, *out_pieces == NULL.
 */
UrsatzStatus workshop_library_list(const char *pieces_dir, WorkshopPieceSummary **out_pieces,
                                                  size_t *out_count);

/*
 * True when id names a piece of pieces_dir that has its stored metadata (the piece exists as list shows it).
 */
bool workshop_library_piece_exists(const char *pieces_dir, const char *id);

/*
 * The display name (filename) of a piece, under the piece lock: *out_filename is caller-owned (free()), "" when the
 * piece has none. URSATZ_ERR_NOT_FOUND if no piece with this id exists or its metadata is unreadable.
 */
UrsatzStatus workshop_library_get_filename(const char *pieces_dir, const char *id, char **out_filename);

/*
 * Reads the piece's notation override from its meta.json: *out_has is false
 * (and *out is zeroed) when it has none.
 * URSATZ_ERR_NOT_FOUND if no piece with this id exists.
 */
UrsatzStatus workshop_library_get_notation(const char *pieces_dir, const char *id, bool *out_has,
                                                          WorkshopNotation *out);


/*
 * A change of a piece's metadata in its meta.json (workshop_analysis_update_piece, workshop_library_update_unlocked), written once. The source and
 * the analysis are untouched. NULL string fields are left unchanged.
 *
 *   filename            display name, stored as given (the caller rejects an empty one)
 *   composer, source    normalized (workshop_meta_normalize_value); "" clears
 *   has_tags, tags      replace all tags (each normalized with workshop_meta_normalize_tag)
 *   add_tags            then added (a duplicate is skipped)
 *   remove_tags         then removed (absent is not an error)
 *   has_notation        replace the notation override with *notation, or clear it
 *                       when notation is NULL
 *
 * URSATZ_ERR_NOT_FOUND if no piece with this id exists.
 * URSATZ_ERR_REQUEST if a value is too long, a tag is
 * invalid, the piece would hold more than 32 tags or a notation member is not
 * one the Analyzer reads; nothing is written and message (may be NULL)
 * receives "composer is too long", "source is too long", "invalid tag",
 * "too many tags" or "invalid notation".
 */
typedef struct {
    const char *filename;
    const char *composer;
    const char *source;
    bool has_tags;
    const char *const *tags;
    size_t tag_count;
    const char *const *add_tags;
    size_t add_tag_count;
    const char *const *remove_tags;
    size_t remove_tag_count;
    bool has_notation;
    const WorkshopNotation *notation;
} WorkshopMetaUpdate;


/* NULL string fields match any value; every field given must match (workshop_meta_matches). The Library route fills it from its query
 * parameters. */
typedef struct {
    const char *composer;
    const char *source;
    const char *tag;
    bool composer_unassigned; /* match an empty composer */
    bool source_unassigned;   /* match an empty source */
    bool untagged;            /* match a piece without tags */
} WorkshopPieceFilter;

/* The pieces of pieces_dir that match filter (NULL: all) as the Library list shows them: *out is a JSON array of {id, filename, format,
 * uploaded_at, composer, source, tags, notation, analysis_status}. */
UrsatzStatus workshop_library_list_json(const char *pieces_dir, const WorkshopPieceFilter *filter, cJSON **out);

/* The distinct composers, sources and tags with piece counts: {composers: [{value, count}], sources, tags, unassigned: {composer,
 * source}, untagged, total} in *out. */
UrsatzStatus workshop_library_groups_json(const char *pieces_dir, cJSON **out);

/* One piece as the list shows it, in *out (JSON null when the piece is not in the list). */
UrsatzStatus workshop_library_entry_json(const char *pieces_dir, const char *id, cJSON **out);

/* A notation override as JSON: null when the piece has none. */
cJSON *workshop_library_notation_json(bool has_notation, const WorkshopNotation *notation);

/* The members of a PATCH body (filename, composer, source, tags, add_tags, remove_tags, notation) as a WorkshopMetaUpdate. The update's
 * lists and notation point into the patch and its body, which must outlive it; release it with workshop_library_patch_free. URSATZ_ERR_REQUEST
 * with "invalid <member>" in error for a member of the wrong type (filename: also when empty); URSATZ_ERR_UNSATISFIABLE with "nothing to
 * update" when no member is given. Unknown members are ignored. */
typedef struct {
    WorkshopMetaUpdate update;
    WorkshopNotation notation;
    const char **tags;
    const char **add_tags;
    const char **remove_tags;
} WorkshopLibraryPatch;

UrsatzStatus workshop_library_patch_parse(const cJSON *body, WorkshopLibraryPatch *patch, UrsatzError *error);
void workshop_library_patch_free(WorkshopLibraryPatch *patch);

/* ---- what the analysis service builds on ---------------------------------- */

/* The name a status goes by in the routes and the stored flags. */
const char *workshop_library_status_name(AnalyzerDocumentStatus status);

/* Writes <dir_path>/piece.json. */
bool workshop_library_meta_write_new(const char *dir_path, const char *filename, const char *format, const char *uploaded_at);

/* Reads <dir_path>/piece.json into the shape the services hold (the source's format as "format", the analysis key as
 * "ursatz_commit"); NULL when it is missing or is not a piece file. */
cJSON *workshop_library_meta_read(const char *dir_path);

/* Sets the transaction the Library writers of this thread stage into (NULL: none, a write goes straight to its file). */
void workshop_library_set_txn(WorkshopTxn *txn);

/* Writes a file of a piece: staged in the thread's transaction, or atomically in place when there is none. */
bool workshop_library_write_file(const char *path, const void *data, size_t size);

/* Writes meta (in that shape) as <dir_path>/piece.json: staged in the thread's transaction or atomically in place. */
bool workshop_library_meta_write(const char *dir_path, const cJSON *meta);

/* A copy of a string member of meta, "" when it is absent. */
char *workshop_library_meta_string_copy(const cJSON *meta, const char *key);

/* Copies meta's "notation" object. False (and *out zeroed) when it is absent, null or not an object. */
bool workshop_library_meta_notation(const cJSON *meta, WorkshopNotation *out);

const char *workshop_library_source_name(ImportedFormat format, const char *original_filename);

UrsatzStatus workshop_library_piece_create(const char *pieces_dir, char **out_id);

/* pieces_dir/<id> when id is valid and that directory exists. */
bool workshop_library_piece_dir(const char *pieces_dir, const char *id, char *out, size_t out_size);

/* Writes the path of the piece's stored source file into out. */
bool workshop_library_source_path(const char *dir_path, char *out, size_t out_size);

UrsatzStatus workshop_library_delete_piece_unlocked(const char *pieces_dir, const char *id);

bool workshop_library_meta_set_string(cJSON *meta, const char *key, const char *value);

bool workshop_library_meta_set_number(cJSON *meta, const char *key, double value);

UrsatzStatus workshop_library_update_metadata_unlocked(const char *pieces_dir, const char *id, const WorkshopMetaUpdate *update, UrsatzError *error);

bool workshop_library_piece_lock_enter(const char *id, UrsatzStatus *failure);

#endif
