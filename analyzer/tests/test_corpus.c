#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "cJSON.h"
#include <ursatz_analyzer/analysis_document.h>
#include <ursatz_analyzer/analysis_pipeline.h>
#include <ursatz_analyzer/corpus.h>
#include <ursatz_analyzer/input_pipeline.h>

#include "test_support.h"
#include "ursatz_test.h"
#include "test_knowledge.h"

/*
 * Corpus mode over the corpora CMake builds in the build tree: "reference" (the five framework-v1 pieces) and "mixed"
 * (one MIDI, one MusicXML in a subfolder, one .txt, one broken .mid). Checks the index against
 * tests/snapshots/corpus-index.reference.json (with the content hashes compared separately and the input directory
 * masked), that each document is byte for byte what a single-file run renders, and the resume rules. With
 * URSATZ_UPDATE_GOLDENS=1 in the environment, the snapshot is rewritten instead.
 */

static bool append_text(const char *path, const char *text) {
    FILE *file = fopen(path, "ab");
    if (!file) {
        return false;
    }
    size_t length = strlen(text);
    bool ok = fwrite(text, 1, length, file) == length;
    return fclose(file) == 0 && ok;
}

static void make_dir(const char *path) {
    mkdir(path, 0777);
}

static void remove_tree(const char *path) {
    char command[1024];
    snprintf(command, sizeof(command), "rm -rf '%s'", path);
    if (system(command) != 0) {
        fprintf(stderr, "cannot remove %s\n", path);
    }
}

static bool copy_file(const char *from, const char *to) {
    size_t length = 0;
    char *data = test_read_file(from, &length);
    bool ok = data && test_write_file(to, data, length);
    free(data);
    return ok;
}

static bool exists(const char *path) {
    struct stat info;
    return stat(path, &info) == 0;
}

static unsigned long long fnv1a(const char *data, size_t length) {
    unsigned long long hash = 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < length; i++) {
        hash = (hash ^ (unsigned char)data[i]) * 0x100000001b3ULL;
    }
    return hash;
}

static CorpusRunSummary run(const char *input, const char *output, bool resume, CorpusRunResult *result) {
    CorpusRunOptions options = {input, output, resume, NULL, NULL, test_knowledge()};
    CorpusRunSummary summary;
    *result = corpus_run(&options, &summary);
    return summary;
}

static cJSON *read_index(const char *output) {
    char path[1200];
    snprintf(path, sizeof(path), "%s/corpus-index.json", output);
    char *text = test_read_file(path, NULL);
    cJSON *root = text ? cJSON_Parse(text) : NULL;
    free(text);
    return root;
}

/* The index text with the input directory, the Ursatz commit and every content hash masked, so it compares across machines. */
static char *masked_index(const char *output) {
    char path[1200];
    snprintf(path, sizeof(path), "%s/corpus-index.json", output);
    char *text = test_read_file(path, NULL);
    if (!text) {
        return NULL;
    }
    char *masked = malloc(strlen(text) + 64);
    if (!masked) {
        free(text);
        return NULL;
    }
    char *out = masked;
    for (const char *line = text; *line;) {
        size_t length = strcspn(line, "\n");
        const char *key = NULL;
        const char *replacement = NULL;
        if (strncmp(line, "  \"input_dir\": ", 15) == 0) {
            key = "  \"input_dir\": ";
            replacement = "\"<input_dir>\",";
        } else if (strncmp(line, "  \"producer_key\": ", 18) == 0) {
            key = "  \"producer_key\": ";
            replacement = "\"<producer_key>\",";
        } else if (strncmp(line, "      \"content_hash\": ", 22) == 0) {
            key = "      \"content_hash\": ";
            replacement = "\"<hash>\",";
        }
        if (key) {
            out += sprintf(out, "%s%s", key, replacement);
        } else {
            memcpy(out, line, length);
            out += length;
        }
        if (line[length] == '\n') {
            *out++ = '\n';
            line += length + 1;
        } else {
            line += length;
        }
    }
    *out = '\0';
    free(text);
    return masked;
}

static char *render_single(const char *path) {
    ImportedNotes notes;
    if (input_pipeline_import_file(path, INPUT_FORMAT_AUTO, &notes) != INPUT_PIPELINE_OK) {
        return NULL;
    }
    AnalysisReport report;
    if (!run_analysis_pipeline(&notes, test_knowledge(), &report)) {
        imported_notes_destroy(&notes);
        return NULL;
    }
    char *json = test_render_document(path, &notes, &report);
    analysis_report_destroy(&report);
    imported_notes_destroy(&notes);
    return json;
}

static void test_reference(const char *work) {
    char input[1024], output[1024];
    snprintf(input, sizeof(input), "%s/reference", URSATZ_CORPORA_DIR);
    snprintf(output, sizeof(output), "%s/reference", work);

    CorpusRunResult result;
    CorpusRunSummary summary = run(input, output, false, &result);
    CHECKF(result == CORPUS_RUN_OK, "reference run result %d", (int)result);
    CHECKF(summary.analyzed == 5 && summary.skipped == 0 && summary.failed == 0, "reference summary %zu/%zu/%zu",
           summary.analyzed, summary.skipped, summary.failed);

    char *masked = masked_index(output);
    char snapshot_path[1024];
    snprintf(snapshot_path, sizeof(snapshot_path), "%s/corpus-index.reference.json", URSATZ_SNAPSHOT_DIR);
    if (test_update_goldens()) {
        CHECKF(masked && test_write_file(snapshot_path, masked, strlen(masked)), "cannot write %s", snapshot_path);
    } else {
        char *expected = test_read_file(snapshot_path, NULL);
        CHECKF(expected && masked && strcmp(expected, masked) == 0, "index differs from %s", snapshot_path);
        free(expected);
    }
    free(masked);

    cJSON *index = read_index(output);
    const cJSON *pieces = cJSON_GetObjectItemCaseSensitive(index, "pieces");
    CHECKF(cJSON_GetArraySize(pieces) == 5, "index lists %d pieces", cJSON_GetArraySize(pieces));
    const cJSON *piece;
    cJSON_ArrayForEach(piece, pieces) {
        const char *path = cJSON_GetObjectItemCaseSensitive(piece, "path")->valuestring;
        const char *hash = cJSON_GetObjectItemCaseSensitive(piece, "content_hash")->valuestring;
        char input_path[1200], document_path[1200];
        snprintf(input_path, sizeof(input_path), "%s/%s", input, path);
        size_t length = 0;
        char *bytes = test_read_file(input_path, &length);
        char expected_hash[17];
        snprintf(expected_hash, sizeof(expected_hash), "%016llx", bytes ? fnv1a(bytes, length) : 0ULL);
        CHECKF(strcmp(hash, expected_hash) == 0, "%s: content_hash %s, expected %s", path, hash, expected_hash);
        free(bytes);

        snprintf(document_path, sizeof(document_path), "%s/%s",
                 output, cJSON_GetObjectItemCaseSensitive(piece, "document")->valuestring);
        char *written = test_read_file(document_path, NULL);
        char *single = render_single(input_path);
        CHECKF(written && single && strcmp(written, single) == 0, "%s: document differs from a single-file run", path);
        free(written);
        free(single);
    }
    cJSON_Delete(index);

    /* Resume: nothing changed, then one document deleted. */
    summary = run(input, output, true, &result);
    CHECKF(result == CORPUS_RUN_OK && summary.analyzed == 0 && summary.skipped == 5 && summary.failed == 0,
           "resume unchanged: %zu/%zu/%zu", summary.analyzed, summary.skipped, summary.failed);
    char deleted[1200];
    snprintf(deleted, sizeof(deleted), "%s/Field 1.mid.analysis.json", output);
    CHECKF(remove(deleted) == 0, "cannot delete %s", deleted);
    summary = run(input, output, true, &result);
    CHECKF(result == CORPUS_RUN_OK && summary.analyzed == 1 && summary.skipped == 4 && summary.failed == 0,
           "resume after deleting a document: %zu/%zu/%zu", summary.analyzed, summary.skipped, summary.failed);
    CHECKF(exists(deleted), "the deleted document was not written again");

    /* Without --resume every piece is analyzed again. */
    summary = run(input, output, false, &result);
    CHECKF(summary.analyzed == 5 && summary.skipped == 0, "no resume: %zu analyzed, %zu skipped", summary.analyzed,
           summary.skipped);
}

static void test_mixed(const char *work) {
    char input[1024], output[1024], path[1200];
    snprintf(input, sizeof(input), "%s/mixed", URSATZ_CORPORA_DIR);
    snprintf(output, sizeof(output), "%s/mixed", work);

    CorpusRunResult result;
    CorpusRunSummary summary = run(input, output, false, &result);
    CHECKF(result == CORPUS_RUN_OK, "mixed run result %d (one failed piece is not a failed run)", (int)result);
    CHECKF(summary.analyzed == 2 && summary.skipped == 0 && summary.failed == 1, "mixed summary %zu/%zu/%zu",
           summary.analyzed, summary.skipped, summary.failed);

    snprintf(path, sizeof(path), "%s/sub/field-1.musicxml.analysis.json", output);
    CHECKF(exists(path), "the MusicXML in the subfolder has no document");
    snprintf(path, sizeof(path), "%s/notes.txt.analysis.json", output);
    CHECKF(!exists(path), "the .txt file was analyzed");

    cJSON *index = read_index(output);
    const cJSON *pieces = cJSON_GetObjectItemCaseSensitive(index, "pieces");
    CHECKF(cJSON_GetArraySize(pieces) == 3, "the index lists %d pieces, expected 3", cJSON_GetArraySize(pieces));
    const char *order[] = {"Bach 114.mid", "broken.mid", "sub/field-1.musicxml"};
    for (int i = 0; i < 3 && i < cJSON_GetArraySize(pieces); i++) {
        const cJSON *piece = cJSON_GetArrayItem(pieces, i);
        CHECKF(strcmp(cJSON_GetObjectItemCaseSensitive(piece, "path")->valuestring, order[i]) == 0,
               "piece %d is not %s", i, order[i]);
        bool failed = i == 1;
        const cJSON *status = cJSON_GetObjectItemCaseSensitive(piece, "status");
        CHECKF(strcmp(status->valuestring, failed ? "failed" : "analyzed") == 0, "%s: status %s", order[i],
               status->valuestring);
        CHECKF(cJSON_IsNull(cJSON_GetObjectItemCaseSensitive(piece, "document")) == failed, "%s: document", order[i]);
        CHECKF(cJSON_IsString(cJSON_GetObjectItemCaseSensitive(piece, "error")) == failed, "%s: error", order[i]);
        CHECKF(cJSON_IsNull(cJSON_GetObjectItemCaseSensitive(piece, "measures")) == failed, "%s: measures", order[i]);
        CHECKF(cJSON_IsNull(cJSON_GetObjectItemCaseSensitive(piece, "home_key")) == failed, "%s: home_key", order[i]);
    }
    const cJSON *bach = cJSON_GetArrayItem(pieces, 0);
    CHECKF(bach && strcmp(cJSON_GetObjectItemCaseSensitive(bach, "home_key")->valuestring, "G major") == 0,
           "Bach 114 home key");
    cJSON_Delete(index);

    /* The failed piece is tried again on resume; the others are skipped. */
    summary = run(input, output, true, &result);
    CHECKF(summary.analyzed == 0 && summary.skipped == 2 && summary.failed == 1, "mixed resume %zu/%zu/%zu",
           summary.analyzed, summary.skipped, summary.failed);
}

static void test_resume_changes(const char *work) {
    char input[1024], output[1024], path[1200], from[1200];
    snprintf(input, sizeof(input), "%s/touched_in", work);
    snprintf(output, sizeof(output), "%s/touched_out", work);
    make_dir(input);
    snprintf(path, sizeof(path), "%s/sub", input);
    make_dir(path);
    snprintf(from, sizeof(from), "%s/mixed/Bach 114.mid", URSATZ_CORPORA_DIR);
    snprintf(path, sizeof(path), "%s/Bach 114.mid", input);
    CHECKF(copy_file(from, path), "cannot copy the MIDI");
    snprintf(from, sizeof(from), "%s/mixed/sub/field-1.musicxml", URSATZ_CORPORA_DIR);
    snprintf(path, sizeof(path), "%s/sub/field-1.musicxml", input);
    CHECKF(copy_file(from, path), "cannot copy the MusicXML");

    CorpusRunResult result;
    CorpusRunSummary summary = run(input, output, false, &result);
    CHECKF(summary.analyzed == 2, "first run analyzed %zu", summary.analyzed);

    /* Changing one file's bytes re-analyzes only that file. */
    const char *comment = "\n<!-- touched -->\n";
    CHECKF(append_text(path, comment), "cannot touch the MusicXML");
    summary = run(input, output, true, &result);
    CHECKF(result == CORPUS_RUN_OK && summary.analyzed == 1 && summary.skipped == 1 && summary.failed == 0,
           "resume after touching a file: %zu/%zu/%zu", summary.analyzed, summary.skipped, summary.failed);

    /* A piece no longer in the folder is dropped from the index. */
    snprintf(path, sizeof(path), "%s/Bach 114.mid", input);
    CHECKF(remove(path) == 0, "cannot remove the MIDI");
    summary = run(input, output, true, &result);
    CHECKF(summary.analyzed == 0 && summary.skipped == 1, "resume after removing a file: %zu analyzed, %zu skipped",
           summary.analyzed, summary.skipped);
    cJSON *index = read_index(output);
    CHECKF(cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(index, "pieces")) == 1, "removed piece still indexed");
    cJSON_Delete(index);

    /* An index from another analyzer version reuses nothing. */
    snprintf(path, sizeof(path), "%s/corpus-index.json", output);
    char *text = test_read_file(path, NULL);
    char *at = text ? strstr(text, "\"analyzer_version\": \"") : NULL;
    CHECKF(at != NULL, "no analyzer_version in the index");
    if (at) {
        at[21] = 'X';
        CHECKF(test_write_file(path, text, strlen(text)), "cannot rewrite the index");
        summary = run(input, output, true, &result);
        CHECKF(summary.analyzed == 1 && summary.skipped == 0, "resume across versions: %zu analyzed, %zu skipped",
               summary.analyzed, summary.skipped);
    }
    free(text);

    /* An index from another producer key reuses nothing either. */
    text = test_read_file(path, NULL);
    at = text ? strstr(text, "\"producer_key\": \"") : NULL;
    CHECKF(at != NULL, "no producer_key in the index");
    if (at) {
        at[17] = at[17] == 'X' ? 'Y' : 'X';
        CHECKF(test_write_file(path, text, strlen(text)), "cannot rewrite the index");
        summary = run(input, output, true, &result);
        CHECKF(summary.analyzed == 1 && summary.skipped == 0, "resume across keys: %zu analyzed, %zu skipped",
               summary.analyzed, summary.skipped);
        summary = run(input, output, true, &result);
        CHECKF(summary.analyzed == 0 && summary.skipped == 1, "resume under the same key: %zu analyzed, %zu skipped",
               summary.analyzed, summary.skipped);
    }
    free(text);

    /* An index of schema version 1 (it keeps ursatz_commit) reuses nothing and is written again, not refused. */
    text = test_read_file(path, NULL);
    char *version_at = text ? strstr(text, "\"schema_version\": 2") : NULL;
    char *key_at = text ? strstr(text, "\"producer_key\"") : NULL;
    CHECKF(version_at != NULL && key_at != NULL && version_at < key_at, "no schema_version 2 and producer_key in the index");
    if (version_at && key_at && version_at < key_at) {
        const char *old_key = "\"producer_key\"";
        const char *v1_key = "\"ursatz_commit\"";
        char *v1 = malloc(strlen(text) + strlen(v1_key) + 1);
        if (v1) {
            size_t head = (size_t)(version_at - text);
            size_t before_key = (size_t)(key_at - text);
            memcpy(v1, text, head);
            memcpy(v1 + head, "\"schema_version\": 1", strlen("\"schema_version\": 1"));
            size_t cursor = head + strlen("\"schema_version\": 1");
            size_t middle = before_key - (head + strlen("\"schema_version\": 2"));
            memcpy(v1 + cursor, version_at + strlen("\"schema_version\": 2"), middle);
            cursor += middle;
            memcpy(v1 + cursor, v1_key, strlen(v1_key));
            cursor += strlen(v1_key);
            strcpy(v1 + cursor, key_at + strlen(old_key));
            CHECKF(test_write_file(path, v1, strlen(v1)), "cannot rewrite the index as version 1");
            free(v1);
            summary = run(input, output, true, &result);
            CHECKF(result == CORPUS_RUN_OK && summary.analyzed == 1 && summary.skipped == 0,
                   "resume over a version 1 index: %zu analyzed, %zu skipped", summary.analyzed, summary.skipped);
            cJSON *rewritten = read_index(output);
            const cJSON *rewritten_version = cJSON_GetObjectItemCaseSensitive(rewritten, "schema_version");
            CHECKF(cJSON_IsNumber(rewritten_version) && rewritten_version->valuedouble == 2,
                   "the version 1 index was not written again as version 2");
            cJSON_Delete(rewritten);
        }
    }
    free(text);

    /* An index that cannot be read is a run error, not a silent restart. */
    CHECKF(test_write_file(path, "{", 1), "cannot corrupt the index");
    summary = run(input, output, true, &result);
    CHECKF(result == CORPUS_RUN_ERR_INDEX, "unreadable index gave result %d", (int)result);
}

static void test_arguments(const char *work) {
    CorpusRunResult result;
    CorpusRunOptions options = {NULL, NULL, false, NULL, NULL, test_knowledge()};
    CHECKF(corpus_run(&options, NULL) == CORPUS_RUN_ERR_ARGUMENT, "NULL input_dir");
    CHECKF(corpus_run(NULL, NULL) == CORPUS_RUN_ERR_ARGUMENT, "NULL options");
    char missing[1024];
    snprintf(missing, sizeof(missing), "%s/does-not-exist", work);
    run(missing, work, false, &result);
    CHECKF(result == CORPUS_RUN_ERR_INPUT_DIR, "missing input dir gave %d", (int)result);
    char blocker[1024];
    snprintf(blocker, sizeof(blocker), "%s/blocker", work);
    CHECKF(test_write_file(blocker, "x", 1), "cannot write the blocker file");
    char input[1024];
    snprintf(input, sizeof(input), "%s/reference", URSATZ_CORPORA_DIR);
    run(input, blocker, false, &result);
    CHECKF(result == CORPUS_RUN_ERR_OUTPUT_DIR, "output dir that is a file gave %d", (int)result);
}

int main(void) {
    const char *work = URSATZ_CORPUS_WORK_DIR;
    remove_tree(work);
    make_dir(work);

    test_reference(work);
    test_mixed(work);
    test_resume_changes(work);
    test_arguments(work);

    if (test_failure_count == 0) {
        remove_tree(work);
    }
    return test_update_goldens() ? test_goldens_updated("test_corpus") : test_finish("test_corpus");
}
