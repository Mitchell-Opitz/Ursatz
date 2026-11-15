#ifndef URSATZ_ANALYZER_CORPUS_H
#define URSATZ_ANALYZER_CORPUS_H

#include <stdbool.h>
#include <stddef.h>

/*
 * Corpus mode: analyzes every piece in a folder, one analysis document
 * each, written under an output directory.
 */

typedef enum {
    CORPUS_PIECE_ANALYZED = 0,
    CORPUS_PIECE_SKIPPED, /* resume: unchanged since the last run */
    CORPUS_PIECE_FAILED
} CorpusPieceStatus;

/*
 * Called once per piece, in path order, as soon as the piece is done.
 * path is relative to input_dir; error is NULL unless the piece failed.
 */
typedef void (*CorpusPieceCallback)(const char *path, CorpusPieceStatus status, const char *error, void *user);

typedef struct {
    const char *input_dir;
    /* NULL means "<input_dir>/.ursatz-corpus". */
    const char *output_dir;
    bool resume;
    bool markdown;
    /* Optional progress callback (NULL for none) and its user pointer. */
    CorpusPieceCallback on_piece;
    void *user;
} CorpusRunOptions;

typedef struct {
    size_t analyzed;
    size_t skipped;
    size_t failed;
} CorpusRunSummary;

typedef enum {
    CORPUS_RUN_OK = 0,
    CORPUS_RUN_ERR_ARGUMENT,
    CORPUS_RUN_ERR_INPUT_DIR,
    CORPUS_RUN_ERR_OUTPUT_DIR,
    CORPUS_RUN_ERR_INDEX, /* resume: the existing index is unreadable */
    CORPUS_RUN_ERR_WRITE
} CorpusRunResult;

/*
 * Walks input_dir recursively and takes the files with the extensions
 * .mid, .midi, .musicxml and .xml (case-insensitive), sorted by relative
 * path so a run is reproducible. Each piece is imported (format
 * detected), analyzed and written to
 * "<output_dir>/<relative path>.analysis.json", plus
 * "<relative path>.report.md" when markdown is set, with directories
 * created as needed. A piece that cannot be imported or analyzed is
 * counted as failed; that is not a failed run.
 *
 * out may be NULL. On any result other than CORPUS_RUN_OK, a message has
 * been written to stderr.
 */
CorpusRunResult corpus_run(const CorpusRunOptions *options, CorpusRunSummary *out);

#endif
