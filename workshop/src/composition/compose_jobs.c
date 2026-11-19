#include "compose_jobs.h"
#include "base/last_error.h"

#include "compose_common.h"
#include "base/http_status.h"

#include <stdlib.h>
#include <string.h>

void workshop_job_result_json(WorkshopJobResult *out, int status, cJSON *object) {
  out->http_status = status;
  out->json = object ? cJSON_PrintUnformatted(object) : NULL;
  cJSON_Delete(object);
  if (!out->json) {
    out->http_status = 500;
    out->json = strdup("{\"ok\":false,\"error\":\"out of memory\"}");
  }
}

void workshop_job_result_failure_at(WorkshopJobResult *out, int status, const char *message, const char *path) {
  cJSON *object = cJSON_CreateObject();
  if (object) {
    cJSON_AddBoolToObject(object, "ok", 0);
    cJSON_AddStringToObject(object, "error", message);
    if (path && path[0] != '\0') {
      cJSON_AddStringToObject(object, "path", path);
    }
  }
  workshop_job_result_json(out, status, object);
}

void workshop_job_result_failure(WorkshopJobResult *out, int status, const char *message) {
  workshop_job_result_failure_at(out, status, message, NULL);
}

void workshop_compose_reply(UrsatzStatus result, cJSON *item, const UrsatzError *error, const char *invalid_text,
                              const char *failed_text, WorkshopJobResult *out) {
  if (result == URSATZ_OK) {
    cJSON *object = cJSON_CreateObject();
    if (object && item && cJSON_AddBoolToObject(object, "ok", 1) && cJSON_AddItemToObject(object, "item", item)) {
      item = NULL;
      workshop_job_result_json(out, 200, object);
    } else {
      cJSON_Delete(object);
      workshop_job_result_failure(out, 500, "out of memory");
    }
    cJSON_Delete(item);
    return;
  }
  int status = workshop_http_status(result, NULL); /* a refusal at a request member is 400; its path is sent beside it */
  const char *fallback = result == URSATZ_ERR_NOT_FOUND ? "not found"
                         : result == URSATZ_ERR_REQUEST ? invalid_text
                         : result == URSATZ_ERR_UNSATISFIABLE ? "stuck"
                                                              : failed_text;
  if (result == URSATZ_ERR_CANCELLED) {
    workshop_job_result_failure(out, status, "cancelled");
  } else if (result == URSATZ_ERR_NOT_FOUND) {
    workshop_job_result_failure(out, status, error->message[0] ? error->message : fallback);
  } else {
    workshop_job_result_failure_at(out, status, error->message[0] ? error->message : fallback, error->path);
  }
  cJSON_Delete(item);
}

WorkshopComposeJob *workshop_compose_job_new(WorkshopComposeOp op, const char *id) {
  WorkshopComposeJob *job = calloc(1, sizeof(*job));
  if (!job) {
    return NULL;
  }
  job->op = op;
  job->id = strdup(id);
  if (!job->id) {
    free(job);
    return NULL;
  }
  return job;
}

const char *workshop_compose_job_kind(WorkshopComposeOp op) {
  switch (op) {
    case WORKSHOP_COMPOSE_THEME:
      return "theme";
    case WORKSHOP_COMPOSE_PIECE:
      return "piece";
    case WORKSHOP_COMPOSE_PIECE_REGENERATE:
      return "regenerate";
    case WORKSHOP_COMPOSE_PIECE_VARIATIONS:
      return "variations";
    default:
      return "choose";
  }
}

void workshop_compose_job_free(void *input) {
  WorkshopComposeJob *job = input;
  if (job) {
    free(job->section);
    free(job->lane);
    free(job->id);
    free(job);
  }
}

void workshop_compose_job_run(void *input, const UrsatzJob *composer_job, WorkshopJobResult *out) {
  const WorkshopComposeJob *job = input;
  cJSON *item = NULL;
  cJSON *made = NULL;
  UrsatzError error = {{0}, {0}};
  UrsatzStatus result = URSATZ_ERR_INTERNAL;
  const char *invalid_text = "invalid piece";
  const char *failed_text = "the piece could not be composed";
  switch (job->op) {
    case WORKSHOP_COMPOSE_THEME:
      invalid_text = "invalid theme";
      failed_text = "the theme could not be composed";
      result = workshop_theme_compose(job->id, job->has_seed, job->seed, job->update_uses, composer_job, &item, &error);
      break;
    case WORKSHOP_COMPOSE_PIECE:
      result = workshop_piece_compose(job->id, job->has_seed, job->seed, job->update_uses, composer_job, &item, &error);
      break;
    case WORKSHOP_COMPOSE_PIECE_REGENERATE:
      failed_text = "the section could not be written again";
      result = workshop_piece_regenerate(job->id, job->version, job->section, job->lane, job->has_seed, job->seed,
                                           composer_job, &item, &error);
      break;
    case WORKSHOP_COMPOSE_PIECE_VARIATIONS:
      failed_text = "the variations could not be made";
      result = workshop_piece_variations(job->id, job->version, job->section, job->lane, job->count, job->has_seed, job->seed,
                                           composer_job, &made, &error);
      break;
    case WORKSHOP_COMPOSE_PIECE_CHOOSE:
      failed_text = "the variation could not be chosen";
      result = workshop_piece_choose(job->id, job->version, job->set, job->index, composer_job, &item, &error);
      break;
  }
  if (job->op == WORKSHOP_COMPOSE_PIECE_VARIATIONS && result == URSATZ_OK) {
    /* the answer is {"ok": true, "set", "variations": [...]} */
    if (made && cJSON_AddBoolToObject(made, "ok", 1)) {
      workshop_job_result_json(out, 200, made);
    } else {
      cJSON_Delete(made);
      workshop_job_result_failure(out, 500, "out of memory");
    }
    return;
  }
  cJSON_Delete(made);
  workshop_compose_reply(result, item, &error, invalid_text, failed_text, out);
}
