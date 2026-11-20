#ifndef URSATZ_CLI_HELP_H
#define URSATZ_CLI_HELP_H

#include <stdbool.h>
#include <stdio.h>

/*
 * The help that is generated (R-665): `ursatz help commands [--format text|markdown|json]` prints the command reference of the workshop (the tool
 * commands: sheet, markdown or json), `ursatz help grammar` the grammar of the text commands, `ursatz help flags [--format text|markdown]` the flag
 * table, and `ursatz help <command>` (a name with a dot, as item.save) the help of one command. cli_help_topic returns the exit status, or -1 when
 * argv[2] is none of these (the caller then looks for a subcommand of that name). argv[0] is the program, argv[1] "help".
 */
int cli_help_topic(int argc, char **argv);

/* The flag table as the page cli/docs/flags.md (markdown, true) or as plain lines (false), written to out. */
void cli_print_flags(FILE *out, bool markdown);

#endif
