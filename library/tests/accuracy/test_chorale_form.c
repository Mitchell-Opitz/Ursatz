#include <assert.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "io/import_notes.h"
#include "io/musicxml_importer.h"
#include "score_layout.h"
#include "semantics/analysis_key.h"
#include "ursatz_chain/analysis_chain.h"

/*
 * Every corpus chorale runs the whole analysis chain to the form stage. P41 B6 found chorale 16 failing there (its last note,
 * struck in m16, is held into m17, where no note starts: the phrase grid ends at m16 and a leftover phrase of m17 held no
 * note; R-396). Each piece must give at least one phrase, every phrase holds a note and starts the measure after the one
 * before it ends, and chorale 16's last phrase ends at m17. Pieces are found by scanning the directory.
 */

static char *read_file(const char *path, size_t *size) {
    FILE *f = fopen(path, "rb");
    assert(f != NULL);
    fseek(f, 0, SEEK_END);
    long length = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buffer = (char *)malloc((size_t)length + 1);
    assert(buffer != NULL);
    *size = fread(buffer, 1, (size_t)length, f);
    buffer[*size] = '\0';
    fclose(f);
    return buffer;
}

static void check_piece(const char *name) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s/score.musicxml", URSATZ_ACCURACY_CORPUS_DIR, name);
    size_t score_size = 0;
    char *score = read_file(path, &score_size);

    MusicXmlImport imported;
    assert(musicxml_import_buffer(score, score_size, &imported) == MUSICXML_IMPORT_OK);
    ScoreLayout layout;
    assert(score_layout_read(score, score_size, &layout) == SCORE_LAYOUT_OK);
    MeterMap *map = score_layout_meter_map(&layout, "chorale-form-meter");
    assert(map != NULL);
    AnalysisNote *notes = NULL;
    size_t note_count = 0;
    assert(musicxml_import_analysis_notes(&imported, &notes, &note_count));
    size_t key_count = imported.key_signature_count;
    AnalysisKeyEvent *key_events = (AnalysisKeyEvent *)calloc(key_count + 1, sizeof(AnalysisKeyEvent));
    assert(key_events != NULL);
    for (size_t i = 0; i < key_count; i++) {
        key_events[i].time = imported.key_signatures[i].position;
        assert(analysis_key_from_fifths(imported.key_signatures[i].sharps_flats, KEY_STRUCTURE_MODE_MAJOR, &key_events[i].key));
    }

    AnalysisNoteSet set = {notes, note_count, map};
    AnalysisChainConfig config = analysis_chain_config_default();
    config.first_measure = 0;
    AnalysisChain *chain = NULL;
    AnalysisChainStatus status = analysis_chain_run(&set, key_events, key_count, &config, &chain);
    if (status != ANALYSIS_CHAIN_OK) {
        fprintf(stderr, "%s: the chain failed with status %d\n", name, (int)status);
    }
    assert(status == ANALYSIS_CHAIN_OK);

    const StandardFormPhrases *phrases = analysis_chain_phrases(chain);
    assert(phrases != NULL && phrases->count >= 1);
    printf("%s:", name);
    for (size_t i = 0; i < phrases->count; i++) {
        const StandardFormPhraseClaim *claim = &phrases->items[i].claim;
        assert(claim->note_end > claim->note_first);
        assert(i == 0 || claim->first_measure == phrases->items[i - 1].claim.last_measure + 1);
        printf(" %d-%d", claim->first_measure, claim->last_measure);
    }
    printf("\n");
    if (strcmp(name, "chorale-016-bwv311") == 0) {
        assert(phrases->items[phrases->count - 1].claim.last_measure == 17);
    }

    analysis_chain_destroy(chain);
    free(key_events);
    import_analysis_notes_destroy(notes, note_count);
    meter_map_destroy(map);
    score_layout_destroy(&layout);
    musicxml_import_destroy(&imported);
    free(score);
}

int main(void) {
    DIR *d = opendir(URSATZ_ACCURACY_CORPUS_DIR);
    assert(d != NULL);
    int pieces = 0;
    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        if (strncmp(entry->d_name, "chorale-", 8) != 0) {
            continue;
        }
        check_piece(entry->d_name);
        pieces++;
    }
    closedir(d);
    assert(pieces >= 5);
    printf("chorale form: %d pieces\n", pieces);
    return 0;
}
