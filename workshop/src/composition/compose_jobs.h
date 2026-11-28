#ifndef WORKSHOP_COMPOSE_JOBS_H
#define WORKSHOP_COMPOSE_JOBS_H

#include <common/status.h>

#include <stdbool.h>

#include <cJSON.h>

#include "jobs/job_queue.h"
#include "harmonies.h"
#include "pieces.h"
#include "themes.h"

/* The compose runs of the theme and piece services as jobs on the queue (job_queue.h). A job answers what the route used
 * to send: 200 {"ok": true, "item": {...}}, or the failure with the status of its reason. */

typedef enum {
  WORKSHOP_COMPOSE_THEME,
  WORKSHOP_COMPOSE_PIECE,
  WORKSHOP_COMPOSE_HARMONY,
  WORKSHOP_COMPOSE_PIECE_REGENERATE,
  WORKSHOP_COMPOSE_PIECE_VARIATIONS,
  WORKSHOP_COMPOSE_PIECE_CHOOSE
} WorkshopComposeOp;

typedef struct {
  WorkshopComposeOp op;
  char *id;
  bool has_seed;
  long long seed;
  bool update_uses;
  int version;   /* regenerate, variations, choose: the version worked from */
  char *section; /* regenerate, variations */
  char *lane;    /* regenerate (may be NULL), variations */
  int count;     /* variations */
  int set;       /* choose */
  int index;     /* choose */
  /* theme compose and explore: what the call adds to the settings (R-671, R-673), owned copies */
  cJSON *settings;
  cJSON *overrides;
  cJSON *off;
  char *taste;
  bool explore;
  int distance;  /* explore: 0 for the resolved explore.distance */
} WorkshopComposeJob;

/* The job name the queue shows for op: theme, piece, regenerate, variations or choose. */
const char *workshop_compose_job_kind(WorkshopComposeOp op);

/* A job input for op on item id (copied), to hand to workshop_job_submit with the two functions below; NULL on
 * allocation failure. */
WorkshopComposeJob *workshop_compose_job_new(WorkshopComposeOp op, const char *id);

void workshop_compose_job_free(void *input);

/* The WorkshopJobFn of every compose job. */
UrsatzStatus workshop_compose_job_run(void *input, const UrsatzJob *job, WorkshopJobResult *out);

/* Fills out with the answer for a service result: the item on OK, else the failure (status 404, 400, 422, 499, 500) with
 * message or the given text. Takes item (freed). */
void workshop_compose_reply(UrsatzStatus result, cJSON *item, const UrsatzError *error, const char *invalid_text,
                              const char *failed_text, WorkshopJobResult *out);

#endif /* WORKSHOP_COMPOSE_JOBS_H */
