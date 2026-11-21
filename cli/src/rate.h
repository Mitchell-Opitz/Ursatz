#ifndef URSATZ_CLI_RATE_H
#define URSATZ_CLI_RATE_H

#include "commands.h"

/*
 * `rate <ses-id> --round <n> --seed <s> [--rating <batch>=<1-5>]... [--better <batch>|same] [--note <text>]` records the ratings of the samples a listening
 * session made and a pick between its batches (the commands ratings.rate and ratings.pick); `rate item:<id>@v<n> --rating <1-5> [--note <text>]` rates a
 * version of an item (ratings.rate-version). Each id made is printed on a line of its own: `rat-... <batch> <rating>`, `pck-... better <batch>`,
 * `rat-... v<n> <rating>`. Returns 0, 1 for a request the workshop refused or a usage error, 2 for any other failure.
 */
int run_rate(const CliArgs *args);

#endif
