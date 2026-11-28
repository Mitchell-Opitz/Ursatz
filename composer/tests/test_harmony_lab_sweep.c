#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <cJSON.h>
#include <theory/knowledge/knowledge_snapshot.h>
#include <ursatz_composer/composer_api.h>

#include "support/test_composer.h"
#include "support/test_knowledge.h"
#include "support/test_support.h"
#include "ursatz_test.h"

#ifndef COMPOSER_FIXTURE_DIR
#error "COMPOSER_FIXTURE_DIR is not defined"
#endif

/*
 * Gate 2 of P70 (R-770): the harmony lab's path measured over seeds 1 to 10, each case pinned at its measured count (a count may only rise).
 *
 *   a  her plan (fixtures/harmony/sad-dorian-lift.json: C minor with Dorian, 16 bars, bars 1 to 12 shared, bar 13 color, brightness steps up at
 *      13): the chord of every bar from 1 to 12 is one of i, III, v and VII, bar 13's chord holds A natural (ii or IV), and the seeds on which
 *      bar 13's measured brightness is above every bar before it are counted
 *   b  each of the fifteen OMT VII schemas as one span (a schema that loops: 8 bars with `loop` true; one that does not: its own length in bars,
 *      the rest `any`) in the key of its example: the stages come in order on every seed
 *   c  C major, 8 bars, tension `rise`: the bar-level tension falls at most once
 *   d  the six sentences of R-761's measure (four given chords on beats and half beats in 4/4, two in 3/4, one in 6/8): the given chords are in the
 *      plan at their onsets
 *   e  an 8-bar doo-wop plan from the lab (a PAC, so IV V I VI IV V V I), its chords less bar 2 given to a sentence (`chords` of a theme request): every
 *      given chord is in the plan at its onset
 *
 * With `fast`, seed 1 of every case (the smoke test); with a case's letter ("a") that case alone; SWEEP_SEED limits a run to one seed.
 */

#define SEEDS 10
#define SCHEMAS 15

/* The pinned counts, as measured on 2026-11-28 (seeds 1 to 10): the seeds composed of each case, and in a the seeds on which bar 13 is brighter than every bar before
 * it (a strict comparison: her plan's bar 13 is often ii, as bright as the v before it). */
#define PIN_A_COMPOSED 10
#define PIN_A_BRIGHTER 1
#define PIN_B_COMPOSED 150
#define PIN_C_COMPOSED 10
#define PIN_D_COMPOSED 60
#define PIN_E_COMPOSED 10

static bool fast_run = false;
static unsigned first_seed = 1;
static unsigned last_seed = SEEDS;

static const char *member_text(const cJSON *object, const char *name) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);
    return cJSON_IsString(item) ? item->valuestring : "";
}

/* A rational the Composer writes as text ("4" or "9/2"). */
static double fraction(const char *text) {
    char *end = NULL;
    double numerator = strtod(text, &end);
    return end != NULL && *end == '/' ? numerator / strtod(end + 1, NULL) : numerator;
}

/* The response of the progression request on a seed; NULL when the Composer finds no plan (UNSATISFIABLE), which the sweep counts as not composed. */
static cJSON *lab(const char *request, unsigned seed) {
    ComposerResponse response;
    UrsatzStatus status = composer_api_progression(test_context(), request, NULL, &response);
    if (status != URSATZ_OK) {
        printf("seed %u: %s: %s\n", seed, test_status_name(status), response.error.message);
        assert(status == URSATZ_ERR_UNSATISFIABLE);
        composer_response_destroy(&response);
        return NULL;
    }
    cJSON *document = cJSON_Parse(response.json);
    assert(document != NULL);
    composer_response_destroy(&response);
    return document;
}

static const char *chord_of_bar(const cJSON *document, int bar) {
    const cJSON *row;
    cJSON_ArrayForEach(row, cJSON_GetObjectItemCaseSensitive(document, "chords")) {
        if ((int)cJSON_GetObjectItemCaseSensitive(row, "bar")->valuedouble == bar) {
            return member_text(row, "chord");
        }
    }
    return "";
}

static double measure_of_bar(const cJSON *document, int bar, const char *name) {
    const cJSON *row;
    cJSON_ArrayForEach(row, cJSON_GetObjectItemCaseSensitive(document, "measures")) {
        if ((int)cJSON_GetObjectItemCaseSensitive(row, "bar")->valuedouble == bar) {
            return fraction(member_text(row, name));
        }
    }
    assert(false);
    return 0;
}

/* ------------------------------------------------------------------- a */

static unsigned case_a(unsigned *brighter) {
    char *fixture = test_must_read_file(COMPOSER_FIXTURE_DIR "/harmony/sad-dorian-lift.json", NULL);
    unsigned composed = 0;
    *brighter = 0;
    for (unsigned seed = first_seed; seed <= last_seed; seed++) {
        cJSON *request = cJSON_Parse(fixture);
        assert(request != NULL);
        cJSON_ReplaceItemInObject(request, "seed", cJSON_CreateNumber(seed));
        char *printed = cJSON_PrintUnformatted(request);
        cJSON *document = lab(printed, seed);
        free(printed);
        cJSON_Delete(request);
        if (document == NULL) {
            continue;
        }
        composed++;
        for (int bar = 1; bar <= 12; bar++) {
            const char *chord = chord_of_bar(document, bar);
            if (strcmp(chord, "i") != 0 && strcmp(chord, "III") != 0 && strcmp(chord, "v") != 0 && strcmp(chord, "VII") != 0) {
                fprintf(stderr, "a seed %u bar %d: %s\n", seed, bar, chord);
                assert(false);
            }
        }
        const char *lift = chord_of_bar(document, 13);
        assert(strcmp(lift, "ii") == 0 || strcmp(lift, "IV") == 0); /* D F A or F A C: A natural */
        bool above = true;
        for (int bar = 1; bar <= 12; bar++) {
            above = above && measure_of_bar(document, 13, "brightness") > measure_of_bar(document, bar, "brightness");
        }
        *brighter += above;
        cJSON_Delete(document);
    }
    free(fixture);
    return composed;
}

/* ------------------------------------------------------------------- b */

/* Whether the written chords continue the example: a rotation of it, repeated. */
static bool continues(const cJSON *rows, const cJSON *example, int bars, bool loop) {
    int count = cJSON_GetArraySize(example);
    for (int shift = 0; shift < (loop ? count : 1); shift++) {
        bool equal = true;
        for (int i = 0; equal && i < bars; i++) {
            equal = strcmp(member_text(cJSON_GetArrayItem(rows, i), "text"), cJSON_GetArrayItem(example, (i + shift) % count)->valuestring) == 0;
        }
        if (equal) {
            return true;
        }
    }
    return false;
}

static unsigned case_b(void) {
    const KnowledgeSnapshot *knowledge = test_knowledge();
    unsigned composed = 0;
    unsigned checked = 0;
    for (size_t i = 0; i < knowledge_snapshot_schema_count(knowledge); i++) {
        const SchemaEntry *schema = knowledge_snapshot_schema_at(knowledge, i);
        if (schema->subtype != SCHEMA_PROGRESSION) {
            continue;
        }
        checked++;
        const cJSON *entry = knowledge_snapshot_entry_json(knowledge, schema->id);
        const cJSON *example = cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(entry, "examples"), 0);
        const cJSON *chords = cJSON_GetObjectItemCaseSensitive(example, "chords");
        bool loop = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(entry, "content"), "loop"));
        int count = cJSON_GetArraySize(chords);
        int bars = loop ? 8 : (count > 8 ? count : 8);
        int span = loop ? 8 : count;
        for (unsigned seed = first_seed; seed <= last_seed; seed++) {
            char request[768];
            int used = snprintf(request, sizeof request,
                                "{\"key\": \"%s\", \"meter\": \"4/4\", \"bars\": %d, \"cadence\": \"none\", \"seed\": %u, \"palette\": {\"spans\": [{\"bars\": [1, %d], "
                                "\"chords\": \"any\", \"schema\": \"%s\", \"loop\": %s}",
                                member_text(example, "key"), bars, seed, span, schema->id, loop ? "true" : "false");
            if (span < bars) {
                used += snprintf(request + used, sizeof request - (size_t)used, ", {\"bars\": [%d, %d], \"chords\": \"any\"}", span + 1, bars);
            }
            snprintf(request + used, sizeof request - (size_t)used, "]}}");
            cJSON *document = lab(request, seed);
            if (document == NULL) {
                continue;
            }
            composed++;
            const cJSON *rows = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(document, "harmony"), "chords");
            assert(cJSON_GetArraySize(rows) == bars);
            if (!continues(rows, chords, span, loop)) {
                fprintf(stderr, "b %s seed %u: the stages are not the example's\n", schema->id, seed);
                assert(false);
            }
            cJSON_Delete(document);
        }
    }
    assert(checked == SCHEMAS);
    return composed;
}

/* ------------------------------------------------------------------- c */

static unsigned case_c(void) {
    unsigned composed = 0;
    for (unsigned seed = first_seed; seed <= last_seed; seed++) {
        char request[256];
        snprintf(request, sizeof request,
                 "{\"key\": \"C major\", \"meter\": \"4/4\", \"bars\": 8, \"cadence\": \"none\", \"seed\": %u, \"palette\": {\"tension\": {\"shape\": \"rise\"}}}", seed);
        cJSON *document = lab(request, seed);
        if (document == NULL) {
            continue;
        }
        composed++;
        int falls = 0;
        for (int bar = 2; bar <= 8; bar++) {
            falls += measure_of_bar(document, bar, "tension") < measure_of_bar(document, bar - 1, "tension");
        }
        if (falls > 1) {
            fprintf(stderr, "c seed %u: the tension falls %d times\n", seed, falls);
            assert(false);
        }
        cJSON_Delete(document);
    }
    return composed;
}

/* ------------------------------------------------------------------- d, e */

typedef struct {
    const char *meter;
    const char *degrees;
    int bar;
    const char *text;
    size_t count;
    const char *numerals[4];
    long starts[4]; /* in eighths of a whole note from the start of the theme */
} Case;

static const char *const MOTIF_44 = "3:q 2:e 1:e 2:q 3:q | 5:h 4:q 3:q";
static const char *const MOTIF_34 = "1:q 2:e 3:e 4:q | 5:h.";
static const char *const MOTIF_68 = "5:q 4:e 3:q 2:e | 1:q. 3:q.";

static const Case MEASURED[] = {
    {"4/4", MOTIF_44, 4, "IV I@1 II@2 V@3", 4, {"IV", "I", "ii", "V"}, {32, 34, 36, 38}},
    {"4/4", MOTIF_44, 5, "VI II@1 V@2 I@3", 4, {"vi", "ii", "V", "I"}, {40, 42, 44, 46}},
    {"4/4", MOTIF_44, 5, "VI II@3/2 V@2 I@3", 4, {"vi", "ii", "V", "I"}, {40, 43, 44, 46}},
    {"3/4", MOTIF_34, 4, "IV V@1 I@2", 3, {"IV", "V", "I"}, {24, 26, 28}},
    {"3/4", MOTIF_34, 4, "IV V@3/2 I@2", 3, {"IV", "V", "I"}, {24, 27, 28}},
    {"6/8", MOTIF_68, 4, "IV V@2 I@4", 3, {"IV", "V", "I"}, {24, 26, 28}},
};
#define MEASURED_COUNT (sizeof(MEASURED) / sizeof(MEASURED[0]))

/* The theme request of a sentence in C major with the chords (an array text of {bar, text}), 8 bars, the motif in the basic slot, block accompaniment. */
static char *theme_request(const char *meter, const char *degrees, const char *chords, const char *cadence, unsigned seed) {
    char *request = malloc(4096);
    assert(request != NULL);
    snprintf(request, 4096,
             "{\"schema\":\"ursatz-theme-request\",\"schema_version\":2,\"into\":\"sentence\",\"key\":\"C\",\"material\":\"major\",\"meter\":\"%s\","
             "\"bars\":8,\"cadence\":\"%s\",\"development\":\"balanced\",\"seed\":\"%u\",\"plans\":true,"
             "\"motives\":[{\"id\":\"A\",\"form\":\"full\",\"degrees\":\"%s\",\"pickup\":\"0\",\"harmony\":null,\"features\":[],\"role\":\"melodic\"}],"
             "\"slots\":{\"basic\":\"A\"},\"chords\":%s,\"accompaniment\":{\"pattern\":\"block\"}}",
             meter, cadence, seed, degrees, chords);
    return request;
}

/* The Given harmony spans of the plan in the response, in order: numerals and starts in eighths of a whole note; the count. */
static size_t given_harmony(const char *plan_json, char numerals[][32], long *eighths, size_t capacity) {
    cJSON *plan = cJSON_Parse(plan_json);
    assert(plan != NULL);
    size_t given = 0;
    const cJSON *span;
    cJSON_ArrayForEach(span, cJSON_GetObjectItemCaseSensitive(plan, "chords")) {
        const cJSON *composer = cJSON_GetObjectItemCaseSensitive(span, "composer");
        if (strcmp(member_text(composer, "state"), "given") != 0) {
            continue;
        }
        assert(given < capacity);
        snprintf(numerals[given], 32, "%s", member_text(cJSON_GetObjectItemCaseSensitive(composer, "payload"), "roman_numeral"));
        const cJSON *start = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(span, "span"), "start");
        long num = (long)cJSON_GetObjectItemCaseSensitive(start, "num")->valuedouble;
        long den = (long)cJSON_GetObjectItemCaseSensitive(start, "den")->valuedouble;
        assert((num * 8) % den == 0);
        eighths[given++] = num * 8 / den;
    }
    cJSON_Delete(plan);
    return given;
}

/* Composes the theme request; true when it composed, with the given harmony of its plan in the arrays (themes' first seed is the seed asked for). */
static bool compose_theme(const char *request, unsigned seed, char numerals[][32], long *eighths, size_t *count) {
    ComposerResponse response;
    UrsatzStatus status = composer_api_theme(test_context(), request, NULL, &response);
    if (status != URSATZ_OK) {
        printf("seed %u: %s: %s\n", seed, test_status_name(status), response.error.message);
        assert(status == URSATZ_ERR_UNSATISFIABLE);
        composer_response_destroy(&response);
        return false;
    }
    cJSON *document = cJSON_Parse(response.json);
    assert(document != NULL && response.plan_json != NULL);
    const cJSON *theme = cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(document, "themes"), 0);
    bool own_seed = (unsigned)cJSON_GetObjectItemCaseSensitive(theme, "seed")->valuedouble == seed;
    if (own_seed) {
        *count = given_harmony(response.plan_json, numerals, eighths, 16);
    }
    cJSON_Delete(document);
    composer_response_destroy(&response);
    return own_seed;
}

static unsigned case_d(void) {
    unsigned composed = 0;
    for (size_t c = 0; c < MEASURED_COUNT; c++) {
        const Case *test = &MEASURED[c];
        for (unsigned seed = first_seed; seed <= last_seed; seed++) {
            char chords[128];
            snprintf(chords, sizeof chords, "[{\"bar\":%d,\"text\":\"%s\"}]", test->bar, test->text);
            char *request = theme_request(test->meter, test->degrees, chords, "PAC", seed);
            char numerals[16][32];
            long eighths[16];
            size_t count = 0;
            bool done = compose_theme(request, seed, numerals, eighths, &count);
            free(request);
            if (!done) {
                continue;
            }
            composed++;
            if (count != test->count) {
                fprintf(stderr, "d %s '%s' seed %u: %zu given chords, expected %zu\n", test->meter, test->text, seed, count, test->count);
                assert(false);
            }
            for (size_t i = 0; i < count; i++) {
                assert(strcmp(numerals[i], test->numerals[i]) == 0 && eighths[i] == test->starts[i]);
            }
        }
    }
    return composed;
}

/* The bar of the lab's plan the sentence is left to write (bars from 0). The plan is IV V I VI IV V V I (the loop's rotation the lab drew, V V I from the PAC): a sentence refuses a V in bar 1 or a VI in bar 5 against its cadence, and writes the plan with its bar 2 left blank. */
#define LEFT_OUT_BAR 2

static unsigned case_e(void) {
    /* the lab's doo-wop plan (doo-wop in bars 1 to 6, the PAC writes bars 7 and 8; the same on every sentence seed) */
    const char *lab_request =
        "{\"key\": \"C major\", \"meter\": \"4/4\", \"bars\": 8, \"cadence\": \"PAC\", \"seed\": 1, \"palette\": {\"spans\": [{\"bars\": [1, 6], "
        "\"chords\": \"any\", \"schema\": \"omt.schema.doo-wop\", \"loop\": true}, {\"bars\": [7, 8], \"chords\": \"any\"}]}}";
    cJSON *plan = lab(lab_request, 1);
    assert(plan != NULL);
    const cJSON *rows = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(plan, "harmony"), "chords");
    assert(cJSON_GetArraySize(rows) == 8);
    char chords[512] = "[";
    char wanted[8][32];
    long wanted_bar[8];
    size_t given = 0;
    for (int bar = 0; bar < 8; bar++) {
        if (bar == LEFT_OUT_BAR) {
            continue;
        }
        const char *text = member_text(cJSON_GetArrayItem(rows, bar), "text");
        snprintf(wanted[given], sizeof wanted[given], "%s", text);
        wanted_bar[given] = bar;
        size_t used = strlen(chords);
        snprintf(chords + used, sizeof chords - used, "%s{\"bar\":%d,\"text\":\"%s\"}", given > 0 ? "," : "", bar, text);
        given++;
    }
    strcat(chords, "]");
    cJSON_Delete(plan);
    unsigned composed = 0;
    for (unsigned seed = first_seed; seed <= last_seed; seed++) {
        char *request = theme_request("4/4", MOTIF_44, chords, "PAC", seed);
        char numerals[16][32];
        long eighths[16];
        size_t count = 0;
        bool done = compose_theme(request, seed, numerals, eighths, &count);
        free(request);
        if (!done) {
            continue;
        }
        composed++;
        assert(count == given);
        for (size_t i = 0; i < count; i++) {
            if (strcasecmp(numerals[i], wanted[i]) != 0 || eighths[i] != wanted_bar[i] * 8) {
                fprintf(stderr, "e seed %u chord %zu: %s at %ld, expected %s at %ld\n", seed, i, numerals[i], eighths[i], wanted[i], wanted_bar[i] * 8);
                assert(false);
            }
        }
    }
    return composed;
}

/* ------------------------------------------------------------------- main */

static void report(const char *name, unsigned composed, unsigned pinned, unsigned runs) {
    printf("%s: %u of %u composed (pinned %u)\n", name, composed, runs, pinned);
    if (fast_run) {
        assert(composed == runs);
    } else if (first_seed == 1 && last_seed == SEEDS) {
        assert(composed >= pinned);
    }
}

int main(int argc, char **argv) {
    fast_run = argc > 1 && strcmp(argv[1], "fast") == 0;
    const char *only = argc > 1 && !fast_run ? argv[1] : NULL;
    const char *seed_only = getenv("SWEEP_SEED");
    first_seed = seed_only != NULL ? (unsigned)strtoul(seed_only, NULL, 10) : 1;
    last_seed = seed_only != NULL ? first_seed : (fast_run ? 1 : SEEDS);
    setvbuf(stdout, NULL, _IOLBF, 0);
    unsigned runs = last_seed - first_seed + 1;
    if (only == NULL || strcmp(only, "a") == 0) {
        unsigned brighter = 0;
        unsigned composed = case_a(&brighter);
        report("a her plan", composed, PIN_A_COMPOSED, runs);
        printf("a bar 13 brighter than every bar before it: %u (pinned %u)\n", brighter, PIN_A_BRIGHTER);
        if (!fast_run && first_seed == 1 && last_seed == SEEDS) {
            assert(brighter >= PIN_A_BRIGHTER);
        }
    }
    if (only == NULL || strcmp(only, "b") == 0) {
        report("b the fifteen schemas", case_b(), PIN_B_COMPOSED, runs * SCHEMAS);
    }
    if (only == NULL || strcmp(only, "c") == 0) {
        report("c tension rise", case_c(), PIN_C_COMPOSED, runs);
    }
    if (only == NULL || strcmp(only, "d") == 0) {
        report("d the measured sentences", case_d(), PIN_D_COMPOSED, runs * (unsigned)MEASURED_COUNT);
    }
    if (only == NULL || strcmp(only, "e") == 0) {
        report("e a doo-wop plan on a sentence", case_e(), PIN_E_COMPOSED, runs);
    }
    printf("test_harmony_lab_sweep%s: ok\n", fast_run ? " fast" : "");
    return 0;
}
