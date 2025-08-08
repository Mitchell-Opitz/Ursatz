#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_analyzer/analysis_document.h>
#include <ursatz_analyzer/analysis_pipeline.h>
#include <ursatz_analyzer/midi_pipeline.h>

#include "cJSON.h"

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

static const cJSON *member(const cJSON *object, const char *key) {
    return cJSON_GetObjectItemCaseSensitive(object, key);
}

static double number(const cJSON *object, const char *key) {
    const cJSON *item = member(object, key);
    return cJSON_IsNumber(item) ? item->valuedouble : -1e9;
}

static const char *text(const cJSON *object, const char *key) {
    const cJSON *item = member(object, key);
    return cJSON_IsString(item) ? item->valuestring : "";
}

static double time_of(const cJSON *rational) {
    return number(rational, "num") / number(rational, "den");
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
    CHECK(strcmp(text(family, "framework"), "standard-form-v1") == 0);
    CHECK(strcmp(text(family, "label"), "motive") == 0);
    const cJSON *occurrences = member(family, "occurrences");
    const cJSON *development = member(family, "development");
    int count = cJSON_GetArraySize(occurrences);
    CHECK(count >= 2);
    CHECK(cJSON_GetArraySize(development) == count);
    CHECK(number(family, "note_count") >= 3);
    int index = 0;
    const cJSON *occurrence;
    cJSON_ArrayForEach(occurrence, occurrences) {
        CHECK((int)number(occurrence, "index") == index);
        CHECK(known_kind(text(occurrence, "label")));
        CHECK(index > 0 || strcmp(text(occurrence, "label"), "exact") == 0);
        double score = number(occurrence, "score");
        CHECK(score >= 0.0 && score <= 1.0);
        double uncertainty = number(occurrence, "uncertainty");
        CHECK(uncertainty >= 0.0 && uncertainty <= 1.0 && distance(uncertainty, 1.0 - score) < 1e-9);
        CHECK(cJSON_GetArraySize(member(occurrence, "note_ids")) == (int)number(family, "note_count") ||
              strcmp(text(occurrence, "label"), "exact") != 0);
        const cJSON *id;
        cJSON_ArrayForEach(id, member(occurrence, "note_ids")) {
            CHECK(id->valuedouble >= 0 && id->valuedouble < (double)note_count);
        }
        CHECK(number(member(occurrence, "measures"), "first") <= number(member(occurrence, "measures"), "last"));
        CHECK(number(occurrence, "beat") >= 1.0);
        const cJSON *phrase = member(occurrence, "phrase");
        const cJSON *idea = member(occurrence, "idea");
        CHECK(cJSON_IsNull(phrase) || cJSON_IsNumber(phrase));
        if (cJSON_IsNumber(phrase)) {
            const cJSON *claim = cJSON_GetArrayItem(phrases, (int)phrase->valuedouble);
            CHECK(claim != NULL);
            CHECK(!cJSON_IsNumber(idea) ||
                  (claim && idea->valuedouble < cJSON_GetArraySize(member(claim, "ideas"))));
            CHECK(!claim || number(member(occurrence, "measures"), "first") >= number(member(claim, "measures"), "first"));
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
        const cJSON *source = cJSON_GetArrayItem(occurrences, (int)number(step, "occurrence"));
        CHECK(source != NULL);
        if (!source) {
            continue;
        }
        double start = time_of(member(member(source, "span"), "start"));
        CHECK(start >= previous_time);
        previous_time = start;
        CHECK((int)number(step, "measure") == (int)number(member(source, "measures"), "first"));
        if (number(step, "occurrence") == 0) {
            CHECK(cJSON_IsNull(member(step, "transformation")));
        } else {
            CHECK(strcmp(text(step, "transformation"), text(source, "label")) == 0);
        }
        CHECK(index == 0 ? cJSON_IsNull(member(step, "from")) : number(step, "from") >= 0);
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
                cJSON_ArrayForEach(candidate, member(family, "occurrences")) {
                    if (distance(beats_of(number(member(candidate, "measures"), "first"), number(candidate, "beat")), at) >
                        1.0) {
                        continue;
                    }
                    occurrence_right = occurrence_right || strcmp(text(candidate, "label"), want->occurrences[o].kind) == 0;
                    found = found || (o == 0 && distance(beats_of(number(member(candidate, "measures"), "first"),
                                                                  number(candidate, "beat")),
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

int main(void) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/framework-v1-corpus/Mozart 545.mid", URSATZ_FIXTURE_DIR);
    ImportedNotes notes;
    AnalysisReport report;
    if (midi_pipeline_import_file(path, &notes) != MIDI_PIPELINE_OK || !run_analysis_pipeline(&notes, &report)) {
        fprintf(stderr, "cannot analyze %s\n", path);
        return 1;
    }
    char *json = analysis_document_render(path, &notes, &report);
    cJSON *document = json ? cJSON_Parse(json) : NULL;
    free(json);
    CHECK(document != NULL);
    CHECK(report.motive_count > 0);
    if (document) {
        const cJSON *motives = member(document, "motives");
        CHECK((size_t)cJSON_GetArraySize(motives) == report.motive_count);
        const cJSON *family;
        cJSON_ArrayForEach(family, motives) {
            check_family(family, member(document, "phrases"), notes.count);
        }
        printf("Mozart 545: %d motive families\n", cJSON_GetArraySize(motives));
        AnnotatedFamily families[16];
        snprintf(path, sizeof(path), "%s/annotations/mozart545_motives.txt", URSATZ_FIXTURE_DIR);
        size_t family_count = load_annotation(path, families, 16);
        CHECK(family_count > 0);
        check_annotation(motives, families, family_count);
        cJSON_Delete(document);
    }
    analysis_report_destroy(&report);
    imported_notes_destroy(&notes);
    if (failures > 0) {
        fprintf(stderr, "test_motives: %d failure(s)\n", failures);
        return 1;
    }
    printf("test_motives: ok\n");
    return 0;
}
