/* The job queue (service/job_queue.h): a job runs and keeps its answer, progress reaches the state, a running theme
 * compose is cancelled, two workers run two jobs at once and stop joins the workers. The jobs call the Composer API
 * directly; no server is started. */

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <cJSON.h>

#include "jobs/job_queue.h"
#include "ursatz_composer/composer_api.h"
#include "ursatz_check.h"
#include "test_http.h"
#include "test_knowledge.h"

#ifndef URSATZ_GUI_TEST_THEME_REQUEST
#error "URSATZ_GUI_TEST_THEME_REQUEST must name the theme request fixture"
#endif

/* Polls job id until its state is one of the finished ones (or timeout_ms passes); the last state. */
static WorkshopJobState wait_finished(const char *id, int timeout_ms, WorkshopJobInfo *info) {
  WorkshopJobState state = WORKSHOP_JOB_QUEUED;
  for (int waited = 0; waited <= timeout_ms; waited += 20) {
    workshop_job_info_destroy(info);
    if (!workshop_job_get(id, info)) {
      break;
    }
    state = info->state;
    if (state != WORKSHOP_JOB_QUEUED && state != WORKSHOP_JOB_RUNNING) {
      break;
    }
    test_sleep_ms(20);
  }
  return state;
}

/* --- a job that answers ---------------------------------------------------- */

static UrsatzStatus answer_job(void *input, const UrsatzJob *job, WorkshopJobResult *out) {
  (void)job;
  out->http_status = *(int *)input;
  out->tree = cJSON_Parse("{\"ok\":true}");
  return out->http_status < 300 ? URSATZ_OK : URSATZ_ERR_REQUEST;
}

static int g_freed = 0;
static void count_free(void *input) {
  g_freed++;
  free(input);
}

static void test_answer(void) {
  int *input = malloc(sizeof(int));
  *input = 200;
  char id[WORKSHOP_JOB_ID_BYTES];
  check(workshop_job_submit("answer", answer_job, input, count_free, id), "a job is accepted");
  WorkshopJobInfo info = {0};
  check(wait_finished(id, 5000, &info) == WORKSHOP_JOB_DONE, "a 200 job ends done");
  check(info.http_status == 200 && cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(info.result, "ok")), "the job keeps its status and answer");
  check(strcmp(info.kind, "answer") == 0, "the job keeps its kind");
  workshop_job_info_destroy(&info);

  input = malloc(sizeof(int));
  *input = 422;
  check(workshop_job_submit("answer", answer_job, input, count_free, id), "a second job is accepted");
  check(wait_finished(id, 5000, &info) == WORKSHOP_JOB_FAILED && info.http_status == 422, "a 422 job ends failed");
  workshop_job_info_destroy(&info);
  check(!workshop_job_get("job-nothere", &info), "an unknown job is not found");
  check(!workshop_job_cancel("job-nothere"), "an unknown job cannot be cancelled");
}

/* --- progress ---------------------------------------------------------------- */

static volatile int g_release = 0;

static UrsatzStatus progress_job(void *input, const UrsatzJob *job, WorkshopJobResult *out) {
  (void)input;
  job->progress(job->user, "melody", 6, 9);
  for (int i = 0; i < 500 && !g_release; i++) {
    test_sleep_ms(10);
  }
  out->http_status = 200;
  out->tree = cJSON_Parse("{}");
  return URSATZ_OK;
}

static void test_progress(void) {
  char id[WORKSHOP_JOB_ID_BYTES];
  g_release = 0;
  check(workshop_job_submit("progress", progress_job, NULL, NULL, id), "a job that reports progress is accepted");
  WorkshopJobInfo info = {0};
  bool seen = false;
  for (int i = 0; i < 250 && !seen; i++) {
    workshop_job_info_destroy(&info);
    seen = workshop_job_get(id, &info) && info.state == WORKSHOP_JOB_RUNNING && strcmp(info.stage, "melody") == 0 &&
           info.done == 6 && info.total == 9;
    test_sleep_ms(10);
  }
  check(seen, "the stage and its count are seen while the job runs");
  g_release = 1;
  check(wait_finished(id, 5000, &info) == WORKSHOP_JOB_DONE, "the job ends done once released");
  workshop_job_info_destroy(&info);
}

/* --- cancel of a running theme compose ------------------------------------------ */

static UrsatzStatus theme_job(void *input, const UrsatzJob *job, WorkshopJobResult *out) {
  const char *request = input;
  ComposerResponse response;
  memset(&response, 0, sizeof(response));
  UrsatzStatus status = composer_api_theme(workshop_composer_context_global(), request, job, &response);
  out->http_status = status == URSATZ_OK ? 200 : status == URSATZ_ERR_CANCELLED ? 499 : 500;
  out->tree = cJSON_Parse("{}");
  composer_response_destroy(&response);
  return status;
}

static void test_cancel(void) {
  char *text = test_read_file(URSATZ_GUI_TEST_THEME_REQUEST, NULL);
  cJSON *request = text ? cJSON_Parse(text) : NULL;
  free(text);
  check(request != NULL, "the theme request fixture is read");
  if (!request) {
    return;
  }
  /* Sixteen themes with the Analyzer's check: many seeds are tried, each reports, so the cancel is seen at a report. */
  cJSON_AddStringToObject(request, "suggest", "16");
  cJSON_AddBoolToObject(request, "check", 1);
  cJSON_AddStringToObject(request, "seed", "1");
  char *printed = cJSON_PrintUnformatted(request);
  cJSON_Delete(request);

  char id[WORKSHOP_JOB_ID_BYTES];
  check(workshop_job_submit("theme", theme_job, printed, free, id), "a theme compose is accepted");
  WorkshopJobInfo info = {0};
  bool running = false;
  for (int i = 0; i < 500 && !running; i++) {
    workshop_job_info_destroy(&info);
    running = workshop_job_get(id, &info) && info.state == WORKSHOP_JOB_RUNNING && info.stage[0] != '\0';
    test_sleep_ms(10);
  }
  check(running, "the theme compose is running and has reported a stage");
  check(workshop_job_cancel(id), "the running job is asked to cancel");
  check(wait_finished(id, 20000, &info) == WORKSHOP_JOB_CANCELLED, "the cancelled theme compose ends cancelled");
  check(info.result == NULL, "a cancelled job has no answer");
  workshop_job_info_destroy(&info);

  /* A job still queued is cancelled at once and never runs. */
  g_release = 0;
  char blocker[WORKSHOP_JOB_ID_BYTES];
  char waiting[WORKSHOP_JOB_ID_BYTES];
  workshop_job_submit("progress", progress_job, NULL, NULL, blocker);
  workshop_job_submit("progress", progress_job, NULL, NULL, waiting);
  char third[WORKSHOP_JOB_ID_BYTES];
  workshop_job_submit("progress", progress_job, NULL, NULL, third);
  check(workshop_job_cancel(third), "a queued job is cancelled");
  workshop_job_get(third, &info);
  check(info.state == WORKSHOP_JOB_CANCELLED, "a queued job is cancelled at once");
  workshop_job_info_destroy(&info);
  g_release = 1;
  wait_finished(blocker, 5000, &info);
  wait_finished(waiting, 5000, &info);
  workshop_job_info_destroy(&info);
}

/* --- two workers at once ------------------------------------------------------------ */

static pthread_mutex_t g_meet_lock = PTHREAD_MUTEX_INITIALIZER;
static int g_meeting = 0;
static int g_met = 0;

static UrsatzStatus meet_job(void *input, const UrsatzJob *job, WorkshopJobResult *out) {
  (void)input;
  (void)job;
  pthread_mutex_lock(&g_meet_lock);
  g_meeting++;
  pthread_mutex_unlock(&g_meet_lock);
  /* Each job waits for the other to be running too: only two workers at once get both through. */
  for (int i = 0; i < 500; i++) {
    pthread_mutex_lock(&g_meet_lock);
    int both = g_meeting >= 2;
    pthread_mutex_unlock(&g_meet_lock);
    if (both) {
      pthread_mutex_lock(&g_meet_lock);
      g_met++;
      pthread_mutex_unlock(&g_meet_lock);
      break;
    }
    test_sleep_ms(10);
  }
  out->http_status = 200;
  out->tree = cJSON_Parse("{}");
  return URSATZ_OK;
}

static void test_two_workers(void) {
  char first[WORKSHOP_JOB_ID_BYTES];
  char second[WORKSHOP_JOB_ID_BYTES];
  workshop_job_submit("meet", meet_job, NULL, NULL, first);
  workshop_job_submit("meet", meet_job, NULL, NULL, second);
  WorkshopJobInfo info = {0};
  wait_finished(first, 10000, &info);
  wait_finished(second, 10000, &info);
  workshop_job_info_destroy(&info);
  check(g_met == 2, "two workers run two jobs at the same time");
}

/* --- stop joins ------------------------------------------------------------------------- */

static volatile int g_stopped_cleanly = 0;

static UrsatzStatus endless_job(void *input, const UrsatzJob *job, WorkshopJobResult *out) {
  (void)input;
  while (!job->cancelled(job->user)) {
    test_sleep_ms(5);
  }
  g_stopped_cleanly = 1;
  out->http_status = 499;
  out->tree = cJSON_Parse("{}");
  return URSATZ_ERR_CANCELLED;
}

static void test_stop(void) {
  char id[WORKSHOP_JOB_ID_BYTES];
  int before = g_freed;
  int *input = malloc(sizeof(int));
  workshop_job_submit("endless", endless_job, NULL, NULL, id);
  workshop_job_submit("answer", answer_job, input, count_free, id);
  test_sleep_ms(100);
  workshop_jobs_stop();
  check(g_stopped_cleanly == 1, "stop cancels a running job and joins its worker");
  check(g_freed > before, "stop frees the input of every job");
  check(!workshop_job_submit("answer", answer_job, NULL, NULL, id), "a stopped queue accepts no job");
  workshop_jobs_stop();
  check(true, "stop is safe twice");
}

/* --- per-kind limits, the list and retention -------------------------------------------------------------------------------- */

static pthread_mutex_t g_limit_lock = PTHREAD_MUTEX_INITIALIZER;
static int g_limited_running = 0;
static int g_limited_peak = 0;
static volatile int g_limit_release = 0;

/* A job of the limited kind counts itself; the one of the other kind (input not NULL) only waits. */
static UrsatzStatus limited_job(void *input, const UrsatzJob *job, WorkshopJobResult *out) {
  (void)job;
  if (!input) {
    pthread_mutex_lock(&g_limit_lock);
    int running = ++g_limited_running;
    if (running > g_limited_peak) {
      g_limited_peak = running;
    }
    pthread_mutex_unlock(&g_limit_lock);
  }
  for (int i = 0; i < 500 && !g_limit_release; i++) {
    test_sleep_ms(10);
  }
  if (!input) {
    pthread_mutex_lock(&g_limit_lock);
    g_limited_running--;
    pthread_mutex_unlock(&g_limit_lock);
  }
  out->http_status = 200;
  out->tree = cJSON_Parse("{}");
  return URSATZ_OK;
}

static void test_limits_and_list(void) {
  /* "links" and "style" run one at a time by default; any other kind can be limited */
  workshop_jobs_set_limit("limited", 1);
  g_limit_release = 0;
  char ids[3][WORKSHOP_JOB_ID_BYTES];
  char other[WORKSHOP_JOB_ID_BYTES];
  for (int i = 0; i < 3; i++) {
    check(workshop_job_submit("limited", limited_job, NULL, NULL, ids[i]), "a job of a limited kind is accepted");
  }
  check(workshop_job_submit("unlimited", limited_job, (void *)(uintptr_t)1, NULL, other), "a job of an unlimited kind is accepted");
  test_sleep_ms(300);
  WorkshopJobInfo *list = NULL;
  size_t count = 0;
  check(workshop_jobs_list(&list, &count) && count == 4, "the list holds the four jobs that have not finished");
  int running = 0;
  int queued = 0;
  for (size_t i = 0; i < count; i++) {
    running += list[i].state == WORKSHOP_JOB_RUNNING;
    queued += list[i].state == WORKSHOP_JOB_QUEUED;
  }
  check(running == 2 && queued == 2, "one limited job runs beside the unlimited one and the other two wait");
  check(strcmp(list[0].id, other) == 0 && strcmp(list[count - 1].id, ids[0]) == 0, "the list is newest first");
  workshop_job_list_destroy(list, count);
  g_limit_release = 1;
  WorkshopJobInfo info = {0};
  for (int i = 0; i < 3; i++) {
    check(wait_finished(ids[i], 10000, &info) == WORKSHOP_JOB_DONE, "every waiting job runs in its turn");
  }
  wait_finished(other, 10000, &info);
  workshop_job_info_destroy(&info);
  check(g_limited_peak == 1, "never more than one job of the limited kind ran at once");
  check(workshop_jobs_list(&list, &count) && count == 0, "the list is empty once every job has finished");
  workshop_job_list_destroy(list, count);
  workshop_jobs_set_limit("limited", 0);
}

static UrsatzStatus quick_job(void *input, const UrsatzJob *job, WorkshopJobResult *out) {
  (void)input;
  (void)job;
  out->http_status = 200;
  out->tree = cJSON_Parse("{\"ok\":true}");
  return URSATZ_OK;
}

static void test_retention(void) {
  char first[WORKSHOP_JOB_ID_BYTES];
  char id[WORKSHOP_JOB_ID_BYTES];
  WorkshopJobInfo info = {0};
  check(workshop_job_submit("quick", quick_job, NULL, NULL, first), "the first of many jobs is accepted");
  for (int i = 0; i < 80; i++) {
    workshop_job_submit("quick", quick_job, NULL, NULL, id);
  }
  wait_finished(id, 10000, &info);
  workshop_job_info_destroy(&info);
  check(!workshop_job_get(first, &info), "past the newest 64 finished jobs the oldest are dropped");
  check(workshop_job_get(id, &info) && info.result, "the newest keeps its answer");
  workshop_job_info_destroy(&info);
}

int main(void) {
  test_knowledge();
  check(workshop_jobs_start(2), "the queue starts with two workers");
  test_answer();
  test_progress();
  test_cancel();
  test_two_workers();
  test_limits_and_list();
  test_retention();
  test_stop();
  return test_finish("job_queue");
}
