#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "cJSON.h"

#include <ursatz_analyzer/analyzer_api.h>
#include <ursatz_analyzer/analyzer_api.h>

#include "test_support.h"
#include "ursatz_test.h"
#include "test_knowledge.h"

/*
 * The corpus passages the Composer API takes inline (corpus_passages.h, R-619), over Bach 114 analyzed into a folder of the build tree:
 *   - the passages come ranked best first, each at 1/2 or more, with its notes in time order and the chords of the piece
 *     in time order with numerals, places and key labels; max cuts the list,
 *   - a document analyzed by another version makes its passages stale, and they are returned all the same,
 *   - a corpus with nothing analyzed gives no passage and is not an error,
 *   - the refusals: a passage that cannot be read, a numerals text that is not numerals, a corpus that is not one,
 *     NULL arguments and a max of 0,
 *   - the JSON (analyzer_corpus_passages_to_json): an array with the members of the schema, equal to the passages.
 */

static char work_dir[1024];

/* C5 E5 G5 B4, eighth notes at 480 ticks to the quarter: a MIDI file of one track (the motive test_similar_material searches). */
static bool write_motive_midi(const char *path) {
    static const unsigned char pitches[4] = {72, 76, 79, 71};
    unsigned char track[64];
    size_t n = 0;
    for (int i = 0; i < 4; i++) {
        unsigned char on[] = {0x00, 0x90, pitches[i], 80, 0x81, 0x70, 0x80, pitches[i], 0};
        memcpy(track + n, on, sizeof on);
        n += sizeof on;
    }
    static const unsigned char end[] = {0x00, 0xFF, 0x2F, 0x00};
    memcpy(track + n, end, sizeof end);
    n += sizeof end;
    unsigned char file[128];
    size_t size = test_smf_type0(file, 480, track, n);
    return test_write_file(path, (const char *)file, size);
}

/* Copies Bach 114 into a folder of its own and analyzes it into <name>_analyzed; the analyzed folder is the corpus. */
static bool make_corpus(const char *name, char *corpus, size_t size) {
    char input[1200], source[1200], copy[1400];
    snprintf(input, sizeof input, "%s/%s_input", work_dir, name);
    snprintf(corpus, size, "%s/%s_analyzed", work_dir, name);
    mkdir(input, 0777);
    snprintf(source, sizeof source, "%s/framework-v1-corpus/Bach 114.mid", URSATZ_FIXTURES_DIR);
    snprintf(copy, sizeof copy, "%s/Bach 114.mid", input);
    size_t bytes = 0;
    char *data = test_read_file(source, &bytes);
    if (!data || !test_write_file(copy, data, bytes)) {
        return false;
    }
    free(data);
    AnalyzerCorpusOptions run = {input, corpus, false, NULL, NULL, test_knowledge()};
    AnalyzerCorpusSummary summary;
    return analyzer_corpus_run(&run, &summary, NULL) == URSATZ_OK && summary.analyzed == 1;
}

static bool ascending(const AnalyzerCorpusPassage *passage) {
    for (size_t i = 1; i < passage->note_count; i++) {
        if (rational_cmp(passage->notes[i - 1].onset, passage->notes[i].onset) > 0) {
            return false;
        }
    }
    for (size_t i = 1; i < passage->chord_count; i++) {
        const AnalyzerPassageChord *a = &passage->chords[i - 1], *b = &passage->chords[i];
        if (a->measure > b->measure || (a->measure == b->measure && a->beat > b->beat)) {
            return false;
        }
    }
    return true;
}

static void check_passages(const char *what, const AnalyzerCorpusPassages *passages) {
    CHECKF(passages->count > 0, "%s: no passage", what);
    for (size_t i = 0; i < passages->count; i++) {
        const AnalyzerCorpusPassage *passage = &passages->passages[i];
        CHECKF(strcmp(passage->piece, "Bach 114.mid") == 0, "%s %zu: piece %s", what, i, passage->piece);
        CHECKF(rational_cmp(passage->similarity, rational_ratio(1, 2)) >= 0 && rational_cmp(passage->similarity, rational_ratio(1, 1)) <= 0,
               "%s %zu: similarity %lld/%lld", what, i, passage->similarity.num, passage->similarity.den);
        CHECKF(i == 0 || rational_cmp(passages->passages[i - 1].similarity, passage->similarity) >= 0, "%s %zu: not best first", what, i);
        CHECKF(passage->start_measure >= 1 && passage->end_measure >= passage->start_measure, "%s %zu: place", what, i);
        CHECKF(passage->note_count > 0 && passage->notes, "%s %zu: no notes", what, i);
        CHECKF(passage->chord_count > 0 && passage->chords, "%s %zu: no chords", what, i);
        CHECKF(ascending(passage), "%s %zu: notes or chords out of time order", what, i);
        for (size_t n = 0; n < passage->note_count; n++) {
            CHECKF(passage->notes[n].pitch >= 0 && passage->notes[n].pitch <= 127 && passage->notes[n].duration.num > 0, "%s %zu note %zu", what, i, n);
        }
        for (size_t c = 0; c < passage->chord_count; c++) {
            CHECKF(passage->chords[c].numeral && passage->chords[c].numeral[0] && passage->chords[c].key_label, "%s %zu chord %zu", what, i, c);
        }
    }
}

static void test_search(const char *corpus) {
    char passage_file[1200];
    snprintf(passage_file, sizeof passage_file, "%s/motive.mid", work_dir);
    CHECK(write_motive_midi(passage_file));

    AnalyzerCorpusPassages like;
    CHECKF(analyzer_corpus_passages_numerals(corpus, test_knowledge(), "I IV V I", 100, &like) == URSATZ_OK,
           "the numerals search failed");
    check_passages("numerals", &like);
    size_t all = like.count;
    for (size_t i = 0; i < like.count; i++) {
        CHECKF(!like.passages[i].stale, "numerals %zu: a current document is stale", i);
    }
    analyzer_corpus_passages_destroy(&like);
    CHECK(like.passages == NULL && like.count == 0);

    AnalyzerCorpusPassages cut;
    CHECKF(analyzer_corpus_passages_numerals(corpus, test_knowledge(), "I IV V I", 1, &cut) == URSATZ_OK && cut.count == 1 && all >= 1,
           "max 1 gives %zu of %zu", cut.count, all);
    analyzer_corpus_passages_destroy(&cut);

    AnalyzerCorpusPassages motive;
    UrsatzStatus status = analyzer_corpus_passages_like(corpus, test_knowledge(), passage_file, 10, &motive);
    CHECKF(status == URSATZ_OK && motive.count <= 10, "the passage search gives status %d", (int)status);
    if (motive.count > 0) {
        check_passages("passage", &motive);
    }
    analyzer_corpus_passages_destroy(&motive);
}

/* The document is stale when another version analyzed it: its passages carry it, and the others do not. */
static void test_stale(const char *corpus) {
    char path[1300];
    snprintf(path, sizeof path, "%s/Bach 114.mid.analysis.json", corpus);
    size_t size = 0;
    char *text = test_read_file(path, &size);
    if (!text) {
        snprintf(path, sizeof path, "%s/Bach 114.analysis.json", corpus);
        text = test_read_file(path, &size);
    }
    CHECK(text != NULL);
    cJSON *document = text ? cJSON_Parse(text) : NULL;
    free(text);
    cJSON *metadata = document ? cJSON_GetObjectItemCaseSensitive(document, "metadata") : NULL;
    CHECK(cJSON_IsObject(metadata) && cJSON_GetObjectItemCaseSensitive(metadata, "analyzer_version"));
    if (!cJSON_IsObject(metadata)) {
        cJSON_Delete(document);
        return;
    }
    /* The documents of the corpus are the current version's: none is stale yet. */
    char index_path[1300];
    size_t stale_documents = 99;
    snprintf(index_path, sizeof index_path, "%s/corpus-index.json", corpus);
    CHECKF(analyzer_corpus_stale_count(index_path, &stale_documents) == URSATZ_OK && stale_documents == 0, "%zu stale documents before", stale_documents);
    cJSON_ReplaceItemInObjectCaseSensitive(metadata, "analyzer_version", cJSON_CreateString("0.0.0-another"));
    char *changed = cJSON_Print(document);
    CHECK(changed && test_write_file(path, changed, strlen(changed)));
    cJSON_free(changed);
    cJSON_Delete(document);

    AnalyzerCorpusPassages stale;
    CHECKF(analyzer_corpus_passages_numerals(corpus, test_knowledge(), "I IV V I", 100, &stale) == URSATZ_OK && stale.count > 0,
           "a stale document is still searched");
    for (size_t i = 0; i < stale.count; i++) {
        CHECKF(stale.passages[i].stale, "passage %zu of a stale document is not marked stale", i);
    }
    analyzer_corpus_passages_destroy(&stale);

    /* The count sees the one document another version analyzed; a missing index is an I/O error, a file that is not an index a format error. */
    CHECKF(analyzer_corpus_stale_count(index_path, &stale_documents) == URSATZ_OK && stale_documents == 1, "%zu stale documents after", stale_documents);
    char missing[1300];
    snprintf(missing, sizeof missing, "%s/no-such-index.json", corpus);
    CHECKF(analyzer_corpus_stale_count(missing, &stale_documents) == URSATZ_ERR_IO && stale_documents == 0, "a missing index");
    char not_index[1300];
    snprintf(not_index, sizeof not_index, "%s/not-an-index.json", corpus);
    CHECK(test_write_file(not_index, "{}", 2));
    CHECKF(analyzer_corpus_stale_count(not_index, &stale_documents) == URSATZ_ERR_FORMAT && stale_documents == 0, "a file that is no index");
    remove(not_index);
}

static void test_json(const char *corpus) {
    AnalyzerCorpusPassages passages;
    CHECK(analyzer_corpus_passages_numerals(corpus, test_knowledge(), "I IV V I", 2, &passages) == URSATZ_OK && passages.count > 0);
    char *text = analyzer_corpus_passages_to_json(&passages);
    cJSON *array = text ? cJSON_Parse(text) : NULL;
    CHECK(cJSON_IsArray(array) && (size_t)cJSON_GetArraySize(array) == passages.count);
    for (size_t i = 0; array && i < passages.count; i++) {
        const AnalyzerCorpusPassage *passage = &passages.passages[i];
        const cJSON *item = cJSON_GetArrayItem(array, (int)i);
        const cJSON *start = cJSON_GetObjectItemCaseSensitive(item, "start");
        const cJSON *similarity = cJSON_GetObjectItemCaseSensitive(item, "similarity");
        const cJSON *notes = cJSON_GetObjectItemCaseSensitive(item, "notes");
        const cJSON *chords = cJSON_GetObjectItemCaseSensitive(item, "chords");
        CHECK(strcmp(cJSON_GetObjectItemCaseSensitive(item, "piece")->valuestring, passage->piece) == 0);
        CHECK(cJSON_GetObjectItemCaseSensitive(start, "measure")->valuedouble == passage->start_measure &&
              cJSON_GetObjectItemCaseSensitive(start, "beat")->valuedouble == passage->start_beat);
        CHECK(cJSON_GetObjectItemCaseSensitive(similarity, "num")->valuedouble == (double)passage->similarity.num &&
              cJSON_GetObjectItemCaseSensitive(similarity, "den")->valuedouble == (double)passage->similarity.den);
        CHECK(cJSON_IsBool(cJSON_GetObjectItemCaseSensitive(item, "stale")));
        CHECK(cJSON_IsArray(notes) && (size_t)cJSON_GetArraySize(notes) == passage->note_count);
        CHECK(cJSON_IsArray(chords) && (size_t)cJSON_GetArraySize(chords) == passage->chord_count);
        const cJSON *first = cJSON_GetArrayItem(chords, 0);
        CHECK(cJSON_IsString(cJSON_GetObjectItemCaseSensitive(first, "numeral")) && cJSON_IsNumber(cJSON_GetObjectItemCaseSensitive(first, "beat")) &&
              cJSON_IsString(cJSON_GetObjectItemCaseSensitive(first, "key")));
        const cJSON *note = cJSON_GetArrayItem(notes, 0);
        CHECK(cJSON_IsNumber(cJSON_GetObjectItemCaseSensitive(note, "midi")) &&
              cJSON_IsObject(cJSON_GetObjectItemCaseSensitive(note, "onset")) && cJSON_IsObject(cJSON_GetObjectItemCaseSensitive(note, "duration")));
    }
    cJSON_Delete(array);
    free(text);
    analyzer_corpus_passages_destroy(&passages);

    AnalyzerCorpusPassages none = {0};
    text = analyzer_corpus_passages_to_json(&none);
    CHECK(text && strcmp(text, "[]") == 0);
    free(text);
    CHECK(analyzer_corpus_passages_to_json(NULL) == NULL);
}

/* Nothing analyzed in the corpus: no passage, and no error. */
static void test_no_match(void) {
    char input[1200], corpus[1200];
    snprintf(input, sizeof input, "%s/empty_input", work_dir);
    snprintf(corpus, sizeof corpus, "%s/empty_analyzed", work_dir);
    mkdir(input, 0777);
    char broken[1400];
    snprintf(broken, sizeof broken, "%s/broken.mid", input);
    CHECK(test_write_file(broken, "not a midi file", 15));
    AnalyzerCorpusOptions run = {input, corpus, false, NULL, NULL, test_knowledge()};
    AnalyzerCorpusSummary summary;
    CHECK(analyzer_corpus_run(&run, &summary, NULL) == URSATZ_OK && summary.analyzed == 0);
    AnalyzerCorpusPassages none;
    CHECKF(analyzer_corpus_passages_numerals(corpus, test_knowledge(), "I IV V I", 100, &none) == URSATZ_OK && none.count == 0 &&
               none.passages == NULL,
           "an empty corpus is a result with no passage");
    analyzer_corpus_passages_destroy(&none);
}

static void test_refusals(const char *corpus) {
    AnalyzerCorpusPassages out;
    char passage_file[1200];
    snprintf(passage_file, sizeof passage_file, "%s/motive.mid", work_dir);
    CHECK(analyzer_corpus_passages_like(corpus, test_knowledge(), "no-such-file.mid", 10, &out) == URSATZ_ERR_REQUEST && out.count == 0);
    CHECK(analyzer_corpus_passages_numerals(corpus, test_knowledge(), "not numerals", 10, &out) == URSATZ_ERR_REQUEST);
    CHECK(analyzer_corpus_passages_numerals(corpus, test_knowledge(), "I", 10, &out) == URSATZ_ERR_REQUEST);
    CHECK(analyzer_corpus_passages_like("no-such-directory", test_knowledge(), passage_file, 10, &out) == URSATZ_ERR_NOT_FOUND);
    CHECK(analyzer_corpus_passages_numerals("no-such-directory", test_knowledge(), "I IV V I", 10, &out) == URSATZ_ERR_NOT_FOUND);
    CHECK(analyzer_corpus_passages_numerals(NULL, test_knowledge(), "I IV V I", 10, &out) == URSATZ_ERR_REQUEST);
    CHECK(analyzer_corpus_passages_numerals(corpus, NULL, "I IV V I", 10, &out) == URSATZ_ERR_REQUEST);
    CHECK(analyzer_corpus_passages_numerals(corpus, test_knowledge(), NULL, 10, &out) == URSATZ_ERR_REQUEST);
    CHECK(analyzer_corpus_passages_numerals(corpus, test_knowledge(), "I IV V I", 0, &out) == URSATZ_ERR_REQUEST);
    CHECK(analyzer_corpus_passages_numerals(corpus, test_knowledge(), "I IV V I", 10, NULL) == URSATZ_ERR_REQUEST);
    CHECK(analyzer_corpus_passages_like(corpus, test_knowledge(), NULL, 10, &out) == URSATZ_ERR_REQUEST);
    analyzer_corpus_passages_destroy(NULL);
}

int main(void) {
    snprintf(work_dir, sizeof work_dir, "%s", URSATZ_PASSAGES_WORK_DIR);
    char command[1400];
    snprintf(command, sizeof command, "rm -rf '%s'", work_dir);
    if (system(command) != 0) {
        fprintf(stderr, "cannot clear %s\n", work_dir);
    }
    mkdir(work_dir, 0777);
    char corpus[1200];
    if (!make_corpus("bach", corpus, sizeof corpus)) {
        fprintf(stderr, "cannot analyze Bach 114\n");
        return 1;
    }
    test_search(corpus);
    test_json(corpus);
    test_no_match();
    test_refusals(corpus);
    test_stale(corpus); /* last: it changes a document of the corpus */
    return test_finish("test_corpus_passages");
}
