#ifndef URSATZ_COMPOSER_CLI_COUNTERPOINT_H
#define URSATZ_COMPOSER_CLI_COUNTERPOINT_H

#include "commands.h"

/*
 * The `counterpoint` subcommand: argv[1] is "counterpoint". Returns the process exit status:
 *   0  success
 *   1  usage or an invalid argument
 *   2  the request cannot be satisfied: the line cannot be read or is not one the species can be written against, or no
 *      counter-melody holds the rules ("cannot be written as asked: <reason>")
 *   3  an output file could not be written
 */
int run_counterpoint(const CliArgs *args);

#endif
