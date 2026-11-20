#ifndef URSATZ_CLI_RENDER_H
#define URSATZ_CLI_RENDER_H

#include "commands.h"

#define RENDER_USAGE_LINE \
    "Usage: ursatz render <plan.json> [--midi <out.mid>] [--musicxml <out.xml>] [--tempo <bpm>] [--plain-midi]"

/* "ursatz render ..."; the one input is the plan file. Returns the exit status. */
int run_render(const CliArgs *args);

#endif
