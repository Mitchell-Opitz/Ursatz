#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <chain/analysis_chain.h>

#include "model/pitch/pitch_numeric_value.h"

#include <ursatz_analyzer/analysis_document.h>
#include <ursatz_analyzer/analysis_result.h>
#include <ursatz_analyzer/composer_api.h>
#include <ursatz_analyzer/input_pipeline.h>
#include <ursatz_analyzer/style_profile.h>

#include "pipeline/analysis_result_internal.h"
#include "style/style_profile_internal.h"
#include "ursatz_test.h"
#include "test_knowledge.h"

/*
 * The hearing entry and the style reference (composer_api.h) on the five reference pieces:
 *   - the events are not copied: the result's notes point at the caller's events,
 *   - the report equals that of analyzer_analyze over the same notes,
 *   - bad arguments are refused,
 *   - style_profile_add_result gives the statistics of style_profile_add_document_json on the rendered document,
 *   - the distance against a style reference equals that of a second hearing of the same events,
 *   - the violations of a result equal those of the result of analyzer_analyze on the same notes.
 *
 *   test_hearing          the five pieces
 *   test_hearing smoke    Bach 114 only (the valgrind run)
 */

static const char *const PIECES[] = {"Bach 114.mid", "Mozart 545.mid", "Clementi 36.mid", "Field 1.mid", "Field 5.mid"};

/* Report entries and cadences by piece (the order of PIECES): hearing without harmonic spans gives the B4 pin, one chord per
   segment as before Ursatz P39; the default hearing reads the chords on spans (R-378). */
typedef struct {
    size_t entries;
    size_t cadences;
} Reading;
/* The cadences are those claimed at the phrase points since Ursatz P41 (R-390): without spans they were 5, 15, 15, 13, 12 and with
   spans 0, 23, 5, 17, 3; the entries did not move. Since Ursatz P42 (keys placed on phrases, R-397) they are, without spans,
   0, 8, 8, 8, 2 (were 2, 9, 7, 8, 3) and with spans 0, 16, 4, 9, 0 (were 0, 15, 4, 9, 1); the entries did not move. */
static const Reading WITHOUT_SPANS[] = {{155, 0}, {606, 8}, {205, 8}, {268, 8}, {169, 2}};
static const Reading WITH_SPANS[] = {{54, 0}, {210, 16}, {46, 4}, {107, 9}, {82, 0}};

static int tonic_of(const ImportedNotes *notes) {
    int fifths = notes->key_signature.sharps_flats;
    int major = (((fifths * 7) % 12) + 12) % 12;
    return notes->key_signature.is_minor ? (major + 9) % 12 : major;
}

static bool same_rational(StyleRational a, StyleRational b) {
    return a.numerator == b.numerator && a.denominator == b.denominator;
}

static bool same_comparison(const StyleComparison *a, const StyleComparison *b) {
    if (a->overall_present != b->overall_present || (a->overall_present && !same_rational(a->overall, b->overall)) ||
        a->category_count != b->category_count) {
        return false;
    }
    for (int d = 0; d < STYLE_DIMENSION_COUNT; d++) {
        if (a->dimension_present[d] != b->dimension_present[d] ||
            (a->dimension_present[d] && !same_rational(a->dimension[d], b->dimension[d]))) {
            return false;
        }
    }
    for (size_t i = 0; i < a->category_count; i++) {
        if (strcmp(a->categories[i].category, b->categories[i].category) != 0 ||
            a->categories[i].in_a != b->categories[i].in_a || a->categories[i].in_b != b->categories[i].in_b) {
            return false;
        }
    }
    return true;
}

static bool same_statistics(const StyleProfile *a, const StyleProfile *b) {
    StyleStatistic *left = NULL, *right = NULL;
    size_t left_count = 0, right_count = 0;
    bool same = style_profile_statistics(a, &left, &left_count) == STYLE_PROFILE_OK &&
                style_profile_statistics(b, &right, &right_count) == STYLE_PROFILE_OK && left_count == right_count;
    for (size_t i = 0; same && i < left_count; i++) {
        same = strcmp(left[i].category, right[i].category) == 0 && strcmp(left[i].label, right[i].label) == 0 &&
               left[i].count == right[i].count && left[i].total == right[i].total;
    }
    free(left);
    free(right);
    return same && left_count > 0;
}

/* A reference style: the profile of one piece, as a model. */
static StyleModel *model_of_document(const char *json, const char *file) {
    StyleProfile *profile = style_profile_create();
    StyleModel *model = NULL;
    CHECKF(profile && style_profile_add_document_json(profile, json) == STYLE_PROFILE_OK, "reference profile");
    CHECKF(profile && style_model_file_write(profile, "reference", file) == STYLE_MODEL_FILE_OK &&
               style_model_file_read_without(file, NULL, 0, &model, NULL) == STYLE_MODEL_FILE_OK,
           "reference model");
    style_profile_destroy(profile);
    return model;
}

static int *midi_of(const ImportedNotes *notes) {
    int *midi = malloc((notes->count ? notes->count : 1) * sizeof *midi);
    for (size_t i = 0; midi && i < notes->count; i++) {
        midi[i] = (int)pitch_numeric_value_position_numerator(notes->pitch_values[i]);
    }
    return midi;
}

static void check_arguments(const AnalyzerHearing *hearing) {
    AnalyzerResult *result = (AnalyzerResult *)1;
    AnalyzerHearing no_midi = *hearing;
    no_midi.midi = NULL;
    CHECKF(analyzer_hear(&no_midi, test_hearing_defaults(), &result) == ANALYZER_ERR_ARGUMENT && result == NULL, "no midi");
    CHECKF(analyzer_hear(NULL, test_hearing_defaults(), &result) == ANALYZER_ERR_ARGUMENT, "no hearing");
    CHECKF(analyzer_hear(hearing, test_hearing_defaults(), NULL) == ANALYZER_ERR_ARGUMENT, "no out");
    int *bad = malloc(hearing->count * sizeof *bad);
    memcpy(bad, hearing->midi, hearing->count * sizeof *bad);
    bad[0] = 200;
    AnalyzerHearing out_of_range = *hearing;
    out_of_range.midi = bad;
    CHECKF(analyzer_hear(&out_of_range, test_hearing_defaults(), &result) == ANALYZER_ERR_ARGUMENT, "midi 200");
    free(bad);
    AnalyzerHearing bad_key = *hearing;
    bad_key.has_key = true;
    bad_key.key_tonic_pitch_class = 12;
    CHECKF(analyzer_hear(&bad_key, test_hearing_defaults(), &result) == ANALYZER_ERR_ARGUMENT, "tonic 12");
}

static bool same_violations(const AnalyzerViolation *a, const AnalyzerViolation *b, size_t count) {
    for (size_t i = 0; i < count; i++) {
        if (strcmp(a[i].rule, b[i].rule) != 0 || a[i].hard != b[i].hard || a[i].measure != b[i].measure ||
            a[i].beat != b[i].beat || a[i].note_id_count != b[i].note_id_count ||
            memcmp(a[i].note_ids, b[i].note_ids, a[i].note_id_count * sizeof *a[i].note_ids) != 0) {
            return false;
        }
    }
    return true;
}

/* The violations of a heard result equal those of the direct analysis of the same notes, and bad arguments are
   refused. */
static void check_violations(const char *fixture, const AnalyzerResult *heard, const AnalyzerResult *direct) {
    AnalyzerViolation *from_result = NULL, *from_fragment = NULL;
    size_t result_count = 99, fragment_count = 99;
    CHECKF(analyzer_result_violations(heard, &from_result, &result_count) == ANALYZER_OK, "%s: result violations",
           fixture);
    CHECKF(analyzer_result_violations(direct, &from_fragment, &fragment_count) == ANALYZER_OK,
           "%s: direct violations", fixture);
    CHECKF(result_count == fragment_count, "%s: %zu result violations, %zu direct violations", fixture, result_count,
           fragment_count);
    CHECKF((result_count == 0) == (from_result == NULL), "%s: array and count disagree", fixture);
    if (result_count == fragment_count) {
        CHECKF(same_violations(from_result, from_fragment, result_count), "%s: the violations differ", fixture);
    }
    analyzer_violations_free(from_result, result_count);
    analyzer_violations_free(from_fragment, fragment_count);

    AnalyzerViolation *none = (AnalyzerViolation *)1;
    size_t count = 7;
    CHECKF(analyzer_result_violations(NULL, &none, &count) == ANALYZER_ERR_ARGUMENT, "%s: violations of NULL", fixture);
    CHECKF(analyzer_result_violations(heard, NULL, &count) == ANALYZER_ERR_ARGUMENT, "%s: violations without out",
           fixture);
    CHECKF(analyzer_result_violations(heard, &none, NULL) == ANALYZER_ERR_ARGUMENT, "%s: violations without count",
           fixture);
}

static void check_piece(size_t piece, const StyleModel *reference_model) {
    const char *fixture = PIECES[piece];
    char path[1024];
    snprintf(path, sizeof(path), "%s/framework-v1-corpus/%s", URSATZ_FIXTURES_DIR, fixture);
    ImportedNotes notes;
    if (input_pipeline_import_file(path, INPUT_FORMAT_AUTO, &notes) != INPUT_PIPELINE_OK) {
        CHECKF(false, "cannot import %s", path);
        return;
    }
    int *midi = midi_of(&notes);
    AnalyzerHearing hearing = {(const NoteEvent *const *)notes.events,
                               notes.count,
                               midi,
                               notes.meter.meter_map,
                               notes.key_signature.found,
                               tonic_of(&notes),
                               notes.key_signature.is_minor,
                               notes.streams};

    /* The events are the caller's. */
    AnalyzerResult *heard = NULL;
    CHECKF(analyzer_hear(&hearing, test_hearing_defaults(), &heard) == ANALYZER_OK && heard, "%s: hear", fixture);
    if (heard) {
        const AnalysisReport *report = analyzer_result_report(heard);
        CHECKF(report->entry_count == WITH_SPANS[piece].entries && report->cadence_count == WITH_SPANS[piece].cadences,
               "%s: default hearing %zu entries and %zu cadences, expected %zu and %zu", fixture, report->entry_count,
               report->cadence_count, WITH_SPANS[piece].entries, WITH_SPANS[piece].cadences);
    }
    AnalyzerHearingOptions no_spans = test_hearing_options();
    no_spans.analysis.harmonic_spans = false;
    AnalyzerResult *unspanned = NULL;
    CHECKF(analyzer_hear(&hearing, &no_spans, &unspanned) == ANALYZER_OK && unspanned, "%s: hear without spans", fixture);
    if (unspanned) {
        const AnalysisReport *report = analyzer_result_report(unspanned);
        CHECKF(report->entry_count == WITHOUT_SPANS[piece].entries && report->cadence_count == WITHOUT_SPANS[piece].cadences,
               "%s: hearing without spans %zu entries and %zu cadences, expected %zu and %zu", fixture, report->entry_count,
               report->cadence_count, WITHOUT_SPANS[piece].entries, WITHOUT_SPANS[piece].cadences);
    }
    analyzer_result_destroy(unspanned);
    AnalyzerResult *direct = NULL;
    CHECKF(analyzer_analyze(&notes, test_analyzer_defaults(), &direct) == ANALYZER_OK && direct, "%s: analyze", fixture);
    if (!heard || !direct) {
        free(midi);
        imported_notes_destroy(&notes);
        return;
    }
    const ImportedNotes *borrowed = analyzer_result_notes(heard);
    bool same_events = borrowed && borrowed->count == notes.count;
    for (size_t i = 0; same_events && i < notes.count; i++) {
        same_events = borrowed->events[i] == notes.events[i];
    }
    CHECKF(same_events, "%s: the result's notes do not point at the caller's events", fixture);
    const AnalysisReport *heard_report = analyzer_result_report(heard);
    const AnalysisReport *direct_report = analyzer_result_report(direct);
    CHECKF(heard_report->entry_count == direct_report->entry_count &&
               heard_report->key_region_count == direct_report->key_region_count &&
               heard_report->progression_count == direct_report->progression_count,
           "%s: the hearing's report differs from the direct analysis", fixture);

    /* The document of a second hearing of the same events (the notes carry no file source). */
    AnalyzerResult *rendered = NULL;
    CHECKF(analyzer_hear(&hearing, test_hearing_defaults(), &rendered) == ANALYZER_OK && rendered, "%s: hear again", fixture);
    char *json = rendered ? analysis_document_render_result("<fragment>", rendered) : NULL;
    /* The result's profile equals the profile of its document. */
    StyleProfile *from_result = style_profile_create();
    StyleProfile *from_json = style_profile_create();
    CHECKF(from_result && from_json && style_profile_add_result(from_result, heard) == STYLE_PROFILE_OK &&
               style_profile_add_document_json(from_json, json) == STYLE_PROFILE_OK,
           "%s: profiles", fixture);
    CHECKF(from_result && from_json && same_statistics(from_result, from_json),
           "%s: profile of the result differs from the profile of its document", fixture);
    style_profile_destroy(from_result);
    style_profile_destroy(from_json);

    /* The rendered document exists and holds the profile of the heard result; the distance of the result is that of
       a second hearing of the same events. */
    CHECKF(json != NULL, "%s: no rendered document", fixture);
    AnalyzerStyleReference *reference = NULL;
    CHECKF(analyzer_style_reference_create(reference_model, NULL, 0, &reference) == ANALYZER_OK && reference,
           "%s: reference", fixture);
    StyleComparison old_distance = {0}, new_distance = {0};
    CHECKF(analyzer_result_style_distance(rendered, reference, &old_distance) == ANALYZER_OK, "%s: second distance",
           fixture);
    CHECKF(analyzer_result_style_distance(heard, reference, &new_distance) == ANALYZER_OK, "%s: new distance", fixture);
    CHECKF(same_comparison(&old_distance, &new_distance), "%s: the distances differ", fixture);
    AnalyzerStyleReference *unknown = (AnalyzerStyleReference *)1;
    const char *const bad_name[] = {"no-such-category"};
    CHECKF(analyzer_style_reference_create(reference_model, bad_name, 1, &unknown) == ANALYZER_ERR_ARGUMENT && !unknown,
           "%s: unknown category", fixture);
    check_violations(fixture, heard, direct);
    style_comparison_destroy(&old_distance);
    style_comparison_destroy(&new_distance);
    analyzer_style_reference_destroy(reference);
    analyzer_style_reference_destroy(NULL);

    check_arguments(&hearing);

    free(json);
    analyzer_result_destroy(rendered);
    analyzer_result_destroy(direct);
    analyzer_result_destroy(heard);
    free(midi);
    imported_notes_destroy(&notes);
}

int main(int argc, char **argv) {
    bool smoke = argc > 1 && strcmp(argv[1], "smoke") == 0;
    /* The reference style is Bach 114's own profile: short to make, and every category it has is present. */
    char path[1024], file[1024];
    snprintf(path, sizeof(path), "%s/framework-v1-corpus/%s", URSATZ_FIXTURES_DIR, "Bach 114.mid");
    snprintf(file, sizeof(file), "%s/hearing-reference.style.json", URSATZ_HEARING_WORK_DIR);
    ImportedNotes notes;
    StyleModel *model = NULL;
    AnalyzerResult *result = NULL;
    if (input_pipeline_import_file(path, INPUT_FORMAT_AUTO, &notes) == INPUT_PIPELINE_OK &&
        analyzer_analyze(&notes, test_analyzer_defaults(), &result) == ANALYZER_OK) {
        char *json = analysis_document_render_result("<fragment>", result);
        model = json ? model_of_document(json, file) : NULL;
        free(json);
    }
    CHECKF(model != NULL, "reference model");
    analyzer_result_destroy(result);
    imported_notes_destroy(&notes);
    for (size_t i = 0; model && i < (smoke ? 1 : sizeof PIECES / sizeof PIECES[0]); i++) {
        check_piece(i, model);
    }
    style_model_destroy(model);
    return test_finish("test_hearing");
}
