#ifndef URSATZ_COMPOSER_CLI_PLAN_CMD_H
#define URSATZ_COMPOSER_CLI_PLAN_CMD_H

#include "commands.h"

/* "ursatz plan ..."; argv[1] is "plan". Returns the exit status. */
int run_plan(const CliArgs *args);

#endif
