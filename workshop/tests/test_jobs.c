/* Link runs and style builds as jobs (R-645): two link runs run one after the other (the "links" kind runs one job at a time), a link
 * run and a style build cancelled between pieces leave no half-written file (no pending, removed, stage or temporary entry, a run
 * record that reads, no style), and the builds that follow run as if nothing happened. Runs in its own working directory. */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include <cJSON.h>

#include <ursatz_analyzer/analyzer_api.h>

#include "analysis/analysis.h"
#include "analysis/links.h"
#include "analysis/styles.h"
#include "items/collections.h"
#include "items/formats.h"
#include "items/layout.h"
#include "items/store.h"
#include "items/subjects.h"
#include "jobs/job_queue.h"
#include <common/files.h>
#include "ursatz_check.h"
#include "test_knowledge.h"
#include "command_context_support.h"

#ifndef URSATZ_GUI_TEST_LINK_FIXTURES
#error "URSATZ_GUI_TEST_LINK_FIXTURES must name the link fixtures directory"
#endif

#define PIECES 8

static char g_materials[64];
static char g_subjects[64];
static char g_ids[PIECES][64];

static const char *str(const cJSON *object, const char *name) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);
    return cJSON_IsString(item) ? item->valuestring : "";
}

static void sleep_ms(int ms) {
    usleep((useconds_t)ms * 1000);
}

static bool write_json(const char *path, const cJSON *value) {
    char *text = cJSON_Print(value);
    bool ok = text && test_write_file(path, text, strlen(text));
    free(text);
    return ok;
}

static bool import_piece(const char *name, char *out_id, size_t out_size) {
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", URSATZ_GUI_TEST_LINK_FIXTURES, name);
    char *text = test_read_file(path, NULL);
    char *id = NULL;
    bool ok = text && workshop_analysis_import_and_save(WORKSHOP_DIR_PIECES, (const unsigned char *)text, strlen(text), name, &id) == URSATZ_OK;
    free(text);
    if (ok) {
        snprintf(out_id, out_size, "%s", id);
    }
    free(id);
    return ok;
}

static void piece_path(const char *id, const char *file, char *out, size_t size) {
    snprintf(out, size, WORKSHOP_DIR_PIECES "/%s/%s", id, file);
}

/* True when the piece's stored analysis is from this Analyzer. */
static bool piece_is_current(const char *id) {
    char path[400];
    piece_path(id, "analysis.json", path, sizeof(path));
    cJSON *document = test_read_json(path);
    const char *version = str(cJSON_GetObjectItemCaseSensitive(document, "metadata"), "analyzer_version");
    bool current = strcmp(version, analyzer_version()) == 0;
    cJSON_Delete(document);
    return current;
}

static int current_pieces(void) {
    int count = 0;
    for (int i = 0; i < PIECES; i++) {
        count += piece_is_current(g_ids[i]) ? 1 : 0;
    }
    return count;
}

/* Makes the piece stale by Analyzer version in both files: a job that reads it analyzes it again. */
static void make_stale(const char *id) {
    char path[400];
    piece_path(id, "analysis.json", path, sizeof(path));
    cJSON *document = test_read_json(path);
    cJSON *metadata = cJSON_GetObjectItemCaseSensitive(document, "metadata");
    cJSON_ReplaceItemInObjectCaseSensitive(metadata, "analyzer_version", cJSON_CreateString("0.0.1-old"));
    check_quiet(write_json(path, document), "write the stale document");
    cJSON_Delete(document);
    piece_path(id, "piece.json", path, sizeof(path));
    cJSON *meta = test_read_json(path);
    cJSON_ReplaceItemInObjectCaseSensitive(meta, "analyzer_version", cJSON_CreateString("0.0.1-old"));
    check_quiet(write_json(path, meta), "write the stale meta");
    cJSON_Delete(meta);
}

static void make_all_stale(void) {
    for (int i = 0; i < PIECES; i++) {
        make_stale(g_ids[i]);
    }
}

/* Counts the entries under folder that a transaction or a write in flight leaves behind (pending, removed, stage and temporary names). */
static int leftovers(const char *folder) {
    CommonNames names = {0};
    if (common_dir_list(folder, &names, NULL) != URSATZ_OK) {
        return 0;
    }
    int found = 0;
    for (size_t i = 0; i < names.count; i++) {
        const char *name = names.items[i];
        char path[1024];
        snprintf(path, sizeof(path), "%s/%s", folder, name);
        if (name[0] == '.' && (strncmp(name, ".pending-", 9) == 0 || strncmp(name, ".removed-", 9) == 0)) {
            found++;
        } else if (strstr(name, ".stage-") != NULL || strstr(name, ".tmp-") != NULL) {
            found++;
        }
        struct stat info;
        if (lstat(path, &info) == 0 && S_ISDIR(info.st_mode)) {
            found += leftovers(path);
        }
    }
    common_names_free(&names);
    return found;
}

static void expect_clean(const char *what) {
    check_quiet(leftovers("library") == 0 && leftovers("settings") == 0 && leftovers("cache") == 0, what);
}

/* Waits for the job to be over; its last state in *info (free it). false on timeout. */
static bool wait_job(const char *job, WorkshopJobInfo *info) {
    for (int waited = 0; waited < 120000; waited += 5) {
        if (!workshop_job_get(job, info)) {
            return false;
        }
        if (info->state != WORKSHOP_JOB_QUEUED && info->state != WORKSHOP_JOB_RUNNING) {
            return true;
        }
        workshop_job_info_destroy(info);
        sleep_ms(5);
    }
    return false;
}

/* Two link runs started one after the other run one after the other: never both running, the second queued while the first runs. */
static void test_serial_runs(void) {
    WorkshopLinkScope scope = {0};
    scope.kind = WORKSHOP_LINK_SCOPE_ALL;
    char *first = NULL;
    char *second = NULL;
    char first_job[WORKSHOP_JOB_ID_BYTES];
    char second_job[WORKSHOP_JOB_ID_BYTES];
    check_quiet(workshop_link_run_start(&scope, &first, first_job) == URSATZ_OK && first, "the first run starts");
    check_quiet(workshop_link_run_start(&scope, &second, second_job) == URSATZ_OK && second, "the second run starts");
    check_quiet(strcmp(first_job, second_job) != 0, "each run has a job of its own");

    bool overlapped = false;
    bool second_waited = false;
    bool first_over = false;
    bool second_over = false;
    for (int waited = 0; waited < 120000 && !(first_over && second_over); waited += 2) {
        WorkshopJobInfo one;
        WorkshopJobInfo two;
        bool got_one = workshop_job_get(first_job, &one);
        bool got_two = workshop_job_get(second_job, &two);
        if (got_one && got_two) {
            overlapped = overlapped || (one.state == WORKSHOP_JOB_RUNNING && two.state == WORKSHOP_JOB_RUNNING);
            second_waited = second_waited || (one.state == WORKSHOP_JOB_RUNNING && two.state == WORKSHOP_JOB_QUEUED);
            first_over = one.state == WORKSHOP_JOB_DONE;
            second_over = two.state == WORKSHOP_JOB_DONE;
            check_quiet(!(two.state == WORKSHOP_JOB_DONE && one.state != WORKSHOP_JOB_DONE), "the second run does not end before the first");
        }
        if (got_one) {
            workshop_job_info_destroy(&one);
        }
        if (got_two) {
            workshop_job_info_destroy(&two);
        }
        sleep_ms(2);
    }
    check_quiet(first_over && second_over, "both runs end done");
    check_quiet(!overlapped, "the runs never run at once");
    check_quiet(second_waited, "the second run waited in the queue while the first ran");

    cJSON *one = NULL;
    cJSON *two = NULL;
    check_quiet(workshop_link_run_get(first, &one) == URSATZ_OK && workshop_link_run_get(second, &two) == URSATZ_OK, "both runs read");
    check_quiet(strcmp(str(one, "status"), "done") == 0 && strcmp(str(two, "status"), "done") == 0, "both run records say done");
    check_quiet(strcmp(str(one, "finished_at"), str(two, "finished_at")) <= 0, "the first run finished before the second");
    cJSON_Delete(one);
    cJSON_Delete(two);
    check_quiet(workshop_link_run_delete(first) == URSATZ_OK && workshop_link_run_delete(second) == URSATZ_OK, "the runs are deleted");
    free(first);
    free(second);
    expect_clean("two link runs leave no pending entry");
}

/* A link run cancelled between pieces: the job is cancelled, nothing half-written stays, and the next run runs. */
static void test_link_cancel(void) {
    make_all_stale();
    check_quiet(current_pieces() == 0, "every piece is stale before the run");
    WorkshopLinkScope scope = {0};
    scope.kind = WORKSHOP_LINK_SCOPE_ALL;
    char *run_id = NULL;
    char job[WORKSHOP_JOB_ID_BYTES];
    check_quiet(workshop_link_run_start(&scope, &run_id, job) == URSATZ_OK && run_id, "a run starts");

    /* The job analyzes the stale pieces one by one: cancel it once the first is done. */
    for (int waited = 0; waited < 120000 && current_pieces() == 0; waited += 1) {
        sleep_ms(1);
    }
    check_quiet(workshop_job_cancel(job), "the job is asked to stop");
    WorkshopJobInfo info;
    check_quiet(wait_job(job, &info), "the cancelled job ends");
    check_quiet(info.state == WORKSHOP_JOB_CANCELLED || info.state == WORKSHOP_JOB_DONE, "the job ends cancelled (or done when it was already over)");
    bool cancelled = info.state == WORKSHOP_JOB_CANCELLED;
    workshop_job_info_destroy(&info);
    check_quiet(cancelled, "the run was cancelled between pieces");
    check_quiet(current_pieces() < PIECES, "the run stopped before it analyzed every piece");
    expect_clean("a cancelled link run leaves no pending, removed or temporary entry");

    cJSON *run = NULL;
    UrsatzStatus read = workshop_link_run_get(run_id, &run);
    check_quiet(read == URSATZ_ERR_NOT_FOUND || (read == URSATZ_OK && strcmp(str(run, "status"), "done") != 0),
                "the cancelled run is gone or not done");
    cJSON_Delete(run);
    char record[400];
    snprintf(record, sizeof(record), WORKSHOP_DIR_LINKS "/%s/run.json", run_id);
    if (common_path_exists(record)) {
        cJSON *stored = NULL;
        check_quiet(workshop_format_read(record, WORKSHOP_FORMAT_LINK_RUN, true, &stored, NULL) == URSATZ_OK, "its record still reads");
        cJSON_Delete(stored);
    }
    workshop_link_run_delete(run_id);
    free(run_id);

    /* The next run goes through: the stale pieces are analyzed again inside it. */
    char *next = NULL;
    char next_job[WORKSHOP_JOB_ID_BYTES];
    check_quiet(workshop_link_run_start(&scope, &next, next_job) == URSATZ_OK && next, "the next run starts");
    check_quiet(wait_job(next_job, &info) && info.state == WORKSHOP_JOB_DONE, "the next run ends done");
    workshop_job_info_destroy(&info);
    check_quiet(current_pieces() == PIECES, "the next run analyzed every stale piece");
    workshop_link_run_delete(next);
    free(next);
    expect_clean("the next run leaves nothing behind");
}

typedef struct {
    int progress_calls;
    int cancel_after;
    unsigned last_total;
} Probe;

static void probe_progress(void *user, const char *stage, unsigned done, unsigned total) {
    Probe *probe = user;
    probe->progress_calls++;
    probe->last_total = total;
    check_quiet(strcmp(stage, "pieces") == 0 && done <= total, "a style build reports pieces d/t");
}

static bool probe_cancelled(void *user) {
    const Probe *probe = user;
    return probe->progress_calls >= probe->cancel_after;
}

static int styles_listed(void) {
    cJSON *list = NULL;
    int count = workshop_style_list(&list) == URSATZ_OK ? cJSON_GetArraySize(list) : -1;
    cJSON_Delete(list);
    return count;
}

/* A style build cancelled between pieces writes nothing; the same build then runs to the end. */
static void test_style_cancel(void) {
    make_all_stale();
    const char *ids[PIECES];
    for (int i = 0; i < PIECES; i++) {
        ids[i] = g_ids[i];
    }
    WorkshopStyleSource source = {0};
    source.kind = WORKSHOP_STYLE_SOURCE_PIECES;
    source.piece_ids = ids;
    source.piece_count = PIECES;

    Probe probe = {0, 3, 0}; /* the first report is 0/8, then one per piece: stop before the third piece */
    UrsatzJob job = {probe_progress, probe_cancelled, &probe};
    char *style_id = NULL;
    UrsatzStatus result = workshop_style_build("Cancelled", &source, &job, &style_id, NULL, NULL);
    check_quiet(result == URSATZ_ERR_CANCELLED && style_id == NULL, "a cancelled build answers CANCELLED");
    check_quiet(probe.last_total == PIECES, "the build reported the number of pieces");
    check_quiet(styles_listed() == 0, "a cancelled build stores no style");
    check_quiet(current_pieces() == 2, "it stopped between pieces: two were analyzed, six were not");
    expect_clean("a cancelled style build leaves no pending, removed or temporary entry");

    probe = (Probe){0, 1000, 0};
    result = workshop_style_build("Whole", &source, &job, &style_id, NULL, NULL);
    check_quiet(result == URSATZ_OK && style_id, "the same build runs to the end");
    check_quiet(probe.progress_calls == PIECES + 1, "it reported every piece");
    check_quiet(styles_listed() == 1 && current_pieces() == PIECES, "the style is stored and every piece is current");

    /* A rebuild cancelled between pieces leaves the style as it was. */
    cJSON *before = NULL;
    check_quiet(workshop_style_get(style_id, &before) == URSATZ_OK, "read the style");
    probe = (Probe){0, 2, 0};
    check_quiet(workshop_style_rebuild(style_id, &job) == URSATZ_ERR_CANCELLED, "a cancelled rebuild answers CANCELLED");
    cJSON *after = NULL;
    check_quiet(workshop_style_get(style_id, &after) == URSATZ_OK && strcmp(str(before, "updated_at"), str(after, "updated_at")) == 0,
                "a cancelled rebuild leaves the style as it was");
    cJSON_Delete(before);
    cJSON_Delete(after);
    expect_clean("a cancelled rebuild leaves nothing behind");
    free(style_id);
}

int main(void) {
    command_context_support_begin(); /* the services are called directly: a write needs a command (command_context_support.h) */
    test_knowledge();
    check_quiet(workshop_jobs_start(2), "the job queue starts");
    snprintf(g_materials, sizeof(g_materials), "materials_%d", (int)time(NULL));
    snprintf(g_subjects, sizeof(g_subjects), "subjects_%d", (int)time(NULL));
    system("rm -rf library settings cache projects data materials_* subjects_*");
    common_dir_make(WORKSHOP_DIR_PIECES, NULL);
    mkdir("data", 0755);
    check_quiet(ursatz_gui_collection_service_init(), "init collections");
    check_quiet(workshop_material_service_init(), "init materials");
    check_quiet(workshop_subject_service_init(), "init subjects");
    check_quiet(workshop_link_service_init(), "init links");

    const char *files[PIECES] = {"a.musicxml", "b.musicxml", "c.musicxml", "d.musicxml", "a.musicxml", "b.musicxml", "c.musicxml", "d.musicxml"};
    bool imported = true;
    for (int i = 0; i < PIECES; i++) {
        imported = import_piece(files[i], g_ids[i], sizeof(g_ids[i])) && imported;
    }
    check_quiet(imported, "import the pieces");
    if (imported) {
        test_serial_runs();
        test_link_cancel();
        test_style_cancel();
    }

    workshop_jobs_stop();
    workshop_link_service_shutdown();
    return test_finish("workshop_jobs");
}
