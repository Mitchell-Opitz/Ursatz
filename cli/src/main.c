#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>

#include "cli_tools.h"
#include "commands.h"
#include "flags.h"
#include "session.h"

/* The list of commands: the usage line and one line for each command. */
static void print_commands(FILE *out) {
    fprintf(out, "Usage: ursatz <command> [options]\n");
    fprintf(out, "       ursatz help <command>\n");
    fprintf(out, "       ursatz --version\n");
    fprintf(out, "\nCommands:\n");
    for (size_t i = 0; i < CLI_COMMAND_COUNT; i++) {
        fprintf(out, "  %-14s %s\n", CLI_COMMANDS[i].name, CLI_COMMANDS[i].summary);
    }
}

static int print_version(void) {
    WorkshopReply reply;
    UrsatzError error;
    memset(&error, 0, sizeof error);
    Workshop *workshop = cli_tools();
    if (!workshop || workshop_tool(workshop, "versions", NULL, NULL, &reply, &error) != URSATZ_OK) {
        return 1;
    }
    const cJSON *composer_version = cJSON_GetObjectItemCaseSensitive(reply.json, "composer_version");
    const cJSON *analyzer_version = cJSON_GetObjectItemCaseSensitive(reply.json, "analyzer_version");
    printf("ursatz (composer %s, analyzer %s)\n", cJSON_IsString(composer_version) ? composer_version->valuestring : "",
           cJSON_IsString(analyzer_version) ? analyzer_version->valuestring : "");
    workshop_reply_destroy(&reply);
    return 0;
}

/* Runs the command with its arguments parsed against its row; a parse failure is exit 1. */
static int run_command(const CliCommand *command, int argc, char **argv) {
    CliArgs args;
    if (!cli_parse_or_fail(command, argc, argv, &args)) {
        return 1;
    }
    if (args.help) {
        cli_print_help(command, stdout);
        return 0;
    }
    return command->run(&args);
}

/* The global flags before the command (--data, --port, --actor), taken out of argv: *out is what they say and *shift how many arguments they took. A
 * failure is printed and false. */
static bool read_global_flags(int argc, char **argv, CliGlobal *out, int *shift) {
    int i = 1;
    memset(out, 0, sizeof *out);
    while (i < argc && cli_flag_global(argv[i])) {
        if (i + 1 >= argc) {
            fprintf(stderr, "Error: %s needs a value\n", argv[i]);
            return false;
        }
        if (strcmp(argv[i], "--data") == 0) {
            out->data = argv[i + 1];
        } else if (strcmp(argv[i], "--actor") == 0) {
            out->actor = argv[i + 1];
        } else {
            char *end = NULL;
            long port = strtol(argv[i + 1], &end, 10);
            if (end == argv[i + 1] || *end != '\0' || port < 1 || port > 65535) {
                fprintf(stderr, "Error: --port takes a whole number from 1 to 65535, not '%s'\n", argv[i + 1]);
                return false;
            }
            out->port = (int)port;
        }
        i += 2;
    }
    *shift = i - 1;
    return true;
}

int main(int argc, char **argv) {
    CliGlobal global;
    int shift = 0;
    if (!read_global_flags(argc, argv, &global, &shift)) {
        return 1;
    }
    cli_global_set(&global);
    if (shift > 0) {
        /* argv[0] stays the program; the command and its arguments follow the global flags. */
        argv[shift] = argv[0];
        argv += shift;
        argc -= shift;
    }
    if (argc == 1) {
        print_commands(stderr);
        return 0;
    }
    const char *name = argv[1];
    if (strcmp(name, "--help") == 0 || (strcmp(name, "help") == 0 && argc == 2)) {
        print_commands(stdout);
        return 0;
    }
    if (strcmp(name, "--version") == 0 && argc == 2) {
        return print_version();
    }
    if (strcmp(name, "help") == 0) {
        /* "ursatz help <command>": the command's own help. */
        const CliCommand *command = cli_command_find(argv[2]);
        if (!command) {
            fprintf(stderr, "Error: unknown command '%s' (see ursatz --help)\n", argv[2]);
            return 1;
        }
        char *help_argv[] = {argv[0], argv[2], (char *)"--help", NULL};
        return run_command(command, 3, help_argv);
    }
    const CliCommand *command = cli_command_find(name);
    if (!command) {
        fprintf(stderr, "Error: unknown command '%s' (see ursatz --help)\n", name);
        return 1;
    }
    return run_command(command, argc, argv);
}
