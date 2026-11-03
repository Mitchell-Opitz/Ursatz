#include "request_read.h"

#include <stdio.h>
#include <string.h>

#include "api/api_internal.h"
#include "cli_options.h"

bool request_member_string(const cJSON *object, const char *name, bool required, const char **out) {
    ComposerError error;
    memset(&error, 0, sizeof error);
    if (api_request_string(object, "", name, required, out, &error)) {
        return true;
    }
    cli_fail(CLI_EXIT_USAGE, "%s", error.message);
    return false;
}

bool request_member_pickup(const cJSON *object, char *text, size_t size, const char **out) {
    ComposerError error;
    memset(&error, 0, sizeof error);
    if (api_request_pickup(object, "", text, size, out, &error)) {
        return true;
    }
    cli_fail(CLI_EXIT_USAGE, "%s", error.message);
    return false;
}

int request_read_motive(const char *what, const cJSON *object, const char *role, const PlanMeter *meter, const AnalysisKey *key,
                        Motive *out) {
    ComposerError error;
    memset(&error, 0, sizeof error);
    ComposerStatus status = api_read_degrees_motive(what, "", object, role, meter, key, out, &error);
    return status == COMPOSER_OK ? 0 : cli_fail(CLI_EXIT_USAGE, "%s", error.message);
}

bool request_read_functions(const char *what, const cJSON *functions, unsigned *out) {
    ComposerError error;
    memset(&error, 0, sizeof error);
    if (api_read_functions(what, "", functions, out, &error) == COMPOSER_OK) {
        return true;
    }
    cli_fail(CLI_EXIT_USAGE, "%s", error.message);
    return false;
}

static void print_number(const ComposerRatio *ratio) {
    if (ratio->denominator == 1) {
        printf("%lu", ratio->numerator);
    } else {
        printf("%g", (double)ratio->numerator / (double)ratio->denominator);
    }
}

void cli_print_brief(const GrowBrief *brief, bool json) {
    if (json) {
        cJSON *document = api_brief_json(brief);
        char *text = document ? cJSON_PrintUnformatted(document) : NULL;
        printf("\"brief\":%s", text ? text : "{}");
        cJSON_free(text);
        cJSON_Delete(document);
        return;
    }
    printf("brief: mode %s (%s)", brief->minor ? "minor" : "major", brief->mode_from);
    if (brief->tempo.denominator > 0) {
        printf(", tempo ");
        print_number(&brief->tempo);
        printf(" (%s)", brief->tempo_from);
    }
    if (brief->has_density) {
        printf(", density ");
        print_number(&brief->density);
        printf(" attacks per beat");
    }
    if (brief->without_count > 0) {
        printf(", without");
        for (size_t i = 0; i < brief->without_count; i++) {
            printf("%s %s", i ? "," : "", brief->without[i]);
        }
    }
    printf("\n");
}
