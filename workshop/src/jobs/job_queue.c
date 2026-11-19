#define _POSIX_C_SOURCE 200809L

#include "job_queue.h"

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define DEFAULT_WORKERS 2
#define MAX_WORKERS 8
#define KEEP_FINISHED 64
#define KEEP_SECONDS 600
/* The Composer runs deep; request threads get the same 32 MB (CIVETWEB_THREAD_STACK_SIZE), so workers do too. */
#define WORKER_STACK_BYTES (32u * 1024u * 1024u)

typedef struct Job {
  char id[WORKSHOP_JOB_ID_BYTES];
  char kind[WORKSHOP_JOB_KIND_BYTES];
  WorkshopJobState state;
  char stage[WORKSHOP_JOB_STAGE_BYTES];
  unsigned done;
  unsigned total;
  int http_status;
  cJSON *result;
  bool cancel_requested;
  WorkshopJobFn run;
  void *input;
  void (*free_input)(void *);
  UrsatzJob composer_job;
  time_t finished_at;
  struct Job *next;
} Job;

static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_wake = PTHREAD_COND_INITIALIZER;
static Job *g_head = NULL; /* oldest first */
static Job *g_tail = NULL;
static pthread_t g_threads[MAX_WORKERS];
static size_t g_thread_count = 0;
static bool g_running = false;
static bool g_stopping = false;
static unsigned g_serial = 0;

void workshop_job_result_json(WorkshopJobResult *out, int status, cJSON *object) {
  out->http_status = status;
  out->tree = object;
  if (!out->tree) {
    out->http_status = 500;
    out->tree = cJSON_Parse("{\"ok\":false,\"error\":\"out of memory\"}");
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

const char *workshop_job_state_name(WorkshopJobState state) {
  switch (state) {
    case WORKSHOP_JOB_QUEUED:
      return "queued";
    case WORKSHOP_JOB_RUNNING:
      return "running";
    case WORKSHOP_JOB_DONE:
      return "done";
    case WORKSHOP_JOB_FAILED:
      return "failed";
    default:
      return "cancelled";
  }
}

static bool finished(const Job *job) {
  return job->state == WORKSHOP_JOB_DONE || job->state == WORKSHOP_JOB_FAILED || job->state == WORKSHOP_JOB_CANCELLED;
}

static void job_free(Job *job) {
  if (job->input && job->free_input) {
    job->free_input(job->input);
  }
  cJSON_Delete(job->result);
  free(job);
}

static Job *find_locked(const char *id) {
  for (Job *job = g_head; job; job = job->next) {
    if (strcmp(job->id, id) == 0) {
      return job;
    }
  }
  return NULL;
}

/* Drops finished jobs older than KEEP_SECONDS and, past KEEP_FINISHED, the oldest finished ones. */
static void prune_locked(void) {
  time_t now = time(NULL);
  size_t finished_count = 0;
  for (Job *job = g_head; job; job = job->next) {
    finished_count += finished(job) ? 1 : 0;
  }
  Job *previous = NULL;
  Job *job = g_head;
  while (job) {
    Job *next = job->next;
    bool drop = finished(job) && (now - job->finished_at > KEEP_SECONDS || finished_count > KEEP_FINISHED);
    if (drop) {
      finished_count--;
      if (previous) {
        previous->next = next;
      } else {
        g_head = next;
      }
      if (g_tail == job) {
        g_tail = previous;
      }
      job_free(job);
    } else {
      previous = job;
    }
    job = next;
  }
}

static void on_progress(void *user, const char *stage, unsigned done, unsigned total) {
  Job *job = user;
  pthread_mutex_lock(&g_lock);
  snprintf(job->stage, sizeof(job->stage), "%s", stage ? stage : "");
  job->done = done;
  job->total = total;
  pthread_mutex_unlock(&g_lock);
}

static bool on_cancelled(void *user) {
  Job *job = user;
  pthread_mutex_lock(&g_lock);
  bool cancelled = job->cancel_requested;
  pthread_mutex_unlock(&g_lock);
  return cancelled;
}

/* The limits on how many jobs of a kind run at once; "links" and "style" run one at a time (R-645). */
#define MAX_LIMITS 16
static struct {
  char kind[WORKSHOP_JOB_KIND_BYTES];
  unsigned limit;
} g_limits[MAX_LIMITS] = {{"links", 1}, {"style", 1}};
static size_t g_limit_count = 2;

void workshop_jobs_set_limit(const char *kind, unsigned limit) {
  pthread_mutex_lock(&g_lock);
  for (size_t i = 0; i < g_limit_count; i++) {
    if (strcmp(g_limits[i].kind, kind) == 0) {
      g_limits[i].limit = limit;
      pthread_mutex_unlock(&g_lock);
      return;
    }
  }
  if (g_limit_count < MAX_LIMITS) {
    snprintf(g_limits[g_limit_count].kind, sizeof(g_limits[g_limit_count].kind), "%s", kind);
    g_limits[g_limit_count++].limit = limit;
  }
  pthread_mutex_unlock(&g_lock);
}

static unsigned limit_of_locked(const char *kind) {
  for (size_t i = 0; i < g_limit_count; i++) {
    if (strcmp(g_limits[i].kind, kind) == 0) {
      return g_limits[i].limit;
    }
  }
  return 0;
}

static unsigned running_of_locked(const char *kind) {
  unsigned running = 0;
  for (Job *job = g_head; job; job = job->next) {
    running += job->state == WORKSHOP_JOB_RUNNING && strcmp(job->kind, kind) == 0 ? 1 : 0;
  }
  return running;
}

/* The oldest queued job whose kind has room to run. */
static Job *next_queued_locked(void) {
  for (Job *job = g_head; job; job = job->next) {
    if (job->state != WORKSHOP_JOB_QUEUED) {
      continue;
    }
    unsigned limit = limit_of_locked(job->kind);
    if (limit == 0 || running_of_locked(job->kind) < limit) {
      return job;
    }
  }
  return NULL;
}

static void *worker_main(void *arg) {
  (void)arg;
  pthread_mutex_lock(&g_lock);
  for (;;) {
    Job *job = NULL;
    while (!g_stopping && !(job = next_queued_locked())) {
      pthread_cond_wait(&g_wake, &g_lock);
    }
    if (g_stopping) {
      break;
    }
    job->state = WORKSHOP_JOB_RUNNING;
    pthread_mutex_unlock(&g_lock);

    WorkshopJobResult out = {500, NULL};
    UrsatzStatus status = job->run(job->input, &job->composer_job, &out);

    pthread_mutex_lock(&g_lock);
    bool ok = status == URSATZ_OK && out.http_status >= 200 && out.http_status < 300;
    if (status == URSATZ_ERR_CANCELLED || (job->cancel_requested && !ok)) {
      job->state = WORKSHOP_JOB_CANCELLED;
      cJSON_Delete(out.tree);
    } else {
      job->state = ok ? WORKSHOP_JOB_DONE : WORKSHOP_JOB_FAILED;
      job->http_status = out.http_status;
      job->result = out.tree;
    }
    job->finished_at = time(NULL);
    prune_locked();
    pthread_cond_broadcast(&g_wake);
  }
  pthread_mutex_unlock(&g_lock);
  return NULL;
}

static size_t worker_count(size_t workers) {
  if (workers == 0) {
    workers = DEFAULT_WORKERS;
  }
  return workers > MAX_WORKERS ? MAX_WORKERS : workers;
}

bool workshop_jobs_start(size_t workers) {
  pthread_mutex_lock(&g_lock);
  if (g_running) {
    pthread_mutex_unlock(&g_lock);
    return true;
  }
  g_stopping = false;
  g_running = true;
  size_t wanted = worker_count(workers);
  pthread_mutex_unlock(&g_lock);

  pthread_attr_t attributes;
  pthread_attr_init(&attributes);
  pthread_attr_setstacksize(&attributes, WORKER_STACK_BYTES);
  size_t started = 0;
  for (; started < wanted; started++) {
    if (pthread_create(&g_threads[started], &attributes, worker_main, NULL) != 0) {
      break;
    }
  }
  pthread_attr_destroy(&attributes);
  g_thread_count = started;
  if (started == 0) {
    pthread_mutex_lock(&g_lock);
    g_running = false;
    pthread_mutex_unlock(&g_lock);
    return false;
  }
  return true;
}

void workshop_jobs_stop(void) {
  pthread_mutex_lock(&g_lock);
  if (!g_running) {
    pthread_mutex_unlock(&g_lock);
    return;
  }
  g_stopping = true;
  for (Job *job = g_head; job; job = job->next) {
    if (!finished(job)) {
      job->cancel_requested = true;
    }
  }
  pthread_cond_broadcast(&g_wake);
  pthread_mutex_unlock(&g_lock);

  for (size_t i = 0; i < g_thread_count; i++) {
    pthread_join(g_threads[i], NULL);
  }
  g_thread_count = 0;

  pthread_mutex_lock(&g_lock);
  Job *job = g_head;
  g_head = NULL;
  g_tail = NULL;
  g_running = false;
  g_stopping = false;
  pthread_mutex_unlock(&g_lock);
  while (job) {
    Job *next = job->next;
    job_free(job);
    job = next;
  }
}

bool workshop_job_submit(const char *kind, WorkshopJobFn run, void *input, void (*free_input)(void *),
                           char id_out[WORKSHOP_JOB_ID_BYTES]) {
  Job *job = calloc(1, sizeof(*job));
  if (!job) {
    if (input && free_input) {
      free_input(input);
    }
    return false;
  }
  job->state = WORKSHOP_JOB_QUEUED;
  job->run = run;
  job->input = input;
  job->free_input = free_input;
  snprintf(job->kind, sizeof(job->kind), "%s", kind ? kind : "");
  job->composer_job.progress = on_progress;
  job->composer_job.cancelled = on_cancelled;
  job->composer_job.user = job;

  pthread_mutex_lock(&g_lock);
  if (!g_running || g_stopping) {
    pthread_mutex_unlock(&g_lock);
    job_free(job);
    return false;
  }
  struct timespec now;
  clock_gettime(CLOCK_REALTIME, &now);
  uint32_t mixed = (uint32_t)now.tv_nsec ^ ((uint32_t)now.tv_sec * 2654435761u);
  snprintf(job->id, sizeof(job->id), "job-%08x%04x", (unsigned)mixed, ++g_serial & 0xffffu);
  prune_locked();
  if (g_tail) {
    g_tail->next = job;
  } else {
    g_head = job;
  }
  g_tail = job;
  snprintf(id_out, WORKSHOP_JOB_ID_BYTES, "%s", job->id);
  pthread_cond_signal(&g_wake);
  pthread_mutex_unlock(&g_lock);
  return true;
}

bool workshop_job_get(const char *id, WorkshopJobInfo *info) {
  memset(info, 0, sizeof(*info));
  pthread_mutex_lock(&g_lock);
  Job *job = find_locked(id);
  if (!job) {
    pthread_mutex_unlock(&g_lock);
    return false;
  }
  snprintf(info->id, sizeof(info->id), "%s", job->id);
  snprintf(info->kind, sizeof(info->kind), "%s", job->kind);
  info->state = job->state;
  snprintf(info->stage, sizeof(info->stage), "%s", job->stage);
  info->done = job->done;
  info->total = job->total;
  info->http_status = job->http_status;
  info->result = job->result ? cJSON_Duplicate(job->result, 1) : NULL;
  pthread_mutex_unlock(&g_lock);
  return true;
}

bool workshop_job_cancel(const char *id) {
  pthread_mutex_lock(&g_lock);
  Job *job = find_locked(id);
  if (!job) {
    pthread_mutex_unlock(&g_lock);
    return false;
  }
  if (job->state == WORKSHOP_JOB_QUEUED) {
    job->state = WORKSHOP_JOB_CANCELLED;
    job->cancel_requested = true;
    job->finished_at = time(NULL);
    pthread_cond_broadcast(&g_wake);
  } else if (job->state == WORKSHOP_JOB_RUNNING) {
    job->cancel_requested = true;
  }
  pthread_mutex_unlock(&g_lock);
  return true;
}

void workshop_job_info_destroy(WorkshopJobInfo *info) {
  if (info) {
    cJSON_Delete(info->result);
    memset(info, 0, sizeof(*info));
  }
}

bool workshop_jobs_list(WorkshopJobInfo **out, size_t *count) {
  *out = NULL;
  *count = 0;
  pthread_mutex_lock(&g_lock);
  size_t live = 0;
  for (Job *job = g_head; job; job = job->next) {
    live += finished(job) ? 0 : 1;
  }
  WorkshopJobInfo *infos = live ? calloc(live, sizeof(*infos)) : NULL;
  if (live && !infos) {
    pthread_mutex_unlock(&g_lock);
    return false;
  }
  /* the queue is oldest first; the list is newest first */
  size_t made = live;
  for (Job *job = g_head; job; job = job->next) {
    if (finished(job)) {
      continue;
    }
    WorkshopJobInfo *info = &infos[--made];
    snprintf(info->id, sizeof(info->id), "%s", job->id);
    snprintf(info->kind, sizeof(info->kind), "%s", job->kind);
    info->state = job->state;
    snprintf(info->stage, sizeof(info->stage), "%s", job->stage);
    info->done = job->done;
    info->total = job->total;
  }
  pthread_mutex_unlock(&g_lock);
  *out = infos;
  *count = live;
  return true;
}

void workshop_job_list_destroy(WorkshopJobInfo *infos, size_t count) {
  for (size_t i = 0; infos && i < count; i++) {
    workshop_job_info_destroy(&infos[i]);
  }
  free(infos);
}
