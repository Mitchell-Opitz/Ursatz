#ifndef URSATZ_ANALYZER_INTERNAL_MOTIVE_LINKS_H
#define URSATZ_ANALYZER_INTERNAL_MOTIVE_LINKS_H

#include <ursatz_analyzer/analyzer_api.h>

/* What of the motive links only the Analyzer's command line and tests use (the API header has the scope, the cache and the run). */

/*
 * Adds every analyzed piece of <output_dir>/corpus-index.json, in index order, with the index path as its piece id.
 * Stops at the first piece that fails and leaves the pieces before it in the scope.
 */
UrsatzStatus motive_link_scope_add_corpus(MotiveLinkScope *scope, const char *output_dir);

/*
 * The id of the piece the last failed analyzer_links_scope_add_document (or the piece add_corpus stopped at) was for, or
 * NULL when none failed. Valid until the next failure or the scope is destroyed.
 */
const char *motive_link_scope_failed_piece(const MotiveLinkScope *scope);

#endif
