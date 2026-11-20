#ifndef URSATZ_CLI_SHOW_H
#define URSATZ_CLI_SHOW_H

#include "commands.h"

/*
 * The `show` and `history` subcommands (R-658): what the workshop holds under an address, and what the journal says was done. Each returns the exit
 * status: 0, 1 usage or a refusal of the request, 2 any other failure.
 */
int run_show(const CliArgs *args);
int run_history(const CliArgs *args);

#endif
