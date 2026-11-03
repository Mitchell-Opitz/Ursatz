#include "cli_options.h"

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <texture_rules.h>

#include "plan.h"

#include <ursatz_composer/harmony_phrase.h>
#include "util/plan_util.h"

static const char *const ACCOMPANIMENT_WORDS[] = {"any", "block", "arpeggiated", "alberti", "oom-pah", "motive"};

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

int cli_read_export_options(const char *tempo_text, bool plain_midi, PlanExportOptions *out) {
    *out = plan_export_options_default();
    if (plain_midi) {
        out->track_names = false;
        out->section_markers = false;
    }
    if (!tempo_text) {
        return 0;
    }
    ComposerRatio tempo;
    if (!cli_parse_ratio(tempo_text, &tempo) || tempo.denominator == 0 || tempo.numerator < 20UL * tempo.denominator ||
        tempo.numerator > 400UL * tempo.denominator) {
        return cli_fail(CLI_EXIT_USAGE,
                        "--tempo must be from 20 to 400 quarter notes per minute, a whole number or a fraction like "
                        "195/2, not '%s'",
                        tempo_text);
    }
    out->tempo_numerator = tempo.numerator;
    out->tempo_denominator = tempo.denominator;
    return 0;
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

static const char *const STYLE_CATEGORIES[] = {
    STYLE_CATEGORY_CHORD_NUMERAL,     STYLE_CATEGORY_CHORD_PROGRESSION_2, STYLE_CATEGORY_CHORD_PROGRESSION_3,
    STYLE_CATEGORY_CADENCE_TYPE,      STYLE_CATEGORY_HARMONIC_RHYTHM,     STYLE_CATEGORY_DURATION,
    STYLE_CATEGORY_METER,             STYLE_CATEGORY_SYNCOPATION,         STYLE_CATEGORY_MELODIC_INTERVAL,
    STYLE_CATEGORY_CONTOUR,           STYLE_CATEGORY_PHRASE_RANGE,        STYLE_CATEGORY_MOTIVE_DEGREES,
    STYLE_CATEGORY_MOTIVE_INTERVALS,  STYLE_CATEGORY_MOTIVE_RHYTHM,       STYLE_CATEGORY_MOTIVE_NCT_FIGURE,
    STYLE_CATEGORY_THEME_FORM,        STYLE_CATEGORY_PHRASE_LENGTH,       STYLE_CATEGORY_SECTION_LETTERS,
    STYLE_CATEGORY_MODE,              STYLE_CATEGORY_TEMPO,               STYLE_CATEGORY_NOTE_DENSITY};

int cli_style_without_add(CliStyleWithout *list, const char *name) {
    bool known = false;
    for (size_t i = 0; i < sizeof STYLE_CATEGORIES / sizeof *STYLE_CATEGORIES; i++) {
        known = known || strcmp(name, STYLE_CATEGORIES[i]) == 0;
    }
    if (!known) {
        char names[640];
        size_t used = 0;
        names[0] = '\0';
        for (size_t i = 0; i < sizeof STYLE_CATEGORIES / sizeof *STYLE_CATEGORIES && used < sizeof names; i++) {
            int written = snprintf(names + used, sizeof names - used, "%s%s", i ? ", " : "", STYLE_CATEGORIES[i]);
            used += written > 0 ? (size_t)written : 0;
        }
        return cli_fail(CLI_EXIT_USAGE, "--style-without: '%s' is not a style category (the categories are %s)", name,
                        names);
    }
    for (size_t i = 0; i < list->count; i++) {
        if (strcmp(list->names[i], name) == 0) {
            return 0;
        }
    }
    if (list->count >= CLI_STYLE_WITHOUT_MAX) {
        return cli_fail(CLI_EXIT_USAGE, "--style-without is given at most %d times", CLI_STYLE_WITHOUT_MAX);
    }
    list->names[list->count++] = name;
    return 0;
}

int cli_load_style(const char *path, const char *const *without, size_t without_count, ComposerStyle **out) {
    ComposerStyleResult loaded = composer_style_load_without(path, without, without_count, out);
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
                   const char *const *without, size_t without_count, ComposerStyle **out) {
    ComposerStyle *first = NULL;
    ComposerStyle *second = NULL;
    int status = cli_load_style(style_path, without, without_count, &first);
    if (status == 0) {
        status = cli_load_style(blend_path, without, without_count, &second);
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

int cli_write_explanation(const char *final_path, const char *write_path, const Explanation *explanation) {
    size_t length = strlen(final_path);
    bool json = length >= 5 && cli_equals_ignore_case(final_path + length - 5, ".json");
    char *text = json ? explain_to_json(explanation) : explain_to_text(explanation);
    if (!text) {
        return -1;
    }
    FILE *file = fopen(write_path, "wb");
    bool ok = file && fputs(text, file) >= 0;
    free(text);
    if (file && fclose(file) != 0) {
        ok = false;
    }
    return ok ? 0 : -1;
}

/* --- seeding from a corpus ------------------------------------------------------- */

static int seed_failure(CorpusSeedResult result, const char *corpus_dir, const char *what) {
    switch (result) {
    case CORPUS_SEED_ERR_NO_MATCH:
        return cli_fail(CLI_EXIT_UNSATISFIABLE, "no passage of the corpus '%s' is similar to %s (a score of 1/2 or more)",
                        corpus_dir, what);
    case CORPUS_SEED_ERR_QUERY:
        return cli_fail(CLI_EXIT_USAGE, "cannot read %s as a passage to look for", what);
    case CORPUS_SEED_ERR_CORPUS:
        return cli_fail(CLI_EXIT_UNSATISFIABLE,
                        "cannot read the corpus '%s' (the output directory of ursatz-analyzer --corpus)", corpus_dir);
    default:
        return cli_fail(CLI_EXIT_UNSATISFIABLE, "%s", corpus_seed_result_name(result));
    }
}

int cli_seed_motive(const char *corpus_dir, const char *like_path, const PlanMeter *meter, const PlanKey *key,
                    unsigned long seed, CorpusSeed *out) {
    CorpusSeedResult result = corpus_seed_motive(corpus_dir, like_path, meter, key, seed, out);
    return result == CORPUS_SEED_OK ? 0 : seed_failure(result, corpus_dir, like_path);
}

int cli_seed_progression(const char *corpus_dir, const char *numerals_text, PlanKeyMode mode, CorpusSeed *out) {
    CorpusSeedResult result = corpus_seed_progression(corpus_dir, numerals_text, mode, out);
    return result == CORPUS_SEED_OK ? 0 : seed_failure(result, corpus_dir, numerals_text);
}

void cli_seed_numerals_text(const CorpusSeed *seed, char *out, size_t size) {
    size_t used = 0;
    out[0] = '\0';
    for (size_t i = 0; i < seed->numeral_count && used + 1 < size; i++) {
        int written = snprintf(out + used, size - used, i ? " %s" : "%s", seed->numerals[i]);
        if (written < 0 || (size_t)written >= size - used) {
            break;
        }
        used += (size_t)written;
    }
}

void cli_print_seed_line(const char *what, const CorpusSeed *seed) {
    char span[96];
    corpus_seed_describe(seed, span, sizeof(span));
    printf("%s from %s %s, score %lu/%lu\n", what, seed->piece, span, seed->score_numerator, seed->score_denominator);
}

void cli_print_seed_json(const CorpusSeed *seed) {
    printf("{\"piece\":");
    cli_print_json_string(seed->piece);
    printf(",\"start\":{\"measure\":%d,\"beat\":%g},\"end\":{\"measure\":%d,\"beat\":%g},\"score\":\"%lu/%lu\"}",
           seed->start_measure, seed->start_beat, seed->end_measure, seed->end_beat, seed->score_numerator,
           seed->score_denominator);
}

int cli_seed_source(const CorpusSeed *seed, PlanSource *out) {
    memset(out, 0, sizeof(*out));
    if (snprintf(out->piece, PLAN_SOURCE_PIECE_MAX, "%s", seed->piece) >= PLAN_SOURCE_PIECE_MAX) {
        return cli_fail(CLI_EXIT_UNSATISFIABLE, "the source piece's path is too long to record");
    }
    out->start_measure = seed->start_measure;
    out->start_beat = seed->start_beat;
    out->end_measure = seed->end_measure;
    out->end_beat = seed->end_beat;
    out->score_numerator = seed->score_numerator;
    out->score_denominator = seed->score_denominator;
    return 0;
}

int cli_add_source(PiecePlan *plan, NodeId node, const CorpusSeed *seed) {
    PlanConstraint constraint;
    memset(&constraint, 0, sizeof(constraint));
    constraint.kind = PLAN_CONSTRAINT_SOURCE;
    int status = cli_seed_source(seed, &constraint.params.source);
    if (status != 0) {
        return status;
    }
    if (node == PLAN_NODE_NONE || plan_node_add_constraint(plan, node, &constraint) != PLAN_OK) {
        return cli_fail(CLI_EXIT_UNSATISFIABLE, "cannot record the source of the seeded material in the plan");
    }
    return 0;
}

NodeId cli_find_node(const PiecePlan *plan, const char *name) {
    for (NodeId id = 1; id <= plan_node_count(plan); id++) {
        const char *found = plan_node_name(plan, id);
        if (found && strcmp(found, name) == 0) {
            return id;
        }
    }
    return PLAN_NODE_NONE;
}

/* --- the node a command works on ------------------------------------------------ */


int cli_check_target(CliTargetOptions *options, const char *command, const char *usage) {
    if (!options->node_text == !options->section_text) {
        return cli_fail(CLI_EXIT_USAGE, "%s needs a node: give --node <id> or --section <letter>, not both\n%s", command, usage);
    }
    if (options->node_text && !cli_parse_whole_number(options->node_text, 1000000UL, &options->node)) {
        return cli_fail(CLI_EXIT_USAGE, "--node must be a whole number (the id in the plan file), not '%s'", options->node_text);
    }
    options->has_lane = false;
    if (options->lane_text) {
        options->has_lane = true;
        if (cli_equals_ignore_case(options->lane_text, "harmony")) {
            options->lane = PLAN_LANE_HARMONY;
        } else if (cli_equals_ignore_case(options->lane_text, "bass")) {
            options->lane = PLAN_LANE_BASS;
        } else if (cli_equals_ignore_case(options->lane_text, "melody")) {
            options->lane = PLAN_LANE_MELODY;
        } else if (cli_equals_ignore_case(options->lane_text, "inner")) {
            options->lane = PLAN_LANE_INNER_VOICE;
        } else {
            return cli_fail(CLI_EXIT_USAGE, "--lane must be harmony, bass, melody or inner, not '%s'", options->lane_text);
        }
    }
    return 0;
}

static const char *const TARGET_LANE_LABELS[] = {"meter", "key", "harmony", "bass", "melody", "inner voice"};

/* The node the options name, or the exit status of the error it printed. */
int cli_find_target_node(const PiecePlan *plan, const CliTargetOptions *options, bool quiet, NodeId *out) {
    if (options->node_text) {
        if (!plan_has_node(plan, (NodeId)options->node)) {
            return quiet ? CLI_EXIT_UNSATISFIABLE : cli_fail(CLI_EXIT_UNSATISFIABLE, "the plan has no node %lu (ids run from 1 to %zu)", options->node,
                            plan_node_count(plan));
        }
        *out = (NodeId)options->node;
        return 0;
    }
    CliSection sections[CLI_MAX_SECTIONS];
    size_t count = cli_piece_sections(plan, sections, CLI_MAX_SECTIONS);
    count = count > CLI_MAX_SECTIONS ? CLI_MAX_SECTIONS : count;
    for (size_t i = 0; i < count; i++) {
        if (strcmp(sections[i].letter, options->section_text) == 0 ||
            (options->section_text[1] == '\0' && cli_equals_ignore_case(sections[i].letter, options->section_text))) {
            *out = sections[i].node;
            return 0;
        }
    }
    return quiet ? CLI_EXIT_UNSATISFIABLE : cli_fail(CLI_EXIT_UNSATISFIABLE, "the plan has no section '%s'", options->section_text);
}

/*
 * The targets: the node, or with a lane every span of it under the node (all the voices for inner). Into a new array;
 * the exit status of the error printed otherwise.
 */
int cli_find_targets(const PiecePlan *plan, const CliTargetOptions *options, FillTarget **out, size_t *count) {
    NodeId node;
    int status = cli_find_target_node(plan, options, false, &node);
    if (status != 0) {
        return status;
    }
    *out = calloc(plan_lane_span_count(plan) + 1, sizeof(FillTarget));
    *count = 0;
    if (!*out) {
        return cli_fail(CLI_EXIT_UNSATISFIABLE, "out of memory");
    }
    if (!options->has_lane) {
        (*out)[(*count)++] = (FillTarget){FILL_TARGET_NODE, node};
        return 0;
    }
    for (LaneSpanId id = 1; id <= plan_lane_span_count(plan); id++) {
        if (plan_has_lane_span(plan, id) && plan_lane_span_lane(plan, id) == options->lane &&
            plan_in_subtree(plan, plan_lane_span_owner(plan, id), node)) {
            (*out)[(*count)++] = (FillTarget){FILL_TARGET_LANE_SPAN, id};
        }
    }
    if (*count == 0) {
        free(*out);
        *out = NULL;
        return cli_fail(CLI_EXIT_UNSATISFIABLE, "node %zu has no %s spans", (size_t)node, TARGET_LANE_LABELS[options->lane]);
    }
    return 0;
}

/* The name of the target in words: "section B", "node 12", "the harmony of section B". */
void cli_describe_target(const PiecePlan *plan, const CliTargetOptions *options, char *out, size_t size) {
    NodeId node;
    char what[64];
    if (cli_find_target_node(plan, options, true, &node) != 0) {
        snprintf(out, size, "the node");
        return;
    }
    CliSection sections[CLI_MAX_SECTIONS];
    size_t count = cli_piece_sections(plan, sections, CLI_MAX_SECTIONS);
    count = count > CLI_MAX_SECTIONS ? CLI_MAX_SECTIONS : count;
    snprintf(what, sizeof(what), "node %zu", (size_t)node);
    for (size_t i = 0; i < count; i++) {
        if (sections[i].node == node) {
            snprintf(what, sizeof(what), "section %s", sections[i].letter);
        }
    }
    if (options->has_lane) {
        snprintf(out, size, "the %s of %s", TARGET_LANE_LABELS[options->lane], what);
    } else {
        snprintf(out, size, "%s", what);
    }
}

int cli_report_refused(RegenerateResult result, const char *what) {
    switch (result) {
    case REGENERATE_ERR_LOCKED:
        return cli_fail(CLI_EXIT_UNSATISFIABLE,
                        "%s is Given (locked) or has Given material among its phrases; locking is for good in a plan, to unlock go "
                        "back to an earlier plan file",
                        what);
    case REGENERATE_ERR_BAD_TARGET:
        return cli_fail(CLI_EXIT_UNSATISFIABLE,
                        "%s cannot be regenerated: a motive, the material, the meter and key, and spans the Composer did not "
                        "open have nothing to write again",
                        what);
    case REGENERATE_ERR_NOT_FOUND:
        return cli_fail(CLI_EXIT_UNSATISFIABLE, "%s is not in the plan", what);
    default:
        return cli_fail(CLI_EXIT_UNSATISFIABLE, "cannot regenerate %s: %s", what, regenerate_result_name(result));
    }
}

void cli_discard_pending(CliPendingFile *pending, size_t count) {
    for (size_t i = 0; i < count; i++) {
        remove(pending[i].temporary);
    }
}

bool cli_parse_pickup(const char *text, int beat_unit, PlanTime *out) {
    char *end = NULL;
    long numerator = strtol(text, &end, 10);
    long denominator = 1;
    if (end == text || numerator < 0) {
        return false;
    }
    if (*end == '/') {
        const char *rest = end + 1;
        denominator = strtol(rest, &end, 10);
        if (end == rest || denominator < 1) {
            return false;
        }
    }
    return *end == '\0' && plan_time_make(numerator, (unsigned long)(denominator * beat_unit), out) == PLAN_OK;
}

char *cli_motive_text(const Motive *motive) {
    return motive->form == MOTIVE_FORM_RHYTHM    ? motive_to_rhythm_text(motive)
           : motive->form == MOTIVE_FORM_CONTOUR ? motive_to_contour_text(motive)
                                                 : motive_to_notes_text(motive);
}
