#include "compose_jobs.h"

#include <stdlib.h>
#include <string.h>

void ursatz_gui_job_result_json(UrsatzGuiJobResult *out, int status, cJSON *object) {
  out->http_status = status;
  out->json = object ? cJSON_PrintUnformatted(object) : NULL;
  cJSON_Delete(object);
  if (!out->json) {
    out->http_status = 500;
    out->json = strdup("{\"ok\":false,\"error\":\"out of memory\"}");
  }
}

void ursatz_gui_job_result_failure(UrsatzGuiJobResult *out, int status, const char *message) {
  cJSON *object = cJSON_CreateObject();
  if (object) {
    cJSON_AddBoolToObject(object, "ok", 0);
    cJSON_AddStringToObject(object, "error", message);
  }
  ursatz_gui_job_result_json(out, status, object);
}

void ursatz_gui_compose_reply(UrsatzGuiThemeResult result, cJSON *item, const char *message, const char *invalid_text,
                              const char *failed_text, UrsatzGuiJobResult *out) {
  switch (result) {
    case URSATZ_GUI_THEME_OK: {
      cJSON *object = cJSON_CreateObject();
      if (object && item && cJSON_AddBoolToObject(object, "ok", 1) && cJSON_AddItemToObject(object, "item", item)) {
        item = NULL;
        ursatz_gui_job_result_json(out, 200, object);
      } else {
        cJSON_Delete(object);
        ursatz_gui_job_result_failure(out, 500, "out of memory");
      }
      break;
    }
    case URSATZ_GUI_THEME_ERR_NOT_FOUND:
      ursatz_gui_job_result_failure(out, 404, message[0] ? message : "not found");
      break;
    case URSATZ_GUI_THEME_ERR_INVALID:
      ursatz_gui_job_result_failure(out, 400, message[0] ? message : invalid_text);
      break;
    case URSATZ_GUI_THEME_ERR_UNPROCESSABLE:
      ursatz_gui_job_result_failure(out, 422, message[0] ? message : "stuck");
      break;
    case URSATZ_GUI_THEME_ERR_CANCELLED:
      ursatz_gui_job_result_failure(out, 499, "cancelled");
      break;
    default:
      ursatz_gui_job_result_failure(out, 500, message[0] ? message : failed_text);
      break;
  }
  cJSON_Delete(item);
}

UrsatzGuiComposeJob *ursatz_gui_compose_job_new(UrsatzGuiComposeOp op, const char *id) {
  UrsatzGuiComposeJob *job = calloc(1, sizeof(*job));
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

const char *ursatz_gui_compose_job_kind(UrsatzGuiComposeOp op) {
  switch (op) {
    case URSATZ_GUI_COMPOSE_THEME:
      return "theme";
    case URSATZ_GUI_COMPOSE_PIECE:
      return "piece";
    case URSATZ_GUI_COMPOSE_PIECE_REGENERATE:
      return "regenerate";
    case URSATZ_GUI_COMPOSE_PIECE_VARIATIONS:
      return "variations";
    default:
      return "choose";
  }
}

void ursatz_gui_compose_job_free(void *input) {
  UrsatzGuiComposeJob *job = input;
  if (job) {
    free(job->section);
    free(job->lane);
    free(job->id);
    free(job);
  }
}

void ursatz_gui_compose_job_run(void *input, const ComposerJob *composer_job, UrsatzGuiJobResult *out) {
  const UrsatzGuiComposeJob *job = input;
  cJSON *item = NULL;
  cJSON *made = NULL;
  char message[512] = "";
  UrsatzGuiThemeResult result = URSATZ_GUI_THEME_ERR_FAILED;
  const char *invalid_text = "invalid piece";
  const char *failed_text = "the piece could not be composed";
  switch (job->op) {
    case URSATZ_GUI_COMPOSE_THEME:
      invalid_text = "invalid theme";
      failed_text = "the theme could not be composed";
      result = ursatz_gui_theme_compose(job->id, job->has_seed, job->seed, job->update_uses, composer_job, &item, message,
                                        sizeof(message));
      break;
    case URSATZ_GUI_COMPOSE_PIECE:
      result = ursatz_gui_piece_compose(job->id, job->has_seed, job->seed, job->update_uses, composer_job, &item, message,
                                        sizeof(message));
      break;
    case URSATZ_GUI_COMPOSE_PIECE_REGENERATE:
      failed_text = "the section could not be written again";
      result = ursatz_gui_piece_regenerate(job->id, job->version, job->section, job->lane, job->has_seed, job->seed,
                                           composer_job, &item, message, sizeof(message));
      break;
    case URSATZ_GUI_COMPOSE_PIECE_VARIATIONS:
      failed_text = "the variations could not be made";
      result = ursatz_gui_piece_variations(job->id, job->version, job->section, job->lane, job->count, job->has_seed, job->seed,
                                           composer_job, &made, message, sizeof(message));
      break;
    case URSATZ_GUI_COMPOSE_PIECE_CHOOSE:
      failed_text = "the variation could not be chosen";
      result = ursatz_gui_piece_choose(job->id, job->version, job->set, job->index, composer_job, &item, message,
                                       sizeof(message));
      break;
  }
  if (job->op == URSATZ_GUI_COMPOSE_PIECE_VARIATIONS && result == URSATZ_GUI_THEME_OK) {
    /* the answer is {"ok": true, "set", "variations": [...]} */
    if (made && cJSON_AddBoolToObject(made, "ok", 1)) {
      ursatz_gui_job_result_json(out, 200, made);
    } else {
      cJSON_Delete(made);
      ursatz_gui_job_result_failure(out, 500, "out of memory");
    }
    return;
  }
  cJSON_Delete(made);
  ursatz_gui_compose_reply(result, item, message, invalid_text, failed_text, out);
}
