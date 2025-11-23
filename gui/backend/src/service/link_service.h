#ifndef URSATZ_GUI_LINK_SERVICE_H
#define URSATZ_GUI_LINK_SERVICE_H

#include <stdbool.h>
#include <stddef.h>

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

#endif /* URSATZ_GUI_LINK_SERVICE_H */
