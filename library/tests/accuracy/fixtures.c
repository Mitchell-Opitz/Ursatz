#define _POSIX_C_SOURCE 200809L
#include "fixtures.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int accuracy_run_cadences(void);
int accuracy_run_form(void);
int accuracy_run_texture(void);
int accuracy_run_rhythm(void);
int accuracy_run_melody(void);
int accuracy_run_motive(void);
int accuracy_run_phrase_model(void);
int accuracy_run_schema(void);

/* Runs fn with stdout captured; the captured text is returned (caller frees), or NULL. */
static char *capture(int (*fn)(void)) {
    fflush(stdout);
    int saved = dup(STDOUT_FILENO);
    FILE *tmp = tmpfile();
    if (saved < 0 || tmp == NULL) {
        return NULL;
    }
    dup2(fileno(tmp), STDOUT_FILENO);
    int result = fn();
    fflush(stdout);
    dup2(saved, STDOUT_FILENO);
    close(saved);
    long size = ftell(tmp);
    fseek(tmp, 0, SEEK_SET);
    char *text = (char *)malloc((size_t)size + 1);
    if (text != NULL) {
        size_t got = fread(text, 1, (size_t)size, tmp);
        text[got] = '\0';
    }
    fclose(tmp);
    if (result != 0) {
        free(text);
        return NULL;
    }
    return text;
}

/* The `occurrence`-th line (from 0) of text that contains needle, copied into line. */
static bool find_line(const char *text, const char *needle, int occurrence, char *line, size_t size) {
    const char *p = text;
    while (*p != '\0') {
        const char *eol = strchr(p, '\n');
        size_t length = eol != NULL ? (size_t)(eol - p) : strlen(p);
        if (length < size) {
            char buffer[512];
            if (length >= sizeof(buffer)) {
                length = sizeof(buffer) - 1;
            }
            memcpy(buffer, p, length);
            buffer[length] = '\0';
            if (strstr(buffer, needle) != NULL) {
                if (occurrence == 0) {
                    snprintf(line, size, "%s", buffer);
                    return true;
                }
                occurrence--;
            }
        }
        if (eol == NULL) {
            break;
        }
        p = eol + 1;
    }
    return false;
}

/* The `index`-th "X of Y" pair on a line. */
static bool nth_pair(const char *line, int index, double *x, double *y) {
    for (const char *p = line; *p != '\0'; p++) {
        if (*p < '0' || *p > '9' || (p > line && p[-1] >= '0' && p[-1] <= '9')) {
            continue;
        }
        char *end = NULL;
        double a = strtod(p, &end);
        if (strncmp(end, " of ", 4) != 0 || end[4] < '0' || end[4] > '9') {
            continue;
        }
        double b = strtod(end + 4, &end);
        if (index == 0) {
            *x = a;
            *y = b;
            return true;
        }
        index--;
        p = end - 1;
    }
    return false;
}

static PieceReport *piece_named(AccuracyReport *report, const char *name, const char *note) {
    for (size_t i = 0; i < report->piece_count; i++) {
        if (strcmp(report->pieces[i].name, name) == 0) {
            return &report->pieces[i];
        }
    }
    return accuracy_report_add_piece(report, name, note);
}

static bool set_pair(PieceReport *piece, MetricId m, const char *text, const char *needle, int occurrence, int pair,
                     char *message, size_t message_size) {
    char line[512];
    double x = 0.0;
    double y = 0.0;
    if (!find_line(text, needle, occurrence, line, sizeof(line)) || !nth_pair(line, pair, &x, &y)) {
        snprintf(message, message_size, "no \"N of M\" (pair %d) on the line %d containing \"%s\"", pair, occurrence,
                 needle);
        return false;
    }
    piece->num[m] += x;
    piece->den[m] += y;
    return true;
}

static bool cadences(AccuracyReport *report, char *message, size_t message_size) {
    static const char *const NAMES[5] = {"Bach 114", "Mozart 545", "Clementi 36", "Field 1", "Field 5"};
    char *text = capture(accuracy_run_cadences);
    if (text == NULL) {
        snprintf(message, message_size, "the cadence test failed");
        return false;
    }
    for (int i = 0; i < 5; i++) {
        char line[512];
        double found = 0, annotated = 0, right = 0, found_again = 0, reported = 0, false_positives = 0;
        if (!find_line(text, "  chain: ", i, line, sizeof(line)) ||
            sscanf(line, "  chain: %lf of %lf annotated cadences found, %lf of %lf found with the right type, %lf "
                         "reported in the stretches, %lf false positive(s)",
                   &found, &annotated, &right, &found_again, &reported, &false_positives) != 6) {
            snprintf(message, message_size, "no cadence summary line for %s", NAMES[i]);
            free(text);
            return false;
        }
        PieceReport *p = piece_named(report, NAMES[i], "reference fixture");
        p->num[METRIC_CADENCE_RECALL] += found;
        p->den[METRIC_CADENCE_RECALL] += annotated;
        p->num[METRIC_CADENCE_TYPE] += right;
        p->den[METRIC_CADENCE_TYPE] += found_again;
        p->num[METRIC_CADENCE_PRECISION] += reported - false_positives;
        p->den[METRIC_CADENCE_PRECISION] += reported;
    }
    free(text);
    return true;
}

static bool form(AccuracyReport *report, char *message, size_t message_size) {
    char *text = capture(accuracy_run_form);
    if (text == NULL) {
        snprintf(message, message_size, "the form test failed");
        return false;
    }
    PieceReport *p = piece_named(report, "Five reference pieces, pooled", "phrase form, pooled over the five reference pieces");
    const char *needle = "All five pieces, the whole chain: boundaries";
    bool ok = set_pair(p, METRIC_PHRASE_BOUNDARIES, text, needle, 0, 0, message, message_size) &&
              set_pair(p, METRIC_THEME_TYPES, text, needle, 0, 1, message, message_size) &&
              set_pair(p, METRIC_EXPANSIONS, text, needle, 0, 2, message, message_size);
    free(text);
    return ok;
}

bool accuracy_collect_fixtures(AccuracyReport *report, char *message, size_t message_size) {
    if (!cadences(report, message, message_size) || !form(report, message, message_size)) {
        return false;
    }
    char *text = capture(accuracy_run_texture);
    if (text == NULL) {
        snprintf(message, message_size, "the texture test failed");
        return false;
    }
    PieceReport *p = piece_named(report, "Field 1", "reference fixture");
    bool ok = set_pair(p, METRIC_TEXTURE_MEASURES, text, "measures with the right category", 0, 0, message, message_size) &&
              set_pair(p, METRIC_TEXTURE_CHANGES, text, "change points found", 0, 0, message, message_size);
    free(text);
    if (!ok) {
        return false;
    }

    text = capture(accuracy_run_rhythm);
    if (text == NULL) {
        snprintf(message, message_size, "the rhythm test failed");
        return false;
    }
    p = piece_named(report, "Field 5", "reference fixture");
    ok = set_pair(p, METRIC_RHYTHM_SYNCOPATIONS, text, "syncopations found", 0, 0, message, message_size) &&
         set_pair(p, METRIC_RHYTHM_HYPERDOWNBEATS, text, "hyperdownbeats found", 1, 0, message, message_size);
    free(text);
    if (!ok) {
        return false;
    }

    text = capture(accuracy_run_melody);
    if (text == NULL) {
        snprintf(message, message_size, "the melody test failed");
        return false;
    }
    p = piece_named(report, "Bach 114", "reference fixture");
    ok = set_pair(p, METRIC_MELODY_CLIMAXES, text, "climaxes right", 1, 0, message, message_size) &&
         set_pair(p, METRIC_MELODY_RANGES, text, "climaxes right", 1, 1, message, message_size) &&
         set_pair(p, METRIC_MELODY_LEAPS, text, "climaxes right", 1, 2, message, message_size);
    free(text);
    if (!ok) {
        return false;
    }

    text = capture(accuracy_run_motive);
    if (text == NULL) {
        snprintf(message, message_size, "the motive test failed");
        return false;
    }
    p = piece_named(report, "Mozart 545", "reference fixture");
    ok = set_pair(p, METRIC_MOTIVE_OCCURRENCES, text, "families found", 0, 1, message, message_size);
    free(text);
    if (!ok) {
        return false;
    }

    text = capture(accuracy_run_phrase_model);
    if (text == NULL) {
        snprintf(message, message_size, "the phrase model test failed");
        return false;
    }
    ok = set_pair(p, METRIC_PHRASE_MODEL_LABELS, text, "annotated segments get the right label", 1, 0, message, message_size);
    free(text);
    if (!ok) {
        return false;
    }

    text = capture(accuracy_run_schema);
    if (text == NULL) {
        snprintf(message, message_size, "the schema test failed");
        return false;
    }
    p = piece_named(report, "Galant fixture", "hand-written fixture");
    ok = set_pair(p, METRIC_SCHEMAS_SEQUENCES, text, "the whole chain (outer voices", 0, 0, message, message_size);
    free(text);
    return ok;
}
