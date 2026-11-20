#ifndef URSATZ_CLI_FLAGS_H
#define URSATZ_CLI_FLAGS_H

#include <stdbool.h>
#include <stddef.h>

#include "commands.h"

/*
 * The one flag table of ursatz (R-657): every flag of every subcommand once, each with one meaning and one help line that holds for every
 * command that takes it (a command's own range or default goes in its description). A command names the flags it takes in its row
 * (commands.h); cli_parse accepts only those and the alias of one.
 */
typedef enum {
    CLI_FLAG_SWITCH, /* no value: given or not */
    CLI_FLAG_VALUE,  /* one value, given once */
    CLI_FLAG_REPEAT  /* a value, given any number of times; each keeps its position in argv */
} CliFlagKind;

typedef struct {
    const char *name;  /* "--seed" */
    const char *alias; /* another name that reads as this flag ("--mode" for "--material"), or NULL */
    CliFlagKind kind;
    const char *value; /* the value's placeholder in the help, "<n>", NULL for a switch */
    const char *help;
} CliFlag;

/* Every flag, sorted by name. */
extern const CliFlag CLI_FLAGS[];
extern const size_t CLI_FLAG_COUNT;

/* The flag named name (or whose alias it is); NULL when the table has none. */
const CliFlag *cli_flag_find(const char *name);

/* Whether name is a global flag: --data, --port or --actor stand before the subcommand and no command row names them. */
bool cli_flag_global(const char *name);

#define CLI_MAX_REPEATS 64 /* repeated values of all the flags of one command line */
#define CLI_MAX_INPUTS 4
#define CLI_FLAG_CAPACITY 128 /* at least CLI_FLAG_COUNT */

typedef struct {
    size_t flag; /* the index into CLI_FLAGS */
    const char *value;
    int position; /* the index in argv */
} CliRepeat;

/*
 * What a command line holds once parsed: per flag of the table a switch's state, a value's text or the repeated values with their positions,
 * and the inputs (the positional arguments). argc and argv are the program's own: argv[0] is the program, argv[1] the command, and the
 * flags and inputs follow from argv[2].
 */
struct CliArgs {
    int argc;
    char **argv;
    const CliCommand *command;
    bool help; /* --help was given */
    bool given[CLI_FLAG_CAPACITY];
    const char *value[CLI_FLAG_CAPACITY];
    CliRepeat repeats[CLI_MAX_REPEATS];
    size_t repeat_count;
    const char *inputs[CLI_MAX_INPUTS];
    size_t input_count;
};

/*
 * Parses argv[2..] for command. A flag is accepted when the command's row names it (or the alias of one); a value may start with "-" (the next
 * argument is taken as it is); a lone "-" is an input; --help anywhere asks for the help. A failure fills message (size bytes) with one of
 * "unknown option '<arg>' (see ursatz <command> --help)", "<flag> needs a value", "<flag> given twice" and "<command> takes <n> input(s)"
 * ("at most <n>"), and returns false.
 */
bool cli_parse(const CliCommand *command, int argc, char **argv, CliArgs *out, char *message, size_t size);

/* Parses and prints "Error: <message>" to stderr on a failure; false then (the command's exit status is 1). */
bool cli_parse_or_fail(const CliCommand *command, int argc, char **argv, CliArgs *out);

/* The readers; flag is the flag's name ("--seed"). A flag that is not in the table is never given. */
const char *cli_value(const CliArgs *args, const char *flag);
bool cli_switch(const CliArgs *args, const char *flag);
size_t cli_repeat_count(const CliArgs *args, const char *flag);
/* The i-th value of a repeated flag, in argv order; its position in argv in *position when that is not NULL. */
const char *cli_repeat_value(const CliArgs *args, const char *flag, size_t i, int *position);

#endif
