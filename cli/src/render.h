#ifndef URSATZ_CLI_RENDER_H
#define URSATZ_CLI_RENDER_H

#include <stdbool.h>

#include <cJSON.h>

#include "commands.h"

/* "ursatz render ..."; the one input is the plan file. Returns the exit status. */
int run_render(const CliArgs *args);

/* Renders a plan object (taken) to the files asked for, as `render` does: the exit status. plan_name names the plan in the messages. */
int cli_render_plan(cJSON *plan, const char *plan_name, const char *midi_path, const char *musicxml_path, const char *tempo_text, bool plain_midi);

#endif
