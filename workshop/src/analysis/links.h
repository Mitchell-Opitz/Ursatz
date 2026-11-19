#ifndef WORKSHOP_LINK_SERVICE_H
#define WORKSHOP_LINK_SERVICE_H

#include <common/status.h>

#include <stdbool.h>
#include <stddef.h>

#include <cJSON.h>

#include "jobs/job_queue.h"

/*
 * Link service: links the motive families of a set of Library pieces through the Analyzer's motive links (R-154,
 * R-156). This module is the only caller of ursatz_analyzer/analyzer_api.h.
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
    WORKSHOP_LINK_SCOPE_ALL = 0,
    WORKSHOP_LINK_SCOPE_COLLECTION,
    WORKSHOP_LINK_SCOPE_FILTER,
    WORKSHOP_LINK_SCOPE_SUBJECT,
    WORKSHOP_LINK_SCOPE_PROJECT
} WorkshopLinkScopeKind;

/* Fields the kind does not use are ignored; NULL is the same as "". */
typedef struct {
    WorkshopLinkScopeKind kind;
    const char *collection_id;
    const char *composer;
    const char *source;
    const char *tag;
    const char *subject_id;
    const char *project_id;
} WorkshopLinkScope;

/*
 * The piece ids of the scope, in id order, without repeats. On OK, *out_ids has *out_count entries (at least one);
 * free with workshop_link_ids_free. ERR_EMPTY when the scope names no piece. An id a collection or subject keeps
 * for a piece that no longer exists is returned as is: a run skips it with the reason "not found".
 */
UrsatzStatus workshop_link_scope_resolve(const WorkshopLinkScope *scope, char ***out_ids, size_t *out_count);

void workshop_link_ids_free(char **ids, size_t count);

/*
 * Runs (R-156). A run links the families of a scope's pieces on a worker thread and is kept as links/runs/<id>/run.json
 * (written to "<file>.tmp" and renamed):
 *   {"id": "lnk-<hex>", "scope": {"kind", "collection_id", "filter": {"composer", "source", "tag"}, "subject_id",
 *    "project_id"} (members the kind does not use are null), "created_at", "finished_at" (null while running),
 *    "status": "running" | "done" | "failed", "message", "done", "total", "skipped": [{"id", "reason"}],
 *    and on done "result": the Analyzer's analyzer_links_render output}
 * One cache, links/motive-link-cache.json, serves every scope. One run at a time.
 */

/* Prepares the store in dir (created if absent). A run left "running" by an earlier process is marked failed with
 * "interrupted". Returns false when the store cannot be used. */
bool workshop_link_service_init(void);

/* Waits for a run in progress to end, then forgets the store's directory. */
void workshop_link_service_shutdown(void);

/*
 * Queues a run over the scope on the job queue (kind "links", one at a time: a second run waits, R-645). Its record is committed with the
 * status "queued" (a transaction, links.start) before the job is submitted; the job writes the run as it ends in another. On OK, *out_id (may be NULL) is
 * the new run's id, caller-owned, and out_job (may be NULL) the id of its job. The scope is resolved first: INVALID_SCOPE, NOT_FOUND, EMPTY as
 * workshop_link_scope_resolve.
 */
UrsatzStatus workshop_link_run_start(const WorkshopLinkScope *scope, char **out_id, char out_job[WORKSHOP_JOB_ID_BYTES]);

/*
 * Lists every run, newest first (then id), as {id, scope, status, created_at, finished_at, done, total} objects.
 * *out_runs is a caller-owned array (free with cJSON_Delete). A directory without a readable run.json is not a run.
 */
UrsatzStatus workshop_link_run_list(cJSON **out_runs);

/* The whole run.json, with the live done and total while it runs. NOT_FOUND if absent. Caller-owned. */
UrsatzStatus workshop_link_run_get(const char *id, cJSON **out_run);

/* Deletes the run. NOT_FOUND if absent, BUSY while it runs. */
UrsatzStatus workshop_link_run_delete(const char *id);

/*
 * The links with this piece on either side, from the newest done run of each scope: an array of {"run": id,
 * "scope": {...}, "link": {...}} objects, newest run first. Caller-owned; an empty array when there are none.
 */
UrsatzStatus workshop_link_piece_links(const char *piece_id, cJSON **out_links);

/* The title a piece is shown with: its filename without the extension (as the Library shows it), or the id when the
 * piece is gone. Newly allocated, or NULL when memory ran out. */
char *workshop_link_piece_title(const char *piece_id);

/* ---- the links routes' scope and titles (A20261109-47) ---- */

/* Reads a request's scope {"kind", "collection_id", "filter": {composer, source, tag}, "subject_id", "project_id"}; the strings point into
 * json. false when it cannot be a scope. */
bool workshop_link_scope_parse(const cJSON *json, WorkshopLinkScope *scope);

/* Adds "title" (the piece's filename) to every link end and every group member of a run's "result"; false on no memory. */
bool workshop_link_titles_for_run(cJSON *run);

/* Adds the titles to each end of each entry's "link" of a piece's links ([{"link": {"a", "b", ...}, ...}]); false on no memory. */
bool workshop_link_titles_for_piece_links(cJSON *links);

#endif /* WORKSHOP_LINK_SERVICE_H */
