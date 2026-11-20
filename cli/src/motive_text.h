#ifndef URSATZ_COMPOSER_CLI_MOTIVE_TEXT_H
#define URSATZ_COMPOSER_CLI_MOTIVE_TEXT_H

#include <stdbool.h>
#include <stdio.h>

/*
 * The text of the `motive` command: the analysis document the composer API answers with (composer_api_motive), as the
 * lines the command prints without --json. Reads the document only. Returns false when it is not an analysis document.
 */
bool motive_text_print(const char *response_json, FILE *out);

#endif
