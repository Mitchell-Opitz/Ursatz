#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "accuracy_gate.h"

/* A report with one chorale-like piece and one fixture piece: chord_root 80% (8 of 10), key 50% (5 of 10), and
   cadence_recall 1 of 4. The other metrics have no data. */
static AccuracyReport make_report(double root_num, double key_num) {
    AccuracyReport report;
    memset(&report, 0, sizeof(report));
    PieceReport *a = accuracy_report_add_piece(&report, "piece-a", "test");
    a->num[METRIC_CHORD_ROOT] = root_num;
    a->den[METRIC_CHORD_ROOT] = 10.0;
    a->num[METRIC_KEY] = key_num;
    a->den[METRIC_KEY] = 10.0;
    PieceReport *b = accuracy_report_add_piece(&report, "piece-b", "test");
    b->num[METRIC_CADENCE_RECALL] = 1.0;
    b->den[METRIC_CADENCE_RECALL] = 4.0;
    return report;
}

static const char *BASELINE =
    "{\n  \"tolerance_points\": 0.50,\n  \"overall\": {\n    \"chord_root\": 0.800000,\n    \"key\": 0.500000,\n"
    "    \"cadence_recall\": 0.250000\n  },\n  \"pieces\": {\n    \"piece-a\": {\n      \"chord_root\": 0.8\n    }\n  }\n}\n";

static const GateLine *line_named(const GateResult *r, const char *name) {
    for (size_t i = 0; i < r->line_count; i++) {
        if (strcmp(r->lines[i].name, name) == 0) {
            return &r->lines[i];
        }
    }
    return NULL;
}

static void test_unchanged_passes(void) {
    AccuracyReport report = make_report(8.0, 5.0);
    GateResult r;
    assert(accuracy_gate_evaluate(BASELINE, &report, &r) == GATE_PARSE_OK);
    assert(r.failures == 0 && r.line_count == 3 && r.tolerance_points == 0.5);
    assert(line_named(&r, "chord_root")->status == GATE_OK);
}

static void test_a_drop_within_the_tolerance_passes(void) {
    /* chord_root 7.96 of 10 = 79.6%: 0.4 points under 80% */
    AccuracyReport report = make_report(7.96, 5.0);
    GateResult r;
    assert(accuracy_gate_evaluate(BASELINE, &report, &r) == GATE_PARSE_OK);
    assert(r.failures == 0);
    const GateLine *line = line_named(&r, "chord_root");
    assert(line->status == GATE_OK && line->change_points < -0.39 && line->change_points > -0.41);
}

static void test_a_drop_beyond_the_tolerance_fails(void) {
    AccuracyReport report = make_report(7.9, 5.0); /* 79%: 1 point under */
    GateResult r;
    assert(accuracy_gate_evaluate(BASELINE, &report, &r) == GATE_PARSE_OK);
    assert(r.failures == 1);
    assert(line_named(&r, "chord_root")->status == GATE_DROPPED);
    assert(line_named(&r, "key")->status == GATE_OK);
}

static void test_a_drop_of_exactly_the_tolerance_passes(void) {
    AccuracyReport report = make_report(7.95, 5.0); /* 79.5%: exactly 0.5 under */
    GateResult r;
    assert(accuracy_gate_evaluate(BASELINE, &report, &r) == GATE_PARSE_OK);
    assert(line_named(&r, "chord_root")->status == GATE_OK);
}

static void test_an_improvement_passes(void) {
    AccuracyReport report = make_report(10.0, 9.0);
    GateResult r;
    assert(accuracy_gate_evaluate(BASELINE, &report, &r) == GATE_PARSE_OK);
    assert(r.failures == 0);
    assert(line_named(&r, "key")->change_points > 39.9);
}

static void test_a_metric_that_went_missing_fails(void) {
    AccuracyReport report = make_report(8.0, 5.0);
    report.pieces[1].den[METRIC_CADENCE_RECALL] = 0.0; /* the fixture gave nothing */
    GateResult r;
    assert(accuracy_gate_evaluate(BASELINE, &report, &r) == GATE_PARSE_OK);
    assert(r.failures == 1 && line_named(&r, "cadence_recall")->status == GATE_MISSING);
}

static void test_a_metric_not_in_the_baseline_fails(void) {
    AccuracyReport report = make_report(8.0, 5.0);
    report.pieces[0].num[METRIC_CHORD_QUALITY] = 5.0;
    report.pieces[0].den[METRIC_CHORD_QUALITY] = 10.0;
    GateResult r;
    assert(accuracy_gate_evaluate(BASELINE, &report, &r) == GATE_PARSE_OK);
    assert(r.failures == 1 && line_named(&r, "chord_quality")->status == GATE_UNRECORDED);
}

static void test_the_tolerance_comes_from_the_baseline(void) {
    const char *wide = "{\"tolerance_points\": 5.0, \"overall\": {\"chord_root\": 0.8, \"key\": 0.5, \"cadence_recall\": 0.25}}";
    AccuracyReport report = make_report(7.5, 5.0); /* 75%: 5 points under */
    GateResult r;
    assert(accuracy_gate_evaluate(wide, &report, &r) == GATE_PARSE_OK);
    assert(r.failures == 0 && r.tolerance_points == 5.0);
    const char *tight = "{\"tolerance_points\": 0.0, \"overall\": {\"chord_root\": 0.8, \"key\": 0.5, \"cadence_recall\": 0.25}}";
    report = make_report(7.99, 5.0);
    assert(accuracy_gate_evaluate(tight, &report, &r) == GATE_PARSE_OK);
    assert(r.failures == 1);
}

static void test_bad_baselines_are_refused(void) {
    AccuracyReport report = make_report(8.0, 5.0);
    GateResult r;
    assert(accuracy_gate_evaluate(NULL, &report, &r) == GATE_PARSE_ERR_NULL_ARGUMENT);
    assert(accuracy_gate_evaluate("{\"overall\": {}}", &report, &r) == GATE_PARSE_ERR_NO_TOLERANCE);
    assert(accuracy_gate_evaluate("{\"tolerance_points\": 0.5}", &report, &r) == GATE_PARSE_ERR_NO_OVERALL);
    assert(accuracy_gate_evaluate("{\"tolerance_points\": 0.5, \"overall\": {\"key\": }}", &report, &r) ==
           GATE_PARSE_ERR_FORMAT);
    assert(accuracy_gate_evaluate("{\"tolerance_points\": -1, \"overall\": {}}", &report, &r) == GATE_PARSE_ERR_FORMAT);
}

static void test_the_file_the_harness_writes_is_read_back(void) {
    AccuracyReport report = make_report(8.0, 5.0);
    const char *path = "accuracy_gate_roundtrip.json";
    assert(accuracy_write_baseline(&report, path, 0.5));
    GateResult r;
    GateParseStatus status;
    assert(accuracy_gate_evaluate_file(path, &report, &r, &status) && status == GATE_PARSE_OK);
    assert(r.failures == 0 && r.line_count == 3);
    remove(path);
    assert(!accuracy_gate_evaluate_file("no/such/baseline.json", &report, &r, &status));
}

int main(void) {
    test_unchanged_passes();
    test_a_drop_within_the_tolerance_passes();
    test_a_drop_beyond_the_tolerance_fails();
    test_a_drop_of_exactly_the_tolerance_passes();
    test_an_improvement_passes();
    test_a_metric_that_went_missing_fails();
    test_a_metric_not_in_the_baseline_fails();
    test_the_tolerance_comes_from_the_baseline();
    test_bad_baselines_are_refused();
    test_the_file_the_harness_writes_is_read_back();
    printf("accuracy gate: all tests passed\n");
    return 0;
}
