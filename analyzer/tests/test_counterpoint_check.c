#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"

#include <ursatz_analyzer/analysis_document.h>
#include <ursatz_analyzer/analysis_pipeline.h>
#include <ursatz_analyzer/analysis_report.h>
#include <ursatz_analyzer/markdown_report.h>
#include <ursatz_analyzer/musicxml_pipeline.h>

#include "pipeline/counterpoint_pipeline.h"

/*
 * The counterpoint check, on Fux's Dorian cantus firmus exercises (the first and second species exercises of Ursatz's
 * frameworks/standard-counterpoint/tests/first_species_test_util.h and second_species_test_util.h, written as MusicXML
 * in tests/counterpoint/ with the cantus firmus in the lower part): each species has one exercise that breaks no hard
 * rule and one with a parallel fifth. Asserted: the library's verdict and the group that carries the fifth, the document
 * and the Markdown of a check, that a check that is off leaves the section null and absent, that voices which are not an
 * exercise of the species report a shape error, and that input that is not two voices has no check.
 */

static int failures = 0;

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition);          \
            failures++;                                                                            \
        }                                                                                          \
    } while (0)

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

static bool has_violation(const AnalysisCounterpointCheck *check, const char *group, bool hard) {
    for (size_t i = 0; i < check->violation_count; i++) {
        if (strcmp(check->violations[i].group, group) == 0 && check->violations[i].hard == hard) {
            return true;
        }
    }
    return false;
}

static bool has_hard_violation(const AnalysisCounterpointCheck *check) {
    for (size_t i = 0; i < check->violation_count; i++) {
        if (check->violations[i].hard) {
            return true;
        }
    }
    return false;
}

/* Checks the file as the species and leaves the report for the caller. */
static bool check_file(const char *name, int species, bool cantus_upper, ImportedNotes *notes, AnalysisReport *report,
                       CounterpointCheckResult *result) {
    if (!analyze(URSATZ_COUNTERPOINT_DIR, name, notes, report)) {
        return false;
    }
    *result = counterpoint_check_collect(notes, species, cantus_upper, report);
    return true;
}

static void check_exercise(const char *name, int species, bool valid, const char *fifth_group) {
    ImportedNotes notes;
    AnalysisReport report;
    CounterpointCheckResult result;
    if (!check_file(name, species, false, &notes, &report, &result)) {
        return;
    }
    const AnalysisCounterpointCheck *check = &report.counterpoint_check;
    CHECK(result == COUNTERPOINT_CHECK_OK);
    CHECK(check->present && check->species == species && !check->cantus_upper);
    CHECK(check->shape_error == NULL);
    CHECK(check->framework && strcmp(check->framework, "standard-counterpoint-v1") == 0);
    CHECK(check->valid == valid);
    CHECK(check->uncertainty == 0); /* two parts: the input's own streams are the voices */
    CHECK(check->note_id_count == notes.count);
    CHECK(has_hard_violation(check) == !valid);
    if (fifth_group) {
        CHECK(has_violation(check, fifth_group, true));
    }
    analysis_report_destroy(&report);
    imported_notes_destroy(&notes);
}

static const cJSON *member(const cJSON *object, const char *key) {
    return cJSON_GetObjectItemCaseSensitive(object, key);
}

/* The document and the Markdown of a check with a parallel fifth, and of the same piece with the check off. */
static void check_output(void) {
    ImportedNotes notes;
    AnalysisReport report;
    CounterpointCheckResult result;
    if (!check_file("first-species-parallel-fifth.musicxml", 1, false, &notes, &report, &result)) {
        return;
    }
    char *json = analysis_document_render("first-species-parallel-fifth.musicxml", &notes, &report);
    AnalysisDocument *document = analysis_document_build("first-species-parallel-fifth.musicxml", &notes, &report);
    char *markdown = document ? markdown_report_render(document) : NULL;
    cJSON *parsed = json ? cJSON_Parse(json) : NULL;
    const cJSON *check = parsed ? member(parsed, "counterpoint_check") : NULL;
    CHECK(cJSON_IsObject(check));
    if (cJSON_IsObject(check)) {
        CHECK(cJSON_GetNumberValue(member(check, "species")) == 1);
        CHECK(strcmp(cJSON_GetStringValue(member(check, "cantus")), "lower") == 0);
        CHECK(cJSON_IsFalse(member(check, "valid")));
        CHECK(cJSON_IsNull(member(check, "shape_error")));
        const cJSON *violations = member(check, "violations");
        bool found = false;
        const cJSON *violation;
        cJSON_ArrayForEach(violation, violations) {
            const char *group = cJSON_GetStringValue(member(violation, "group"));
            const char *severity = cJSON_GetStringValue(member(violation, "severity"));
            found = found || (group && severity && strcmp(group, "harmony") == 0 && strcmp(severity, "hard") == 0);
            CHECK(cJSON_IsObject(member(violation, "location")));
            CHECK(cJSON_IsArray(member(violation, "note_ids")));
            CHECK(strcmp(cJSON_GetStringValue(member(violation, "framework")), "standard-counterpoint-v1") == 0);
        }
        CHECK(found);
    }
    CHECK(markdown != NULL);
    if (markdown) {
        CHECK(strstr(markdown, "## Counterpoint check") != NULL);
        CHECK(strstr(markdown, "**Verdict:** not valid.") != NULL);
        CHECK(strstr(markdown, "| harmony | hard |") != NULL);
    }
    cJSON_Delete(parsed);
    free(markdown);
    free(json);
    analysis_document_destroy(document);
    analysis_report_destroy(&report);
    imported_notes_destroy(&notes);

    /* the check off: null in the document, no section in the report */
    if (!analyze(URSATZ_COUNTERPOINT_DIR, "first-species-parallel-fifth.musicxml", &notes, &report)) {
        return;
    }
    json = analysis_document_render("first-species-parallel-fifth.musicxml", &notes, &report);
    document = analysis_document_build("first-species-parallel-fifth.musicxml", &notes, &report);
    markdown = document ? markdown_report_render(document) : NULL;
    parsed = json ? cJSON_Parse(json) : NULL;
    CHECK(parsed && cJSON_IsNull(member(parsed, "counterpoint_check")));
    CHECK(markdown && strstr(markdown, "Counterpoint check") == NULL);
    cJSON_Delete(parsed);
    free(markdown);
    free(json);
    analysis_document_destroy(document);
    analysis_report_destroy(&report);
    imported_notes_destroy(&notes);
}

/* The first species exercise is not a fourth species one (it has no half rest and no syncopation), and either voice can be the
   cantus firmus. */
static void check_shape_and_cantus(void) {
    ImportedNotes notes;
    AnalysisReport report;
    CounterpointCheckResult result;
    if (check_file("first-species-valid.musicxml", 4, false, &notes, &report, &result)) {
        const AnalysisCounterpointCheck *check = &report.counterpoint_check;
        CHECK(result == COUNTERPOINT_CHECK_OK && check->present);
        CHECK(check->shape_error != NULL);
        CHECK(!check->valid && check->violation_count == 0);
        analysis_report_destroy(&report);
        imported_notes_destroy(&notes);
    }
    if (check_file("first-species-valid.musicxml", 1, true, &notes, &report, &result)) {
        const AnalysisCounterpointCheck *check = &report.counterpoint_check;
        CHECK(result == COUNTERPOINT_CHECK_OK && check->present && check->cantus_upper);
        CHECK(check->shape_error == NULL);
        analysis_report_destroy(&report);
        imported_notes_destroy(&notes);
    }
}

/* One voice, three voices and four voices are not two voices, and nothing is written to the report. */
static void check_not_two_voices(void) {
    static const char *const FILES[] = {"two-parts.musicxml", "bach-114.musicxml"};
    for (size_t i = 0; i < sizeof(FILES) / sizeof(FILES[0]); i++) {
        ImportedNotes notes;
        AnalysisReport report;
        if (!analyze(URSATZ_MUSICXML_DIR, FILES[i], &notes, &report)) {
            continue;
        }
        CHECK(counterpoint_check_collect(&notes, 1, false, &report) == COUNTERPOINT_CHECK_NOT_TWO_VOICES);
        CHECK(!report.counterpoint_check.present);
        analysis_report_destroy(&report);
        imported_notes_destroy(&notes);
    }
}

int main(void) {
    check_exercise("first-species-valid.musicxml", 1, true, NULL);
    check_exercise("first-species-parallel-fifth.musicxml", 1, false, "harmony");
    check_exercise("second-species-valid.musicxml", 2, true, NULL);
    check_exercise("second-species-parallel-fifth.musicxml", 2, false, "strong-beats");
    check_output();
    check_shape_and_cantus();
    check_not_two_voices();
    if (failures > 0) {
        fprintf(stderr, "test_counterpoint_check: %d check(s) failed\n", failures);
        return 1;
    }
    printf("test_counterpoint_check: ok\n");
    return 0;
}
