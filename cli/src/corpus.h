#ifndef URSATZ_CLI_CORPUS_H
#define URSATZ_CLI_CORPUS_H

#include "commands.h"

/*
 * The `corpus` and `style-profile` subcommands (R-658): the Analyzer's --corpus and --style-profile modes, over the workshop's tools corpus and
 * style-profile. Each returns the exit status: 0 success, 1 usage, 2 the run failed.
 */
int run_corpus(const CliArgs *args);
int run_style_profile(const CliArgs *args);

#endif
