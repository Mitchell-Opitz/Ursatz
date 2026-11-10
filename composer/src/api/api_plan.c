#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <standard-texture/texture_rules.h>

#include <ursatz_composer/constraint.h>
#include <ursatz_composer/corpus_seed.h>
#include <ursatz_composer/form_template.h>
#include <ursatz_composer/given.h>
#include <ursatz_composer/harmony_phrase.h>
#include <ursatz_composer/motive.h>
#include <ursatz_composer/piece.h>
#include <ursatz_composer/plan_json.h>

#include "api_grow.h"
#include "api_vocabulary.h"
#include "api_workflow.h"
#include "motive/motive_internal.h"
#include "util/request_parse.h"

/*
 * `plan` as a composer API entry (docs/cli-plan.md): the plan of a whole short piece from a form template, with the
 * motive and the given material placed in it. Nothing is composed.
 */

#define SIZE_SEARCH_MAX 96
/* The most given phrases, progressions and bass lines one plan takes, and the numerals in one progression. */
#define MAX_GIVEN 8
#define MAX_NUMERALS 64
/* The form a plan with given material and no form is made as, and the section the material goes in. */
#define DEFAULT_GIVEN_BARS 24
#define DEFAULT_GIVEN_SECTION "B"
#define MAX_NOTICES 4

typedef enum {
    GIVEN_MELODY,
    GIVEN_PROGRESSION,
    GIVEN_BASS
} GivenKind;

static const char *const GIVEN_KIND_NAMES[] = {"melody", "progression", "bass"};

/* One given phrase, progression or bass line. */
typedef struct {
    GivenKind kind;
    const char *source;     /* the name to show: a file, or the numerals of a progression */
    const char *numerals;   /* the numerals of a progression (or those to look for with from_corpus) */
    bool from_corpus;       /* like_progression: numerals are looked for in the corpus and replaced by the passage's */
    char found_numerals[1024];
    CorpusSeed found;
    const cJSON *motive;    /* the motive request of a melody or bass line */
    const char *at_text;
    bool has_cells;
    unsigned long cells;
    char letter[PLAN_SECTION_LETTER_MAX];
    unsigned long bar; /* from 1 */
} Given;

typedef struct {
    const cJSON *root;
    const char *form_text;
    const char *key_text;
    const char *mode_text;
    const char *meter_text;
    bool modulate;
    const cJSON *motive;
    const char *motive_source;
    const char *from_corpus;
    const char *like_path;
    const char *accompaniment_text;
    const char *texture_text;
    const char *style_path;
    const char *style_weight_text;
    const char *blend_path;
    const char *blend_weight_text;
    const char *blend_weights_text;
    const char *without[PLAN_STYLE_WITHOUT_MAX];
    size_t without_count;
    Given given[MAX_GIVEN];
    size_t given_count;
    /* Read from the members above. */
    bool form_defaulted;
    size_t given_melodies;
    FormTemplateKind form;
    AnalysisKey key;
    char key_name[24];
    char key_request[64];
    PlanMeter meter;
    unsigned long bars;
    unsigned long count;
    bool has_accompaniment;
    PlanAccompaniment accompaniment;
    unsigned long seed;
    ComposerRatio style_weight; /* 0/0 when no style_weight was given */
    ComposerStyleWeights blend_weights;
    bool seeded; /* the motive is a passage of the corpus, kept in seed_found */
    CorpusSeed seed_found;
    PlanSource source;
    char *notices[MAX_NOTICES];
    size_t notice_count;
} PlanBuild;

static ComposerStatus fail_alloc(ComposerError *error) {
    return api_fail(error, COMPOSER_ERR_ALLOC, "", "out of memory");
}

/* Adds item to object under name and returns true; the item is deleted when it is NULL or cannot be added. */
static bool add(cJSON *object, const char *name, cJSON *item) {
    if (!object || !item || !cJSON_AddItemToObject(object, name, item)) {
        cJSON_Delete(item);
        return false;
    }
    return true;
}

/* --- reading the request --------------------------------------------------------------------------- */

/* A whole number from 0 to maximum, a JSON number or digits; "<name> must be a whole number, not '<value>'". */
static bool read_number(const cJSON *object, const char *name, unsigned long maximum, unsigned long *out, ComposerError *error) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);
    if (!item || cJSON_IsNull(item)) {
        return true;
    }
    char text[48] = "";
    if (cJSON_IsNumber(item)) {
        snprintf(text, sizeof text, "%g", item->valuedouble);
    } else if (cJSON_IsString(item)) {
        snprintf(text, sizeof text, "%s", item->valuestring);
    }
    if (!request_parse_whole_number(text, maximum, out)) {
        char path[64];
        snprintf(path, sizeof path, "/%s", name);
        api_fail(error, COMPOSER_ERR_REQUEST, path, "%s must be a whole number, not '%s'", name, text);
        return false;
    }
    return true;
}

/* The member as text (a number or a string); false when it is absent or null. */
static bool member_text(const cJSON *object, const char *name, char *out, size_t size) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);
    out[0] = '\0';
    if (!item || cJSON_IsNull(item)) {
        return false;
    }
    if (cJSON_IsNumber(item)) {
        snprintf(out, size, "%g", item->valuedouble);
    } else if (cJSON_IsString(item)) {
        snprintf(out, size, "%s", item->valuestring);
    }
    return true;
}

static ComposerStatus read_given(PlanBuild *state, ComposerError *error) {
    const cJSON *list = cJSON_GetObjectItemCaseSensitive(state->root, "given");
    if (!list || cJSON_IsNull(list)) {
        return COMPOSER_OK;
    }
    if (!cJSON_IsArray(list)) {
        return api_fail(error, COMPOSER_ERR_REQUEST, "/given", "given must be a list of phrases, progressions and bass lines");
    }
    const cJSON *item = NULL;
    cJSON_ArrayForEach(item, list) {
        char base[48];
        snprintf(base, sizeof base, "/given/%zu", state->given_count);
        if (state->given_count == MAX_GIVEN) {
            return api_fail(error, COMPOSER_ERR_REQUEST, "/given", "plan takes %d given phrases, progressions and bass lines at most",
                            MAX_GIVEN);
        }
        if (!cJSON_IsObject(item)) {
            return api_fail(error, COMPOSER_ERR_REQUEST, base, "a given material must be an object");
        }
        Given *given = &state->given[state->given_count];
        const char *kind = NULL;
        bool present = false;
        if (!api_member_string(item, "kind", &kind, &present, error) || !kind) {
            return kind ? COMPOSER_ERR_REQUEST
                        : api_fail(error, COMPOSER_ERR_REQUEST, base, "a given material needs a kind: melody, progression, "
                                                                      "like_progression or bass");
        }
        given->from_corpus = strcmp(kind, "like_progression") == 0;
        if (strcmp(kind, "melody") == 0) {
            given->kind = GIVEN_MELODY;
        } else if (strcmp(kind, "bass") == 0) {
            given->kind = GIVEN_BASS;
        } else if (strcmp(kind, "progression") == 0 || given->from_corpus) {
            given->kind = GIVEN_PROGRESSION;
        } else {
            return api_fail(error, COMPOSER_ERR_REQUEST, base,
                            "kind must be melody, progression, like_progression or bass, not '%s'", kind);
        }
        if (!api_member_string(item, "source", &given->source, &present, error) ||
            !api_member_string(item, "at", &given->at_text, &present, error)) {
            return COMPOSER_ERR_REQUEST;
        }
        if (given->kind == GIVEN_PROGRESSION) {
            if (!api_member_string(item, "numerals", &given->numerals, &present, error)) {
                return COMPOSER_ERR_REQUEST;
            }
            if (!given->numerals) {
                return api_fail(error, COMPOSER_ERR_REQUEST, base, "a %s needs its numerals", kind);
            }
            if (!given->source) {
                given->source = given->numerals;
            }
        } else {
            given->motive = cJSON_GetObjectItemCaseSensitive(item, "motive");
            if (!cJSON_IsObject(given->motive)) {
                return api_fail(error, COMPOSER_ERR_REQUEST, base, "a given %s needs a \"motive\" object", kind);
            }
            if (!given->source) {
                given->source = "";
                if (!api_member_string(given->motive, "source", &given->source, &present, error)) {
                    return COMPOSER_ERR_REQUEST;
                }
                given->source = given->source ? given->source : "";
            }
        }
        const cJSON *cells = cJSON_GetObjectItemCaseSensitive(item, "cells");
        given->has_cells = cells && !cJSON_IsNull(cells);
        if (given->has_cells && given->kind != GIVEN_PROGRESSION) {
            return api_fail(error, COMPOSER_ERR_REQUEST, base, "cells goes with a progression, not '%s'", given->source);
        }
        state->given_count++;
    }
    return COMPOSER_OK;
}

static ComposerStatus read_request(PlanBuild *state, ComposerError *error) {
    const cJSON *root = state->root;
    bool present = false;
    if (!api_member_string(root, "form", &state->form_text, &present, error) ||
        !api_member_string(root, "key", &state->key_text, &present, error) ||
        !api_member_string(root, "mode", &state->mode_text, &present, error) ||
        !api_member_string(root, "meter", &state->meter_text, &present, error) ||
        !api_member_bool(root, "modulate", &state->modulate, &present, error) ||
        !api_member_string(root, "corpus", &state->from_corpus, &present, error) ||
        !api_member_string(root, "like", &state->like_path, &present, error) ||
        !api_member_string(root, "accompaniment", &state->accompaniment_text, &present, error) ||
        !api_member_string(root, "texture", &state->texture_text, &present, error) ||
        !api_member_string(root, "style", &state->style_path, &present, error) ||
        !api_member_string(root, "style_weight", &state->style_weight_text, &present, error) ||
        !api_member_string(root, "blend", &state->blend_path, &present, error) ||
        !api_member_string(root, "blend_weight", &state->blend_weight_text, &present, error) ||
        !api_member_string(root, "blend_weights", &state->blend_weights_text, &present, error)) {
        return COMPOSER_ERR_REQUEST;
    }
    state->motive = cJSON_GetObjectItemCaseSensitive(root, "motive");
    if (cJSON_IsNull(state->motive)) {
        state->motive = NULL;
    }
    if (state->motive) {
        if (!cJSON_IsObject(state->motive)) {
            return api_fail(error, COMPOSER_ERR_REQUEST, "/motive", "motive must be a motive request: {\"input\": {...}}");
        }
        if (!api_member_string(state->motive, "source", &state->motive_source, &present, error)) {
            return COMPOSER_ERR_REQUEST;
        }
    }
    const cJSON *without = cJSON_GetObjectItemCaseSensitive(root, "style_without");
    if (without && !cJSON_IsNull(without)) {
        if (!cJSON_IsArray(without)) {
            return api_fail(error, COMPOSER_ERR_REQUEST, "/style_without", "style_without must be a list of style categories");
        }
        const cJSON *name = NULL;
        cJSON_ArrayForEach(name, without) {
            if (!cJSON_IsString(name)) {
                return api_fail(error, COMPOSER_ERR_REQUEST, "/style_without", "style_without must be a list of style categories");
            }
            ComposerStatus known = api_style_category_check(name->valuestring, error);
            if (known != COMPOSER_OK) {
                return known;
            }
            if (state->without_count == PLAN_STYLE_WITHOUT_MAX) {
                return api_fail(error, COMPOSER_ERR_REQUEST, "/style_without", "style_without names at most %d style categories",
                                PLAN_STYLE_WITHOUT_MAX);
            }
            state->without[state->without_count++] = name->valuestring;
        }
    }
    return read_given(state, error);
}

static void append_number(char *out, size_t size, unsigned long value, bool first) {
    size_t used = strlen(out);
    snprintf(out + used, size - used, first ? "%lu" : ", %lu", value);
}

/* The sizes the form has, for the message about one it has not. */
static void describe_sizes(FormTemplateKind kind, char *out, size_t size) {
    FormTemplate layout;
    out[0] = '\0';
    bool first = true;
    for (unsigned long bars = 1; bars <= SIZE_SEARCH_MAX; bars++) {
        if (form_template_layout(kind, bars, 0, &layout)) {
            append_number(out, size, bars, first);
            first = false;
        }
    }
    size_t used = strlen(out);
    snprintf(out + used, size - used, " bars");
    bool counted = false;
    for (unsigned long count = 1; count <= FORM_TEMPLATE_MAX_SECTIONS; count++) {
        if (form_template_layout(kind, 0, count, &layout)) {
            used = strlen(out);
            snprintf(out + used, size - used, counted ? ", %lu" : " (or count %lu", count);
            counted = true;
        }
    }
    if (counted) {
        used = strlen(out);
        snprintf(out + used, size - used, ")");
    }
}

/* The cells of a bar of the meter, which the harmony operators place chords on (harmony_rhythm.c). */
static unsigned long cells_in_bar(const PlanMeter *meter) {
    return (meter->beat_unit == 4 && meter->beats_per_bar == 4) || meter->beat_unit == 8 ? 2 : 1;
}

/* "B", "B:3", "A'" and "A':5": the section's letter and the bar (from 1) of it. */
static ComposerStatus read_at(Given *given, size_t index, bool form_defaulted, ComposerError *error) {
    char path[64];
    snprintf(path, sizeof path, "/given/%zu/at", index);
    if (!given->at_text) {
        if (!form_defaulted) {
            return api_fail(error, COMPOSER_ERR_REQUEST, path, "'%s' needs an at: the section it goes in, \"B\" or \"B:3\"",
                            given->source);
        }
        snprintf(given->letter, sizeof(given->letter), "%s", DEFAULT_GIVEN_SECTION);
        given->bar = 1;
        return COMPOSER_OK;
    }
    const char *colon = strchr(given->at_text, ':');
    size_t length = colon ? (size_t)(colon - given->at_text) : strlen(given->at_text);
    if (length == 0 || length >= sizeof(given->letter)) {
        return api_fail(error, COMPOSER_ERR_REQUEST, path,
                        "at '%s' must be a section's letter, then optionally a colon and a bar from 1", given->at_text);
    }
    snprintf(given->letter, sizeof(given->letter), "%.*s", (int)length, given->at_text);
    given->bar = 1;
    if (colon && (!request_parse_whole_number(colon + 1, 100000, &given->bar) || given->bar < 1)) {
        return api_fail(error, COMPOSER_ERR_REQUEST, path, "at '%s': the bar must be a whole number from 1", given->at_text);
    }
    return COMPOSER_OK;
}

static ComposerStatus check_given(PlanBuild *state, ComposerError *error) {
    for (size_t i = 0; i < state->given_count; i++) {
        Given *given = &state->given[i];
        ComposerStatus status = read_at(given, i, state->form_defaulted, error);
        if (status != COMPOSER_OK) {
            return status;
        }
        given->cells = cells_in_bar(&state->meter);
        if (given->has_cells) {
            char path[64];
            char text[48];
            snprintf(path, sizeof path, "/given/%zu", i);
            const cJSON *item = cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(state->root, "given"), (int)i);
            unsigned long cells = 0;
            member_text(item, "cells", text, sizeof text);
            if (!request_parse_whole_number(text, 64, &cells) || cells < 1) {
                return api_fail(error, COMPOSER_ERR_REQUEST, path, "cells must be a whole number from 1, not '%s'", text);
            }
            given->cells = cells;
        }
        state->given_melodies += given->kind == GIVEN_MELODY;
    }
    return COMPOSER_OK;
}

static ComposerStatus check_request(PlanBuild *state, ComposerError *error) {
    if (!state->form_text && state->given_count > 0) {
        /* Given material with no form to put it in: the middle of a ternary form, B. */
        state->form_text = "ternary";
        state->form_defaulted = true;
        state->bars = DEFAULT_GIVEN_BARS;
    }
    if (!state->form_text || !state->key_text || !state->meter_text) {
        return api_fail(error, COMPOSER_ERR_REQUEST, "", "the request needs a form, a key and a meter");
    }
    if (!form_template_from_name(state->form_text, &state->form)) {
        char names[160] = "";
        for (int kind = 0; kind < FORM_TEMPLATE_KIND_COUNT; kind++) {
            size_t used = strlen(names);
            snprintf(names + used, sizeof(names) - used, kind ? ", %s" : "%s", form_template_name((FormTemplateKind)kind));
        }
        return api_fail(error, COMPOSER_ERR_REQUEST, "/form", "unknown form '%s'; the forms are %s", state->form_text, names);
    }
    /* The key is a tonic, with the mode from "mode" (major by default), or tonic and mode together. */
    bool has_mode_in_key = strchr(state->key_text, ' ') != NULL;
    if (has_mode_in_key && state->mode_text) {
        return api_fail(error, COMPOSER_ERR_REQUEST, "/mode", "key '%s' names a mode already; leave out mode", state->key_text);
    }
    if (state->mode_text && api_choice_find_ignore_case(API_MODES, state->mode_text) < 0) {
        char modes[64];
        api_choice_list(API_MODES, modes, sizeof modes);
        return api_fail(error, COMPOSER_ERR_REQUEST, "/mode", "mode must be %s, not '%s'", modes, state->mode_text);
    }
    if (snprintf(state->key_request, sizeof(state->key_request), has_mode_in_key ? "%s" : "%s %s", state->key_text,
                 state->mode_text ? state->mode_text : "major") >= (int)sizeof(state->key_request) ||
        !request_parse_key(state->key_request, &state->key)) {
        return api_fail(error, COMPOSER_ERR_REQUEST, "/key",
                        "unknown key '%s'; write a tonic letter A-G with an optional # or b, for example \"D\" with mode "
                        "\"minor\", or \"D minor\"",
                        state->key_text);
    }
    api_key_name(&state->key, state->key_name, sizeof(state->key_name));
    if (!request_parse_meter(state->meter_text, &state->meter) || !harmony_meter_supported(&state->meter)) {
        return api_fail_meter(error, "/meter", state->meter_text);
    }
    if (!read_number(state->root, "bars", SIZE_SEARCH_MAX * 8, &state->bars, error) ||
        !read_number(state->root, "count", 64, &state->count, error)) {
        return COMPOSER_ERR_REQUEST;
    }
    char bars_text[48];
    char count_text[48];
    bool has_bars = member_text(state->root, "bars", bars_text, sizeof bars_text);
    bool has_count = member_text(state->root, "count", count_text, sizeof count_text);
    FormTemplate layout;
    if (!form_template_layout(state->form, state->bars, state->count, &layout)) {
        char sizes[160];
        describe_sizes(state->form, sizes, sizeof(sizes));
        return api_fail(error, COMPOSER_ERR_REQUEST, has_bars ? "/bars" : "/count", "%s has no size of %s%s%s%s%s; it has %s",
                        form_template_name(state->form), has_bars ? bars_text : "", has_bars ? " bars" : "",
                        has_bars && has_count ? " with " : "", has_count ? "count " : "", has_count ? count_text : "", sizes);
    }
    ComposerStatus status = check_given(state, error);
    if (status != COMPOSER_OK) {
        return status;
    }
    bool likes_progression = false;
    for (size_t i = 0; i < state->given_count; i++) {
        likes_progression = likes_progression || state->given[i].from_corpus;
    }
    if ((state->like_path || likes_progression) && !state->from_corpus) {
        return api_fail(error, COMPOSER_ERR_REQUEST, "", "%s needs corpus <dir>", state->like_path ? "like" : "like_progression");
    }
    if (state->from_corpus && !state->like_path && !likes_progression) {
        return api_fail(error, COMPOSER_ERR_REQUEST, "/corpus", "corpus needs like <file> or a like_progression \"<numerals>\"");
    }
    if (state->like_path && state->motive) {
        return api_fail(error, COMPOSER_ERR_REQUEST, "/like", "like and motive both give the motive; use one");
    }
    if (state->accompaniment_text || state->texture_text) {
        state->has_accompaniment = true;
        status = api_read_accompaniment(state->accompaniment_text, state->texture_text, state->meter_text, &state->meter,
                                        &state->accompaniment, error);
        if (status != COMPOSER_OK) {
            return status;
        }
    }
    if (state->style_weight_text && !state->style_path) {
        return api_fail(error, COMPOSER_ERR_REQUEST, "/style_weight", "style_weight needs style");
    }
    if (state->without_count > 0 && !state->style_path) {
        return api_fail(error, COMPOSER_ERR_REQUEST, "/style_without", "style_without needs style");
    }
    if (state->style_weight_text && !request_parse_ratio(state->style_weight_text, &state->style_weight)) {
        return api_fail(error, COMPOSER_ERR_REQUEST, "/style_weight",
                        "style_weight must be a fraction like 1/2 or a whole number, not '%s'", state->style_weight_text);
    }
    if (state->style_path && strlen(state->style_path) >= PLAN_STYLE_PATH_MAX) {
        return api_fail(error, COMPOSER_ERR_REQUEST, "/style", "style path is longer than %d characters", PLAN_STYLE_PATH_MAX - 1);
    }
    if (state->blend_path && strlen(state->blend_path) >= PLAN_STYLE_PATH_MAX) {
        return api_fail(error, COMPOSER_ERR_REQUEST, "/blend", "blend path is longer than %d characters", PLAN_STYLE_PATH_MAX - 1);
    }
    status = api_read_blend(state->style_path != NULL, state->blend_path, state->blend_weight_text, state->blend_weights_text,
                            &state->blend_weights, error);
    if (status != COMPOSER_OK) {
        return status;
    }
    bool seeded = false;
    unsigned long seed = 0;
    if (!api_member_whole(state->root, "seed", 0, API_SEED_MAX, &seed, &seeded, error)) {
        return COMPOSER_ERR_REQUEST;
    }
    state->seed = seeded ? seed : api_default_seed();
    return COMPOSER_OK;
}

/* --- the corpus ------------------------------------------------------------------------------------ */

static void add_notice(PlanBuild *state, const CorpusSeed *seed, const char *dir) {
    if (seed->stale_documents == 0 || state->notice_count == MAX_NOTICES) {
        return;
    }
    char text[1200];
    snprintf(text, sizeof text, "%zu stale documents in '%s' (analyzed by another version; run the corpus again)",
             seed->stale_documents, dir);
    state->notices[state->notice_count] = strdup(text);
    state->notice_count += state->notices[state->notice_count] != NULL;
}

static ComposerStatus seed_failure(CorpusSeedResult result, const char *corpus_dir, const char *what, const char *path,
                                   ComposerError *error) {
    switch (result) {
    case CORPUS_SEED_ERR_NO_MATCH:
        return api_fail(error, COMPOSER_ERR_UNSATISFIABLE, path, "no passage of the corpus '%s' is similar to %s (a score of 1/2 or more)",
                        corpus_dir, what);
    case CORPUS_SEED_ERR_QUERY:
        return api_fail(error, COMPOSER_ERR_REQUEST, path, "cannot read %s as a passage to look for", what);
    case CORPUS_SEED_ERR_CORPUS:
        return api_fail(error, COMPOSER_ERR_UNSATISFIABLE, "/corpus",
                        "cannot read the corpus '%s' (the output directory of ursatz-analyzer --corpus)", corpus_dir);
    default:
        return api_fail(error, COMPOSER_ERR_UNSATISFIABLE, path, "%s", corpus_seed_result_name(result));
    }
}

static ComposerStatus seed_source(const CorpusSeed *seed, PlanSource *out, ComposerError *error) {
    memset(out, 0, sizeof(*out));
    if (snprintf(out->piece, PLAN_SOURCE_PIECE_MAX, "%s", seed->piece) >= PLAN_SOURCE_PIECE_MAX) {
        return api_fail(error, COMPOSER_ERR_UNSATISFIABLE, "", "the source piece's path is too long to record");
    }
    out->start_measure = seed->start_measure;
    out->start_beat = seed->start_beat;
    out->end_measure = seed->end_measure;
    out->end_beat = seed->end_beat;
    out->score_numerator = seed->score_numerator;
    out->score_denominator = seed->score_denominator;
    return COMPOSER_OK;
}

static ComposerStatus add_source(PiecePlan *plan, NodeId node, const CorpusSeed *seed, ComposerError *error) {
    PlanConstraint constraint;
    memset(&constraint, 0, sizeof(constraint));
    constraint.kind = PLAN_CONSTRAINT_SOURCE;
    ComposerStatus status = seed_source(seed, &constraint.params.source, error);
    if (status != COMPOSER_OK) {
        return status;
    }
    if (node == PLAN_NODE_NONE || plan_node_add_constraint(plan, node, &constraint) != PLAN_OK) {
        return api_fail(error, COMPOSER_ERR_UNSATISFIABLE, "", "cannot record the source of the seeded material in the plan");
    }
    return COMPOSER_OK;
}

/* --- the motives ----------------------------------------------------------------------------------- */

/* The motive request object read in the plan's meter and key: input, meter and key. */
static ComposerStatus read_motive(const PlanBuild *state, const cJSON *motive, const char *base, Motive *out,
                                  ComposerError *error) {
    cJSON *request = cJSON_CreateObject();
    const cJSON *input = cJSON_GetObjectItemCaseSensitive(motive, "input");
    if (!request || !cJSON_AddStringToObject(request, "meter", state->meter_text) ||
        !cJSON_AddStringToObject(request, "key", state->key_request) ||
        (input && !cJSON_AddItemReferenceToObject(request, "input", (cJSON *)input))) {
        cJSON_Delete(request);
        return fail_alloc(error);
    }
    ComposerStatus status = api_read_motive(request, base, out, NULL, error);
    cJSON_Delete(request);
    return status;
}

/* --- the given material ---------------------------------------------------------------------------- */

/* The node of the section with the letter, or PLAN_NODE_NONE; the letters are listed into names when it has none. */
static NodeId section_named(const PiecePlan *plan, const char *letter, char *names, size_t size) {
    ApiSection sections[API_MAX_SECTIONS];
    size_t count = api_plan_sections(plan, sections, API_MAX_SECTIONS);
    names[0] = '\0';
    for (size_t i = 0; i < count && i < API_MAX_SECTIONS; i++) {
        if (request_equals_ignore_case(sections[i].letter, letter)) {
            return sections[i].node;
        }
        size_t used = strlen(names);
        snprintf(names + used, size - used, i ? ", %s" : "%s", sections[i].letter);
    }
    return PLAN_NODE_NONE;
}

/* Places the given material in the plan. */
static ComposerStatus place_given(PlanBuild *state, PiecePlan *plan, ComposerError *error) {
    for (size_t i = 0; i < state->given_count; i++) {
        const Given *given = &state->given[i];
        char names[128];
        char path[64];
        snprintf(path, sizeof path, "/given/%zu", i);
        NodeId section = section_named(plan, given->letter, names, sizeof(names));
        if (section == PLAN_NODE_NONE) {
            return api_fail(error, COMPOSER_ERR_REQUEST, path, "at '%s': the piece has no section '%s'; its sections are %s",
                            given->at_text ? given->at_text : given->letter, given->letter, names);
        }
        GivenResult placed;
        if (given->kind == GIVEN_PROGRESSION) {
            char text[1024];
            char *numerals[MAX_NUMERALS];
            size_t count = 0;
            if (snprintf(text, sizeof(text), "%s", given->numerals) >= (int)sizeof(text)) {
                return api_fail(error, COMPOSER_ERR_REQUEST, path, "the progression is too long");
            }
            for (char *token = strtok(text, " \t,|"); token; token = strtok(NULL, " \t,|")) {
                if (count == MAX_NUMERALS) {
                    return api_fail(error, COMPOSER_ERR_REQUEST, path, "a progression has %d chords at most", MAX_NUMERALS);
                }
                numerals[count++] = token;
            }
            placed = plan_give_progression(plan, section, given->bar - 1, (const char *const *)numerals, count, given->cells);
        } else {
            Motive *line = malloc(sizeof(*line));
            if (!line) {
                return fail_alloc(error);
            }
            char base[96];
            snprintf(base, sizeof base, "%s/motive", path);
            ComposerStatus status = read_motive(state, given->motive, base, line, error);
            if (status != COMPOSER_OK) {
                free(line);
                return status;
            }
            PlanNote lane_notes[MOTIVE_MAX_NOTES];
            motive_lane_notes(line, lane_notes);
            placed = given->kind == GIVEN_MELODY ? plan_give_melody(plan, section, given->bar - 1, lane_notes, line->note_count)
                                                 : plan_give_bass(plan, section, given->bar - 1, lane_notes, line->note_count);
            free(line);
        }
        if (placed != GIVEN_OK) {
            return api_fail(error, COMPOSER_ERR_UNSATISFIABLE, path, "cannot place the given %s '%s' at %s:%lu: %s",
                            GIVEN_KIND_NAMES[given->kind], given->source, given->letter, given->bar, given_result_name(placed));
        }
        if (given->from_corpus) {
            ComposerStatus recorded = add_source(plan, section, &given->found, error);
            if (recorded != COMPOSER_OK) {
                return recorded;
            }
        }
    }
    return COMPOSER_OK;
}

/* --- the result ------------------------------------------------------------------------------------ */

static cJSON *given_document(const PlanBuild *state) {
    cJSON *list = cJSON_CreateArray();
    for (size_t i = 0; list && i < state->given_count; i++) {
        const Given *given = &state->given[i];
        cJSON *item = cJSON_CreateObject();
        bool ok = item && cJSON_AddStringToObject(item, "kind", GIVEN_KIND_NAMES[given->kind]) &&
                  cJSON_AddStringToObject(item, "source", given->source) && cJSON_AddStringToObject(item, "section", given->letter) &&
                  cJSON_AddNumberToObject(item, "bar", (double)given->bar);
        ok = ok && (given->kind != GIVEN_PROGRESSION || cJSON_AddNumberToObject(item, "cells", (double)given->cells));
        ok = ok && (!given->from_corpus || add(item, "from", api_seed_json(&given->found)));
        if (!ok || !cJSON_AddItemToArray(list, item)) {
            cJSON_Delete(item);
            cJSON_Delete(list);
            return NULL;
        }
    }
    return list;
}

static cJSON *plan_document(const PlanBuild *state, const PiecePlan *plan) {
    ApiSection sections[API_MAX_SECTIONS];
    size_t count = api_plan_sections(plan, sections, API_MAX_SECTIONS);
    if (count > API_MAX_SECTIONS) {
        count = API_MAX_SECTIONS;
    }
    size_t bars = 0;
    for (size_t i = 0; i < count; i++) {
        bars += sections[i].bars;
    }
    char meter[16];
    snprintf(meter, sizeof meter, "%d/%d", state->meter.beats_per_bar, state->meter.beat_unit);
    cJSON *document = cJSON_CreateObject();
    bool ok = document && cJSON_AddStringToObject(document, "form", form_template_name(state->form)) &&
              cJSON_AddNumberToObject(document, "bars", (double)bars) && cJSON_AddStringToObject(document, "key", state->key_name) &&
              cJSON_AddStringToObject(document, "meter", meter) && cJSON_AddNumberToObject(document, "seed", (double)state->seed) &&
              cJSON_AddBoolToObject(document, "modulate", state->modulate);
    ok = ok && add(document, "motive", state->motive_source ? cJSON_CreateString(state->motive_source) : cJSON_CreateNull());
    ok = ok && (!state->seeded || add(document, "motive_source", api_seed_json(&state->seed_found)));
    ok = ok && add(document, "accompaniment",
                   state->has_accompaniment ? cJSON_CreateString(api_accompaniment_name(state->accompaniment.pattern)) : cJSON_CreateNull());
    cJSON *list = ok ? cJSON_CreateArray() : NULL;
    for (size_t i = 0; list && i < count; i++) {
        ok = cJSON_AddItemToArray(list, api_section_json(&sections[i]));
    }
    ok = ok && add(document, "sections", list);
    ok = ok && (state->given_count == 0 || add(document, "given", given_document(state)));
    if (ok && state->style_path) {
        char weight[48];
        snprintf(weight, sizeof weight, "%lu/%lu", state->style_weight.denominator ? state->style_weight.numerator : 1UL,
                 state->style_weight.denominator ? state->style_weight.denominator : 1UL);
        cJSON *style = cJSON_CreateObject();
        ok = style && cJSON_AddStringToObject(style, "file", state->style_path) && cJSON_AddStringToObject(style, "weight", weight) &&
             add(document, "style", style);
        if (!ok) {
            cJSON_Delete(style);
        }
        if (ok && state->blend_path) {
            cJSON *blend = cJSON_CreateObject();
            cJSON *weights = blend ? cJSON_CreateObject() : NULL;
            ok = weights && cJSON_AddStringToObject(blend, "file", state->blend_path);
            for (int d = 0; ok && d < STYLE_DIMENSION_COUNT; d++) {
                snprintf(weight, sizeof weight, "%lu/%lu", state->blend_weights.numerator[d], state->blend_weights.denominator[d]);
                ok = cJSON_AddStringToObject(weights, style_dimension_name((StyleDimension)d), weight) != NULL;
            }
            ok = ok && add(blend, "weights", weights);
            if (ok) {
                weights = NULL;
            }
            cJSON_Delete(weights);
            ok = ok && add(document, "blend", blend);
            if (!ok) {
                cJSON_Delete(blend);
            }
        }
    }
    /* What the text output needs and the command's JSON does not carry. */
    if (ok && state->has_accompaniment) {
        ok = add(document, "accompaniment_texture",
                 state->accompaniment.texture >= 0 ? cJSON_CreateString(standard_texture_name((StandardTexture)state->accompaniment.texture))
                                                   : cJSON_CreateNull());
    }
    ok = ok && (!state->form_defaulted || cJSON_AddTrueToObject(document, "defaulted_form"));
    if (ok && state->notice_count > 0) {
        cJSON *notices = cJSON_CreateArray();
        for (size_t i = 0; notices && i < state->notice_count; i++) {
            ok = cJSON_AddItemToArray(notices, cJSON_CreateString(state->notices[i]));
        }
        ok = ok && add(document, "notices", notices);
    }
    if (!ok) {
        cJSON_Delete(document);
        return NULL;
    }
    return document;
}

/* --- the entry ------------------------------------------------------------------------------------- */

static ComposerStatus build_plan(PlanBuild *state, PiecePlan **out, ComposerError *error) {
    Motive *motive = calloc(1, sizeof(*motive));
    bool has_motive = false;
    ComposerStatus status = motive ? COMPOSER_OK : fail_alloc(error);
    if (status == COMPOSER_OK && state->like_path) {
        CorpusSeedResult result =
            corpus_seed_motive(state->from_corpus, state->like_path, &state->meter, &state->key, state->seed, &state->seed_found);
        if (result == CORPUS_SEED_OK) {
            add_notice(state, &state->seed_found, state->from_corpus);
            status = seed_source(&state->seed_found, &state->source, error);
        } else {
            status = seed_failure(result, state->from_corpus, state->like_path, "/like", error);
        }
        if (status == COMPOSER_OK) {
            state->seeded = true;
            *motive = state->seed_found.motive;
            has_motive = true;
        }
    }
    for (size_t i = 0; status == COMPOSER_OK && i < state->given_count; i++) {
        Given *given = &state->given[i];
        if (!given->from_corpus) {
            continue;
        }
        CorpusSeedResult result = corpus_seed_progression(state->from_corpus, given->numerals, state->key.mode, &given->found);
        if (result != CORPUS_SEED_OK) {
            char path[64];
            snprintf(path, sizeof path, "/given/%zu", i);
            status = seed_failure(result, state->from_corpus, given->numerals, path, error);
            break;
        }
        add_notice(state, &given->found, state->from_corpus);
        size_t used = 0;
        given->found_numerals[0] = '\0';
        for (size_t n = 0; n < given->found.numeral_count && used + 1 < sizeof given->found_numerals; n++) {
            int written = snprintf(given->found_numerals + used, sizeof given->found_numerals - used, n ? " %s" : "%s",
                                   given->found.numerals[n]);
            if (written < 0 || (size_t)written >= sizeof given->found_numerals - used) {
                break;
            }
            used += (size_t)written;
        }
        given->numerals = given->found_numerals;
        given->source = given->found_numerals;
    }
    if (status == COMPOSER_OK && state->motive) {
        status = read_motive(state, state->motive, "/motive", motive, error);
        has_motive = status == COMPOSER_OK;
    }
    if (status == COMPOSER_OK && state->style_path) {
        ComposerStyle *style = NULL;
        if (state->blend_path) {
            status = api_load_blend(state->style_path, state->blend_path, &state->blend_weights, state->without,
                                    state->without_count, &style, error);
        } else {
            status = api_load_style(state->style_path, state->without, state->without_count, "/style", "/style_without", &style,
                                    error);
        }
        composer_style_destroy(style);
    }
    if (status != COMPOSER_OK) {
        free(motive);
        return status;
    }

    PieceRequest request;
    memset(&request, 0, sizeof(request));
    request.form = form_template_name(state->form);
    request.key = state->key;
    request.meter = state->meter;
    request.bars = state->bars;
    request.count = state->count;
    request.motive = has_motive ? motive : NULL;
    request.source = state->seeded ? &state->source : NULL;
    request.has_accompaniment = state->has_accompaniment;
    request.accompaniment = state->accompaniment;
    request.modulate = state->modulate;
    request.seed = state->seed;
    request.given_melodies = state->given_melodies;
    if (state->style_path) {
        request.has_style = true;
        snprintf(request.style.path, PLAN_STYLE_PATH_MAX, "%s", state->style_path);
        request.style.weight_numerator = state->style_weight.numerator;
        request.style.weight_denominator = state->style_weight.denominator;
        for (size_t i = 0; i < state->without_count; i++) {
            snprintf(request.style.without[i], PLAN_STYLE_WITHOUT_NAME_MAX, "%s", state->without[i]);
        }
        request.style.without_count = state->without_count;
        if (state->blend_path) {
            snprintf(request.style.blend_path, PLAN_STYLE_PATH_MAX, "%s", state->blend_path);
            for (int d = 0; d < STYLE_DIMENSION_COUNT; d++) {
                request.style.blend_numerator[d] = state->blend_weights.numerator[d];
                request.style.blend_denominator[d] = state->blend_weights.denominator[d];
            }
        }
    }
    PiecePlan *plan = NULL;
    PieceResult built = piece_build_plan(&request, &plan);
    if (built != PIECE_OK) {
        size_t theme_bars = 8;
        size_t idea_bars = 2;
        (void)piece_basic_idea(&request, &theme_bars, &idea_bars);
        ComposerStatus failed =
               built == PIECE_ERR_MOTIVE_TOO_LONG
                   ? api_fail_motif_long(error, "/motive", request.motive, &state->meter, theme_bars, idea_bars)
               : built == PIECE_ERR_BAD_SIZE
                   ? api_fail(error, COMPOSER_ERR_REQUEST, "/form", "%s has no such size", form_template_name(state->form))
               : built == PIECE_ERR_BAD_REQUEST
                   ? api_fail(error, COMPOSER_ERR_UNSATISFIABLE, "",
                              "this piece cannot be planned as asked: the motive needs at least two notes, and each "
                              "section must be one the harmony operators can write in %s",
                              state->meter_text)
                   : api_fail(error, COMPOSER_ERR_UNSATISFIABLE, "", "%s", piece_result_name(built));
        free(motive);
        return failed;
    }
    free(motive);
    status = place_given(state, plan, error);
    if (status != COMPOSER_OK) {
        plan_destroy(plan);
        return status;
    }
    *out = plan;
    return COMPOSER_OK;
}

ComposerStatus composer_api_plan(const char *request_json, const ComposerJob *job, ComposerResponse *out) {
    (void)job;
    memset(out, 0, sizeof *out);
    PlanBuild *state = calloc(1, sizeof(*state));
    PiecePlan *plan = NULL;
    ComposerStatus status = state ? api_parse_request(request_json, (cJSON **)&state->root, &out->error) : fail_alloc(&out->error);
    if (status == COMPOSER_OK) {
        status = read_request(state, &out->error);
    }
    if (status == COMPOSER_OK) {
        status = check_request(state, &out->error);
    }
    if (status == COMPOSER_OK) {
        status = build_plan(state, &plan, &out->error);
    }
    if (status == COMPOSER_OK) {
        cJSON *document = plan_document(state, plan);
        ApiOutputs outputs;
        memset(&outputs, 0, sizeof outputs);
        status = document ? api_render_outputs(plan, &outputs, out) : fail_alloc(&out->error);
        if (status == COMPOSER_OK) {
            status = api_respond_json(out, document);
        } else {
            cJSON_Delete(document);
        }
    }
    plan_destroy(plan);
    if (state) {
        for (size_t i = 0; i < state->notice_count; i++) {
            free(state->notices[i]);
        }
        cJSON_Delete((cJSON *)state->root);
        free(state);
    }
    if (status != COMPOSER_OK) {
        ComposerError error = out->error;
        composer_response_destroy(out);
        out->error = error;
    }
    return status;
}
