#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_analyzer/analysis_document.h>
#include <ursatz_analyzer/analysis_pipeline.h>
#include <ursatz_analyzer/midi_pipeline.h>
#include <ursatz_analyzer/motive_links.h>

#include "core/rational.h"
#include "pipeline/motive_sketch_map.h"

/*
 * The one mapping from motive features to a MotiveSketch (motive_features_to_sketch). Every family of Mozart 545 is
 * mapped twice: from the report's features, and from the same features read back out of the analysis document, which is
 * how the motive links read a family. The two sketches are equal note for note and in meter and scale, and have the
 * same fingerprint, so the links cache keys stay equal to the ones the report prints. The links path moves a sketch to
 * the key of C before it keeps it; the report's sketch is moved the same way here.
 */

static int failures = 0;

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition);          \
            failures++;                                                                            \
        }                                                                                          \
    } while (0)

static bool same_time(Rational a, Rational b) {
    return rational_cmp(a, b) == 0;
}

static bool transposed_to_c(MotiveSketch *sketch) {
    int shift = sketch->tonic_pc <= 6 ? -sketch->tonic_pc : 12 - sketch->tonic_pc;
    for (size_t i = 0; i < sketch->note_count; i++) {
        sketch->notes[i].pitch += shift;
    }
    sketch->tonic_pc = 0;
    return motive_sketch_validate(sketch);
}

static void check_equal(const MotiveSketch *a, const MotiveSketch *b) {
    CHECK(a->note_count == b->note_count);
    for (size_t i = 0; i < a->note_count && i < b->note_count; i++) {
        CHECK(a->notes[i].pitch == b->notes[i].pitch);
        CHECK(same_time(a->notes[i].onset, b->notes[i].onset));
        CHECK(same_time(a->notes[i].duration, b->notes[i].duration));
    }
    CHECK(a->has_scale == b->has_scale && a->tonic_pc == b->tonic_pc && a->minor == b->minor);
    CHECK(same_time(a->bar, b->bar) && same_time(a->beat, b->beat) && same_time(a->pickup, b->pickup));
    CHECK(same_time(a->length, b->length));
    CHECK(motive_sketch_fingerprint(a) == motive_sketch_fingerprint(b));
}

int main(void) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/framework-v1-corpus/Mozart 545.mid", URSATZ_ANALYZER_FIXTURE_DIR);
    ImportedNotes notes;
    AnalysisReport report;
    if (midi_pipeline_import_file(path, &notes) != MIDI_PIPELINE_OK || !run_analysis_pipeline(&notes, &report)) {
        fprintf(stderr, "cannot analyze %s\n", path);
        return 1;
    }
    char *json = analysis_document_render(path, &notes, &report);
    MotiveLinkScope *scope = motive_link_scope_create();
    CHECK(json != NULL && scope != NULL);
    CHECK(json && scope && motive_link_scope_add_document(scope, "mozart-545", json) == MOTIVE_LINKS_OK);
    CHECK(report.motive_count > 0);
    size_t mapped = 0;
    for (size_t i = 0; i < report.motive_count; i++) {
        const AnalysisMotive *motive = &report.motives[i];
        if (!motive->has_features) {
            continue;
        }
        MotiveSketch from_report;
        MotiveSketch from_document;
        CHECK(motive_features_to_sketch(&motive->features, &from_report));
        char fingerprint[17];
        snprintf(fingerprint, sizeof fingerprint, "%016llx", motive_sketch_fingerprint(&from_report));
        CHECK(strcmp(fingerprint, motive->features.fingerprint) == 0);
        CHECK(transposed_to_c(&from_report));
        CHECK(motive_link_scope_sketch(scope, "mozart-545", i, &from_document));
        check_equal(&from_report, &from_document);
        mapped++;
    }
    CHECK(mapped > 0);
    printf("Mozart 545: %zu motive families mapped both ways\n", mapped);
    motive_link_scope_destroy(scope);
    free(json);
    analysis_report_destroy(&report);
    imported_notes_destroy(&notes);
    return failures == 0 ? 0 : 1;
}
