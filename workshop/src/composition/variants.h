#ifndef WORKSHOP_VARIANTS_H
#define WORKSHOP_VARIANTS_H

#include <cJSON.h>

#include <common/status.h>

#include "composition/compose_jobs.h"
#include "jobs/job_queue.h"

/*
 * The variant engine (A20261109-76): the variants, each-op and reroll jobs, saving a candidate, flagging, promoting, recognizing, linking a
 * form and the variant tree. A route parses its request, calls one function here and answers; the job submissions stay in the routes.
 * Every call returns an UrsatzStatus and fills error with the message of a failure.
 */

typedef enum { WORKSHOP_VARIANTS_VARIANTS, WORKSHOP_VARIANTS_EACH_OP, WORKSHOP_VARIANTS_REROLL } WorkshopVariantsOp;

typedef struct WorkshopVariantsJob WorkshopVariantsJob;

/* Checks the request body of the variants or each-op route (none for a reroll) and makes the job input for item id. Takes body, also
 * on a failure. */
UrsatzStatus workshop_variants_job_new(WorkshopVariantsOp op, const char *id, cJSON *body, WorkshopVariantsJob **out, UrsatzError *error);

/* The job name the queue shows, and the UrsatzGuiJobFn and free function to hand to workshop_job_submit. */
const char *workshop_variants_job_kind(const WorkshopVariantsJob *job);
void workshop_variants_job_run(void *input, const UrsatzJob *job, WorkshopJobResult *out);
void workshop_variants_job_free(void *input);

/* A derived motif made from one candidate: body {variant, name?, flag}. The new item in *item. */
UrsatzStatus workshop_variants_save(const char *id, const cJSON *body, cJSON **item, UrsatzError *error);

/* A new version that changes only derived.flag (and kind): body {flag, kind?}. */
UrsatzStatus workshop_variants_flag(const char *id, const cJSON *body, cJSON **item, UrsatzError *error);

/* A variant becomes a motif of its own: body {name, project_id, note?}. */
UrsatzStatus workshop_variants_promote(const char *id, const cJSON *body, cJSON **item, UrsatzError *error);

/* The item (the target) becomes a form of a source from a recognition: body {source: {item, version}, recognition}. CONFLICT when the
 * item is in the source's lineage. */
UrsatzStatus workshop_variants_link_form(const char *id, const cJSON *body, cJSON **item, UrsatzError *error);

/* How the target of request {source: {item, version}, target: {item, version}} derives from the source: the Composer's document plus
 * "ok" in *document. On a failure *http_status is the status to answer (a Composer refusal keeps its path in error). */
UrsatzStatus workshop_variants_recognize(const cJSON *request, cJSON **document, int *http_status, UrsatzError *error);

/* The item and, recursively, every motif derived from it. */
UrsatzStatus workshop_variants_tree(const char *id, cJSON **tree, UrsatzError *error);

#endif /* WORKSHOP_VARIANTS_H */
