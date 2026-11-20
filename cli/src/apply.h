#ifndef URSATZ_CLI_APPLY_H
#define URSATZ_CLI_APPLY_H

#include "commands.h"

/*
 * The `apply` subcommand (R-658): commands to the data folder, from a file or standard input, as the actor of the session (--actor, default claude).
 * Returns the exit status: 0 every command was applied, 1 usage or the first command the workshop refused as a request, 2 any other failure.
 */
int run_apply(const CliArgs *args);

#endif
