#ifndef URSATZ_TESTS_ACCURACY_ACCURACY_GATE_H
#define URSATZ_TESTS_ACCURACY_ACCURACY_GATE_H

#include <stdbool.h>
#include <stddef.h>

#include "accuracy_report.h"

/*
 * The regression gate (test infrastructure). The gate compares the overall value of every metric with
 * baseline.json and fails when a metric has dropped by MORE than the tolerance, in percentage points; the
 * tolerance is stored in baseline.json (default 0.5). An improvement never fails: raising the baseline is a
 * deliberate act (accuracy_harness --update-baseline, its own commit; docs/testing/accuracy.md).
 *
 * The gate also fails on a mismatch of the two sets of metrics, so that nothing leaves the gate unseen: a metric
 * in the baseline that the run did not produce (a piece, a fixture or a summary line went missing), and a metric
 * the run produced that is not in the baseline (record it with --update-baseline). Per-piece values in the baseline
 * are for reading and diffing; only the overall values are gated.
 */

typedef enum {
    GATE_OK = 0,
    GATE_DROPPED,    /* below the baseline by more than the tolerance */
    GATE_MISSING,    /* in the baseline, not in the run */
    GATE_UNRECORDED  /* in the run, not in the baseline */
} GateStatus;

typedef struct {
    char name[48];
    double baseline;   /* fraction, 0 to 1; meaningful unless GATE_UNRECORDED */
    double current;    /* fraction; meaningful unless GATE_MISSING */
    double change_points; /* (current - baseline) * 100; meaningful for GATE_OK and GATE_DROPPED */
    GateStatus status;
} GateLine;

#define GATE_MAX_LINES (METRIC_COUNT + 8)

typedef struct {
    double tolerance_points;
    GateLine lines[GATE_MAX_LINES];
    size_t line_count;
    size_t failures;
} GateResult;

typedef enum {
    GATE_PARSE_OK = 0,
    GATE_PARSE_ERR_NULL_ARGUMENT,
    GATE_PARSE_ERR_FORMAT,       /* not the JSON this file is written in */
    GATE_PARSE_ERR_NO_TOLERANCE,
    GATE_PARSE_ERR_NO_OVERALL
} GateParseStatus;

/* Evaluates the report against the baseline text (the contents of baseline.json). *out is filled on OK. */
GateParseStatus accuracy_gate_evaluate(const char *baseline_json, const AccuracyReport *report, GateResult *out);

/* The file's text, read and evaluated. False when the file cannot be read (message says so). */
bool accuracy_gate_evaluate_file(const char *path, const AccuracyReport *report, GateResult *out, GateParseStatus *status);

/* Prints the table of lines and one verdict line to f. */
void accuracy_gate_print(const GateResult *result, FILE *f);

#endif
