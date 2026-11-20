#ifndef URSATZ_CLI_ANALYZE_H
#define URSATZ_CLI_ANALYZE_H

#include "commands.h"

/*
 * The `analyze` subcommand (R-658): the Analyzer's default mode and its text export, over the workshop's tool analyze. Without --view it writes the
 * analysis document next to the input and says where; with --view reading it prints the text export (framed as the reading view) and writes nothing. Returns the exit status:
 *   0  success
 *   1  usage, an input that cannot be read, a notation reading that does not apply, an analysis that failed
 *   2  an output file (the layers, the model plan) could not be written
 */
int run_analyze(const CliArgs *args);

#endif
