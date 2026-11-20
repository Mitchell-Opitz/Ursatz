#ifndef URSATZ_CLI_QUERY_H
#define URSATZ_CLI_QUERY_H

#include "commands.h"

/*
 * The `query` subcommand (R-659): any query of the workshop by its name, on the data folder (opened here or served by another process). Returns the
 * exit status: 0, 1 usage or a query the workshop refused as a request, 2 any other failure.
 */
int run_query(const CliArgs *args);

#endif
