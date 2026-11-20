#ifndef URSATZ_COMPOSER_CLI_GROW_TEXT_H
#define URSATZ_COMPOSER_CLI_GROW_TEXT_H

#include <stdbool.h>
#include <stddef.h>

#include <cJSON.h>


/*
 * Prints the text output of `grow` and `theme` from the result document (docs/api.md, the theme entry): the brief line, the
 * brief and the chords of a theme request, the corpus passage the motive came from ("source"), each theme (numbered
 * when numbered), and the files written.
 */
void cli_print_grow_text(const cJSON *document, bool numbered, const char *const *files,
                         size_t file_count);

/* The brief line of a result document's "brief" member: "brief: mode minor (request), tempo 96 (option), ...". */
void cli_print_brief_json(const cJSON *brief);

#endif
