#ifndef URSATZ_CLI_RENDER_H
#define URSATZ_CLI_RENDER_H

#include "commands.h"

/* "ursatz render ..."; the one input is the plan file. Returns the exit status. */
int run_render(const CliArgs *args);

#endif
