#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_analyzer/analysis_document.h>
#include <ursatz_analyzer/analysis_pipeline.h>
#include <ursatz_analyzer/analysis_report.h>
#include <ursatz_analyzer/markdown_report.h>
#include <ursatz_analyzer/musicxml_pipeline.h>

/*
 * The tempo and dynamics markings of the report, on a constructed score (tests/musicxml/markings.musicxml, four
 * measures in 4/4): "Allegro" with quarter = 120 and p in measure 1, the text "cresc." in measure 2, f in measure 3,
 * "rit." and mp in measure 4. Asserted: each marking at its measure, the library's group, rank and change for each, the
 * comparison against the previous level marking, that the markdown names them, and that a file with no markings (the
 * two-part score) reports none.
 */

static int failures = 0;

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition);          \
            failures++;                                                                            \
        }                                                                                          \
    } while (0)

/* The marking starts at the beginning of the given measure of the 4/4 piece (measure 1 is time 0). */
static bool starts_measure(long numerator, unsigned long denominator, int measure) {
    return numerator == (long)(measure - 1) * (long)denominator;
}

static bool analyze(const char *dir, const char *name, ImportedNotes *notes, AnalysisReport *report) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", dir, name);
    if (musicxml_pipeline_import_file(path, notes) != MUSICXML_PIPELINE_OK || !run_analysis_pipeline(notes, report)) {
        fprintf(stderr, "cannot analyze %s\n", path);
        failures++;
        return false;
    }
    return true;
}

static void check_markings(const char *dir) {
    ImportedNotes notes;
    AnalysisReport report;
    if (!analyze(dir, "markings.musicxml", &notes, &report)) {
        return;
    }

    CHECK(report.tempo_marking_count == 2);
    if (report.tempo_marking_count == 2) {
        const AnalysisTempoMarking *allegro = &report.tempo_markings[0];
        CHECK(allegro->text && strcmp(allegro->text, "Allegro") == 0);
        CHECK(starts_measure(allegro->start_numerator, allegro->start_denominator, 1));
        CHECK(starts_measure(allegro->end_numerator, allegro->end_denominator, 4));
        CHECK(allegro->has_metronome && allegro->beat_unit_numerator == 1 && allegro->beat_unit_denominator == 4);
        CHECK(allegro->bpm_numerator == 120 && allegro->bpm_denominator == 1);
        CHECK(allegro->has_rank && allegro->group && strcmp(allegro->group, "fast") == 0);
        CHECK(strcmp(allegro->change, "none") == 0);
        const AnalysisTempoMarking *rit = &report.tempo_markings[1];
        CHECK(rit->text && strcmp(rit->text, "rit.") == 0);
        CHECK(starts_measure(rit->start_numerator, rit->start_denominator, 4));
        CHECK(!rit->has_metronome);
        CHECK(strcmp(rit->change, "gradually-slower") == 0);
    }

    CHECK(report.dynamic_count == 4);
    if (report.dynamic_count == 4) {
        const AnalysisDynamic *p = &report.dynamics[0];
        const AnalysisDynamic *cresc = &report.dynamics[1];
        const AnalysisDynamic *f = &report.dynamics[2];
        const AnalysisDynamic *mp = &report.dynamics[3];
        CHECK(strcmp(p->marking, "p") == 0 && starts_measure(p->start_numerator, p->start_denominator, 1));
        CHECK(p->has_rank && p->rank < 0 && strcmp(p->change, "none") == 0 && p->relation == NULL);
        CHECK(strcmp(cresc->marking, "cresc.") == 0 && starts_measure(cresc->start_numerator, cresc->start_denominator, 2));
        CHECK(!cresc->has_rank && strcmp(cresc->change, "louder") == 0 && cresc->relation == NULL);
        CHECK(strcmp(f->marking, "f") == 0 && starts_measure(f->start_numerator, f->start_denominator, 3));
        CHECK(f->has_rank && f->rank > 0 && f->relation && strcmp(f->relation, "louder") == 0);
        CHECK(strcmp(mp->marking, "mp") == 0 && starts_measure(mp->start_numerator, mp->start_denominator, 4));
        CHECK(mp->relation && strcmp(mp->relation, "quieter") == 0);
    }

    AnalysisDocument *document = analysis_document_build("markings.musicxml", &notes, &report);
    char *markdown = document ? markdown_report_render(document) : NULL;
    CHECK(markdown != NULL);
    if (markdown) {
        CHECK(strstr(markdown, "## Markings") != NULL);
        CHECK(strstr(markdown, "| m. 1, beat 1 | Allegro | 1/4 = 120 | fast | none |") != NULL);
        CHECK(strstr(markdown, "| m. 4, beat 1 | rit. | - | - | gradually-slower |") != NULL);
        CHECK(strstr(markdown, "| m. 3, beat 1 | f | Piano | none | louder |") != NULL);
    }
    free(markdown);
    analysis_document_destroy(document);
    analysis_report_destroy(&report);
    imported_notes_destroy(&notes);
}

static void check_no_markings(const char *dir) {
    ImportedNotes notes;
    AnalysisReport report;
    if (!analyze(dir, "two-parts.musicxml", &notes, &report)) {
        return;
    }
    CHECK(report.tempo_marking_count == 0);
    CHECK(report.dynamic_count == 0);
    AnalysisDocument *document = analysis_document_build("two-parts.musicxml", &notes, &report);
    char *markdown = document ? markdown_report_render(document) : NULL;
    CHECK(markdown != NULL);
    if (markdown) {
        CHECK(strstr(markdown, "The score has no tempo marking.") != NULL);
        CHECK(strstr(markdown, "The score has no dynamics marking.") != NULL);
    }
    free(markdown);
    analysis_document_destroy(document);
    analysis_report_destroy(&report);
    imported_notes_destroy(&notes);
}

int main(void) {
    check_markings(URSATZ_MUSICXML_DIR);
    check_no_markings(URSATZ_MUSICXML_DIR);
    if (failures > 0) {
        fprintf(stderr, "test_markings: %d check(s) failed\n", failures);
        return 1;
    }
    printf("test_markings: ok\n");
    return 0;
}
