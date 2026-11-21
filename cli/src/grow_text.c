#include "grow_text.h"

#include <stdio.h>
#include <string.h>

#include "cli_options.h"
#include <common/json.h>

/* The text output of grow and theme, from the result document of the composer API alone. */

static double number_of(const cJSON *object, const char *name) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);
    return cJSON_IsNumber(item) ? item->valuedouble : 0;
}

/* A ratio the way the text writes it: a whole number, else with %g. */
static void print_number(double value) {
    if (value == (double)(unsigned long)value) {
        printf("%lu", (unsigned long)value);
    } else {
        printf("%g", value);
    }
}

static void print_structure(const cJSON *structure) {
    /* The nodes are listed depth first: a node is inside the one before it while it starts before that one ends. */
    double ends[16];
    size_t depth = 0;
    const cJSON *node = NULL;
    cJSON_ArrayForEach(node, structure) {
        double start = number_of(node, "start");
        double end = number_of(node, "end");
        while (depth > 0 && start >= ends[depth - 1]) {
            depth--;
        }
        printf("%*s%s (%s) bars %g-%g\n", 2 + 2 * (int)depth, "", common_json_string(node, "name"), common_json_string(node, "kind"), start, end);
        if (depth < 16) {
            ends[depth++] = end;
        }
    }
}

static void print_theme_text(const cJSON *theme) {
    printf("seed %.0f\n", number_of(theme, "seed"));
    const cJSON *construction = cJSON_GetObjectItemCaseSensitive(theme, "construction");
    if (cJSON_IsObject(construction)) {
        printf("construction: %s\n", common_json_string(construction, "text"));
    }
    printf("\nstructure (bars from the start of the theme)\n");
    print_structure(cJSON_GetObjectItemCaseSensitive(theme, "structure"));
    printf("\n%-4s %-4s %-10s %-12s %s\n", "bar", "beat", "chord", "function", "bass");
    const cJSON *row = NULL;
    cJSON_ArrayForEach(row, cJSON_GetObjectItemCaseSensitive(theme, "chords")) {
        printf("%-4.0f %-4.0f %-10s %-12s %s\n", number_of(row, "bar"), number_of(row, "beat"), common_json_string(row, "chord"),
               common_json_string(row, "function"), common_json_string(row, "bass"));
    }
    printf("\nmotive developments (m0 is the motive given)\n");
    cJSON_ArrayForEach(row, cJSON_GetObjectItemCaseSensitive(theme, "developments")) {
        printf("  %s: %s\n", common_json_string(row, "motive"), common_json_string(row, "summary"));
    }
    if (cJSON_IsArray(cJSON_GetObjectItemCaseSensitive(theme, "slots"))) {
        printf("\nslots (the motive each idea is made from)\n");
    }
    cJSON_ArrayForEach(row, cJSON_GetObjectItemCaseSensitive(theme, "slots")) {
        const cJSON *item = cJSON_GetObjectItemCaseSensitive(row, "item");
        const cJSON *chain = cJSON_GetObjectItemCaseSensitive(row, "chain");
        const cJSON *connection = cJSON_GetObjectItemCaseSensitive(row, "connection");
        printf("  %s: %s slot, motive %s, %s", common_json_string(row, "idea"), common_json_string(row, "slot"), common_json_string(row, "motive"),
               common_json_string(row, "source"));
        if (cJSON_IsString(item)) {
            printf(" %s", item->valuestring);
        }
        if (cJSON_IsString(chain)) {
            printf(" (%s)", chain->valuestring);
        }
        if (cJSON_IsNumber(connection)) {
            printf(", connection %.0f%%", connection->valuedouble);
        }
        printf("\n");
    }
    const cJSON *score = cJSON_GetObjectItemCaseSensitive(theme, "score");
    double numerator = number_of(score, "numerator");
    double denominator = number_of(score, "denominator");
    unsigned long percent = denominator > 0 ? (unsigned long)((100.0 * numerator + denominator / 2) / denominator) : 0;
    printf("\nstandard-form rule score %.0f/%.0f (%lu%%)\n", numerator, denominator, percent);
    printf("%s\n", common_json_string(theme, "heard_text"));
    const cJSON *distance = cJSON_GetObjectItemCaseSensitive(theme, "style_distance");
    if (cJSON_IsObject(distance)) {
        printf("\nstyle distance (the Analyzer's, 0 is the style, 1 is nothing alike)\n");
        static const char *const NAMES[] = {"harmony", "rhythm", "melody", "form", "overall"};
        for (size_t i = 0; i < sizeof NAMES / sizeof NAMES[0]; i++) {
            const cJSON *value = cJSON_GetObjectItemCaseSensitive(distance, NAMES[i]);
            if (cJSON_IsNumber(value)) {
                printf("  %-8s %.4f\n", NAMES[i], value->valuedouble);
            }
        }
    }
}

/* The brief as theme prints it: grow's brief lines. */
void cli_print_brief_json(const cJSON *brief) {
    const cJSON *material = cJSON_GetObjectItemCaseSensitive(brief, "material");
    char word[64];
    printf("brief: mode %s (%s)", cli_material_word(common_json_string(material, "material"), word, sizeof word), common_json_string(material, "from"));
    const cJSON *brief_tempo = cJSON_GetObjectItemCaseSensitive(brief, "tempo");
    if (cJSON_IsNumber(cJSON_GetObjectItemCaseSensitive(brief_tempo, "qpm"))) {
        printf(", tempo ");
        print_number(number_of(brief_tempo, "qpm"));
        printf(" (%s)", common_json_string(brief_tempo, "from"));
    }
    if (cJSON_IsNumber(cJSON_GetObjectItemCaseSensitive(brief, "density"))) {
        printf(", density ");
        print_number(number_of(brief, "density"));
        printf(" attacks per beat");
    }
    const cJSON *without = cJSON_GetObjectItemCaseSensitive(brief, "without");
    if (cJSON_GetArraySize(without) > 0) {
        printf(", without");
        const cJSON *name = NULL;
        size_t at = 0;
        cJSON_ArrayForEach(name, without) {
            printf("%s %s", at++ ? "," : "", name->valuestring);
        }
    }
    printf("\n");
}

void cli_print_grow_text(const cJSON *document, bool numbered, const char *const *files, size_t file_count) {
    printf("%s (%s), key %s, meter %s, %.0f bars, cadence %s", common_json_string(document, "into"), common_json_string(document, "pairing"),
           common_json_string(document, "key"), common_json_string(document, "meter"), number_of(document, "bars"), common_json_string(document, "cadence"));
    if (cJSON_GetObjectItemCaseSensitive(document, "first_cadence")) {
        printf(", first cadence %s", common_json_string(document, "first_cadence"));
    }
    if (cJSON_GetObjectItemCaseSensitive(document, "accompaniment")) {
        printf(", accompaniment %s", common_json_string(document, "accompaniment"));
        if (cJSON_GetObjectItemCaseSensitive(document, "texture")) {
            printf(" (%s)", common_json_string(document, "texture"));
        }
    }
    const char *voicing = common_json_string(document, "voicing");
    if (voicing[0]) {
        printf(", %s", strcmp(voicing, "chorale") == 0 ? "chorale (four parts)" : "voiced in four parts");
    }
    printf("\n");
    const cJSON *tempo = cJSON_GetObjectItemCaseSensitive(document, "tempo");
    if (cJSON_IsNumber(cJSON_GetObjectItemCaseSensitive(tempo, "qpm"))) {
        printf("tempo: ");
        print_number(number_of(tempo, "qpm"));
        printf("%s\n", strcmp(common_json_string(tempo, "from"), "style") == 0 ? " (from the style)" : "");
    }
    const cJSON *brief = cJSON_GetObjectItemCaseSensitive(document, "brief");
    if (brief) {
        cli_print_brief_json(brief);
    }
    const cJSON *entry = NULL;
    cJSON_ArrayForEach(entry, cJSON_GetObjectItemCaseSensitive(document, "chords")) {
        printf("chords bar %.0f: %s\n", number_of(entry, "bar"), common_json_string(entry, "text"));
    }
    const cJSON *source = cJSON_GetObjectItemCaseSensitive(document, "source");
    if (source) {
        const cJSON *start = cJSON_GetObjectItemCaseSensitive(source, "start");
        const cJSON *end = cJSON_GetObjectItemCaseSensitive(source, "end");
        printf("motive from %s m.%.0f b.%g - m.%.0f b.%g, score %s\n", common_json_string(source, "piece"), number_of(start, "measure"),
               number_of(start, "beat"), number_of(end, "measure"), number_of(end, "beat"), common_json_string(source, "score"));
    }
    const cJSON *themes = cJSON_GetObjectItemCaseSensitive(document, "themes");
    size_t count = (size_t)cJSON_GetArraySize(themes);
    for (size_t k = 0; k < count; k++) {
        if (numbered) {
            printf("\ntheme %zu of %zu\n", k + 1, count);
        } else {
            printf("\n");
        }
        print_theme_text(cJSON_GetArrayItem(themes, (int)k));
    }
    for (size_t i = 0; i < file_count; i++) {
        printf("wrote %s\n", files[i]);
    }
}
