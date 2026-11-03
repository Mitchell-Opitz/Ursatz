#include "piece_text.h"

#include <stdio.h>
#include <string.h>

#include "grow_text.h"

static double number_of(const cJSON *object, const char *name) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);
    return cJSON_IsNumber(item) ? item->valuedouble : 0;
}

static const char *string_of(const cJSON *object, const char *name) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);
    return cJSON_IsString(item) ? item->valuestring : "";
}

/* A time "2/1" as the text writes it: a whole number, else numerator/denominator. */
static void print_time(const char *time) {
    long long numerator = 0;
    long long denominator = 1;
    if (sscanf(time, "%lld/%lld", &numerator, &denominator) == 2 && denominator == 1) {
        printf("%lld", numerator);
    } else {
        printf("%s", time);
    }
}

static const char *yes_no(const cJSON *object, const char *name) {
    return cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(object, name)) ? "yes" : "no";
}

static void print_version_text(const char *label, const cJSON *version) {
    printf("\n%s version\n", label);
    const cJSON *row = NULL;
    cJSON_ArrayForEach(row, cJSON_GetObjectItemCaseSensitive(version, "sections")) {
        printf("  %-10s %-2s %-3s start ", string_of(row, "id"), string_of(row, "part"), string_of(row, "letter"));
        print_time(string_of(row, "start"));
        printf(", %.0f beats", number_of(row, "beats"));
        if (cJSON_IsString(cJSON_GetObjectItemCaseSensitive(row, "cadence"))) {
            printf(", cadence %s", string_of(row, "cadence"));
        }
        if (cJSON_IsFalse(cJSON_GetObjectItemCaseSensitive(row, "in_scope"))) {
            printf(" (kept)");
        } else if (cJSON_IsString(cJSON_GetObjectItemCaseSensitive(row, "heard"))) {
            printf(" (heard %s)", string_of(row, "heard"));
        }
        printf(", %s fill (%s)\n", string_of(row, "fill"), string_of(row, "source"));
    }
    const cJSON *score = cJSON_GetObjectItemCaseSensitive(version, "score");
    printf("  score %.0f/%.0f\n", number_of(score, "numerator"), number_of(score, "denominator"));
    const cJSON *seam = cJSON_GetObjectItemCaseSensitive(version, "seam");
    if (cJSON_IsObject(seam)) {
        printf("  seam: %.0f of 4 rules held (progression %s, no parallels %s, step %s, leading tone %s)\n", number_of(seam, "held"),
               yes_no(seam, "progression"), yes_no(seam, "no_parallels"), yes_no(seam, "step"), yes_no(seam, "leading_tone"));
    }
    const cJSON *check = cJSON_GetObjectItemCaseSensitive(version, "analyzer_check");
    if (cJSON_IsObject(check)) {
        printf("  analyzer check: %.0f of %.0f agreed\n", number_of(check, "agreed"), number_of(check, "planned"));
    }
    const cJSON *distance = cJSON_GetObjectItemCaseSensitive(version, "style_distance");
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

void cli_print_piece_text(const cJSON *document, const char *const *files, size_t file_count) {
    printf("piece in %s, meter %s, seed %.0f\n", string_of(document, "key"), string_of(document, "meter"), number_of(document, "seed"));
    cli_print_brief_json(cJSON_GetObjectItemCaseSensitive(document, "brief"));
    const cJSON *regenerated = cJSON_GetObjectItemCaseSensitive(document, "regenerated");
    if (cJSON_IsObject(regenerated)) {
        const char *lane = string_of(regenerated, "lane");
        printf("regenerated %s of section %s\n", strcmp(lane, "all") == 0 ? "all lanes" : lane, string_of(regenerated, "section"));
    } else if (regenerated) {
        printf("regenerated nothing; the ending version is built again\n");
    }
    const cJSON *loop = cJSON_GetObjectItemCaseSensitive(document, "loop");
    if (cJSON_IsObject(loop)) {
        printf("loop from ");
        print_time(string_of(loop, "start"));
        printf(" to ");
        print_time(string_of(loop, "end"));
        printf(" (whole notes)\n");
    }
    const cJSON *versions = cJSON_GetObjectItemCaseSensitive(document, "versions");
    print_version_text("loop", cJSON_GetObjectItemCaseSensitive(versions, "loop"));
    print_version_text("ending", cJSON_GetObjectItemCaseSensitive(versions, "ending"));
    for (size_t i = 0; i < file_count; i++) {
        printf("wrote %s\n", files[i]);
    }
}

void cli_print_piece_variations_text(const cJSON *document, const char *const *plans, const char *const *midis,
                                     const char *const *musicxmls) {
    printf("%s of section %s\n", strcmp(string_of(document, "lane"), "harmony") == 0 ? "harmonizations" : "variations",
           string_of(document, "section"));
    const cJSON *item = NULL;
    size_t at = 0;
    cJSON_ArrayForEach(item, cJSON_GetObjectItemCaseSensitive(document, "variations")) {
        const cJSON *loop = cJSON_GetObjectItemCaseSensitive(item, "loop");
        const cJSON *score = cJSON_GetObjectItemCaseSensitive(loop, "score");
        printf("variation %zu  seed %.0f  score %.0f/%.0f", ++at, number_of(item, "seed"), number_of(score, "numerator"),
               number_of(score, "denominator"));
        const cJSON *seam = cJSON_GetObjectItemCaseSensitive(loop, "seam");
        if (cJSON_IsObject(seam)) {
            printf("  seam %.0f of 4 rules held", number_of(seam, "held"));
        }
        printf("\n");
    }
    printf("kept %.0f of %.0f, %.0f seeds tried\n", number_of(document, "kept"), number_of(document, "requested"),
           number_of(document, "seeds_tried"));
    for (size_t i = 0; i < at; i++) {
        printf("wrote %s, %s, %s\n", plans[i], midis[i], musicxmls[i]);
    }
}
