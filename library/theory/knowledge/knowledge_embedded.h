#ifndef URSATZ_THEORY_KNOWLEDGE_KNOWLEDGE_EMBEDDED_H
#define URSATZ_THEORY_KNOWLEDGE_KNOWLEDGE_EMBEDDED_H

#include <stddef.h>

/*
 * The knowledge files of knowledge/ as the build embedded them (cmake/embed_json.py): every *.json except the schemas, sorted by
 * path. `text` ends in a NUL byte that `size` does not count. Only the snapshot (knowledge_snapshot.c) reads them.
 */
typedef struct {
    const char *path; /* from the knowledge folder, with "/": "material/omt.json" */
    const char *text;
    size_t size;
} KnowledgeEmbeddedFile;

extern const KnowledgeEmbeddedFile knowledge_embedded_files[];
extern const size_t knowledge_embedded_file_count;

#endif
