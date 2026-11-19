#include <ursatz_analyzer/text_export.h>

#include <stdlib.h>
#include <string.h>

#include "cJSON.h"

#include <ursatz_analyzer/analysis_document.h>
#include <ursatz_analyzer/input_pipeline.h>
#include <ursatz_analyzer/score_export.h>

#include "export/analysis_document_internal.h"
#include "export/score_export_internal.h"
#include "export/text_export_internal.h"

/* A fact on one line: line breaks in the name or value become spaces. */
static void add_fact(CommonText *out, const AnalyzerTextExportFact *fact) {
    common_text_append(out, fact->name ? fact->name : "");
    common_text_append(out, ": ");
    for (const char *c = fact->value ? fact->value : ""; *c; c++) {
        char one[2] = {(*c == '\n' || *c == '\r') ? ' ' : *c, '\0'};
        common_text_append(out, one);
    }
    common_text_append(out, "\n");
}

static void add_facts(CommonText *out, const AnalyzerTextExportFact *facts, size_t count) {
    for (size_t i = 0; i < count; i++) {
        add_fact(out, &facts[i]);
    }
}

static void add_line(CommonText *out, const char *name, const char *value) {
    AnalyzerTextExportFact fact = {name, value};
    add_fact(out, &fact);
}

/* ---- segment marks ---- */

typedef struct {
    const ScoreTextEvent *event;
} NoteRef;

static int note_ref_compare(const void *left, const void *right) {
    const ScoreTextEvent *a = ((const NoteRef *)left)->event;
    const ScoreTextEvent *b = ((const NoteRef *)right)->event;
    int by_onset = rational_cmp(a->onset, b->onset);
    if (by_onset != 0) {
        return by_onset;
    }
    if (a->part != b->part) {
        return a->part < b->part ? -1 : 1;
    }
    if (a->staff != b->staff) {
        return a->staff < b->staff ? -1 : 1;
    }
    if (a->voice != b->voice) {
        return a->voice < b->voice ? -1 : 1;
    }
    if (a->order != b->order) {
        return a->order < b->order ? -1 : 1;
    }
    return 0;
}

/* The section's note events (rests left out) in time order, as pointers into the score. */
static NoteRef *collect_notes(const ScoreTextScore *score, size_t *count) {
    size_t total = 0;
    for (size_t m = 0; m < score->measure_count; m++) {
        total += score->measures[m].event_count;
    }
    NoteRef *notes = calloc(total ? total : 1, sizeof(NoteRef));
    if (!notes) {
        return NULL;
    }
    *count = 0;
    for (size_t m = 0; m < score->measure_count; m++) {
        for (size_t e = 0; e < score->measures[m].event_count; e++) {
            if (!score->measures[m].events[e].rest) {
                notes[(*count)++].event = &score->measures[m].events[e];
            }
        }
    }
    qsort(notes, *count, sizeof(NoteRef), note_ref_compare);
    return notes;
}

/* Builds the marks for the section's segments. ANALYZER_OK, or ANALYZER_ERR_ARGUMENT for a segment outside the
   notes. *marks is allocated (possibly empty) on OK. */
static AnalyzerStatus build_marks(const ScoreTextScore *score, const AnalyzerTextExportSegment *segments,
                                  size_t segment_count, TextExportMark **marks, size_t *mark_count) {
    *marks = NULL;
    *mark_count = 0;
    if (segment_count == 0) {
        return ANALYZER_OK;
    }
    size_t note_count = 0;
    NoteRef *notes = collect_notes(score, &note_count);
    TextExportMark *built = calloc(segment_count * 2, sizeof(TextExportMark));
    if (!notes || !built) {
        free(notes);
        free(built);
        return ANALYZER_ERR_ALLOC;
    }
    for (size_t i = 0; i < segment_count; i++) {
        const AnalyzerTextExportSegment *segment = &segments[i];
        if (!segment->label || segment->first > segment->last || segment->last >= note_count) {
            free(notes);
            free(built);
            return ANALYZER_ERR_ARGUMENT;
        }
        built[(*mark_count)++] = (TextExportMark){notes[segment->first].event, segment->label, false};
        built[(*mark_count)++] = (TextExportMark){notes[segment->last].event, segment->label, true};
    }
    free(notes);
    *marks = built;
    return ANALYZER_OK;
}

/* ---- one section ---- */

static AnalyzerStatus write_section(CommonText *out, const AnalyzerTextExportSection *section, const KnowledgeSnapshot *knowledge) {
    common_text_appendf(out, "\n## %s\n", section->label);
    add_facts(out, section->facts, section->fact_count);
    if (!section->musicxml) {
        common_text_append(out, "not composed\n");
        return ANALYZER_OK;
    }

    ImportedScore notes;
    if (imported_score_import_buffer((const unsigned char *)section->musicxml, strlen(section->musicxml), IMPORTED_FORMAT_AUTO, &notes, NULL) !=
        IMPORTED_SCORE_OK) {
        return ANALYZER_ERR_LIBRARY;
    }
    AnalyzerOptions options = analyzer_options_default();
    options.knowledge = knowledge;
    AnalyzerResult *result = NULL;
    AnalyzerStatus status = analyzer_analyze(&notes, &options, &result);
    AnalysisDocument *built = NULL;
    ScoreLayout layout;
    memset(&layout, 0, sizeof(layout));
    ScoreTextScore *score = NULL;
    TextExportMark *marks = NULL;
    size_t mark_count = 0;

    /* The document tree and the score layout are made once, from the result and the import: nothing is rendered and read back. */
    if (status == ANALYZER_OK) {
        built = analysis_document_build_from_result(section->label, result);
        status = built ? ANALYZER_OK : ANALYZER_ERR_ALLOC;
    }
    if (status == ANALYZER_OK) {
        status = analyzer_score_layout(&notes, &layout);
    }
    if (status == ANALYZER_OK) {
        score = score_text_from_layout(&layout);
        if (!score) {
            status = ANALYZER_ERR_LIBRARY;
        }
    }
    if (status == ANALYZER_OK) {
        status = build_marks(score, section->segments, section->segment_count, &marks, &mark_count);
    }
    if (status == ANALYZER_OK) {
        size_t first = 0;
        size_t last = score->measure_count - 1;
        text_export_write_summary(out, built->root, score, first, last, true);
        TextExportMarks mark_list = {marks, mark_count};
        text_export_write_measures(out, built->root, score, first, last, &mark_list);
    }

    free(marks);
    score_text_destroy(score);
    score_layout_destroy(&layout);
    analysis_document_destroy(built);
    analyzer_result_destroy(result);
    imported_score_destroy(&notes);
    return status;
}

AnalyzerStatus analyzer_text_export_material(const AnalyzerTextExportMaterial *material, char **text) {
    if (text) {
        *text = NULL;
    }
    if (!material || !material->kind || !material->knowledge || !text) {
        return ANALYZER_ERR_ARGUMENT;
    }
    for (size_t i = 0; i < material->section_count; i++) {
        if (!material->sections[i].label) {
            return ANALYZER_ERR_ARGUMENT;
        }
    }

    CommonText out = {NULL, 0, 0, false};
    common_text_appendf(&out, "# Ursatz export v%d: %s\n", URSATZ_ANALYZER_TEXT_EXPORT_VERSION, material->kind);
    if (material->name) {
        add_line(&out, "name", material->name);
    }
    if (material->id) {
        add_line(&out, "id", material->id);
    }
    common_text_appendf(&out, "version: %d\n", material->version);
    add_facts(&out, material->facts, material->fact_count);
    text_export_write_legend(&out);

    AnalyzerStatus status = ANALYZER_OK;
    for (size_t i = 0; i < material->section_count && status == ANALYZER_OK; i++) {
        status = write_section(&out, &material->sections[i], material->knowledge);
    }
    if (status == ANALYZER_OK && out.failed) {
        status = ANALYZER_ERR_ALLOC;
    }
    if (status != ANALYZER_OK) {
        free(out.data);
        return status;
    }
    *text = out.data;
    return ANALYZER_OK;
}
