#define _POSIX_C_SOURCE 200809L
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "accuracy_report.h"
#include "chorale_chain.h"
#include "fixtures.h"

/*
 * accuracy_harness: measures the whole chain against annotations and writes the report.
 *
 *   1. Chorales. Each piece under the corpus directory (corpus/chorale-*) is run through the full chain
 *      (chorale_chain.h) and compared with its RomanText analysis: chord root, chord quality, Roman numeral and
 *      local key, time-weighted.
 *   2. Reference fixtures. The existing hand-annotated checks (cadences, phrase form, texture, rhythm, melody,
 *      motives, the phrase model, schemas) are run as they are, and their whole-chain results are read
 *      (fixtures.h).
 *   3. report.md and report.json are written to the report directory.
 *
 * Options:
 *   --corpus-dir DIR     the chorale corpus (default: tests/accuracy/corpus)
 *   --report-dir DIR     where report.md and report.json go (default: the build directory)
 *   --update-baseline    also write baseline.json and copy the report next to it (tests/accuracy), recording this
 *                        run as the baseline. Raising the baseline is a deliberate act; see docs/testing/accuracy.md.
 *   --baseline FILE      the baseline file (default: tests/accuracy/baseline.json)
 *   --tolerance POINTS   the tolerance written by --update-baseline (default 0.5)
 */

static int compare_names(const void *a, const void *b) {
    return strcmp((const char *)a, (const char *)b);
}

static size_t list_chorales(const char *corpus_dir, char (*names)[ACCURACY_NAME_MAX], size_t capacity) {
    DIR *d = opendir(corpus_dir);
    if (d == NULL) {
        return 0;
    }
    size_t count = 0;
    struct dirent *entry;
    while ((entry = readdir(d)) != NULL && count < capacity) {
        size_t length = strlen(entry->d_name);
        if (strncmp(entry->d_name, "chorale-", 8) == 0 && length < ACCURACY_NAME_MAX) {
            memcpy(names[count], entry->d_name, length + 1);
            count++;
        }
    }
    closedir(d);
    qsort(names, count, ACCURACY_NAME_MAX, compare_names);
    return count;
}

static double seconds_since(const struct timespec *start) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (double)(now.tv_sec - start->tv_sec) + (double)(now.tv_nsec - start->tv_nsec) / 1e9;
}

int main(int argc, char **argv) {
    const char *corpus_dir = URSATZ_ACCURACY_CORPUS_DIR;
    const char *report_dir = URSATZ_ACCURACY_OUT_DIR;
    const char *baseline_path = URSATZ_ACCURACY_DIR "/baseline.json";
    bool update_baseline = false;
    double tolerance = 0.5;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--corpus-dir") == 0 && i + 1 < argc) {
            corpus_dir = argv[++i];
        } else if (strcmp(argv[i], "--report-dir") == 0 && i + 1 < argc) {
            report_dir = argv[++i];
        } else if (strcmp(argv[i], "--baseline") == 0 && i + 1 < argc) {
            baseline_path = argv[++i];
        } else if (strcmp(argv[i], "--tolerance") == 0 && i + 1 < argc) {
            tolerance = atof(argv[++i]);
        } else if (strcmp(argv[i], "--update-baseline") == 0) {
            update_baseline = true;
        } else {
            fprintf(stderr, "accuracy_harness: unknown or incomplete option %s\n", argv[i]);
            return 2;
        }
    }

    struct timespec start;
    clock_gettime(CLOCK_MONOTONIC, &start);
    static AccuracyReport report;
    memset(&report, 0, sizeof(report));

    static char names[24][ACCURACY_NAME_MAX];
    size_t chorales = list_chorales(corpus_dir, names, 24);
    if (chorales == 0) {
        fprintf(stderr, "accuracy_harness: no chorale-* pieces under %s\n", corpus_dir);
        return 2;
    }
    for (size_t i = 0; i < chorales; i++) {
        char dir[1024];
        snprintf(dir, sizeof(dir), "%s/%s", corpus_dir, names[i]);
        ChoraleScore score;
        char message[256] = "";
        struct timespec piece_start;
        clock_gettime(CLOCK_MONOTONIC, &piece_start);
        ChoraleChainStatus status = chorale_chain_run(dir, &score, message, sizeof(message));
        if (status != CHORALE_CHAIN_OK) {
            fprintf(stderr, "accuracy_harness: %s: %s\n", names[i], message);
            return 1;
        }
        PieceReport *p = accuracy_report_add_piece(&report, names[i], "chorale: full chain against its RomanText analysis");
        if (p == NULL) {
            return 1;
        }
        p->num[METRIC_CHORD_ROOT] = score.root_right;
        p->den[METRIC_CHORD_ROOT] = score.chord_time;
        p->num[METRIC_CHORD_QUALITY] = score.quality_right;
        p->den[METRIC_CHORD_QUALITY] = score.chord_time;
        p->num[METRIC_ROMAN_NUMERAL] = score.numeral_right;
        p->den[METRIC_ROMAN_NUMERAL] = score.chord_time;
        p->num[METRIC_ROMAN_NUMERAL_CHAIN_KEY] = score.numeral_chain_key_right;
        p->den[METRIC_ROMAN_NUMERAL_CHAIN_KEY] = score.chord_time;
        p->num[METRIC_KEY] = score.key_right;
        p->den[METRIC_KEY] = score.annotated_time;
        p->has_unsupported_share = true;
        p->unsupported_share = score.annotated_time > 0.0 ? score.unsupported_time / score.annotated_time : 0.0;
        printf("%s: %zu segments, %zu key regions, %.2fs\n", names[i], score.segments, score.key_regions,
               seconds_since(&piece_start));
    }

    char message[256] = "";
    if (!accuracy_collect_fixtures(&report, message, sizeof(message))) {
        fprintf(stderr, "accuracy_harness: fixtures: %s\n", message);
        return 1;
    }

    char path[1100];
    snprintf(path, sizeof(path), "%s/report.md", report_dir);
    char note[256];
    snprintf(note, sizeof(note), "Corpus: %zu chorales (`tests/accuracy/corpus/`, see `LICENSES.md` there).", chorales);
    bool ok = accuracy_write_markdown(&report, path, note);
    snprintf(path, sizeof(path), "%s/report.json", report_dir);
    ok = accuracy_write_json(&report, path) && ok;
    if (!ok) {
        fprintf(stderr, "accuracy_harness: cannot write the report to %s\n", report_dir);
        return 1;
    }

    printf("Overall:\n");
    for (int m = 0; m < METRIC_COUNT; m++) {
        double v = 0.0;
        if (accuracy_overall_value(&report, (MetricId)m, &v)) {
            printf("  %-26s %6.2f%%\n", accuracy_metric_info((MetricId)m)->name, v * 100.0);
        }
    }

    if (update_baseline) {
        if (!accuracy_write_baseline(&report, baseline_path, tolerance)) {
            fprintf(stderr, "accuracy_harness: cannot write %s\n", baseline_path);
            return 1;
        }
        snprintf(path, sizeof(path), "%s/report.md", URSATZ_ACCURACY_DIR);
        ok = accuracy_write_markdown(&report, path, note);
        snprintf(path, sizeof(path), "%s/report.json", URSATZ_ACCURACY_DIR);
        ok = accuracy_write_json(&report, path) && ok;
        if (!ok) {
            fprintf(stderr, "accuracy_harness: cannot write the report snapshot to %s\n", URSATZ_ACCURACY_DIR);
            return 1;
        }
        printf("baseline written to %s (tolerance %.2f points)\n", baseline_path, tolerance);
    }

    printf("accuracy_harness: %zu chorales, %.1fs\n", chorales, seconds_since(&start));
    return 0;
}
