#ifndef URSATZ_CLI_COMMANDS_H
#define URSATZ_CLI_COMMANDS_H

#include <stddef.h>
#include <stdio.h>

/*
 * The subcommands of ursatz (R-657): one table, one row per command. main dispatches through it, and the help of every command is printed from
 * its row and the flag table (flags.h).
 *
 * CliArgs (flags.h) holds what a command is run with: its flags and inputs, parsed against the row before the command runs.
 */
typedef struct CliArgs CliArgs;

typedef struct {
    const char *name;
    const char *usage;        /* the synopsis after "ursatz": one line for each form of the command, separated by "\n", every flag listed */
    const char *summary;      /* one line, for the list of commands */
    const char *description;  /* what the command does, the notes on its options, the exit status and the examples, all with "ursatz" */
    const char *const *flags; /* the names of the flags the command takes, NULL ended; each is in CLI_FLAGS */
    int inputs_min;           /* how many inputs (positional arguments) the command takes */
    int inputs_max;
    int (*run)(const CliArgs *args);
} CliCommand;

extern const CliCommand CLI_COMMANDS[];
extern const size_t CLI_COMMAND_COUNT;

/* The row named name, NULL when there is none. */
const CliCommand *cli_command_find(const char *name);

/* Prints the help of a command (stdout, exit 0 after it): "Usage:" and the row's usage, the description, then "Options:" and one line for
 * each flag of the row, from the flag table. */
void cli_print_help(const CliCommand *command, FILE *out);

/* The usage of the command named name as "Usage: ursatz ..." lines, for the message of a usage error; "" when there is no such row. Valid until
 * the next call. */
const char *cli_usage_of(const char *name);

#endif
