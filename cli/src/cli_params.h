#ifndef URSATZ_CLI_PARAMS_H
#define URSATZ_CLI_PARAMS_H

#include <stdbool.h>
#include <stddef.h>

#include "flags.h"

/*
 * What every command does with its flags (R-691): a table of rows reads each flag into a member of the command's options struct, and
 * one function says what a command needs. A switch row's member is a bool; a value row's member is a const char *.
 */
typedef struct {
    const char *flag;
    size_t offset;   /* offsetof(Options, member) */
    bool is_switch;
} CliOptionRow;

/* Sets each member the rows name: a switch to cli_switch(args, flag), a value to cli_value(args, flag) (NULL when absent). */
void cli_options_read(const CliArgs *args, const CliOptionRow *rows, size_t count, void *options);

/*
 * Prints "Error: <command> needs <what>" and the exit status of a usage error (CLI_EXIT_USAGE). With_usage adds the command's usage
 * line after it; without, the text ends in " (see --help)". Returns the status, for `return cli_needs(...)`.
 */
int cli_needs(const char *command, const char *what, bool with_usage);

#endif
