#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_analyzer/analysis_pipeline.h>
#include <ursatz_analyzer/analysis_report.h>
#include <ursatz_analyzer/midi_pipeline.h>

/*
 * The texture of the report, against the hand reading of Field 1 (Nocturne No. 1 in E-flat major, in 12/8), the same
 * DRAFT annotation (tests/fixtures/annotations/field1_texture.txt) that Ursatz's tests/integration/test_texture_field1.c
 * scores the texture adapter against. The annotation reads the category (monophony, heterophony, homophony,
 * polyphony); homorhythm and melody and accompaniment are both homophony. The Analyzer runs the same adapter on the same
 * kernel voice separation (one stream per MIDI track, so per hand) with the library's defaults, so the result is the
 * adapter's:
 *
 *   A MEASURE is RIGHT when the category of the passage holding it is the annotated one. Only annotated measures count.
 *   A CHANGE POINT is FOUND when a passage after the first starts within one measure of it.
 *
 * Asserted: at least 80% of the annotated measures right and every annotated change point found, as Ursatz asserts,
 * and the structure of the report: the streams are the file's two tracks, the passages tile the measures in time
 * order, there is a change between each consecutive pair, and the notes of the passages are in time order.
 */

#define MEASURE_TARGET 0.80

static int failures = 0;

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition);          \
            failures++;                                                                            \
        }                                                                                          \
    } while (0)

typedef struct {
    int first;
    int last;
    char category[32];
} AnnotatedPassage;

typedef struct {
    int range_first;
    int range_last;
    AnnotatedPassage passages[32];
    size_t passage_count;
    int changes[32];
    size_t change_count;
} Annotation;

static bool load_annotation(const char *path, Annotation *a) {
    FILE *file = fopen(path, "r");
    if (!file) {
        return false;
    }
    memset(a, 0, sizeof(*a));
    char line[512];
    while (fgets(line, sizeof(line), file)) {
        int x = 0;
        int y = 0;
        char word[32];
        if (line[0] == '#') {
            continue;
        }
        if (sscanf(line, "range %d %d", &x, &y) == 2) {
            a->range_first = x;
            a->range_last = y;
        } else if (sscanf(line, "passage %d %d %31s", &x, &y, word) == 3 && a->passage_count < 32) {
            a->passages[a->passage_count].first = x;
            a->passages[a->passage_count].last = y;
            snprintf(a->passages[a->passage_count++].category, sizeof(a->passages[0].category), "%s", word);
        } else if (sscanf(line, "change %d", &x) == 1 && a->change_count < 32) {
            a->changes[a->change_count++] = x;
        }
    }
    fclose(file);
    return a->passage_count > 0 && a->range_last >= a->range_first;
}

static const char *annotated_category(const Annotation *a, int measure) {
    for (size_t i = 0; i < a->passage_count; i++) {
        if (measure >= a->passages[i].first && measure <= a->passages[i].last) {
            return a->passages[i].category;
        }
    }
    return NULL;
}

static const char *found_category(const AnalysisReport *report, int measure) {
    for (size_t i = 0; i < report->texture_passage_count; i++) {
        const AnalysisTexturePassage *passage = &report->texture_passages[i];
        if (measure >= passage->first_measure && measure <= passage->last_measure) {
            return passage->category;
        }
    }
    return NULL;
}

static void check_structure(const ImportedNotes *notes, const AnalysisReport *report) {
    size_t highest = 0;
    for (size_t i = 0; i < notes->count; i++) {
        highest = notes->streams[i] > highest ? notes->streams[i] : highest;
    }
    CHECK(highest == 2 || highest == 1); /* a conductor track, if any, and one track per hand */
    CHECK(report->texture_passage_count > 0);
    CHECK(report->texture_change_count + 1 == report->texture_passage_count);
    size_t attacked = 0;
    for (size_t i = 0; i < report->texture_passage_count; i++) {
        const AnalysisTexturePassage *passage = &report->texture_passages[i];
        CHECK(passage->last_measure >= passage->first_measure);
        CHECK(passage->windows == passage->last_measure - passage->first_measure + 1 ||
              passage->first_measure == 0); /* a pickup measure is a short window of its own */
        CHECK(passage->uncertainty >= 0.0 && passage->uncertainty <= 1.0);
        CHECK(passage->voices_max >= 1);
        if (i > 0) {
            const AnalysisTexturePassage *before = &report->texture_passages[i - 1];
            CHECK(passage->first_measure == before->last_measure + 1);
            CHECK(strcmp(passage->texture, before->texture) != 0);
            const AnalysisTextureChange *change = &report->texture_changes[i - 1];
            CHECK(change->from == i - 1 && change->to == i);
            CHECK(change->boundary_measure == passage->first_measure);
            CHECK(strcmp(change->from_texture, before->texture) == 0 && strcmp(change->to_texture, passage->texture) == 0);
            CHECK(change->category_changed == (strcmp(change->from_texture, change->to_texture) != 0 &&
                                               strcmp(before->category, passage->category) != 0));
        }
        for (size_t n = 1; n < passage->note_id_count; n++) {
            CHECK(passage->note_ids[n - 1] < passage->note_ids[n]);
        }
        attacked += passage->note_id_count;
    }
    CHECK(attacked == notes->count);
}

int main(void) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/framework-v1-corpus/Field 1.mid", URSATZ_ANALYZER_FIXTURE_DIR);
    ImportedNotes notes;
    AnalysisReport report;
    if (midi_pipeline_import_file(path, &notes) != MIDI_PIPELINE_OK) {
        fprintf(stderr, "cannot import %s\n", path);
        return 1;
    }
    if (!run_analysis_pipeline(&notes, &report)) {
        fprintf(stderr, "cannot analyze %s\n", path);
        imported_notes_destroy(&notes);
        return 1;
    }
    Annotation annotation;
    snprintf(path, sizeof(path), "%s/annotations/field1_texture.txt", URSATZ_ANALYZER_FIXTURE_DIR);
    if (!load_annotation(path, &annotation)) {
        fprintf(stderr, "cannot read %s\n", path);
        analysis_report_destroy(&report);
        imported_notes_destroy(&notes);
        return 1;
    }

    check_structure(&notes, &report);

    size_t scored = 0;
    size_t right = 0;
    for (int measure = annotation.range_first; measure <= annotation.range_last; measure++) {
        const char *want = annotated_category(&annotation, measure);
        const char *got = found_category(&report, measure);
        if (!want || !got) {
            continue;
        }
        scored++;
        if (strcmp(want, got) == 0) {
            right++;
        } else {
            printf("  measure %d: annotated %s, found %s\n", measure, want, got);
        }
    }
    double rate = scored == 0 ? 0.0 : (double)right / (double)scored;
    printf("Field 1: measures with the right category: %zu of %zu (%.1f%%)\n", right, scored, 100.0 * rate);
    CHECK(scored > 0 && rate >= MEASURE_TARGET);

    size_t found_changes = 0;
    for (size_t c = 0; c < annotation.change_count; c++) {
        bool found = false;
        for (size_t i = 1; i < report.texture_passage_count; i++) {
            int start = report.texture_passages[i].first_measure;
            found = found || (start >= annotation.changes[c] - 1 && start <= annotation.changes[c] + 1);
        }
        found_changes += found ? 1 : 0;
        if (!found) {
            printf("  missed change point at measure %d\n", annotation.changes[c]);
        }
    }
    printf("Field 1: change points found: %zu of %zu\n", found_changes, annotation.change_count);
    CHECK(found_changes == annotation.change_count);

    analysis_report_destroy(&report);
    imported_notes_destroy(&notes);
    if (failures > 0) {
        fprintf(stderr, "test_texture: %d failure(s)\n", failures);
        return 1;
    }
    printf("test_texture: ok\n");
    return 0;
}
