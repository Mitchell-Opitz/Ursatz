#include "accuracy_gate.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char name[48];
    double value;
} Pair;

static const char *skip_space(const char *p) {
    while (*p != '\0' && isspace((unsigned char)*p)) {
        p++;
    }
    return p;
}

/* A quoted key at p; copies it and returns the text after the closing quote, or NULL. */
static const char *read_key(const char *p, char *key, size_t size) {
    p = skip_space(p);
    if (*p != '"') {
        return NULL;
    }
    p++;
    size_t n = 0;
    while (*p != '\0' && *p != '"') {
        if (n + 1 >= size) {
            return NULL;
        }
        key[n++] = *p++;
    }
    if (*p != '"') {
        return NULL;
    }
    key[n] = '\0';
    return p + 1;
}

/* The members of the flat object whose '{' is at p: "name": number, ... Returns the text after '}'. */
static const char *read_flat_object(const char *p, Pair *pairs, size_t capacity, size_t *count) {
    p = skip_space(p);
    if (*p != '{') {
        return NULL;
    }
    p++;
    *count = 0;
    for (;;) {
        p = skip_space(p);
        if (*p == '}') {
            return p + 1;
        }
        if (*count > 0) {
            if (*p != ',') {
                return NULL;
            }
            p++;
        }
        if (*count >= capacity) {
            return NULL;
        }
        p = read_key(p, pairs[*count].name, sizeof(pairs[*count].name));
        if (p == NULL) {
            return NULL;
        }
        p = skip_space(p);
        if (*p != ':') {
            return NULL;
        }
        p = skip_space(p + 1);
        char *end = NULL;
        pairs[*count].value = strtod(p, &end);
        if (end == p) {
            return NULL;
        }
        p = end;
        (*count)++;
    }
}

GateParseStatus accuracy_gate_evaluate(const char *baseline_json, const AccuracyReport *report, GateResult *out) {
    if (baseline_json == NULL || report == NULL || out == NULL) {
        return GATE_PARSE_ERR_NULL_ARGUMENT;
    }
    const char *tolerance_at = strstr(baseline_json, "\"tolerance_points\"");
    if (tolerance_at == NULL) {
        return GATE_PARSE_ERR_NO_TOLERANCE;
    }
    char key[48];
    const char *p = read_key(tolerance_at, key, sizeof(key));
    p = p != NULL ? skip_space(p) : NULL;
    if (p == NULL || *p != ':') {
        return GATE_PARSE_ERR_FORMAT;
    }
    char *end = NULL;
    double tolerance = strtod(skip_space(p + 1), &end);
    if (end == skip_space(p + 1) || tolerance < 0.0) {
        return GATE_PARSE_ERR_FORMAT;
    }
    const char *overall_at = strstr(baseline_json, "\"overall\"");
    if (overall_at == NULL) {
        return GATE_PARSE_ERR_NO_OVERALL;
    }
    p = read_key(overall_at, key, sizeof(key));
    p = p != NULL ? skip_space(p) : NULL;
    if (p == NULL || *p != ':') {
        return GATE_PARSE_ERR_FORMAT;
    }
    Pair pairs[GATE_MAX_LINES];
    size_t pair_count = 0;
    if (read_flat_object(p + 1, pairs, GATE_MAX_LINES, &pair_count) == NULL) {
        return GATE_PARSE_ERR_FORMAT;
    }

    memset(out, 0, sizeof(*out));
    out->tolerance_points = tolerance;
    bool seen[METRIC_COUNT] = {false};
    for (size_t i = 0; i < pair_count && out->line_count < GATE_MAX_LINES; i++) {
        GateLine *line = &out->lines[out->line_count++];
        snprintf(line->name, sizeof(line->name), "%s", pairs[i].name);
        line->baseline = pairs[i].value;
        line->status = GATE_MISSING;
        for (int m = 0; m < METRIC_COUNT; m++) {
            if (strcmp(accuracy_metric_info((MetricId)m)->name, pairs[i].name) != 0) {
                continue;
            }
            seen[m] = true;
            double current = 0.0;
            if (accuracy_overall_value(report, (MetricId)m, &current)) {
                line->current = current;
                line->change_points = (current - line->baseline) * 100.0;
                /* the baseline is stored to six decimals, so a drop of exactly the tolerance is not a failure */
                line->status = line->change_points < -tolerance - 1e-6 ? GATE_DROPPED : GATE_OK;
            }
        }
        if (line->status != GATE_OK) {
            out->failures++;
        }
    }
    for (int m = 0; m < METRIC_COUNT && out->line_count < GATE_MAX_LINES; m++) {
        double current = 0.0;
        if (!seen[m] && accuracy_overall_value(report, (MetricId)m, &current)) {
            GateLine *line = &out->lines[out->line_count++];
            snprintf(line->name, sizeof(line->name), "%s", accuracy_metric_info((MetricId)m)->name);
            line->current = current;
            line->status = GATE_UNRECORDED;
            out->failures++;
        }
    }
    return GATE_PARSE_OK;
}

bool accuracy_gate_evaluate_file(const char *path, const AccuracyReport *report, GateResult *out, GateParseStatus *status) {
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        return false;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *text = (char *)malloc((size_t)size + 1);
    if (text == NULL) {
        fclose(f);
        return false;
    }
    size_t got = fread(text, 1, (size_t)size, f);
    text[got] = '\0';
    fclose(f);
    *status = accuracy_gate_evaluate(text, report, out);
    free(text);
    return true;
}

void accuracy_gate_print(const GateResult *result, FILE *f) {
    fprintf(f, "Gate against baseline (tolerance %.2f points):\n", result->tolerance_points);
    for (size_t i = 0; i < result->line_count; i++) {
        const GateLine *line = &result->lines[i];
        switch (line->status) {
        case GATE_OK:
            fprintf(f, "  ok       %-26s baseline %6.2f%%  now %6.2f%%  %+.2f\n", line->name, line->baseline * 100.0,
                    line->current * 100.0, line->change_points);
            break;
        case GATE_DROPPED:
            fprintf(f, "  DROPPED  %-26s baseline %6.2f%%  now %6.2f%%  %+.2f points\n", line->name,
                    line->baseline * 100.0, line->current * 100.0, line->change_points);
            break;
        case GATE_MISSING:
            fprintf(f, "  MISSING  %-26s baseline %6.2f%%  not produced by this run\n", line->name,
                    line->baseline * 100.0);
            break;
        case GATE_UNRECORDED:
            fprintf(f, "  NEW      %-26s now %6.2f%%  not in the baseline (record it with --update-baseline)\n",
                    line->name, line->current * 100.0);
            break;
        }
    }
    if (result->failures == 0) {
        fprintf(f, "accuracy gate: passed\n");
    } else {
        fprintf(f, "accuracy gate: FAILED (%zu metric(s)); a deliberate change is recorded with "
                   "--update-baseline in its own commit\n",
                result->failures);
    }
}
