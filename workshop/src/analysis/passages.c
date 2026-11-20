#include "analysis/passages.h"

#include <stdlib.h>

#include <ursatz_analyzer/analyzer_api.h>

#include "base/knowledge.h"

UrsatzStatus workshop_corpus_passages(const char *dir, const char *like, const char *numerals, size_t limit, cJSON **array, UrsatzError *error) {
    *array = NULL;
    const KnowledgeSnapshot *knowledge = workshop_knowledge_snapshot();
    AnalyzerCorpusPassages found;
    UrsatzStatus searched = like ? analyzer_corpus_passages_like(dir, knowledge, like, limit, &found)
                                 : analyzer_corpus_passages_numerals(dir, knowledge, numerals, limit, &found);
    if (searched == URSATZ_ERR_REQUEST) {
        return ursatz_error_set(error, URSATZ_ERR_REQUEST, like ? "/params/like" : "/params/numerals",
                                "cannot read %s as a passage to look for", like ? like : numerals);
    }
    if (searched == URSATZ_ERR_NOT_FOUND) {
        return ursatz_error_set(error, URSATZ_ERR_NOT_FOUND, "/params/dir", "cannot read the corpus '%s'", dir);
    }
    if (searched != URSATZ_OK) {
        return ursatz_error_set(error, searched, "", "%s", knowledge ? "the passages cannot be searched" : "a knowledge snapshot is required");
    }
    char *text = analyzer_corpus_passages_to_json(&found);
    analyzer_corpus_passages_destroy(&found);
    *array = text ? cJSON_Parse(text) : NULL;
    free(text);
    return *array ? URSATZ_OK : ursatz_error_set(error, URSATZ_ERR_ALLOC, "", "out of memory");
}
