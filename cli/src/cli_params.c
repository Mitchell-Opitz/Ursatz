#include "cli_params.h"

#include "cli_options.h"
#include "commands.h"

void cli_options_read(const CliArgs *args, const CliOptionRow *rows, size_t count, void *options) {
    for (size_t i = 0; i < count; i++) {
        char *member = (char *)options + rows[i].offset;
        if (rows[i].is_switch) {
            *(bool *)member = cli_switch(args, rows[i].flag);
        } else {
            *(const char **)member = cli_value(args, rows[i].flag);
        }
    }
}

int cli_needs(const char *command, const char *what, bool with_usage) {
    if (with_usage) {
        return cli_fail(CLI_EXIT_USAGE, "%s needs %s\n%s", command, what, cli_usage_of(command));
    }
    return cli_fail(CLI_EXIT_USAGE, "%s needs %s (see --help)", command, what);
}
