#ifndef URSATZ_CLI_KNOWLEDGE_H
#define URSATZ_CLI_KNOWLEDGE_H

#include "commands.h"

/*
 * The `knowledge` subcommand (R-765): find, show or check knowledge entries.
 *   knowledge find <words> [--kind <kind>]        the entries whose id, name or alias holds every word, one line each
 *   knowledge show <id> [--format <text|json>]    one entry, as Ursatz Text (the default) or as loaded
 *   knowledge check <file>                        whether a knowledge document loads beside the built-in entries and the data folder's files
 * On the data folder when there is one (--data or URSATZ_DATA), else on the built-in entries. Returns the exit status: 0, 1 usage, 2 a refusal.
 */
int run_knowledge(const CliArgs *args);

#endif
