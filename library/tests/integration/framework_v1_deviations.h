#ifndef URSATZ_TESTS_INTEGRATION_FRAMEWORK_V1_DEVIATIONS_H
#define URSATZ_TESTS_INTEGRATION_FRAMEWORK_V1_DEVIATIONS_H

/*
 * The deviations of the Framework v1 verification (test glue). tests/fixtures/framework-v1-corpus/expected_deviations.md holds two tables:
 * the CAUSES (method limits of an L2/L3 stage, each with the roadmap item that would remove it) and the DEVIATIONS (one per location where the
 * chain's result is not the expected one: piece, area, location, expected, ACTUAL, cause). The test READS that file:
 *
 *   - a check that passes while the file lists a deviation for its location fails (the deviation is gone: remove the row);
 *   - a check that fails passes as a deviation only when the file lists it AND its actual value is exactly what the chain gives now; any other
 *     failure is a real failure of the test;
 *   - a deviation row no check used fails (the location moved or the area was removed);
 *   - a deviation whose cause is not in the causes table fails.
 *
 * So nothing is skipped and nothing is loosened: a deviation is an exact, reviewed statement of what the chain does, and it breaks when the chain changes.
 */

#define FV1_MAX_DEVIATIONS 400
#define FV1_MAX_CAUSES 40

typedef struct {
    char id[16];
    char piece[24];
    char area[24];
    char location[72];
    char expected[160];
    char actual[480];
    char cause[16];
    bool used;
} Fv1Deviation;

typedef struct {
    char id[16];
    char limit[200];
    char roadmap[160];
    char roadmap_id[24];
} Fv1Cause;

typedef struct {
    Fv1Deviation deviations[FV1_MAX_DEVIATIONS];
    size_t deviation_count;
    Fv1Cause causes[FV1_MAX_CAUSES];
    size_t cause_count;
} Fv1DeviationTable;

/* Splits a markdown table row on '|' into trimmed cells; returns the cell count. */
static size_t fv1_split_row(char *line, char **cells, size_t capacity) {
    size_t n = 0;
    char *p = line;
    if (*p == '|') {
        p++;
    }
    while (*p != '\0' && n < capacity) {
        char *end = strchr(p, '|');
        if (end == NULL) {
            break;
        }
        *end = '\0';
        while (*p == ' ') {
            p++;
        }
        char *tail = end;
        while (tail > p && tail[-1] == ' ') {
            *--tail = '\0';
        }
        cells[n++] = p;
        p = end + 1;
    }
    return n;
}

static void fv1_load_deviations(const char *path, Fv1DeviationTable *table) {
    memset(table, 0, sizeof(*table));
    size_t size = 0;
    unsigned char *text = pipeline_read_file(path, &size);
    for (char *line = strtok((char *)text, "\n"); line != NULL; line = strtok(NULL, "\n")) {
        char *cells[10];
        if (strncmp(line, "| DEV-", 6) == 0) {
            size_t n = fv1_split_row(line, cells, 10);
            assert(n >= 7 && table->deviation_count < FV1_MAX_DEVIATIONS);
            Fv1Deviation *d = &table->deviations[table->deviation_count++];
            snprintf(d->id, sizeof(d->id), "%s", cells[0]);
            snprintf(d->piece, sizeof(d->piece), "%s", cells[1]);
            snprintf(d->area, sizeof(d->area), "%s", cells[2]);
            snprintf(d->location, sizeof(d->location), "%s", cells[3]);
            snprintf(d->expected, sizeof(d->expected), "%s", cells[4]);
            snprintf(d->actual, sizeof(d->actual), "%s", cells[5]);
            snprintf(d->cause, sizeof(d->cause), "%s", cells[6]);
        } else if (line[0] == '|' && line[2] == 'C' && line[3] >= '0' && line[3] <= '9') {
            size_t n = fv1_split_row(line, cells, 10);
            assert(n >= 4 && table->cause_count < FV1_MAX_CAUSES);
            Fv1Cause *c = &table->causes[table->cause_count++];
            snprintf(c->id, sizeof(c->id), "%s", cells[0]);
            snprintf(c->limit, sizeof(c->limit), "%s", cells[1]);
            snprintf(c->roadmap, sizeof(c->roadmap), "%s", cells[2]);
            snprintf(c->roadmap_id, sizeof(c->roadmap_id), "%s", cells[3]);
        }
    }
    free(text);
}

enum { FV1_AREA_KEYS = 0, FV1_AREA_CADENCES, FV1_AREA_PHRASE_FORM, FV1_AREA_RETURNS, FV1_AREA_COUNT };

static const char *const FV1_AREA_NAMES[FV1_AREA_COUNT] = {"keys", "cadences", "phrase-form", "returns"};

typedef struct {
    size_t pass;
    size_t deviation;
    size_t fail;
} Fv1Count;

typedef struct {
    Fv1DeviationTable *table;
    const char *piece;
    Fv1Count counts[FV1_AREA_COUNT];
} Fv1Verifier;

/* One check. `pass` is whether the chain gave the expected value; `actual` is what it gave, as a stable string. */
static void fv1_check(Fv1Verifier *v, int area, const char *location, bool pass, const char *expected, const char *actual) {
    Fv1Deviation *found = NULL;
    for (size_t i = 0; i < v->table->deviation_count; i++) {
        Fv1Deviation *d = &v->table->deviations[i];
        if (strcmp(d->piece, v->piece) == 0 && strcmp(d->area, FV1_AREA_NAMES[area]) == 0 && strcmp(d->location, location) == 0) {
            found = d;
        }
    }
    if (pass) {
        if (found != NULL) {
            found->used = true;
            v->counts[area].fail++;
            printf("  FAIL %s %s %s: now as expected (%s), so %s is stale: remove its row\n", v->piece, FV1_AREA_NAMES[area], location,
                   actual, found->id);
        } else {
            v->counts[area].pass++;
        }
        return;
    }
    if (found != NULL) {
        found->used = true;
        /* the cause cell is one cause id or several, separated by commas; each must be in the causes table */
        bool cause_known = found->cause[0] != '\0';
        char list[16];
        snprintf(list, sizeof(list), "%s", found->cause);
        for (char *id = strtok(list, ","); id != NULL; id = strtok(NULL, ",")) {
            bool known = false;
            for (size_t i = 0; i < v->table->cause_count; i++) {
                known = known || strcmp(v->table->causes[i].id, id) == 0;
            }
            cause_known = cause_known && known;
        }
        if (strcmp(found->actual, actual) == 0 && strcmp(found->expected, expected) == 0 && cause_known) {
            v->counts[area].deviation++;
            return;
        }
        v->counts[area].fail++;
        printf("  FAIL %s %s %s: %s lists expected \"%s\", actual \"%s\", cause %s%s; the chain gives expected \"%s\", actual \"%s\"\n",
               v->piece, FV1_AREA_NAMES[area], location, found->id, found->expected, found->actual, found->cause,
               cause_known ? "" : " (not in the causes table)", expected, actual);
        return;
    }
    v->counts[area].fail++;
    printf("  FAIL %s %s %s: expected \"%s\", actual \"%s\" (no deviation is recorded for it)\n", v->piece, FV1_AREA_NAMES[area],
           location, expected, actual);
    if (getenv("FRAMEWORK_V1_PRINT_DEVIATIONS") != NULL) {
        printf("DEVROW | DEV-??? | %s | %s | %s | %s | %s | C? |\n", v->piece, FV1_AREA_NAMES[area], location, expected, actual);
    }
}

#endif
