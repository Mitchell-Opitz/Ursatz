#ifndef URSATZ_CLI_COMMANDS_H
#define URSATZ_CLI_COMMANDS_H

#include <stddef.h>

/*
 * The subcommands of ursatz (R-657): one table, one row per command. main dispatches through it, and the help of every command is printed from
 * its row and the flag table (flags.h).
 *
 * CliArgs holds what a command is run with. Until a command reads its flags from the flag table it takes the raw argument list through a
 * wrapper: argv[0] is the program, argv[1] the command's name and the flags and inputs follow.
 */
typedef struct CliArgs {
    int argc;
    char **argv;
} CliArgs;

typedef struct {
    const char *name;
    const char *summary; /* one line, for the list of commands */
    int inputs_min;      /* how many inputs (positional arguments) the command takes */
    int inputs_max;
    int (*run)(const CliArgs *args);
} CliCommand;

extern const CliCommand CLI_COMMANDS[];
extern const size_t CLI_COMMAND_COUNT;

/* The row named name, NULL when there is none. */
const CliCommand *cli_command_find(const char *name);

#endif
