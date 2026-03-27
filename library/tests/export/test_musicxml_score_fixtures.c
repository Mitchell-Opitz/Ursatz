#include "score_roundtrip_util.h"

/*
 * Each hand-written MusicXML fixture (tests/fixtures/musicxml) is imported, written by musicxml_export_write_score and
 * imported again: pickup, meter changes, key changes, tempo and metronome, dynamics, ties across barlines, two staves,
 * two parts, two voices in one staff, divisions changes, spelling. The chord, transposing and grace/cue fixtures are
 * not round-tripped: a chord is two notes of one voice at one position (an overlap), a transposing part's sounding
 * pitch is not what is written, and grace notes are not read.
 */

static void round_trip_fixture(const char *name) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s.musicxml", URSATZ_MUSICXML_DIR, name);
    MusicXmlImport first;
    MusicXmlImportStatus status = musicxml_import_file(path, &first);
    if (status != MUSICXML_IMPORT_OK) {
        printf("%s: import failed: %s\n", name, first.error_message);
    }
    assert(status == MUSICXML_IMPORT_OK);
    char *xml = rt_round_trip(&first);
    free(xml);
    musicxml_import_destroy(&first);
    printf("%s: round-trips\n", name);
}

int main(void) {
    round_trip_fixture("pickup");
    round_trip_fixture("meter_change");
    round_trip_fixture("key_change");
    round_trip_fixture("tempo_metronome");
    round_trip_fixture("dynamics");
    round_trip_fixture("ties_barline");
    round_trip_fixture("two_staves");
    round_trip_fixture("two_parts");
    round_trip_fixture("two_voices_one_staff");
    round_trip_fixture("divisions_change");
    round_trip_fixture("spelling");
    printf("test_musicxml_score_fixtures: all tests passed\n");
    return 0;
}
