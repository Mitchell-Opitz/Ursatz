#ifndef URSATZ_CLI_COMMANDS_H
#define URSATZ_CLI_COMMANDS_H

#include <stdbool.h>
#include <stddef.h>

/*
 * The subcommands of ursatz (R-657): one table, one row per command. main dispatches through it, and the help of every command is printed from
 * its row and the flag table (flags.h).
 *
 * CliArgs (flags.h) holds what a command is run with: its flags and inputs parsed against the row. Until a command reads its flags from the table
 * it takes the raw argument list through a wrapper (argv[0] is the program, argv[1] the command's name and the flags and inputs follow).
 */
typedef struct CliArgs CliArgs;

typedef struct {
    const char *name;
    const char *summary;       /* one line, for the list of commands */
    const char *const *flags;  /* the names of the flags the command takes, NULL ended; each is in CLI_FLAGS */
    int inputs_min;            /* how many inputs (positional arguments) the command takes */
    int inputs_max;
    bool reads_flags;          /* the command reads the flag table: main parses its arguments before it runs (until every command does) */
    int (*run)(const CliArgs *args);
} CliCommand;

extern const CliCommand CLI_COMMANDS[];
extern const size_t CLI_COMMAND_COUNT;

/* The row named name, NULL when there is none. */
const CliCommand *cli_command_find(const char *name);

#endif
