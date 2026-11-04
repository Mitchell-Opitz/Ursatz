#ifndef URSATZ_GUI_COMPOSE_JOBS_H
#define URSATZ_GUI_COMPOSE_JOBS_H

#include <stdbool.h>

#include <cJSON.h>

#include "job_queue.h"
#include "theme_service.h"

/* The compose runs of the theme and piece services as jobs on the queue (job_queue.h). A job answers what the route used
 * to send: 200 {"ok": true, "item": {...}}, or the failure with the status of its reason. */

typedef enum { URSATZ_GUI_COMPOSE_THEME } UrsatzGuiComposeOp;

typedef struct {
  UrsatzGuiComposeOp op;
  char *id;
  bool has_seed;
  long long seed;
  bool update_uses;
} UrsatzGuiComposeJob;

/* A job input for op on item id (copied), to hand to ursatz_gui_job_submit with the two functions below; NULL on
 * allocation failure. */
UrsatzGuiComposeJob *ursatz_gui_compose_job_new(UrsatzGuiComposeOp op, const char *id);

void ursatz_gui_compose_job_free(void *input);

/* The UrsatzGuiJobFn of every compose job. */
void ursatz_gui_compose_job_run(void *input, const ComposerJob *job, UrsatzGuiJobResult *out);

/* Fills out with the answer for a service result: the item on OK, else the failure (status 404, 400, 422, 499, 500) with
 * message or the given text. Takes item (freed). */
void ursatz_gui_compose_reply(UrsatzGuiThemeResult result, cJSON *item, const char *message, const char *invalid_text,
                              const char *failed_text, UrsatzGuiJobResult *out);

#endif /* URSATZ_GUI_COMPOSE_JOBS_H */
