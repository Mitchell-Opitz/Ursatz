#include "cli_options.h"

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <texture_rules.h>

#include <ursatz_composer/harmony_phrase.h>

static const char *const ACCOMPANIMENT_WORDS[] = {"any", "block", "arpeggiated", "alberti", "oom-pah"};

const char *cli_accompaniment_name(PlanAccompanimentPattern pattern) {
    return ACCOMPANIMENT_WORDS[pattern];
}

int cli_read_accompaniment(const char *pattern_text, const char *texture_text, const char *meter_text, const PlanMeter *meter,
                           PlanAccompaniment *out) {
    out->pattern = PLAN_ACCOMPANIMENT_ANY;
    out->texture = -1;
    if (pattern_text) {
        bool found = false;
        for (int p = 0; p < 5 && !found; p++) {
            found = cli_equals_ignore_case(pattern_text, ACCOMPANIMENT_WORDS[p]);
            out->pattern = found ? (PlanAccompanimentPattern)p : out->pattern;
        }
        if (!found) {
            return cli_fail(CLI_EXIT_USAGE, "--accompaniment must be block, arpeggiated, alberti, oom-pah or any, not '%s'",
                            pattern_text);
        }
    }
    if (texture_text) {
        bool found = false;
        for (int t = 0; t < (int)STANDARD_TEXTURE_COUNT && !found; t++) {
            found = cli_equals_ignore_case(texture_text, standard_texture_name((StandardTexture)t));
            out->texture = found ? t : out->texture;
        }
        if (!found) {
            return cli_fail(CLI_EXIT_USAGE, "--texture must be a texture name of standard-texture, for example "
                                            "\"melody and accompaniment\", not '%s'", texture_text);
        }
        if (!standard_texture_is_homophonic((StandardTexture)out->texture)) {
            return cli_fail(CLI_EXIT_USAGE, "--texture '%s' is not one an accompaniment can have; use homophony, "
                                            "homorhythm or \"melody and accompaniment\"", texture_text);
        }
    }
    if (out->pattern == PLAN_ACCOMPANIMENT_ALBERTI && meter->beat_unit == 8) {
        return cli_fail(CLI_EXIT_USAGE, "--accompaniment alberti needs a simple meter, not %s", meter_text);
    }
    return 0;
}

void cli_print_json_string(const char *text) {
    putchar('"');
    for (; *text; text++) {
        if (*text == '"' || *text == '\\') {
            putchar('\\');
        }
        if ((unsigned char)*text >= 0x20) {
            putchar(*text);
        }
    }
    putchar('"');
}

int cli_fail(int status, const char *format, ...) {
    va_list args;
    va_start(args, format);
    fprintf(stderr, "Error: ");
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
    va_end(args);
    return status;
}

bool cli_equals_ignore_case(const char *a, const char *b) {
    for (; *a && *b; a++, b++) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) {
            return false;
        }
    }
    return *a == *b;
}

bool cli_parse_whole_number(const char *text, unsigned long maximum, unsigned long *out) {
    char *end = NULL;
    if (!text || !isdigit((unsigned char)text[0])) {
        return false;
    }
    errno = 0;
    unsigned long value = strtoul(text, &end, 10);
    if (errno != 0 || *end != '\0' || value > maximum) {
        return false;
    }
    *out = value;
    return true;
}

bool cli_parse_ratio(const char *text, ComposerRatio *out) {
    if (!text || !isdigit((unsigned char)text[0])) {
        return false;
    }
    char *end = NULL;
    errno = 0;
    unsigned long numerator = strtoul(text, &end, 10);
    unsigned long denominator = 1;
    if (errno != 0 || numerator > 4294967295UL) {
        return false;
    }
    if (*end == '/') {
        if (!isdigit((unsigned char)end[1])) {
            return false;
        }
        errno = 0;
        denominator = strtoul(end + 1, &end, 10);
        if (errno != 0 || denominator == 0 || denominator > 4294967295UL) {
            return false;
        }
    }
    if (*end != '\0') {
        return false;
    }
    out->numerator = numerator;
    out->denominator = denominator;
    return true;
}

int cli_load_style(const char *path, ComposerStyle **out) {
    ComposerStyleResult loaded = composer_style_load(path, out);
    if (loaded != COMPOSER_STYLE_OK) {
        return cli_fail(loaded == COMPOSER_STYLE_ERR_ARGUMENT ? CLI_EXIT_USAGE : CLI_EXIT_UNSATISFIABLE,
                        "cannot read style '%s': %s", path, composer_style_result_name(loaded));
    }
    return 0;
}

int cli_read_blend(bool has_style, const char *blend_path, const char *all_text, const char *list_text,
                   ComposerStyleWeights *out) {
    if (!blend_path) {
        if (all_text || list_text) {
            return cli_fail(CLI_EXIT_USAGE, "%s needs --blend", all_text ? "--blend-weight" : "--blend-weights");
        }
        return 0;
    }
    if (!has_style) {
        return cli_fail(CLI_EXIT_USAGE, "--blend needs --style");
    }
    ComposerRatio all = {1, 2};
    if (all_text) {
        if (!cli_parse_ratio(all_text, &all)) {
            return cli_fail(CLI_EXIT_USAGE, "--blend-weight must be a fraction like 1/2, not '%s'", all_text);
        }
        if (all.numerator > all.denominator) {
            return cli_fail(CLI_EXIT_USAGE, "--blend-weight must be from 0 to 1, not '%s'", all_text);
        }
    }
    *out = composer_style_weights_all(all);
    if (!list_text) {
        return 0;
    }
    bool seen[STYLE_DIMENSION_COUNT] = {false};
    const char *at = list_text;
    while (*at) {
        const char *end = strchr(at, ',');
        size_t length = end ? (size_t)(end - at) : strlen(at);
        char item[64];
        if (length == 0 || length >= sizeof item) {
            return cli_fail(CLI_EXIT_USAGE, "--blend-weights must be like harmony=1/2,form=0, not '%s'", list_text);
        }
        memcpy(item, at, length);
        item[length] = '\0';
        char *equals = strchr(item, '=');
        StyleDimension dimension;
        if (!equals) {
            return cli_fail(CLI_EXIT_USAGE, "--blend-weights must be like harmony=1/2,form=0, not '%s'", list_text);
        }
        *equals = '\0';
        if (!style_dimension_from_name(item, &dimension)) {
            return cli_fail(CLI_EXIT_USAGE, "--blend-weights: '%s' is not a dimension (harmony, rhythm, melody, form)",
                            item);
        }
        if (seen[dimension]) {
            return cli_fail(CLI_EXIT_USAGE, "--blend-weights names %s twice", item);
        }
        seen[dimension] = true;
        ComposerRatio weight;
        if (!cli_parse_ratio(equals + 1, &weight)) {
            return cli_fail(CLI_EXIT_USAGE, "--blend-weights: %s must be a fraction like 1/2, not '%s'", item,
                            equals + 1);
        }
        if (weight.numerator > weight.denominator) {
            return cli_fail(CLI_EXIT_USAGE, "--blend-weights: %s must be from 0 to 1, not '%s'", item, equals + 1);
        }
        out->numerator[dimension] = weight.numerator;
        out->denominator[dimension] = weight.denominator;
        at += length;
        if (*at == ',') {
            at++;
            if (*at == '\0') {
                return cli_fail(CLI_EXIT_USAGE, "--blend-weights must be like harmony=1/2,form=0, not '%s'", list_text);
            }
        }
    }
    return 0;
}

int cli_load_blend(const char *style_path, const char *blend_path, const ComposerStyleWeights *weights,
                   ComposerStyle **out) {
    ComposerStyle *first = NULL;
    ComposerStyle *second = NULL;
    int status = cli_load_style(style_path, &first);
    if (status == 0) {
        status = cli_load_style(blend_path, &second);
    }
    if (status == 0) {
        ComposerStyleResult blended = composer_style_blend(first, second, weights, out);
        if (blended != COMPOSER_STYLE_OK) {
            status = cli_fail(blended == COMPOSER_STYLE_ERR_ARGUMENT ? CLI_EXIT_USAGE : CLI_EXIT_UNSATISFIABLE,
                              "cannot blend '%s' with '%s': %s", style_path, blend_path,
                              composer_style_result_name(blended));
        }
    }
    composer_style_destroy(first);
    composer_style_destroy(second);
    return status;
}

static const char *const STYLE_DIMENSION_LABELS[STYLE_DIMENSION_COUNT] = {"harmony", "rhythm", "melody", "form"};

void cli_print_style_distance(const StyleComparison *comparison) {
    printf("\nstyle distance (the Analyzer's, 0 is the style, 1 is nothing alike)\n");
    for (int d = 0; d < STYLE_DIMENSION_COUNT; d++) {
        if (comparison->dimension_present[d]) {
            printf("  %-8s %.4f\n", STYLE_DIMENSION_LABELS[d], style_rational_value(comparison->dimension[d]));
        }
    }
    if (comparison->overall_present) {
        printf("  %-8s %.4f\n", "overall", style_rational_value(comparison->overall));
    }
}

void cli_print_style_distance_json(const StyleComparison *comparison) {
    printf("\"style_distance\":{");
    const char *separator = "";
    for (int d = 0; d < STYLE_DIMENSION_COUNT; d++) {
        if (comparison->dimension_present[d]) {
            printf("%s\"%s\":%.4f", separator, STYLE_DIMENSION_LABELS[d], style_rational_value(comparison->dimension[d]));
            separator = ",";
        }
    }
    if (comparison->overall_present) {
        printf("%s\"overall\":%.4f", separator, style_rational_value(comparison->overall));
    }
    printf("}");
}

bool cli_parse_key(const char *text, PlanKey *key) {
    static const int NATURAL[7] = {9, 11, 0, 2, 4, 5, 7}; /* A B C D E F G */
    while (isspace((unsigned char)*text)) {
        text++;
    }
    char letter = (char)toupper((unsigned char)*text);
    if (letter < 'A' || letter > 'G') {
        return false;
    }
    int pc = NATURAL[letter - 'A'];
    text++;
    if (*text == '#') {
        pc++;
        text++;
    } else if (*text == 'b') {
        pc--;
        text++;
    }
    if (!isspace((unsigned char)*text)) {
        return false;
    }
    while (isspace((unsigned char)*text)) {
        text++;
    }
    char mode[8];
    size_t length = 0;
    while (text[length] && !isspace((unsigned char)text[length]) && length + 1 < sizeof(mode)) {
        mode[length] = text[length];
        length++;
    }
    mode[length] = '\0';
    const char *rest = text + length;
    while (isspace((unsigned char)*rest)) {
        rest++;
    }
    if (*rest != '\0') {
        return false;
    }
    if (cli_equals_ignore_case(mode, "major")) {
        key->mode = PLAN_MODE_MAJOR;
    } else if (cli_equals_ignore_case(mode, "minor")) {
        key->mode = PLAN_MODE_MINOR;
    } else {
        return false;
    }
    key->tonic_pitch_class = (pc % 12 + 12) % 12;
    return true;
}

const char *cli_key_name(const PlanKey *key, char *out, size_t size) {
    static const char *const MAJOR[12] = {"C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"};
    static const char *const MINOR[12] = {"C", "C#", "D", "Eb", "E", "F", "F#", "G", "G#", "A", "Bb", "B"};
    int pc = key->tonic_pitch_class;
    snprintf(out, size, "%s %s", key->mode == PLAN_MODE_MINOR ? MINOR[pc] : MAJOR[pc],
             key->mode == PLAN_MODE_MINOR ? "minor" : "major");
    return out;
}

bool cli_parse_meter(const char *text, PlanMeter *meter) {
    unsigned long beats;
    unsigned long unit;
    char first[16];
    const char *slash = strchr(text, '/');
    if (!slash || (size_t)(slash - text) >= sizeof(first)) {
        return false;
    }
    memcpy(first, text, (size_t)(slash - text));
    first[slash - text] = '\0';
    if (!cli_parse_whole_number(first, 64, &beats) || !cli_parse_whole_number(slash + 1, 64, &unit) || beats == 0 ||
        unit == 0) {
        return false;
    }
    meter->beats_per_bar = (int)beats;
    meter->beat_unit = (int)unit;
    meter->bars_per_hyperbar = 0;
    return true;
}

static const char *const CADENCE_NAMES[5] = {"PAC", "IAC", "HC", "DC", "PC"};

bool cli_parse_cadence(const char *text, PlanCadenceType *cadence) {
    for (int i = 0; i < 5; i++) {
        if (cli_equals_ignore_case(text, CADENCE_NAMES[i])) {
            *cadence = (PlanCadenceType)i;
            return true;
        }
    }
    return false;
}

const char *cli_cadence_name(PlanCadenceType cadence) {
    return CADENCE_NAMES[cadence];
}

unsigned long cli_default_seed(void) {
    return ((unsigned long)time(NULL) * 2654435761UL + (unsigned long)clock()) & CLI_SEED_MAX;
}

bool cli_alternative_path(const char *path, size_t k, bool several, char *out, size_t size) {
    if (!several) {
        return snprintf(out, size, "%s", path) < (int)size;
    }
    const char *dot = strrchr(path, '.');
    const char *slash = strrchr(path, '/');
    if (!dot || (slash && dot < slash)) {
        return snprintf(out, size, "%s-%zu", path, k) < (int)size;
    }
    return snprintf(out, size, "%.*s-%zu%s", (int)(dot - path), path, k, dot) < (int)size;
}
