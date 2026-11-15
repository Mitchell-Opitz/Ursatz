#ifndef URSATZ_COMPOSER_CLI_PIECE_TEXT_H
#define URSATZ_COMPOSER_CLI_PIECE_TEXT_H

#include <stddef.h>

#include <cJSON.h>

/*
 * The text output of `piece` from the result document of the composer API alone (docs/api.md, the piece entries): the key,
 * the meter, the brief, what was regenerated, the loop and the sections of each version, then the files written.
 */
void cli_print_piece_text(const cJSON *document, const char *const *files, size_t file_count);

/* The version a variation entry holds: under "loop" for the game loop, else under the form's first version name ("piece"). */
const cJSON *cli_piece_variation_version(const cJSON *item);

/* The text output of `piece --variations`: the variations kept, with the files written for each. */
void cli_print_piece_variations_text(const cJSON *document, const char *const *plans, const char *const *midis,
                                     const char *const *musicxmls);

#endif
