#include "help.h"

#include <stdlib.h>
#include <string.h>

#include <cJSON.h>

#include "cli_tools.h"
#include "commands.h"
#include "flags.h"

/* The commands that take a flag, as the names of their rows, in table order. */
static void commands_of(const CliFlag *flag, char *out, size_t size, bool markdown) {
    out[0] = '\0';
    size_t used = 0;
    for (size_t i = 0; i < CLI_COMMAND_COUNT; i++) {
        for (const char *const *name = CLI_COMMANDS[i].flags; name && *name; name++) {
            if (strcmp(*name, flag->name) == 0 || (flag->alias && strcmp(*name, flag->alias) == 0)) {
                int wrote = snprintf(out + used, used < size ? size - used : 0, markdown ? "%s`%s`" : "%s%s", used ? ", " : "", CLI_COMMANDS[i].name);
                used += wrote > 0 ? (size_t)wrote : 0;
                break;
            }
        }
    }
}

void cli_print_flags(FILE *out, bool markdown) {
    if (markdown) {
        fprintf(out,
                "# Flags\n\n"
                "The flag table of `ursatz` (`cli/src/flags.c`): every flag has one meaning in every command that takes it. `ursatz <command> --help` prints the flags of one command; "
                "this page lists them all with the commands that take each. It is the output of `ursatz help flags --format markdown`: do not edit. Back to the [cli docs](README.md).\n\n"
                "| Flag | Meaning | Commands |\n| --- | --- | --- |\n");
    }
    for (size_t i = 0; i < CLI_FLAG_COUNT; i++) {
        const CliFlag *flag = &CLI_FLAGS[i];
        char commands[512];
        if (cli_flag_global(flag->name)) {
            snprintf(commands, sizeof commands, "%s", markdown ? "every command (stands before it)" : "every command (stands before it)");
        } else {
            commands_of(flag, commands, sizeof commands, markdown);
        }
        if (markdown) {
            fprintf(out, "| `%s%s%s` | %s | %s |\n", flag->name, flag->value ? " " : "", flag->value ? flag->value : "", flag->help, commands);
        } else {
            char left[96];
            snprintf(left, sizeof left, "%s%s%s", flag->name, flag->value ? " " : "", flag->value ? flag->value : "");
            fprintf(out, "%-28s %s [%s]\n", left, flag->help, commands);
        }
    }
    if (markdown) {
        fprintf(out,
                "\n`--mode` is another name of `--material`. A flag that may be repeated (`--given-melody`, `--feature`, `--tone`, ...) says so in its meaning; "
                "the position of a repeated flag among the others is kept, so an `--at` follows the `--given-*` it places.\n");
    }
}

/* The text of the tool commands for `format` (and `name` for help); NULL after printing the failure. */
static char *commands_text(const char *format, const char *name) {
    Workshop *workshop = cli_tools();
    cJSON *params = cJSON_CreateObject();
    if (!workshop || !params || !cJSON_AddStringToObject(params, "format", format) || (name && !cJSON_AddStringToObject(params, "name", name))) {
        cJSON_Delete(params);
        fprintf(stderr, "Error: %s\n", workshop ? "out of memory" : "a knowledge snapshot is required");
        return NULL;
    }
    WorkshopReply reply;
    UrsatzError error;
    memset(&error, 0, sizeof error);
    UrsatzStatus status = workshop_tool(workshop, "commands", params, NULL, &reply, &error);
    cJSON_Delete(params);
    char *text = NULL;
    const cJSON *member = cJSON_GetObjectItemCaseSensitive(reply.json, "text");
    if (status == URSATZ_OK && cJSON_IsString(member)) {
        text = strdup(member->valuestring);
    } else {
        fprintf(stderr, "Error: %s\n", error.message[0] ? error.message : "the command reference is not available");
    }
    workshop_reply_destroy(&reply);
    return text;
}

static int print_commands_text(const char *format, const char *name) {
    char *text = commands_text(format, name);
    if (!text) {
        return 1;
    }
    fputs(text, stdout);
    free(text);
    return 0;
}

/* "--format <value>" after the topic; *value NULL without it. A failure is printed and false. */
static bool read_format(int argc, char **argv, const char **value) {
    *value = NULL;
    for (int i = 3; i < argc; i++) {
        if (strcmp(argv[i], "--format") == 0 && i + 1 < argc && !*value) {
            *value = argv[++i];
        } else {
            fprintf(stderr, "Error: unknown option '%s' (see ursatz help)\n", argv[i]);
            return false;
        }
    }
    return true;
}

int cli_help_topic(int argc, char **argv) {
    const char *topic = argv[2];
    const char *format = NULL;
    if (strcmp(topic, "commands") == 0) {
        if (!read_format(argc, argv, &format)) {
            return 1;
        }
        if (format && strcmp(format, "text") != 0 && strcmp(format, "markdown") != 0 && strcmp(format, "json") != 0) {
            fprintf(stderr, "Error: --format must be text, markdown or json, not '%s'\n", format);
            return 1;
        }
        return print_commands_text(!format || strcmp(format, "text") == 0 ? "sheet" : format, NULL);
    }
    if (strcmp(topic, "grammar") == 0) {
        if (argc > 3) {
            fprintf(stderr, "Error: unknown option '%s' (see ursatz help)\n", argv[3]);
            return 1;
        }
        return print_commands_text("grammar", NULL);
    }
    if (strcmp(topic, "flags") == 0) {
        if (!read_format(argc, argv, &format)) {
            return 1;
        }
        if (format && strcmp(format, "text") != 0 && strcmp(format, "markdown") != 0) {
            fprintf(stderr, "Error: --format must be text or markdown, not '%s'\n", format);
            return 1;
        }
        cli_print_flags(stdout, format && strcmp(format, "markdown") == 0);
        return 0;
    }
    if (strchr(topic, '.') != NULL) {
        if (argc > 3) {
            fprintf(stderr, "Error: unknown option '%s' (see ursatz help)\n", argv[3]);
            return 1;
        }
        return print_commands_text("help", topic);
    }
    return -1;
}
