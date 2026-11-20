#ifndef URSATZ_TEXT_TEXT_VIEW_H
#define URSATZ_TEXT_TEXT_VIEW_H

#include <cJSON.h>

#include "plan/plan.h"
#include "reading/reading.h"
#include "realization/work.h"
#include "text/text.h"
#include "theory/knowledge/knowledge_snapshot.h"

/*
 * The views of R-663: a printed summary of a layer for a reader, framed by "ursatz-text 1 view <view> <address>" and the lines
 * "# leaves out: ..." and "# expand: ...". The views are summary, structure, harmony, line:<name>, part:<name> and reading; full and
 * json are the callers' (they join the layers' texts of text_layers.h). A view reads the layers through their codec documents, so
 * it prints what a file of the layer holds; any of the three layers may be NULL.
 */

typedef struct {
    const char *address; /* what the header names, e.g. "item:pce-2c8d54834a9c499f@v2" or a path */
    const Plan *plan;
    const Work *realization;
    const Reading *reading;
    const KnowledgeSnapshot *knowledge; /* names of materials and relations; NULL prints their ids' last part */
} TextViewSource;

typedef struct {
    int first_measure; /* 0 and 0: every measure */
    int last_measure;
    const char *section; /* NULL: every section; else the label or id of a section (a form node's, else a reading group's) */
} TextViewLimits;

/*
 * The framed view. `limits` may be NULL. Refusals (TextError line 0): "unknown view '<v>'"; "summary needs a plan or a reading" (also
 * structure and harmony); "line:<name> needs a realization or a plan"; "part:<name> needs a realization"; "reading needs a reading";
 * "unknown line '<n>'", "unknown part '<n>'", "unknown section '<n>'"; "measures a-b are outside the piece (bars 1-n)".
 * NULL on failure. Caller frees.
 */
char *text_view_print(const TextViewSource *source, const char *view, const TextViewLimits *limits, TextError *error);

/* A body framed as a view: the header, the body (a final newline is added), "# leaves out: <leaves_out>" and "# expand: <expand>". NULL
   `leaves_out` says "nothing". Caller frees. The Analyzer's export is framed this way as a library piece's reading view (P65 B3). */
char *text_view_frame(const char *view, const char *address, const char *body, const char *leaves_out, const char *expand);

#endif
