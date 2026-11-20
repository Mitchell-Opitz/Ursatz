#ifndef URSATZ_CLI_ITEMS_H
#define URSATZ_CLI_ITEMS_H

#include <stdbool.h>

#include "commands.h"

/*
 * compose, regenerate and render for the items of a data folder (R-658). Each returns the exit status: 0, 1 usage or a refusal of the request,
 * 2 any other failure (a job that failed, an item without the file), 3 an output file could not be written.
 */

/* `compose item:<id>`: composes the theme or piece into its next version and waits for the job (--no-wait prints the job's id). */
int run_compose(const CliArgs *args);

/* `regenerate item:<id> --section <id>`: writes one section of a piece version again, as the next version. Called by run_regenerate when its input is
 * an address. */
int run_regenerate_item(const CliArgs *args);

/* `render item:<id>[@v<n>]`: renders the plan of an item version to MIDI or MusicXML. Called by run_render when its input is an address. */
int run_render_item(const CliArgs *args);

#endif
