#ifndef URSATZ_COMPOSER_CLI_PIECE_H
#define URSATZ_COMPOSER_CLI_PIECE_H

#include "commands.h"

/*
 * The `piece` subcommand: argv[1] is "piece". Composes a looping game track from one JSON request (schema
 * ursatz-piece-request, version 1 or 2; docs/piece-request.md): the loop version, and the ending version built from it
 * (R-172, R-173), or the one version of a piece of another form. Returns the process exit status, as theme's:
 *   0  success
 *   1  usage or an invalid request (a request that does not read, a structure that is not one)
 *   2  the request cannot be satisfied (a motive that cannot be used, no seed works)
 *   3  an output file could not be written
 */
int run_piece(const CliArgs *args);

#endif
