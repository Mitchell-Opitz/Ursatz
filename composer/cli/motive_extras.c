#include "motive_extras.h"

#include <string.h>

#include "cli_options.h"

bool motive_extras_take(MotiveExtras *extras, int argc, char **argv, int *index, bool role_only, int *status) {
    const char *argument = argv[*index];
    bool harmony = !role_only && strcmp(argument, "--harmony") == 0;
    bool feature = !role_only && strcmp(argument, "--feature") == 0;
    bool role = strcmp(argument, "--role") == 0;
    if (!harmony && !feature && !role) {
        return false;
    }
    *status = 0;
    if (*index + 1 >= argc) {
        *status = cli_fail(CLI_EXIT_USAGE, "%s needs a value", argument);
        return true;
    }
    const char *value = argv[++*index];
    const char **slot = harmony ? &extras->harmony_text : &extras->role_text;
    if (feature) {
        if (extras->feature_count == MOTIVE_MAX_FEATURES) {
            *status = cli_fail(CLI_EXIT_USAGE, "a motive has at most %d features", MOTIVE_MAX_FEATURES);
            return true;
        }
        extras->feature_texts[extras->feature_count++] = value;
    } else if (*slot) {
        *status = cli_fail(CLI_EXIT_USAGE, "%s given twice", argument);
    } else {
        *slot = value;
    }
    return true;
}

bool motive_extras_given(const MotiveExtras *extras) {
    return extras->harmony_text || extras->feature_count > 0 || extras->role_text;
}

int motive_extras_options(const MotiveExtras *extras, const PlanMeter *meter, MotiveOptions *options) {
    if (extras->harmony_text) {
        MotiveError error;
        if (!meter ||
            motive_harmony_parse(extras->harmony_text, meter, options->harmony, &options->harmony_count, &error) !=
                MOTIVE_OK) {
            return cli_fail(CLI_EXIT_USAGE, "--harmony '%s' at %s: %s", extras->harmony_text,
                            meter ? error.location : "the meter", meter ? error.message : "is not known");
        }
        options->has_harmony = true;
    }
    if (extras->feature_count > 0) {
        for (size_t i = 0; i < extras->feature_count; i++) {
            MotiveError error;
            if (motive_feature_parse(extras->feature_texts[i], &options->features[i], &error) != MOTIVE_OK) {
                return cli_fail(CLI_EXIT_USAGE, "--feature '%s': %s", extras->feature_texts[i], error.message);
            }
            for (size_t j = 0; j < i; j++) {
                if (strcmp(options->features[j].label, options->features[i].label) == 0) {
                    return cli_fail(CLI_EXIT_USAGE, "--feature label '%s' is used twice", options->features[i].label);
                }
            }
        }
        options->feature_count = extras->feature_count;
        options->has_features = true;
    }
    if (extras->role_text) {
        if (!motive_role_parse(extras->role_text, &options->role)) {
            return cli_fail(CLI_EXIT_USAGE, "--role must be melodic or accompaniment, not '%s'", extras->role_text);
        }
        options->has_role = true;
    }
    return 0;
}

void motive_extras_print_help(FILE *out, bool role_only) {
    if (!role_only) {
        fprintf(out, "  --harmony \"<text>\"   the motive's own chords as Roman numerals with onsets in beats after @, as\n");
        fprintf(out, "                       \"I V6@2 I@3\" (the first needs no onset); up to %d chords\n", MOTIVE_MAX_CHORDS);
        fprintf(out, "  --feature <l:a-b>    a labelled run of notes by 0-based index, as a:0-2; repeatable, up to %d\n",
                MOTIVE_MAX_FEATURES);
    }
    fprintf(out, "  --role <role>        melodic (default) or accompaniment\n");
}
