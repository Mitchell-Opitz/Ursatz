#ifndef URSATZ_CLI_CRITIQUE_H
#define URSATZ_CLI_CRITIQUE_H

#include "commands.h"

/*
 * The `critique` subcommand: argv[1] is "critique". Judges candidate files with the critic (docs/cli-critique.md).
 *   critique <file>... [--settings <file>] [--format text|json]     the files are one set
 *   critique --rated <file> [--settings <file>] [--format text|json]  every pair of a rated set, one request for each set label
 * Returns the process exit status: 0 it ran; 1 usage, an invalid option or an unreadable file; 2 the Composer refused a candidate.
 */
int run_critique(const CliArgs *args);

#endif
