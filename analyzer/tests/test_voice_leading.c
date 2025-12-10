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
 * The voice leading of the report, on a constructed four-part chorale (tests/musicxml/chorale-voice-leading.musicxml,
 * C major, one whole-note chord per measure: I ii V I). It holds one fault of each kind the section flags and nothing
 * else: a parallel fifth between the tenor (voice 2) and the bass (voice 3) from measure 1 to 2, and the soprano's
 * leading tone (voice 0) falling instead of rising to the tonic from measure 3 to 4. Asserted: the four voices, both
 * faults and only those, each at its measure, the doubling of the first two chords, and that the Markdown names both.
 */

static int failures = 0;

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition);          \
            failures++;                                                                            \
        }                                                                                          \
    } while (0)

/* The entry starts at the beginning of the given measure of the 4/4 piece (measure 1 is time 0). */
static bool starts_measure(const AnalysisReport *report, size_t entry, int measure) {
    const AnalysisReportEntry *e = &report->entries[entry];
    return e->time_numerator == (long)(measure - 1) * (long)e->time_denominator;
}

static void check_chorale(const char *dir) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/chorale-voice-leading.musicxml", dir);
    ImportedNotes notes;
    AnalysisReport report;
    if (musicxml_pipeline_import_file(path, &notes) != MUSICXML_PIPELINE_OK || !run_analysis_pipeline(&notes, &report)) {
        fprintf(stderr, "cannot analyze %s\n", path);
        failures++;
        return;
    }
    const AnalysisVoiceLeading *voice_leading = &report.voice_leading;
    CHECK(voice_leading->present);
    CHECK(voice_leading->voice_count == 4);
    CHECK(report.entry_count == 4);
    CHECK(voice_leading->row_count == report.entry_count);
    /* 3 moves between 4 chords, 6 pairs of voices, every voice sounding throughout */
    CHECK(voice_leading->motion_count == 3 * 6);

    CHECK(voice_leading->violation_count == 1);
    if (voice_leading->violation_count == 1) {
        const AnalysisVoiceViolation *violation = &voice_leading->violations[0];
        CHECK(strcmp(violation->rule, "parallel-fifths") == 0);
        CHECK(violation->upper_voice == 2 && violation->lower_voice == 3);
        CHECK(violation->arrival_interval == 5);
        CHECK(starts_measure(&report, violation->entry_from, 1));
        CHECK(starts_measure(&report, violation->entry_to, 2));
    }

    CHECK(voice_leading->resolutions_checked);
    CHECK(voice_leading->resolution_count == 1);
    if (voice_leading->resolution_count == 1) {
        const AnalysisVoiceResolution *resolution = &voice_leading->resolutions[0];
        CHECK(strcmp(resolution->rule, "leading-tone-resolves") == 0);
        CHECK(resolution->voice == 0);
        CHECK(starts_measure(&report, resolution->entry_from, 3));
        CHECK(starts_measure(&report, resolution->entry_to, 4));
    }

    /* C E G C: the root is doubled; D F A A: the fifth; all four voices sound and sit within the library's spacing limits */
    CHECK(voice_leading->chord_count == 4);
    if (voice_leading->chord_count == 4) {
        CHECK(voice_leading->chords[0].doubled_member == 0);
        CHECK(voice_leading->chords[1].doubled_member == 2);
        for (size_t i = 0; i < voice_leading->chord_count; i++) {
            CHECK(voice_leading->chords[i].voices_sounding == 4);
            CHECK(voice_leading->chords[i].spacing_judged);
            CHECK(voice_leading->chords[i].wide_voice < 0);
        }
    }

    AnalysisDocument *document = analysis_document_build(path, &notes, &report);
    char *markdown = document ? markdown_report_render(document) : NULL;
    CHECK(markdown != NULL);
    if (markdown) {
        CHECK(strstr(markdown, "## Voice leading") != NULL);
        CHECK(strstr(markdown, "parallel-fifths") != NULL);
        CHECK(strstr(markdown, "leading-tone-resolves") != NULL);
        CHECK(strstr(markdown, "1 parallel fifth,") != NULL);
    }
    free(markdown);
    analysis_document_destroy(document);
    analysis_report_destroy(&report);
    imported_notes_destroy(&notes);
}

int main(void) {
    check_chorale(URSATZ_MUSICXML_DIR);
    if (failures > 0) {
        fprintf(stderr, "test_voice_leading: %d check(s) failed\n", failures);
        return 1;
    }
    printf("test_voice_leading: ok\n");
    return 0;
}
