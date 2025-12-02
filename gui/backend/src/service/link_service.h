#ifndef URSATZ_GUI_LINK_SERVICE_H
#define URSATZ_GUI_LINK_SERVICE_H

#include <stdbool.h>
#include <stddef.h>

#include <cJSON.h>

/*
 * Link service: links the motive families of a set of Library pieces through the Analyzer's motive links (R-154,
 * R-156). This module is the only caller of ursatz_analyzer/motive_links.h.
 *
 * A scope names the pieces:
 *   ALL         every Library piece
 *   COLLECTION  the pieces of the collection collection_id
 *   FILTER      the pieces that match composer, source and tag (a NULL or empty field matches any; at least one must
 *               be given), as the Library filters them
 *   SUBJECT     the Library pieces the subject subject_id links
 *   PROJECT     the Library pieces linked to every subject that links at least one item of the project project_id
 * Pieces are in Library order (id order). Reads pieces from pieces/<id>/, collections, subjects and materials from
 * their services.
 */

typedef enum {
    URSATZ_GUI_LINK_OK = 0,
    URSATZ_GUI_LINK_ERR_INVALID_SCOPE, /* an unknown kind, a missing or bad id, or an empty filter */
    URSATZ_GUI_LINK_ERR_NOT_FOUND,     /* no such collection, subject, project or run */
    URSATZ_GUI_LINK_ERR_EMPTY,         /* the scope names no piece */
    URSATZ_GUI_LINK_ERR_BUSY,          /* a run is in progress */
    URSATZ_GUI_LINK_ERR_FAILED,
    URSATZ_GUI_LINK_ERR_ALLOC
} UrsatzGuiLinkResult;

typedef enum {
    URSATZ_GUI_LINK_SCOPE_ALL = 0,
    URSATZ_GUI_LINK_SCOPE_COLLECTION,
    URSATZ_GUI_LINK_SCOPE_FILTER,
    URSATZ_GUI_LINK_SCOPE_SUBJECT,
    URSATZ_GUI_LINK_SCOPE_PROJECT
} UrsatzGuiLinkScopeKind;

/* Fields the kind does not use are ignored; NULL is the same as "". */
typedef struct {
    UrsatzGuiLinkScopeKind kind;
    const char *collection_id;
    const char *composer;
    const char *source;
    const char *tag;
    const char *subject_id;
    const char *project_id;
} UrsatzGuiLinkScope;

/*
 * The piece ids of the scope, in id order, without repeats. On OK, *out_ids has *out_count entries (at least one);
 * free with ursatz_gui_link_ids_free. ERR_EMPTY when the scope names no piece. An id a collection or subject keeps
 * for a piece that no longer exists is returned as is: a run skips it with the reason "not found".
 */
UrsatzGuiLinkResult ursatz_gui_link_scope_resolve(const UrsatzGuiLinkScope *scope, char ***out_ids, size_t *out_count);

void ursatz_gui_link_ids_free(char **ids, size_t count);

/*
 * Runs (R-156). A run links the families of a scope's pieces on a worker thread and is kept as links/runs/<id>/run.json
 * (written to "<file>.tmp" and renamed):
 *   {"id": "lnk-<hex>", "scope": {"kind", "collection_id", "filter": {"composer", "source", "tag"}, "subject_id",
 *    "project_id"} (members the kind does not use are null), "created_at", "finished_at" (null while running),
 *    "status": "running" | "done" | "failed", "message", "done", "total", "skipped": [{"id", "reason"}],
 *    and on done "result": the Analyzer's motive_links_render output}
 * One cache, links/motive-link-cache.json, serves every scope. One run at a time.
 */

/* Prepares the store in dir (created if absent). A run left "running" by an earlier process is marked failed with
 * "interrupted". Returns false when the store cannot be used. */
bool ursatz_gui_link_service_init(const char *dir);

/* Waits for a run in progress to end, then forgets the store's directory. */
void ursatz_gui_link_service_shutdown(void);

/*
 * Starts a run over the scope on a worker thread. On OK, *out_id (may be NULL) is the new run's id, caller-owned. The
 * scope is resolved first: INVALID_SCOPE, NOT_FOUND, EMPTY as ursatz_gui_link_scope_resolve; BUSY while another run
 * is in progress.
 */
UrsatzGuiLinkResult ursatz_gui_link_run_start(const UrsatzGuiLinkScope *scope, char **out_id);

/*
 * Lists every run, newest first (then id), as {id, scope, status, created_at, finished_at, done, total} objects.
 * *out_runs is a caller-owned array (free with cJSON_Delete). A directory without a readable run.json is not a run.
 */
UrsatzGuiLinkResult ursatz_gui_link_run_list(cJSON **out_runs);

/* The whole run.json, with the live done and total while it runs. NOT_FOUND if absent. Caller-owned. */
UrsatzGuiLinkResult ursatz_gui_link_run_get(const char *id, cJSON **out_run);

/* Deletes the run. NOT_FOUND if absent, BUSY while it runs. */
UrsatzGuiLinkResult ursatz_gui_link_run_delete(const char *id);

/*
 * The links with this piece on either side, from the newest done run of each scope: an array of {"run": id,
 * "scope": {...}, "link": {...}} objects, newest run first. Caller-owned; an empty array when there are none.
 */
UrsatzGuiLinkResult ursatz_gui_link_piece_links(const char *piece_id, cJSON **out_links);

/* The title a piece is shown with: its filename without the extension (as the Library shows it), or the id when the
 * piece is gone. Newly allocated, or NULL when memory ran out. */
char *ursatz_gui_link_piece_title(const char *piece_id);

#endif /* URSATZ_GUI_LINK_SERVICE_H */
