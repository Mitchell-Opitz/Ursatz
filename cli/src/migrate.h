#ifndef URSATZ_CLI_MIGRATE_H
#define URSATZ_CLI_MIGRATE_H

#include "commands.h"

/*
 * The `migrate` subcommand (R-660): workshop_migrate on the data folder, with no session (the folder must not be open). Prints the report as JSON.
 * Returns the exit status: 0, 1 usage, 2 a folder another process holds, or a migration that was refused or failed.
 */
int run_migrate(const CliArgs *args);

#endif
