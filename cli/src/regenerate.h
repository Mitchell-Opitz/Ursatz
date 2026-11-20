#ifndef URSATZ_COMPOSER_CLI_REGENERATE_H
#define URSATZ_COMPOSER_CLI_REGENERATE_H

#include "commands.h"

/* "ursatz lock ..." and "ursatz regenerate ..."; argv[1] is the command. Return the exit status. */
int run_lock(const CliArgs *args);
int run_regenerate(const CliArgs *args);

#endif
