#ifndef URSATZ_CLI_STATUS_H
#define URSATZ_CLI_STATUS_H

#include "commands.h"

/*
 * The `status` subcommand (R-659): whether the data folder was opened by this command or is served by another Ursatz process, and the versions.
 * Returns the exit status: 0, 1 usage, 2 the folder cannot be opened.
 */
int run_status(const CliArgs *args);

#endif
