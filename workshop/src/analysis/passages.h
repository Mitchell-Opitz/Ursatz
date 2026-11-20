#ifndef URSATZ_WORKSHOP_ANALYSIS_PASSAGES_H
#define URSATZ_WORKSHOP_ANALYSIS_PASSAGES_H

#include <stddef.h>

#include <cJSON.h>

#include <common/status.h>

/*
 * The tool "corpus-passages" (R-656): the passages of a corpus output folder that look like a passage file (like) or a progression (numerals),
 * exactly one of them, the best limit (1 to 100). *array is the passages as analyzer_corpus_passages_to_json writes them (an empty array when
 * nothing matches), the caller deletes it. An unreadable like (or numerals) is URSATZ_ERR_REQUEST at "/params/like" ("/params/numerals"), a
 * corpus folder that does not read URSATZ_ERR_NOT_FOUND at "/params/dir".
 */
UrsatzStatus workshop_corpus_passages(const char *dir, const char *like, const char *numerals, size_t limit, cJSON **array, UrsatzError *error);

#endif
