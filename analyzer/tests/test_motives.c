#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_analyzer/analysis_document.h>
#include <ursatz_analyzer/analysis_pipeline.h>
#include <ursatz_analyzer/midi_pipeline.h>

#include "cJSON.h"

#include "test_support.h"

/*
 * The motive families of the report, for Mozart 545.
 *
 * Structure first: every family has at least two occurrences, the prototype first and "exact", each occurrence a claim
 * with a kind of the library's, a score in [0, 1], and the phrase and idea it lies in (when it lies in one); the
 * development of a family lists its occurrences in time order, the prototype without a transformation and every other
 * one with the kind of its occurrence. The occurrences themselves are not always in time order: the library finds
 * fragments in continuations first, so a prototype can come after its fragments.
 *
 * Then the families Ursatz's tests/integration/test_motive_mozart545.c asserts, against the same hand annotation
 * (tests/fixtures/annotations/mozart545_motives.txt, a DRAFT reading of the exposition, measures 1 to 28): a family is
 * found when the report has a family with an occurrence starting within one beat of the annotated family's first
 * occurrence, and an occurrence is found with the right kind when the report has an occurrence within one beat of it
 * of the annotated kind (exact, real or tonal transposition). Every annotated family must be found, and at least 75%
 * of the annotated occurrences with the right kind. The Analyzer finds the motives over the whole piece with its own
 * cadences, not over the exposition with annotated phrase ends, so its numbers are its own.
 */

#define TARGET_OCCURRENCES 0.75
#define BEATS_PER_MEASURE 4.0

typedef struct {
    int measure;
    double beat;
    const char *kind; /* the library's kind name */
} AnnotatedOccurrence;

typedef struct {
    char name[48];
    AnnotatedOccurrence occurrences[16];
    size_t occurrence_count;
} AnnotatedFamily;

static int failures = 0;

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition);          \
            failures++;                                                                            \
        }                                                                                          \
    } while (0)

static double time_of(const cJSON *rational) {
    return test_json_number(rational, "num") / test_json_number(rational, "den");
}

static double beats_of(double measure, double beat) {
    return (measure - 1.0) * BEATS_PER_MEASURE + (beat - 1.0);
}

static double distance(double a, double b) {
    return a > b ? a - b : b - a;
}

/* The annotation: family lines and their occurrence lines, up to 16 families. Returns the count, 0 when unreadable. */
static size_t load_annotation(const char *path, AnnotatedFamily *families, size_t capacity) {
    FILE *file = fopen(path, "r");
    if (!file) {
        return 0;
    }
    size_t count = 0;
    char line[256];
    while (fgets(line, sizeof(line), file)) {
        char word[48];
        if (line[0] == '#' || sscanf(line, "%47s", word) != 1) {
            continue;
        }
        if (strcmp(word, "family") == 0 && count < capacity) {
            memset(&families[count], 0, sizeof(families[count]));
            if (sscanf(line, "family %47s", families[count].name) == 1) {
                count++;
            }
        } else if (strcmp(word, "occurrence") == 0 && count > 0) {
            AnnotatedFamily *family = &families[count - 1];
            AnnotatedOccurrence *occurrence = &family->occurrences[family->occurrence_count];
            char kind[16];
            if (family->occurrence_count < 16 &&
                sscanf(line, "occurrence %d %lf %15s", &occurrence->measure, &occurrence->beat, kind) == 3) {
                occurrence->kind = strcmp(kind, "exact") == 0
                                       ? "exact"
                                       : (strcmp(kind, "real") == 0 ? "real-transposed" : "tonal-transposed");
                family->occurrence_count++;
            }
        }
    }
    fclose(file);
    return count;
}

static bool known_kind(const char *kind) {
    static const char *KINDS[] = {"exact",    "real-transposed", "tonal-transposed", "inverted", "retrograde",
                                  "augmented", "diminished",      "fragmented",       "varied"};
    for (size_t i = 0; i < sizeof(KINDS) / sizeof(KINDS[0]); i++) {
        if (strcmp(kind, KINDS[i]) == 0) {
            return true;
        }
    }
    return false;
}

/* One family's structure: its occurrences, their order, links and development. */
static void check_family(const cJSON *family, const cJSON *phrases, size_t note_count) {
    CHECK(strcmp(test_json_string(family, "framework"), "standard-form-v1") == 0);
    CHECK(strcmp(test_json_string(family, "label"), "motive") == 0);
    const cJSON *occurrences = test_json_member(family, "occurrences");
    const cJSON *development = test_json_member(family, "development");
    int count = cJSON_GetArraySize(occurrences);
    CHECK(count >= 2);
    CHECK(cJSON_GetArraySize(development) == count);
    CHECK(test_json_number(family, "note_count") >= 3);
    int index = 0;
    const cJSON *occurrence;
    cJSON_ArrayForEach(occurrence, occurrences) {
        CHECK((int)test_json_number(occurrence, "index") == index);
        CHECK(known_kind(test_json_string(occurrence, "label")));
        CHECK(index > 0 || strcmp(test_json_string(occurrence, "label"), "exact") == 0);
        double score = test_json_number(occurrence, "score");
        CHECK(score >= 0.0 && score <= 1.0);
        double uncertainty = test_json_number(occurrence, "uncertainty");
        CHECK(uncertainty >= 0.0 && uncertainty <= 1.0 && distance(uncertainty, 1.0 - score) < 1e-9);
        CHECK(cJSON_GetArraySize(test_json_member(occurrence, "note_ids")) == (int)test_json_number(family, "note_count") ||
              strcmp(test_json_string(occurrence, "label"), "exact") != 0);
        const cJSON *id;
        cJSON_ArrayForEach(id, test_json_member(occurrence, "note_ids")) {
            CHECK(id->valuedouble >= 0 && id->valuedouble < (double)note_count);
        }
        CHECK(test_json_number(test_json_member(occurrence, "measures"), "first") <= test_json_number(test_json_member(occurrence, "measures"), "last"));
        CHECK(test_json_number(occurrence, "beat") >= 1.0);
        const cJSON *phrase = test_json_member(occurrence, "phrase");
        const cJSON *idea = test_json_member(occurrence, "idea");
        CHECK(cJSON_IsNull(phrase) || cJSON_IsNumber(phrase));
        if (cJSON_IsNumber(phrase)) {
            const cJSON *claim = cJSON_GetArrayItem(phrases, (int)phrase->valuedouble);
            CHECK(claim != NULL);
            CHECK(!cJSON_IsNumber(idea) ||
                  (claim && idea->valuedouble < cJSON_GetArraySize(test_json_member(claim, "ideas"))));
            CHECK(!claim || test_json_number(test_json_member(occurrence, "measures"), "first") >= test_json_number(test_json_member(claim, "measures"), "first"));
        } else {
            CHECK(cJSON_IsNull(idea));
        }
        index++;
    }
    /* The development: every occurrence once, in time order; the prototype has no transformation, any other its kind. */
    int steps_seen = 0;
    double previous_time = -1.0;
    index = 0;
    const cJSON *step;
    cJSON_ArrayForEach(step, development) {
        const cJSON *source = cJSON_GetArrayItem(occurrences, (int)test_json_number(step, "occurrence"));
        CHECK(source != NULL);
        if (!source) {
            continue;
        }
        double start = time_of(test_json_member(test_json_member(source, "span"), "start"));
        CHECK(start >= previous_time);
        previous_time = start;
        CHECK((int)test_json_number(step, "measure") == (int)test_json_number(test_json_member(source, "measures"), "first"));
        if (test_json_number(step, "occurrence") == 0) {
            CHECK(cJSON_IsNull(test_json_member(step, "transformation")));
        } else {
            CHECK(strcmp(test_json_string(step, "transformation"), test_json_string(source, "label")) == 0);
        }
        CHECK(index == 0 ? cJSON_IsNull(test_json_member(step, "from")) : test_json_number(step, "from") >= 0);
        steps_seen++;
        index++;
    }
    CHECK(steps_seen == count);
}

static void check_annotation(const cJSON *motives, const AnnotatedFamily *families, size_t family_count) {
    size_t families_found = 0;
    size_t occurrences = 0;
    size_t occurrences_right = 0;
    for (size_t a = 0; a < family_count; a++) {
        const AnnotatedFamily *want = &families[a];
        double first = beats_of(want->occurrences[0].measure, want->occurrences[0].beat);
        bool found = false;
        size_t right = 0;
        for (size_t o = 0; o < want->occurrence_count; o++) {
            double at = beats_of(want->occurrences[o].measure, want->occurrences[o].beat);
            bool occurrence_right = false;
            const cJSON *family;
            cJSON_ArrayForEach(family, motives) {
                const cJSON *candidate;
                cJSON_ArrayForEach(candidate, test_json_member(family, "occurrences")) {
                    if (distance(beats_of(test_json_number(test_json_member(candidate, "measures"), "first"), test_json_number(candidate, "beat")), at) >
                        1.0) {
                        continue;
                    }
                    occurrence_right = occurrence_right || strcmp(test_json_string(candidate, "label"), want->occurrences[o].kind) == 0;
                    found = found || (o == 0 && distance(beats_of(test_json_number(test_json_member(candidate, "measures"), "first"),
                                                                  test_json_number(candidate, "beat")),
                                                         first) <= 1.0);
                }
            }
            occurrences++;
            occurrences_right += occurrence_right ? 1 : 0;
            right += occurrence_right ? 1 : 0;
            if (!occurrence_right) {
                printf("    %s: occurrence at measure %d beat %.2f (%s) not found with that kind\n", want->name,
                       want->occurrences[o].measure, want->occurrences[o].beat, want->occurrences[o].kind);
            }
        }
        families_found += found ? 1 : 0;
        printf("  %s: %s, %zu of %zu occurrences found with the right kind\n", want->name, found ? "found" : "NOT FOUND",
               right, want->occurrence_count);
    }
    double rate = occurrences > 0 ? (double)occurrences_right / (double)occurrences : 0.0;
    printf("  families found %zu of %zu, occurrences found with the right kind %zu of %zu (%.1f%%)\n", families_found,
           family_count, occurrences_right, occurrences, 100.0 * rate);
    CHECK(families_found == family_count);
    CHECK(rate >= TARGET_OCCURRENCES);
}

/* The features of a family: parallel arrays of the prototype's length, intervals and steps one shorter, intervals the
   pitch differences, rhythm the durations, a 16 digit fingerprint, and a degree for every note of every occurrence. */
static void check_features(const cJSON *family) {
    const cJSON *features = test_json_member(family, "features");
    CHECK(cJSON_IsObject(features));
    if (!cJSON_IsObject(features)) {
        return;
    }
    const cJSON *notes = test_json_member(features, "notes");
    int count = cJSON_GetArraySize(notes);
    CHECK(count == (int)test_json_number(family, "note_count"));
    CHECK(cJSON_GetArraySize(test_json_member(features, "degrees")) == count);
    CHECK(cJSON_GetArraySize(test_json_member(features, "rhythm")) == count);
    CHECK(cJSON_GetArraySize(test_json_member(features, "ncts")) == count);
    CHECK(cJSON_GetArraySize(test_json_member(features, "intervals")) == count - 1);
    CHECK(cJSON_GetArraySize(test_json_member(features, "steps")) == count - 1);
    CHECK(strcmp(test_json_string(cJSON_GetArrayItem(notes, 0), "onset"), "0/1") == 0);
    for (int k = 0; k + 1 < count; k++) {
        double step = test_json_number(cJSON_GetArrayItem(notes, k + 1), "pitch") - test_json_number(cJSON_GetArrayItem(notes, k), "pitch");
        CHECK(cJSON_GetArrayItem(test_json_member(features, "intervals"), k)->valuedouble == step);
    }
    for (int k = 0; k < count; k++) {
        CHECK(strcmp(cJSON_GetArrayItem(test_json_member(features, "rhythm"), k)->valuestring,
                     test_json_string(cJSON_GetArrayItem(notes, k), "duration")) == 0);
    }
    const char *fingerprint = test_json_string(features, "fingerprint");
    CHECK(strlen(fingerprint) == 16 && strspn(fingerprint, "0123456789abcdef") == 16);
    const cJSON *occurrence;
    cJSON_ArrayForEach(occurrence, test_json_member(family, "occurrences")) {
        CHECK(cJSON_GetArraySize(test_json_member(occurrence, "degrees")) == cJSON_GetArraySize(test_json_member(occurrence, "note_ids")));
    }
}

static char *joined(const cJSON *array, bool numbers) {
    static char buffer[256];
    buffer[0] = '\0';
    const cJSON *item;
    cJSON_ArrayForEach(item, array) {
        size_t length = strlen(buffer);
        if (numbers) {
            snprintf(buffer + length, sizeof buffer - length, "%s%d", length ? " " : "", (int)item->valuedouble);
        } else {
            snprintf(buffer + length, sizeof buffer - length, "%s%s", length ? " " : "", item->valuestring);
        }
    }
    return buffer;
}

/* The first family of Mozart 545 is the opening figure C5 E5 G5 B4 C5 D5 C5 A5, hand-read; some family in the minor
   reads b6; a non-chord tone on a motive note is typed; the fingerprints do not change between two runs. */
static void check_mozart_features(const cJSON *motives, const AnalysisReport *report, const AnalysisReport *again) {
    const cJSON *first = test_json_member(cJSON_GetArrayItem(motives, 0), "features");
    CHECK(strcmp(joined(test_json_member(first, "degrees"), false), "1 3 5 7 1 2 1 6") == 0);
    CHECK(strcmp(joined(test_json_member(first, "intervals"), true), "4 3 -8 1 2 -2 9") == 0);
    CHECK(strcmp(cJSON_GetArrayItem(test_json_member(first, "rhythm"), 0)->valuestring, "1/2") == 0);
    CHECK(test_json_number(first, "tonic_pc") == 0 && !cJSON_IsTrue(test_json_member(first, "minor")));
    CHECK(strcmp(test_json_string(first, "bar"), "1/1") == 0 && strcmp(test_json_string(first, "beat"), "1/4") == 0 &&
          strcmp(test_json_string(first, "pickup"), "0/1") == 0);
    bool minor_b6 = false;
    bool typed = false;
    const cJSON *family;
    cJSON_ArrayForEach(family, motives) {
        const cJSON *features = test_json_member(family, "features");
        const cJSON *item;
        if (cJSON_IsTrue(test_json_member(features, "minor"))) {
            cJSON_ArrayForEach(item, test_json_member(features, "degrees")) {
                minor_b6 = minor_b6 || strcmp(item->valuestring, "b6") == 0;
            }
        }
        cJSON_ArrayForEach(item, test_json_member(features, "ncts")) {
            typed = typed || (cJSON_IsString(item) && strcmp(item->valuestring, "chord-tone") != 0);
        }
    }
    CHECK(minor_b6);
    CHECK(typed);
    CHECK(report->motive_count == again->motive_count);
    for (size_t i = 0; i < report->motive_count && i < again->motive_count; i++) {
        CHECK(report->motives[i].has_features && again->motives[i].has_features);
        CHECK(strcmp(report->motives[i].features.fingerprint, again->motives[i].features.fingerprint) == 0);
    }
}

int main(void) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/framework-v1-corpus/Mozart 545.mid", URSATZ_FIXTURE_DIR);
    ImportedNotes notes;
    AnalysisReport report;
    if (midi_pipeline_import_file(path, &notes) != MIDI_PIPELINE_OK || !run_analysis_pipeline(&notes, &report)) {
        fprintf(stderr, "cannot analyze %s\n", path);
        return 1;
    }
    ImportedNotes notes_again;
    AnalysisReport report_again;
    if (midi_pipeline_import_file(path, &notes_again) != MIDI_PIPELINE_OK ||
        !run_analysis_pipeline(&notes_again, &report_again)) {
        fprintf(stderr, "cannot analyze %s again\n", path);
        return 1;
    }
    char *json = analysis_document_render(path, &notes, &report);
    cJSON *document = json ? cJSON_Parse(json) : NULL;
    free(json);
    CHECK(document != NULL);
    CHECK(report.motive_count > 0);
    if (document) {
        const cJSON *motives = test_json_member(document, "motives");
        CHECK((size_t)cJSON_GetArraySize(motives) == report.motive_count);
        const cJSON *family;
        cJSON_ArrayForEach(family, motives) {
            check_family(family, test_json_member(document, "phrases"), notes.count);
            check_features(family);
        }
        check_mozart_features(motives, &report, &report_again);
        printf("Mozart 545: %d motive families\n", cJSON_GetArraySize(motives));
        AnnotatedFamily families[16];
        snprintf(path, sizeof(path), "%s/annotations/mozart545_motives.txt", URSATZ_FIXTURE_DIR);
        size_t family_count = load_annotation(path, families, 16);
        CHECK(family_count > 0);
        check_annotation(motives, families, family_count);
        cJSON_Delete(document);
    }
    analysis_report_destroy(&report);
    analysis_report_destroy(&report_again);
    imported_notes_destroy(&notes);
    imported_notes_destroy(&notes_again);
    if (failures > 0) {
        fprintf(stderr, "test_motives: %d failure(s)\n", failures);
        return 1;
    }
    printf("test_motives: ok\n");
    return 0;
}
