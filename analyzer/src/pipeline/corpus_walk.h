#ifndef URSATZ_ANALYZER_CORPUS_WALK_H
#define URSATZ_ANALYZER_CORPUS_WALK_H

#include <stdbool.h>
#include <stddef.h>

#include <ursatz_analyzer/staleness.h>

typedef enum {
    CORPUS_WALK_OK = 0,
    CORPUS_WALK_ERR_ALLOC,
    CORPUS_WALK_ERR_IO,     /* the index or a document cannot be read */
    CORPUS_WALK_ERR_FORMAT, /* the index is not a corpus index */
    CORPUS_WALK_ERR_VISIT   /* the visitor returned false; it keeps its own error */
} CorpusWalkResult;

/* Called for every analyzed piece with the piece's path (as the index records it), the document's path relative to
   the index's directory, the document's text, and its AnalyzerDocumentStatus. Stale documents are visited like
   current ones (the status is information, not a filter). Returns false to stop the walk. */
typedef bool (*CorpusDocumentVisitor)(const char *piece_path, const char *document, const char *document_json,
                                      AnalyzerDocumentStatus status, void *context);

/* Opens the corpus index at index_path (<output dir>/corpus-index.json), checks its format tag, and reads and
   visits the document of every piece whose status is "analyzed", in index order. When input_dir is not NULL the
   index must name its input directory, and *input_dir receives a copy the caller frees (NULL on any failure). */
CorpusWalkResult corpus_walk_documents(const char *index_path, CorpusDocumentVisitor visit, void *context,
                                       char **input_dir);

/* Counts the analyzed pieces of the index at index_path whose document is not ANALYZER_DOCUMENT_CURRENT. */
CorpusWalkResult corpus_walk_stale_count(const char *index_path, size_t *stale);

#endif
