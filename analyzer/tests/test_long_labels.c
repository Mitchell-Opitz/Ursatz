#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_analyzer_internal/analysis_document.h>
#include <io/imported_score.h>
#include "test_knowledge.h"
#include "test_support.h"
#include "ursatz_test.h"

/*
 * Labels are owned strings, not fixed 32-byte buffers: a report whose
 * first entry carries labels longer than 31 characters (swapped in
 * through the report API) keeps them intact through the analysis
 * document, and test_release_report frees them.
 *
 * The input is built here, the 11-note C major track of test_analysis_pipeline.c (a C-E-G chord, then D, F, A, B one at
 * a time, then C-E-G-D): it analyzes to 3 entries, chord I in C major.
 */

static const unsigned char TRACK[] = {
    0x00, 0x90, 0x3C, 0x64, /* delta0  note-on  C4 (60) */
    0x00, 0x90, 0x40, 0x64, /* delta0  note-on  E4 (64) */
    0x00, 0x90, 0x43, 0x64, /* delta0  note-on  G4 (67) */
    0x60, 0x80, 0x3C, 0x40, /* delta96 note-off C4 */
    0x00, 0x80, 0x40, 0x40, /* delta0  note-off E4 */
    0x00, 0x80, 0x43, 0x40, /* delta0  note-off G4 */
    0x00, 0x90, 0x3E, 0x64, /* delta0  note-on  D4 (62) */
    0x60, 0x80, 0x3E, 0x40, /* delta96 note-off D4 */
    0x00, 0x90, 0x41, 0x64, /* delta0  note-on  F4 (65) */
    0x60, 0x80, 0x41, 0x40, /* delta96 note-off F4 */
    0x00, 0x90, 0x45, 0x64, /* delta0  note-on  A4 (69) */
    0x60, 0x80, 0x45, 0x40, /* delta96 note-off A4 */
    0x00, 0x90, 0x47, 0x64, /* delta0  note-on  B4 (71) */
    0x60, 0x80, 0x47, 0x40, /* delta96 note-off B4 */
    0x00, 0x90, 0x3C, 0x64, /* delta0  note-on  C4 (60) */
    0x00, 0x90, 0x40, 0x64, /* delta0  note-on  E4 (64) */
    0x00, 0x90, 0x43, 0x64, /* delta0  note-on  G4 (67) */
    0x00, 0x90, 0x4A, 0x64, /* delta0  note-on  D5 (74) */
    0x60, 0x80, 0x3C, 0x40, /* delta96 note-off C4 */
    0x00, 0x80, 0x40, 0x40, /* delta0  note-off E4 */
    0x00, 0x80, 0x43, 0x40, /* delta0  note-off G4 */
    0x00, 0x80, 0x4A, 0x40, /* delta0  note-off D5 */
    0x00, 0xFF, 0x2F, 0x00  /* end of track */
};

static char *copy_string(const char *s) {
    size_t size = strlen(s) + 1;
    char *copy = malloc(size);
    assert(copy != NULL);
    memcpy(copy, s, size);
    return copy;
}

int main(void) {
    const char *key_label = "C major (long key label beyond thirty-one characters)";
    const char *chord_label = "I (long chord label beyond thirty-one characters)";
    assert(strlen(key_label) > 31);
    assert(strlen(chord_label) > 31);

    unsigned char smf[256];
    size_t size = test_smf_type0(smf, 96, TRACK, sizeof(TRACK));
    char *path = test_temp_path("test_long_labels.mid");
    assert(test_write_file(path, (const char *)smf, size));
    ImportedScore notes;
    assert(imported_score_import_file(path, IMPORTED_FORMAT_MIDI, &notes, NULL) == IMPORTED_SCORE_OK);
    ChainResult report;
    assert(test_analyze_report(&notes, &report));
    assert(report.key_region_count > 0);
    assert(report.entry_count > 0);

    free(report.key_regions[report.home_region].label);
    report.key_regions[report.home_region].label = copy_string(key_label);
    free(report.entries[0].key_label);
    free(report.entries[0].chord_label);
    report.entries[0].key_label = copy_string(key_label);
    report.entries[0].chord_label = copy_string(chord_label);

    AnalysisDocument *document = test_build_document("long_labels.mid", &report);
    assert(document != NULL);
    char *json = analyzer_document_to_json(document);
    assert(json);
    assert(strstr(json, key_label));
    assert(strstr(json, chord_label));
    free(json);
    analyzer_document_destroy(document);
    test_release_report(&report);
    assert(report.entries == NULL);
    assert(report.entry_count == 0);
    imported_score_destroy(&notes);
    remove(path);
    free(path);

    printf("test_long_labels: ok\n");
    return 0;
}
