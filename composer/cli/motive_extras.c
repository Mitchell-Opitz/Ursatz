#include "motive_extras.h"

#include <string.h>

#include "api/api_internal.h"
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

bool motive_extras_add_members(const MotiveExtras *extras, cJSON *object) {
    bool ok = true;
    if (extras->harmony_text) {
        ok = cJSON_AddStringToObject(object, "harmony", extras->harmony_text) != NULL;
    }
    if (ok && extras->feature_count > 0) {
        cJSON *features = cJSON_AddArrayToObject(object, "features");
        ok = features != NULL;
        for (size_t i = 0; ok && i < extras->feature_count; i++) {
            cJSON *item = cJSON_CreateString(extras->feature_texts[i]);
            ok = item && cJSON_AddItemToArray(features, item);
            if (!ok) {
                cJSON_Delete(item);
            }
        }
    }
    if (ok && extras->role_text) {
        ok = cJSON_AddStringToObject(object, "role", extras->role_text) != NULL;
    }
    return ok;
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
