#ifndef URSATZ_COMPOSER_CLI_LISTEN_H
#define URSATZ_COMPOSER_CLI_LISTEN_H

#include "commands.h"

/*
 * The `listen` subcommand: argv[1] is "listen". A batch of motifs or themes from fixed seeds, written as files a listening
 * session plays and reads (docs/cli-listen.md). Returns the process exit status:
 *   0  at least one sample was written
 *   1  usage, an invalid option or an unreadable file
 *   2  no sample could be composed
 *   3  an output file could not be written
 */
int run_listen(const CliArgs *args);

#endif
