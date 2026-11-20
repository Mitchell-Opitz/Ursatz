#ifndef URSATZ_CLI_COMMANDS_H
#define URSATZ_CLI_COMMANDS_H

#include <stddef.h>

/*
 * The subcommands of ursatz (R-657): one table, one row per command. main dispatches through it, and the help of every command is printed from
 * its row and the flag table (flags.h).
 *
 * CliArgs (flags.h) holds what a command is run with: its flags and inputs, parsed against the row before the command runs.
 */
typedef struct CliArgs CliArgs;

typedef struct {
    const char *name;
    const char *summary;       /* one line, for the list of commands */
    const char *const *flags;  /* the names of the flags the command takes, NULL ended; each is in CLI_FLAGS */
    int inputs_min;            /* how many inputs (positional arguments) the command takes */
    int inputs_max;
    int (*run)(const CliArgs *args);
} CliCommand;

extern const CliCommand CLI_COMMANDS[];
extern const size_t CLI_COMMAND_COUNT;

/* The row named name, NULL when there is none. */
const CliCommand *cli_command_find(const char *name);

#endif
