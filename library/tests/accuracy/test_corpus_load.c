#include <assert.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "io/musicxml_importer.h"
#include "score_layout.h"

/*
 * Every piece under corpus/ loads: its score imports, its measure layout is read, and its RomanText analysis
 * parses on that layout with the same last measure as the score. Pieces are found by scanning the directory, so
 * adding a piece needs no edit here.
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
    assert(imported.count > 0);

    ScoreLayout layout;
    assert(score_layout_read(score, score_size, &layout) == SCORE_LAYOUT_OK);
    assert(layout.time_top != 0 && layout.measure_count > 4);
    assert(layout.irregular_count == 0);

    snprintf(path, sizeof(path), "%s/%s/analysis.rntxt", URSATZ_ACCURACY_CORPUS_DIR, name);
    RomanTextGrid grid = score_layout_grid(&layout);
    RomanTextAnalysis analysis;
    RomanTextStatus status = romantext_load(path, &grid, &analysis);
    if (status != ROMANTEXT_OK) {
        fprintf(stderr, "%s: %s\n", name, analysis.error_message);
    }
    assert(status == ROMANTEXT_OK);
    assert(analysis.chord_count > 20 && analysis.key_count >= 1);
    assert(analysis.time_signature_top == (int)layout.time_top && analysis.time_signature_bottom == (int)layout.time_bottom);
    assert(analysis.last_measure == layout.measures[layout.measure_count - 1].number);
    printf("%s: %zu notes, %zu measures, %zu chords, %zu keys, %zu unsupported\n", name, imported.count,
           layout.measure_count, analysis.chord_count, analysis.key_count, analysis.unsupported_count);

    romantext_destroy(&analysis);
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
    printf("corpus: %d pieces load\n", pieces);
    return 0;
}
