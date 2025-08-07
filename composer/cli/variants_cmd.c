#include "variants_cmd.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/variant_registry.h>

#include "cli_options.h"

typedef struct {
    bool list_ops;
    bool json;
    bool help;
} Options;

static void print_help(FILE *out) {
    fprintf(out, "Usage:\n%s\n\n", VARIANTS_USAGE_LINE + 7);
    fprintf(out, "Lists the variant operations of the registry.\n\n");
    fprintf(out, "  --list-ops           print every operation: name, label, category, citation, parameters and the\n");
    fprintf(out, "                       forms it applies to\n");
    fprintf(out, "  --json               print as JSON\n");
    fprintf(out, "  --help               this text\n\n");
    fprintf(out, "Exit status: 0 success; 1 usage or an invalid argument.\n\n");
    fprintf(out, "Example:\n");
    fprintf(out, "  ursatz-composer variants --list-ops\n");
}

static int read_options(int argc, char **argv, Options *options) {
    memset(options, 0, sizeof(*options));
    for (int i = 2; i < argc; i++) {
        const char *argument = argv[i];
        if (strcmp(argument, "--help") == 0) {
            options->help = true;
        } else if (strcmp(argument, "--json") == 0) {
            options->json = true;
        } else if (strcmp(argument, "--list-ops") == 0) {
            options->list_ops = true;
        } else if (argument[0] == '-' && argument[1] != '\0') {
            return cli_fail(CLI_EXIT_USAGE, "unknown option '%s'", argument);
        } else {
            return cli_fail(CLI_EXIT_USAGE, "unexpected argument '%s'", argument);
        }
    }
    return 0;
}

static const char *const FORM_NAMES[] = {"full", "rhythm", "contour"};

static void print_forms(const VariantOperation *operation) {
    const char *separator = "";
    for (unsigned form = 0; form < 3; form++) {
        if (operation->forms & VARIANT_FORM_BIT(form)) {
            printf("%s%s", separator, FORM_NAMES[form]);
            separator = ", ";
        }
    }
    printf("\n");
}

static void print_operations(void) {
    for (size_t i = 0; i < variant_registry_count(); i++) {
        const VariantOperation *operation = variant_registry_at(i);
        printf("%s  %s  (%s; %s)\n", operation->name, operation->label, variant_category_name(operation->category),
               operation->citation);
        for (size_t p = 0; p < operation->param_count; p++) {
            const VariantParam *param = &operation->params[p];
            printf("  --%s  %s  %s\n", param->name, variant_param_kind_name(param->kind), param->description);
        }
        printf("  forms: ");
        print_forms(operation);
    }
}

int run_variants(int argc, char **argv) {
    Options options;
    int status = read_options(argc, argv, &options);
    if (status != 0) {
        return status;
    }
    if (options.help) {
        print_help(stdout);
        return 0;
    }
    if (!options.list_ops) {
        return cli_fail(CLI_EXIT_USAGE, "variants needs --list-ops\n%s", VARIANTS_USAGE_LINE);
    }
    if (options.json) {
        char *json = variant_registry_to_json();
        if (!json) {
            return cli_fail(CLI_EXIT_UNSATISFIABLE, "out of memory");
        }
        fputs(json, stdout);
        free(json);
    } else {
        print_operations();
    }
    return 0;
}
