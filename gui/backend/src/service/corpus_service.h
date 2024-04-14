#ifndef URSATZ_GUI_CORPUS_SERVICE_H
#define URSATZ_GUI_CORPUS_SERVICE_H

#include <stdbool.h>
#include <stddef.h>

/*
 * Shared corpus service: the only place outside persistence_service.c/
 * analysis_service.c allowed to call into Ursatz's corpus_api.h. It wraps
 * corpus_api's opaque CorpusStoreHandle into operations the Corpus tool's
 * route code can call directly, per the boundary tool.h/README describe.
 *
 * Unlike analysis_service.c (which opens a fresh per-piece SQLite file for
 * every call), corpus_api works against a single shared store: this module
 * opens that store once via ursatz_gui_corpus_service_init (called from
 * server.c at startup) and keeps the handle for the life of the process,
 * rather than opening/closing it per request.
 *
 * No behavior beyond corpus_api is added here: create/list Corpora and
 * add/remove/list piece refs fail exactly when corpus_api does, and its
 * failure messages are surfaced as-is (including a generic message for a
 * duplicate-id create -- corpus_api has no distinct "already exists" result
 * for that case today, and manufacturing a precise one would mean reaching
 * into corpus.h/persistence_corpus.h internals, which is out of scope here).
 */

typedef enum {
    URSATZ_GUI_CORPUS_OK = 0,
    URSATZ_GUI_CORPUS_ERR_INVALID_ID,
    URSATZ_GUI_CORPUS_ERR_INVALID_NAME,
    URSATZ_GUI_CORPUS_ERR_ALREADY_MEMBER,
    URSATZ_GUI_CORPUS_ERR_NOT_MEMBER,
    URSATZ_GUI_CORPUS_ERR_FAILED,
    URSATZ_GUI_CORPUS_ERR_ALLOC,
    URSATZ_GUI_CORPUS_ERR_NOT_INITIALIZED
} UrsatzGuiCorpusResult;

typedef struct {
    char *id;
    char *name;
} UrsatzGuiCorpusEntry;

/*
 * Opens (creating if absent) the single shared Corpus store at db_path.
 * Must be called once at server startup before any other function in this
 * module is used. Returns false on failure (mirrors corpus_api_open's
 * fail-fast contract; no handle is retained).
 */
bool ursatz_gui_corpus_service_init(const char *db_path);

/* Closes the store opened by ursatz_gui_corpus_service_init, if any. */
void ursatz_gui_corpus_service_shutdown(void);

/*
 * Validates id as a plain path-segment component: non-empty, and every
 * character alnum, '-', or '_'. Used to check both Corpus ids and piece
 * ref ids taken from HTTP path segments before they reach corpus_api,
 * mirroring analysis_service.c's is_valid_piece_id -- kept as its own
 * copy here rather than shared, since promoting that check to a common
 * header is a cross-module change outside this branch's scope.
 */
bool ursatz_gui_corpus_is_valid_id(const char *id);

/* Creates a Corpus with the given id and name. */
UrsatzGuiCorpusResult ursatz_gui_corpus_create(const char *id, const char *name);

/*
 * Lists every stored Corpus. On URSATZ_GUI_CORPUS_OK, *out_entries is a
 * newly allocated array of *out_count entries (caller frees each entry's
 * id/name, then the array via ursatz_gui_corpus_free_entries); *out_count
 * may be 0 with *out_entries left as NULL.
 */
UrsatzGuiCorpusResult ursatz_gui_corpus_list(UrsatzGuiCorpusEntry **out_entries, size_t *out_count);

void ursatz_gui_corpus_free_entries(UrsatzGuiCorpusEntry *entries, size_t count);

/* Adds/removes a piece ref to/from the named Corpus. Fail-fast, as corpus_api
 * itself is: adding a ref already present, or removing one not present, is
 * an error. */
UrsatzGuiCorpusResult ursatz_gui_corpus_add_piece(const char *corpus_id, const char *piece_id);
UrsatzGuiCorpusResult ursatz_gui_corpus_remove_piece(const char *corpus_id, const char *piece_id);

/*
 * Lists every piece ref in the named Corpus. On URSATZ_GUI_CORPUS_OK,
 * *out_ids is a newly allocated array of *out_count newly allocated id
 * strings (caller frees each string, then the array); *out_count may be 0
 * with *out_ids left as NULL.
 */
UrsatzGuiCorpusResult ursatz_gui_corpus_list_pieces(const char *corpus_id, char ***out_ids, size_t *out_count);

#endif /* URSATZ_GUI_CORPUS_SERVICE_H */
