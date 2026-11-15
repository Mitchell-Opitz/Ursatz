/* The job queue (service/job_queue.h): a job runs and keeps its answer, progress reaches the state, a running theme
 * compose is cancelled, two workers run two jobs at once and stop joins the workers. The jobs call the Composer API
 * directly; no server is started. */

#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <cJSON.h>

#include "service/job_queue.h"
#include "ursatz_composer/composer_api.h"

#ifndef URSATZ_GUI_TEST_THEME_REQUEST
#error "URSATZ_GUI_TEST_THEME_REQUEST must name the theme request fixture"
#endif

static int failures = 0;

static void check(bool cond, const char *what) {
  if (!cond) {
    fprintf(stderr, "FAIL: %s\n", what);
    failures++;
  } else {
    printf("PASS: %s\n", what);
  }
}

static void sleep_ms(int ms) {
  struct timespec ts = {0, (long)ms * 1000 * 1000};
  nanosleep(&ts, NULL);
}

/* Polls job id until its state is one of the finished ones (or timeout_ms passes); the last state. */
static UrsatzGuiJobState wait_finished(const char *id, int timeout_ms, UrsatzGuiJobInfo *info) {
  UrsatzGuiJobState state = URSATZ_GUI_JOB_QUEUED;
  for (int waited = 0; waited <= timeout_ms; waited += 20) {
    ursatz_gui_job_info_destroy(info);
    if (!ursatz_gui_job_get(id, info)) {
      break;
    }
    state = info->state;
    if (state != URSATZ_GUI_JOB_QUEUED && state != URSATZ_GUI_JOB_RUNNING) {
      break;
    }
    sleep_ms(20);
  }
  return state;
}

static char *read_file(const char *path) {
  FILE *file = fopen(path, "rb");
  if (!file) {
    return NULL;
  }
  size_t capacity = 8192;
  size_t length = 0;
  char *buffer = malloc(capacity);
  while (buffer) {
    size_t n = fread(buffer + length, 1, capacity - length - 1, file);
    length += n;
    if (n == 0) {
      break;
    }
    if (length + 1 >= capacity) {
      capacity *= 2;
      buffer = realloc(buffer, capacity);
    }
  }
  fclose(file);
  if (buffer) {
    buffer[length] = '\0';
  }
  return buffer;
}

/* --- a job that answers ---------------------------------------------------- */

static void answer_job(void *input, const ComposerJob *job, UrsatzGuiJobResult *out) {
  (void)job;
  out->http_status = *(int *)input;
  out->json = strdup("{\"ok\":true}");
}

static int g_freed = 0;
static void count_free(void *input) {
  g_freed++;
  free(input);
}

static void test_answer(void) {
  int *input = malloc(sizeof(int));
  *input = 200;
  char id[URSATZ_GUI_JOB_ID_BYTES];
  check(ursatz_gui_job_submit("answer", answer_job, input, count_free, id), "a job is accepted");
  UrsatzGuiJobInfo info = {0};
  check(wait_finished(id, 5000, &info) == URSATZ_GUI_JOB_DONE, "a 200 job ends done");
  check(info.http_status == 200 && info.result && strcmp(info.result, "{\"ok\":true}") == 0, "the job keeps its status and answer");
  check(strcmp(info.kind, "answer") == 0, "the job keeps its kind");
  ursatz_gui_job_info_destroy(&info);

  input = malloc(sizeof(int));
  *input = 422;
  check(ursatz_gui_job_submit("answer", answer_job, input, count_free, id), "a second job is accepted");
  check(wait_finished(id, 5000, &info) == URSATZ_GUI_JOB_FAILED && info.http_status == 422, "a 422 job ends failed");
  ursatz_gui_job_info_destroy(&info);
  check(!ursatz_gui_job_get("job-nothere", &info), "an unknown job is not found");
  check(!ursatz_gui_job_cancel("job-nothere"), "an unknown job cannot be cancelled");
}

/* --- progress ---------------------------------------------------------------- */

static volatile int g_release = 0;

static void progress_job(void *input, const ComposerJob *job, UrsatzGuiJobResult *out) {
  (void)input;
  job->progress(job->user, "melody", 6, 9);
  for (int i = 0; i < 500 && !g_release; i++) {
    sleep_ms(10);
  }
  out->http_status = 200;
  out->json = strdup("{}");
}

static void test_progress(void) {
  char id[URSATZ_GUI_JOB_ID_BYTES];
  g_release = 0;
  check(ursatz_gui_job_submit("progress", progress_job, NULL, NULL, id), "a job that reports progress is accepted");
  UrsatzGuiJobInfo info = {0};
  bool seen = false;
  for (int i = 0; i < 250 && !seen; i++) {
    ursatz_gui_job_info_destroy(&info);
    seen = ursatz_gui_job_get(id, &info) && info.state == URSATZ_GUI_JOB_RUNNING && strcmp(info.stage, "melody") == 0 &&
           info.done == 6 && info.total == 9;
    sleep_ms(10);
  }
  check(seen, "the stage and its count are seen while the job runs");
  g_release = 1;
  check(wait_finished(id, 5000, &info) == URSATZ_GUI_JOB_DONE, "the job ends done once released");
  ursatz_gui_job_info_destroy(&info);
}

/* --- cancel of a running theme compose ------------------------------------------ */

static void theme_job(void *input, const ComposerJob *job, UrsatzGuiJobResult *out) {
  const char *request = input;
  ComposerResponse response;
  memset(&response, 0, sizeof(response));
  ComposerStatus status = composer_api_theme(request, job, &response);
  out->http_status = status == COMPOSER_OK ? 200 : status == COMPOSER_ERR_CANCELLED ? 499 : 500;
  out->json = strdup("{}");
  composer_response_destroy(&response);
}

static void test_cancel(void) {
  char *text = read_file(URSATZ_GUI_TEST_THEME_REQUEST);
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

  char id[URSATZ_GUI_JOB_ID_BYTES];
  check(ursatz_gui_job_submit("theme", theme_job, printed, free, id), "a theme compose is accepted");
  UrsatzGuiJobInfo info = {0};
  bool running = false;
  for (int i = 0; i < 500 && !running; i++) {
    ursatz_gui_job_info_destroy(&info);
    running = ursatz_gui_job_get(id, &info) && info.state == URSATZ_GUI_JOB_RUNNING && info.stage[0] != '\0';
    sleep_ms(10);
  }
  check(running, "the theme compose is running and has reported a stage");
  check(ursatz_gui_job_cancel(id), "the running job is asked to cancel");
  check(wait_finished(id, 20000, &info) == URSATZ_GUI_JOB_CANCELLED, "the cancelled theme compose ends cancelled");
  check(info.result == NULL, "a cancelled job has no answer");
  ursatz_gui_job_info_destroy(&info);

  /* A job still queued is cancelled at once and never runs. */
  g_release = 0;
  char blocker[URSATZ_GUI_JOB_ID_BYTES];
  char waiting[URSATZ_GUI_JOB_ID_BYTES];
  ursatz_gui_job_submit("progress", progress_job, NULL, NULL, blocker);
  ursatz_gui_job_submit("progress", progress_job, NULL, NULL, waiting);
  char third[URSATZ_GUI_JOB_ID_BYTES];
  ursatz_gui_job_submit("progress", progress_job, NULL, NULL, third);
  check(ursatz_gui_job_cancel(third), "a queued job is cancelled");
  ursatz_gui_job_get(third, &info);
  check(info.state == URSATZ_GUI_JOB_CANCELLED, "a queued job is cancelled at once");
  ursatz_gui_job_info_destroy(&info);
  g_release = 1;
  wait_finished(blocker, 5000, &info);
  wait_finished(waiting, 5000, &info);
  ursatz_gui_job_info_destroy(&info);
}

/* --- two workers at once ------------------------------------------------------------ */

static pthread_mutex_t g_meet_lock = PTHREAD_MUTEX_INITIALIZER;
static int g_meeting = 0;
static int g_met = 0;

static void meet_job(void *input, const ComposerJob *job, UrsatzGuiJobResult *out) {
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
    sleep_ms(10);
  }
  out->http_status = 200;
  out->json = strdup("{}");
}

static void test_two_workers(void) {
  char first[URSATZ_GUI_JOB_ID_BYTES];
  char second[URSATZ_GUI_JOB_ID_BYTES];
  ursatz_gui_job_submit("meet", meet_job, NULL, NULL, first);
  ursatz_gui_job_submit("meet", meet_job, NULL, NULL, second);
  UrsatzGuiJobInfo info = {0};
  wait_finished(first, 10000, &info);
  wait_finished(second, 10000, &info);
  ursatz_gui_job_info_destroy(&info);
  check(g_met == 2, "two workers run two jobs at the same time");
}

/* --- stop joins ------------------------------------------------------------------------- */

static volatile int g_stopped_cleanly = 0;

static void endless_job(void *input, const ComposerJob *job, UrsatzGuiJobResult *out) {
  (void)input;
  while (!job->cancelled(job->user)) {
    sleep_ms(5);
  }
  g_stopped_cleanly = 1;
  out->http_status = 499;
  out->json = strdup("{}");
}

static void test_stop(void) {
  char id[URSATZ_GUI_JOB_ID_BYTES];
  int before = g_freed;
  int *input = malloc(sizeof(int));
  ursatz_gui_job_submit("endless", endless_job, NULL, NULL, id);
  ursatz_gui_job_submit("answer", answer_job, input, count_free, id);
  sleep_ms(100);
  ursatz_gui_jobs_stop();
  check(g_stopped_cleanly == 1, "stop cancels a running job and joins its worker");
  check(g_freed > before, "stop frees the input of every job");
  check(!ursatz_gui_job_submit("answer", answer_job, NULL, NULL, id), "a stopped queue accepts no job");
  ursatz_gui_jobs_stop();
  check(true, "stop is safe twice");
}

int main(void) {
  check(ursatz_gui_jobs_start(2), "the queue starts with two workers");
  test_answer();
  test_progress();
  test_cancel();
  test_two_workers();
  test_stop();
  if (failures > 0) {
    fprintf(stderr, "%d check(s) failed\n", failures);
    return 1;
  }
  printf("all checks passed\n");
  return 0;
}
