#ifndef URSATZ_COMPOSER_CLI_VARIATIONS_H
#define URSATZ_COMPOSER_CLI_VARIATIONS_H

#include "commands.h"

/* "ursatz variations ..."; argv[1] is the command. Returns the exit status. */
int run_variations(const CliArgs *args);
/* "ursatz harmonize ..."; the same output, for the harmonizations of a given melody. */
int run_harmonize(const CliArgs *args);

#endif
