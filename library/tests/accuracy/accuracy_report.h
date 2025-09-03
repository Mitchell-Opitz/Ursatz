#ifndef URSATZ_TESTS_ACCURACY_ACCURACY_REPORT_H
#define URSATZ_TESTS_ACCURACY_ACCURACY_REPORT_H

#include <stdbool.h>
#include <stdio.h>

/*
 * The accuracy report (test infrastructure): every metric as a ratio num/den per piece, pooled into an overall
 * ratio per metric, and written as report.md, report.json and baseline.json. A metric with no data for a piece
 * (den 0) is "n/a" and does not enter the overall. The overall is the pooled ratio: the sum of the numerators
 * over the sum of the denominators, so a long piece weighs more than a short one (time-weighted metrics sum
 * whole notes; count metrics sum counts).
 */

typedef enum {
    METRIC_CHORD_ROOT = 0,
    METRIC_CHORD_QUALITY,
    METRIC_ROMAN_NUMERAL,
    METRIC_ROMAN_NUMERAL_CHAIN_KEY,
    METRIC_KEY,
    METRIC_CADENCE_RECALL,
    METRIC_CADENCE_TYPE,
    METRIC_CADENCE_PRECISION,
    METRIC_PHRASE_BOUNDARIES,
    METRIC_THEME_TYPES,
    METRIC_EXPANSIONS,
    METRIC_PHRASE_MODEL_LABELS,
    METRIC_MELODY_CLIMAXES,
    METRIC_MELODY_RANGES,
    METRIC_MELODY_LEAPS,
    METRIC_MOTIVE_OCCURRENCES,
    METRIC_RHYTHM_SYNCOPATIONS,
    METRIC_RHYTHM_HYPERDOWNBEATS,
    METRIC_TEXTURE_MEASURES,
    METRIC_TEXTURE_CHANGES,
    METRIC_SCHEMAS_SEQUENCES,
    METRIC_COUNT
} MetricId;

typedef struct {
    const char *name;        /* the key in report.json and baseline.json */
    const char *title;       /* the column heading in report.md */
    const char *group;       /* "chorale" or "fixture" */
    const char *unit;        /* what the numerator and denominator count */
} MetricInfo;

const MetricInfo *accuracy_metric_info(MetricId id);

#define ACCURACY_MAX_PIECES 40
#define ACCURACY_NAME_MAX 64

typedef struct {
    char name[ACCURACY_NAME_MAX];
    char note[160];               /* shown beside the piece in report.md: what it is made from */
    double num[METRIC_COUNT];
    double den[METRIC_COUNT];
    double unsupported_share;     /* chorales: share of the annotated time under an unsupported chord; -1 for others */
    bool has_unsupported_share;
} PieceReport;

typedef struct {
    PieceReport pieces[ACCURACY_MAX_PIECES];
    size_t piece_count;
} AccuracyReport;

PieceReport *accuracy_report_add_piece(AccuracyReport *report, const char *name, const char *note);

/* num/den of metric m for a piece, or false when den is 0. */
bool accuracy_piece_value(const PieceReport *piece, MetricId m, double *value);

/* The pooled value over all pieces. False when no piece has data. */
bool accuracy_overall_value(const AccuracyReport *report, MetricId m, double *value);

bool accuracy_write_markdown(const AccuracyReport *report, const char *path, const char *corpus_note);
bool accuracy_write_json(const AccuracyReport *report, const char *path);

/* baseline.json: the tolerance in percentage points and every metric, overall and per piece. */
bool accuracy_write_baseline(const AccuracyReport *report, const char *path, double tolerance_points);

#endif
