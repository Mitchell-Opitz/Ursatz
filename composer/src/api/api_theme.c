#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>

#include <theory/harmony/harmony_chord.h>

#include <ursatz_composer/composer.h>
#include <ursatz_composer/composer_api.h>
#include <ursatz_composer/explain.h>
#include <ursatz_composer/export.h>
#include <ursatz_composer/grow.h>
#include <ursatz_composer/harmony_phrase.h>
#include <ursatz_composer/motive.h>
#include <plan/plan_json.h>
#include <ursatz_composer/render.h>

#include "api_grow.h"
#include "api_construction.h"
#include "api_style.h"
#include "api_internal.h"
#include "api_piece.h"
#include "api_piece.h"
#include "api_vocabulary.h"
#include "grow/grow_internal.h"
#include "structure/plan_util.h"
#include "piece/request_chords.h"
#include "structure/request_parse.h"
#include "util/util.h"

/*
 * The theme request (docs/theme-request.md, schema "ursatz-theme-request" version 2) as an API entry: the motives of the
 * request, its slots and ideas, the chords and the brief are read, the seeds are tried one after another, and the plan, the
 * result document, the MIDI file and the MusicXML are made in memory.
 */

#define THEME_MOTIVES API_THEME_MOTIVES_MAX
#define SCHEMA_NAME "ursatz-theme-request"

typedef struct {
    const cJSON *json;
    const char *id;
    size_t index;
} RequestMotive;

typedef struct {
    RequestMotive motives[THEME_MOTIVES];
    size_t motive_count;
    const cJSON *slot_motive[GROW_SLOT_COUNT]; /* the request motive of each slot, NULL for none */
    const char *slot_motive_id[GROW_SLOT_COUNT];
    size_t slot_motive_index[GROW_SLOT_COUNT];
    const cJSON *idea_motive[GROW_IDEA_COUNT]; /* the request motive of each idea with its own, NULL for none */
    const char *idea_motive_id[GROW_IDEA_COUNT];
    size_t idea_motive_index[GROW_IDEA_COUNT];
    const cJSON *accompaniment_motive; /* the "motive" member of an accompaniment of pattern "motive" */
} ThemeRequest;


/* The path of a request motive: "/motives/<index>". */
static void motive_path(char *out, size_t size, size_t index) {
    snprintf(out, size, "/motives/%zu", index);
}

/* The slots: each names a motive of the request (a motive may be in several slots), "basic" required. */
static UrsatzStatus read_slots(ThemeRequest *theme, const cJSON *request, UrsatzError *error) {
    const cJSON *slots = cJSON_GetObjectItemCaseSensitive(request, "slots");
    if (!cJSON_IsObject(slots)) {
        return api_fail(error, URSATZ_ERR_REQUEST, "/slots", "the request needs \"slots\": {\"basic\": <motive id>, ...}");
    }
    for (const cJSON *entry = slots->child; entry; entry = entry->next) {
        bool known = false;
        for (int s = 0; s < GROW_SLOT_COUNT; s++) {
            known = known || strcmp(entry->string, grow_slot_name((GrowSlot)s)) == 0;
        }
        if (!known) {
            return api_fail(error, URSATZ_ERR_REQUEST, "/slots",
                            "unknown slot '%s'; the slots are basic, contrasting, continuation and cadential", entry->string);
        }
    }
    for (int s = 0; s < GROW_SLOT_COUNT; s++) {
        const cJSON *named = cJSON_GetObjectItemCaseSensitive(slots, grow_slot_name((GrowSlot)s));
        char path[64];
        snprintf(path, sizeof path, "/slots/%s", grow_slot_name((GrowSlot)s));
        if (!named || cJSON_IsNull(named)) {
            if (s == GROW_SLOT_BASIC) {
                return api_fail(error, URSATZ_ERR_REQUEST, "/slots/basic", "the request needs a basic slot");
            }
            continue;
        }
        if (!cJSON_IsString(named)) {
            return api_fail(error, URSATZ_ERR_REQUEST, path, "slot '%s' must name a motive by its id", grow_slot_name((GrowSlot)s));
        }
        const RequestMotive *found = NULL;
        for (size_t m = 0; m < theme->motive_count; m++) {
            if (strcmp(theme->motives[m].id, named->valuestring) == 0) {
                found = &theme->motives[m];
            }
        }
        if (!found) {
            return api_fail(error, URSATZ_ERR_REQUEST, path, "slot '%s' names the motive '%s', which the request does not have",
                            grow_slot_name((GrowSlot)s), named->valuestring);
        }
        theme->slot_motive[s] = found->json;
        theme->slot_motive_id[s] = found->id;
        theme->slot_motive_index[s] = found->index;
    }
    return URSATZ_OK;
}

static const RequestMotive *find_motive(const ThemeRequest *theme, const char *id) {
    for (size_t m = 0; m < theme->motive_count; m++) {
        if (strcmp(theme->motives[m].id, id) == 0) {
            return &theme->motives[m];
        }
    }
    return NULL;
}

/* The ideas: each names, by its idea name, a motive of the request (or null for none). */
static UrsatzStatus read_ideas(ThemeRequest *theme, const cJSON *request, UrsatzError *error) {
    const cJSON *ideas = cJSON_GetObjectItemCaseSensitive(request, "ideas");
    if (!ideas || cJSON_IsNull(ideas)) {
        return URSATZ_OK;
    }
    if (!cJSON_IsObject(ideas)) {
        return api_fail(error, URSATZ_ERR_REQUEST, "/ideas", "\"ideas\" must be an object: {\"<idea name>\": <motive id>, ...}");
    }
    for (const cJSON *entry = ideas->child; entry; entry = entry->next) {
        char path[96];
        snprintf(path, sizeof path, "/ideas/%.80s", entry->string);
        int idea = -1;
        for (int k = 0; k < GROW_IDEA_COUNT; k++) {
            if (strcmp(entry->string, grow_idea_name((GrowIdea)k)) == 0) {
                idea = k;
            }
        }
        if (idea < 0) {
            return api_fail(error, URSATZ_ERR_REQUEST, "/ideas", "unknown idea '%s' in \"ideas\"", entry->string);
        }
        if (cJSON_IsNull(entry)) {
            continue;
        }
        if (!cJSON_IsString(entry)) {
            return api_fail(error, URSATZ_ERR_REQUEST, path, "idea '%s' must name a motive by its id", entry->string);
        }
        const RequestMotive *found = find_motive(theme, entry->valuestring);
        if (!found) {
            return api_fail(error, URSATZ_ERR_REQUEST, path, "idea '%s' names the motive '%s', which the request does not have",
                            entry->string, entry->valuestring);
        }
        if (idea == GROW_IDEA_BASIC) {
            if (!theme->slot_motive[GROW_SLOT_BASIC] || theme->slot_motive[GROW_SLOT_BASIC] != found->json) {
                return api_fail(error, URSATZ_ERR_REQUEST, path,
                                "the basic idea is the basic slot; leave it out of \"ideas\" or name the motive of the "
                                "basic slot");
            }
            continue;
        }
        theme->idea_motive[idea] = found->json;
        theme->idea_motive_id[idea] = found->id;
        theme->idea_motive_index[idea] = found->index;
    }
    return URSATZ_OK;
}

/* Reads the motives, the slots and the ideas, and refuses saved forms and motive files, before anything is grown. */
static UrsatzStatus read_motives(ThemeRequest *theme, const cJSON *request, UrsatzError *error) {
    const cJSON *motives = cJSON_GetObjectItemCaseSensitive(request, "motives");
    if (!cJSON_IsArray(motives) || cJSON_GetArraySize(motives) < 1 || cJSON_GetArraySize(motives) > THEME_MOTIVES) {
        return api_fail(error, URSATZ_ERR_REQUEST, "/motives", "the request needs \"motives\": 1 to %d motives",
                        THEME_MOTIVES);
    }
    const cJSON *item = NULL;
    size_t index = 0;
    cJSON_ArrayForEach(item, motives) {
        const char *id;
        char path[48];
        motive_path(path, sizeof path, index);
        if (!cJSON_IsObject(item)) {
            return api_fail(error, URSATZ_ERR_REQUEST, path, "the request needs \"id\"");
        }
        if (!api_request_string(item, path, "id", true, &id, error)) {
            return URSATZ_ERR_REQUEST;
        }
        for (size_t m = 0; m < theme->motive_count; m++) {
            if (strcmp(theme->motives[m].id, id) == 0) {
                return api_fail(error, URSATZ_ERR_REQUEST, path, "the motive id '%s' is used twice", id);
            }
        }
        /* The caller reads a motive file; the request carries its content. */
        if (cJSON_GetObjectItemCaseSensitive(item, "path") || cJSON_GetObjectItemCaseSensitive(item, "file")) {
            const char *member = cJSON_GetObjectItemCaseSensitive(item, "path") ? "path" : "file";
            char member_path[64];
            snprintf(member_path, sizeof member_path, "%s/%s", path, member);
            return api_fail(error, URSATZ_ERR_REQUEST, member_path,
                            "motive '%s': a motive carries its content (\"degrees\"), not a file; the caller reads the file", id);
        }
        theme->motives[theme->motive_count].json = item;
        theme->motives[theme->motive_count].id = id;
        theme->motives[theme->motive_count].index = index;
        theme->motive_count++;
        index++;
    }
    UrsatzStatus status = read_slots(theme, request, error);
    if (status == URSATZ_OK) {
        status = read_ideas(theme, request, error);
    }
    if (status != URSATZ_OK) {
        return status;
    }
    /* Saved forms are not read (R-207): the motives stage invents the forms. */
    for (size_t m = 0; m < theme->motive_count; m++) {
        const cJSON *forms = cJSON_GetObjectItemCaseSensitive(theme->motives[m].json, "forms");
        if (forms && !cJSON_IsNull(forms)) {
            char path[64];
            snprintf(path, sizeof path, "/motives/%zu/forms", theme->motives[m].index);
            return api_fail(error, URSATZ_ERR_REQUEST, path, "motive '%s': saved forms are no longer read; leave \"forms\" out",
                            theme->motives[m].id);
        }
    }
    return URSATZ_OK;
}

/*
 * A basic motif as long as the presentation or the antecedent fills that part whole: it leaves no contrasting or repeating
 * idea, so a contrasting slot or the motive of that idea is refused where it is given.
 */
static UrsatzStatus refuse_idea_beside_part(const GrowMaterial *material, const ThemeRequest *theme,
                                              const GrowSetup *setup, UrsatzError *error) {
    Motive basic;
    MotiveRelationStep step;
    size_t part_bars = setup->bars / 2;
    size_t basic_bars = setup->bars == 4 ? 1 : 2;
    if (setup->has_construction) {
        GrowConstructionBars of;
        grow_construction_bars(&setup->pairing, &setup->construction, &of);
        part_bars = of.beginning;
        basic_bars = setup->construction.idea_bars[GROW_IDEA_BASIC];
    } else if (setup->bars != 4 && setup->bars != 8) {
        return URSATZ_OK; /* a construction chosen at the build (R-469): grow_build_plan_checked checks the idea then */
    }
    Rational theme_bar;
    plan_meter_bar_length(&setup->meter, &theme_bar);
    if (!grow_part_basic_idea(&material->basic, theme_bar, part_bars, basic_bars,
                              setup->pairing.beginning, &basic, &step)) {
        return URSATZ_OK;
    }
    const char *part = setup->pairing.beginning == SECTION_BEGINNING_ANTECEDENT ? "antecedent" : "presentation";
    char path[96];
    if (theme->slot_motive[GROW_SLOT_CONTRASTING]) {
        return api_fail(error, URSATZ_ERR_REQUEST, "/slots/contrasting",
                        "the basic motif fills the whole %s; leave the contrasting idea out", part);
    }
    for (int idea = GROW_IDEA_BASIC + 1; idea < GROW_IDEA_COUNT; idea++) {
        GrowPlace place;
        if (theme->idea_motive[idea] && grow_idea_place(&setup->pairing, (GrowIdea)idea, &place) &&
            place == GROW_PLACE_REPETITION) {
            snprintf(path, sizeof path, "/ideas/%.80s", grow_idea_name((GrowIdea)idea));
            return api_fail(error, URSATZ_ERR_REQUEST, path,
                            "the basic motif fills the whole %s; leave the contrasting idea out", part);
        }
    }
    return URSATZ_OK;
}

/* Fills the motives of the material once the key and the meter are known (the style has given the mode). */
static UrsatzStatus load_material(GrowMaterial *material, const ThemeRequest *theme, const cJSON *request,
                                    const GrowSetup *setup, UrsatzError *error) {
    char what[96];
    char path[48];
    UrsatzStatus status;
    for (int s = 0; s < GROW_SLOT_COUNT; s++) {
        if (!theme->slot_motive[s]) {
            continue;
        }
        snprintf(what, sizeof(what), "motive '%s'", theme->slot_motive_id[s]);
        motive_path(path, sizeof path, theme->slot_motive_index[s]);
        Motive *target = s == GROW_SLOT_BASIC ? &material->basic : &material->slot_motives[s];
        status = api_read_degrees_motive(setup->knowledge, what, path, theme->slot_motive[s], NULL, &setup->meter, &setup->key, target, error);
        if (status != URSATZ_OK) {
            return status;
        }
        if (target->accompaniment) {
            return api_fail(error, URSATZ_ERR_REQUEST, path, "%s is of the accompaniment role; a slot takes a melodic motive",
                            what);
        }
        material->has_slot[s] = s != GROW_SLOT_BASIC;
    }
    for (int idea = GROW_IDEA_BASIC + 1; idea < GROW_IDEA_COUNT; idea++) {
        if (!theme->idea_motive[idea]) {
            continue;
        }
        motive_path(path, sizeof path, theme->idea_motive_index[idea]);
        if (!grow_idea_in_pairing(&setup->pairing, (GrowIdea)idea)) {
            return api_fail(error, URSATZ_ERR_REQUEST, path, "idea '%s' is not in this theme; the pairing has no %s",
                            grow_idea_name((GrowIdea)idea), grow_idea_name((GrowIdea)idea));
        }
        snprintf(what, sizeof(what), "motive '%s'", theme->idea_motive_id[idea]);
        status = api_read_degrees_motive(setup->knowledge, what, path, theme->idea_motive[idea], NULL, &setup->meter, &setup->key,
                                         &material->idea_motives[idea], error);
        if (status != URSATZ_OK) {
            return status;
        }
        if (material->idea_motives[idea].accompaniment) {
            return api_fail(error, URSATZ_ERR_REQUEST, path, "%s is of the accompaniment role; an idea takes a melodic motive",
                            what);
        }
        material->has_idea[idea] = true;
    }
    if (theme->accompaniment_motive) {
        status = api_read_degrees_motive(setup->knowledge, "the accompaniment motive", "/accompaniment/motive", theme->accompaniment_motive,
                                         "accompaniment", &setup->meter, &setup->key, &material->accompaniment_motive, error);
        if (status != URSATZ_OK) {
            return status;
        }
        material->has_accompaniment_motive = true;
    }
    status = refuse_idea_beside_part(material, theme, setup, error);
    if (status != URSATZ_OK) {
        return status;
    }
    return request_read_chords(cJSON_GetObjectItemCaseSensitive(request, "chords"), "/chords", "the theme", 0, &setup->meter, &setup->key,
                               (long)setup->bars, material->chords, material->chord_numerals, GROW_EXT_CHORDS, &material->chord_count,
                               error);
}

/* --- the brief ---------------------------------------------------------------------------------------- */

#define WITHOUT_MAX 8

typedef struct {
    GrowBriefText text;
    char bars_text[16];
    const char *blend;
    const char *blend_weight;
    const char *blend_weights;
    const char *style;
    const char *without[WITHOUT_MAX];
} ThemeBrief;

/* The members of the brief as the text the options of grow take. */
static UrsatzStatus read_brief(ThemeRequest *theme, const cJSON *request, ThemeBrief *brief, UrsatzError *error) {
    const char *into;
    const char *hybrid;
    const char *key;
    const char *material;
    const char *material_member;
    const char *meter;
    const char *cadence;
    const char *first_cadence;
    const char *voicing;
    const char *style;
    const char *weight;
    const char *development;
    if (!api_request_string(request, "", "into", true, &into, error) ||
        !api_request_string(request, "", "development", false, &development, error) ||
        !api_request_string(request, "", "hybrid", false, &hybrid, error) ||
        !api_request_string(request, "", "key", true, &key, error) || !api_read_material(request, "", true, &material, &material_member, error) ||
        !api_request_string(request, "", "meter", true, &meter, error) ||
        !api_request_string(request, "", "cadence", false, &cadence, error) ||
        !api_request_string(request, "", "first_cadence", false, &first_cadence, error) ||
        !api_request_string(request, "", "voicing", false, &voicing, error) ||
        !api_request_string(request, "", "style", false, &style, error) ||
        !api_request_string(request, "", "style_weight", false, &weight, error) ||
        !api_request_string(request, "", "blend", false, &brief->blend, error) ||
        !api_request_string(request, "", "blend_weight", false, &brief->blend_weight, error) ||
        !api_request_string(request, "", "blend_weights", false, &brief->blend_weights, error)) {
        return URSATZ_ERR_REQUEST;
    }
    GrowBriefText *text = &brief->text;
    text->into = into;
    text->hybrid = hybrid;
    text->key = key;
    text->material = material;
    text->material_member = material_member;
    text->meter = meter;
    text->cadence = cadence;
    text->first_cadence = first_cadence;
    text->development = development;
    text->style_weight = style ? weight : NULL; /* read only with a style */
    text->has_style = style != NULL;
    brief->style = style;
    const cJSON *bars = cJSON_GetObjectItemCaseSensitive(request, "bars");
    if (bars && !cJSON_IsNull(bars)) {
        if (!cJSON_IsNumber(bars)) {
            return api_fail(error, URSATZ_ERR_REQUEST, "/bars", "\"bars\" in the request must be a whole number from 3 to 16");
        }
        snprintf(brief->bars_text, sizeof(brief->bars_text), "%g", bars->valuedouble);
        text->bars = brief->bars_text;
    }
    const cJSON *accompaniment = cJSON_GetObjectItemCaseSensitive(request, "accompaniment");
    if (accompaniment && !cJSON_IsNull(accompaniment)) {
        const char *pattern;
        if (!cJSON_IsObject(accompaniment)) {
            return api_fail(error, URSATZ_ERR_REQUEST, "/accompaniment", "\"accompaniment\" must be null or {\"pattern\": ...}");
        }
        if (!api_request_string(accompaniment, "/accompaniment", "pattern", true, &pattern, error)) {
            return URSATZ_ERR_REQUEST;
        }
        if (strcmp(pattern, "motive") == 0) {
            theme->accompaniment_motive = cJSON_GetObjectItemCaseSensitive(accompaniment, "motive");
            if (!cJSON_IsObject(theme->accompaniment_motive)) {
                return api_fail(error, URSATZ_ERR_REQUEST, "/accompaniment/motive",
                                "an accompaniment of pattern \"motive\" needs a \"motive\" member");
            }
            text->accompaniment_motive = true;
        }
        text->accompaniment = pattern;
    }
    if (voicing) {
        if (strcmp(voicing, "satb") == 0) {
            text->voicing = true;
        } else if (strcmp(voicing, "chorale") == 0) {
            text->texture = "chorale";
        } else {
            return api_fail(error, URSATZ_ERR_REQUEST, "/voicing", "\"voicing\" in the request must be satb or chorale, not '%s'",
                            voicing);
        }
    }
    const cJSON *without = cJSON_GetObjectItemCaseSensitive(request, "without");
    if (without && !cJSON_IsNull(without) && !(cJSON_IsArray(without) && cJSON_GetArraySize(without) == 0)) {
        if (!style) {
            return api_fail(error, URSATZ_ERR_REQUEST, "/without", "\"without\" in the request needs a \"style\"");
        }
        if (!cJSON_IsArray(without)) {
            return api_fail(error, URSATZ_ERR_REQUEST, "/without", "\"without\" in the request must be a list of categories");
        }
        const cJSON *category = NULL;
        cJSON_ArrayForEach(category, without) {
            if (!cJSON_IsString(category)) {
                return api_fail(error, URSATZ_ERR_REQUEST, "/without", "\"without\" in the request must be a list of categories");
            }
            bool known = false;
            for (size_t i = 0; i < text->without_count; i++) {
                known = known || strcmp(brief->without[i], category->valuestring) == 0;
            }
            if (known) {
                continue;
            }
            if (text->without_count >= WITHOUT_MAX) {
                return api_fail(error, URSATZ_ERR_REQUEST, "/without", "\"without\" names at most %d style categories",
                                WITHOUT_MAX);
            }
            brief->without[text->without_count++] = category->valuestring;
        }
    }
    return URSATZ_OK;
}

/* --- the outputs --------------------------------------------------------------------------------------- */

/* Adds, for each theme of the document, its plan (with "plans") and its explanation (with "explain"). */
static UrsatzStatus add_theme_members(const KnowledgeSnapshot *knowledge, cJSON *document, const GrowRun *run, GrowDevelopment setup_development,
                                        const ApiOutputOptions *outputs, UrsatzError *error) {
    cJSON *themes = cJSON_GetObjectItemCaseSensitive(document, "themes");
    for (size_t k = 0; k < run->made; k++) {
        cJSON *item = cJSON_GetArrayItem(themes, (int)k);
        const GrowTheme *theme = &run->themes[k];
        if (outputs->plans) {
            PlanJsonError detail;
            PlanProducer producer = composer_plan_producer(knowledge);
            char *text = plan_to_json(theme->plan, &producer, &detail);
            cJSON *plan = text ? cJSON_Parse(text) : NULL;
            free(text);
            if (!plan || !cJSON_AddItemToObject(item, "plan", plan)) {
                cJSON_Delete(plan);
                return api_fail_alloc(error);
            }
        }
        if (outputs->explain) {
            Explanation explanation;
            ExplainResult built = explain_build_grow(theme->plan, api_find_node(theme->plan, "theme"), &theme->outcome,
                                                     theme->checked ? &theme->check : NULL, run->passed, theme->passed_before,
                                                     &explanation);
            char *text = NULL;
            if (built == EXPLAIN_OK) {
                snprintf(explanation.sections[0].development, sizeof(explanation.sections[0].development), "%s",
                         api_development_name(setup_development));
                text = outputs->explain == 2 ? explain_to_json(&explanation) : explain_to_text(&explanation);
                explain_destroy(&explanation);
            }
            if (!text || !cJSON_AddStringToObject(item, "explanation", text)) {
                free(text);
                return api_fail_alloc(error);
            }
            free(text);
        }
    }
    return URSATZ_OK;
}

/* The plan, the MIDI file and the MusicXML of the first theme. */
static UrsatzStatus render_first(const KnowledgeSnapshot *knowledge, const GrowTheme *theme, const ApiOutputOptions *outputs, cJSON *document,
                                   ComposerResponse *out) {
    PlanJsonError json_error;
    PlanProducer producer = composer_plan_producer(knowledge);
    out->plan_json = plan_to_json(theme->plan, &producer, &json_error);
    if (!out->plan_json) {
        return api_fail(&out->error, URSATZ_ERR_INTERNAL, "", "cannot write the plan: %s", json_error.message);
    }
    if (outputs->layers) {
        UrsatzStatus layered = api_layers_json(knowledge, theme->plan, &outputs->export_options, document, &out->error);
        if (layered != URSATZ_OK) {
            return layered;
        }
    }
    if (!outputs->midi && !outputs->musicxml) {
        return URSATZ_OK;
    }
    RenderedPlan rendered;
    PlanExportError detail;
    if (plan_export_render(theme->plan, &rendered, &detail) != PLAN_EXPORT_OK) {
        return api_fail(&out->error, URSATZ_ERR_REQUEST, outputs->midi ? "/midi" : "/musicxml", "cannot render the plan");
    }
    UrsatzStatus status = URSATZ_OK;
    if (outputs->midi &&
        rendered_plan_midi_bytes(knowledge, &rendered, &outputs->export_options, &out->midi, &out->midi_size, &detail) != PLAN_EXPORT_OK) {
        status = api_fail(&out->error, URSATZ_ERR_REQUEST, "/midi", "cannot write the MIDI file: %s", detail.message);
    }
    if (status == URSATZ_OK && outputs->musicxml &&
        rendered_plan_musicxml_text(knowledge, &rendered, &outputs->export_options, &out->musicxml, &detail) != PLAN_EXPORT_OK) {
        status = api_fail(&out->error, URSATZ_ERR_REQUEST, "/musicxml", "cannot write the MusicXML: %s", detail.message);
    }
    rendered_plan_destroy(&rendered);
    return status;
}

/* --- the entry ------------------------------------------------------------------------------------------ */

typedef struct {
    cJSON *request;
    ThemeRequest theme;
    ThemeBrief brief;
    GrowMaterial *material;
    GrowSetup setup;
    ComposerPreferences preferences; /* what the request's "preferences" member gave; the setup points into it */
    bool has_preferences;
    ComposerStyle *style;
    GrowRun run;
    bool ran;
} ThemeState;

static UrsatzStatus theme_prepare(ThemeState *state, const UrsatzJob *job, ApiOutputOptions *outputs, UrsatzError *error) {
    const cJSON *request = state->request;
    const char *schema;
    const cJSON *version = cJSON_GetObjectItemCaseSensitive(request, "schema_version");
    if (!api_request_string(request, "", "schema", true, &schema, error)) {
        return URSATZ_ERR_REQUEST;
    }
    if (strcmp(schema, SCHEMA_NAME) != 0) {
        return api_fail(error, URSATZ_ERR_REQUEST, "/schema", "the request must have \"schema\": \"%s\"", SCHEMA_NAME);
    }
    if (!cJSON_IsNumber(version) || version->valuedouble != 2) {
        return api_fail(error, URSATZ_ERR_REQUEST, "/schema_version",
                        "the request must have \"schema_version\": 2 (see docs/theme-request.md)");
    }
    UrsatzStatus status = read_motives(&state->theme, request, error);
    if (status == URSATZ_OK) {
        status = read_brief(&state->theme, request, &state->brief, error);
    }
    if (status == URSATZ_OK) {
        status = api_read_output_options(request, outputs, error);
    }
    if (status != URSATZ_OK) {
        return status;
    }

    GrowSetup *setup = &state->setup;
    bool present;
    bool check = false;
    unsigned long seed = 0;
    unsigned long suggest = 1;
    if (!api_member_bool(request, "check", &check, &present, error) ||
        !api_member_whole(request, "seed", 0, GROW_SEED_MAX, &seed, &present, error)) {
        return URSATZ_ERR_REQUEST;
    }
    setup->seed = present ? seed : api_default_seed();
    if (!api_member_whole(request, "suggest", 1, GROW_MAX_SUGGESTIONS, &suggest, &present, error)) {
        return URSATZ_ERR_REQUEST;
    }
    setup->suggest = suggest;
    setup->suggested = present;
    setup->check = check;
    setup->explain = outputs->explain != 0;
    setup->job = job;

    /*
     * The settings of the melody, of the development's fidelity minimums and continuation, and of its contrasting ideas.
     * The motif group shapes a theme's drawn rhythms and its new material (R-475, R-477) and nothing else: a theme's
     * motives come from the request (docs/theme-request.md).
     */
    if (!api_read_preferences(request, &state->preferences, &state->has_preferences, error)) {
        return URSATZ_ERR_REQUEST;
    }
    if (state->has_preferences) {
        setup->melody = &state->preferences.melody;
        setup->fidelity = &state->preferences.development;
        setup->continuation = &state->preferences.continuation;
        setup->basic_as_written = state->preferences.basic_as_written;
        setup->contrast = &state->preferences.contrast;
        setup->functions = &state->preferences.functions;
        setup->critic = &state->preferences.critic;
        setup->motif = &state->preferences.motif;
    }

    status = api_grow_read_brief(&state->brief.text, setup, error);
    if (status == URSATZ_OK) {
        status = api_grow_read_construction(request, setup, error);
    }
    if (status != URSATZ_OK) {
        return status;
    }
    ComposerStyleWeights weights;
    memset(&weights, 0, sizeof(weights));
    status = api_read_blend(state->brief.style != NULL, state->brief.blend, state->brief.blend_weight,
                            state->brief.blend_weights, &weights, error);
    if (status != URSATZ_OK) {
        return status;
    }
    /* The tempo of the request wins over the style's, and is the tempo of the files. */
    PlanExportOptions tempo_options = plan_export_options_default();
    status = api_read_tempo(request, &tempo_options, error);
    if (status != URSATZ_OK) {
        return status;
    }
    bool has_tempo = cJSON_GetObjectItemCaseSensitive(request, "tempo") && !cJSON_IsNull(cJSON_GetObjectItemCaseSensitive(request, "tempo"));
    if (has_tempo) {
        outputs->export_options.tempo_numerator = tempo_options.tempo_numerator;
        outputs->export_options.tempo_denominator = tempo_options.tempo_denominator;
    }
    if (state->brief.style) {
        status = api_load_blend(state->brief.style, state->brief.blend, &weights, state->brief.without,
                                state->brief.text.without_count, &state->style, error);
        if (status != URSATZ_OK) {
            return status;
        }
    }
    setup->style = state->style;
    setup->without = state->brief.without;
    setup->without_count = state->brief.text.without_count;
    Rational option_tempo = {outputs->export_options.tempo_numerator, outputs->export_options.tempo_denominator};
    status = api_grow_set_tempo_and_mode(setup, has_tempo ? &option_tempo : NULL, error);
    if (status != URSATZ_OK) {
        return status;
    }
    if (!has_tempo && setup->tempo.den > 0) {
        outputs->export_options.tempo_numerator = setup->tempo.num;
        outputs->export_options.tempo_denominator = setup->tempo.den;
    }

    state->material = calloc(1, sizeof(*state->material));
    if (!state->material) {
        return api_fail_alloc(error);
    }
    status = load_material(state->material, &state->theme, request, setup, error);
    if (status != URSATZ_OK) {
        return status;
    }
    setup->motive = &state->material->basic;
    setup->accompaniment_motive = state->material->has_accompaniment_motive ? &state->material->accompaniment_motive : NULL;
    setup->material = state->material;
    return URSATZ_OK;
}

UrsatzStatus composer_api_theme(const ComposerContext *context, const char *request_json,
                                  const UrsatzJob *job, ComposerResponse *out) {
    memset(out, 0, sizeof *out);
    CommonPracticeMaterials materials;
    UrsatzStatus context_status = api_require_knowledge(context, &materials, &out->error);
    if (context_status != URSATZ_OK) {
        return context_status;
    }
    ThemeState state;
    memset(&state, 0, sizeof(state));
    state.setup.knowledge = context->knowledge;
    state.setup.materials = &materials;
    ApiOutputOptions outputs;
    UrsatzStatus status = api_parse_request(request_json, &state.request, &out->error);
    if (status == URSATZ_OK) {
        status = theme_prepare(&state, job, &outputs, &out->error);
    }
    if (status == URSATZ_OK) {
        state.ran = true;
        status = api_grow_run(&state.setup, &state.run, &out->error);
    }
    if (status == URSATZ_OK) {
        GrowThemeInfo info;
        memset(&info, 0, sizeof(info));
        for (int s = 0; s < GROW_SLOT_COUNT; s++) {
            info.slot_motive_id[s] = state.theme.slot_motive_id[s];
        }
        for (int idea = 0; idea < GROW_IDEA_COUNT; idea++) {
            info.idea_motive_id[idea] = state.theme.idea_motive_id[idea];
        }
        const cJSON *chords = cJSON_GetObjectItemCaseSensitive(state.request, "chords");
        info.chords = cJSON_IsArray(chords) ? chords : NULL;
        cJSON *document = api_grow_result_json(&state.setup, &state.run, &info);
        status = document ? add_theme_members(context->knowledge, document, &state.run, state.setup.development, &outputs, &out->error) : api_fail_alloc(&out->error);
        if (status == URSATZ_OK && state.has_preferences &&
            !cJSON_AddItemToObject(document, "preferences", composer_preferences_write(&state.preferences))) {
            status = api_fail_alloc(&out->error);
        }
        if (status == URSATZ_OK) {
            status = render_first(context->knowledge, &state.run.themes[0], &outputs, document, out);
        }
        if (status == URSATZ_OK) {
            status = api_respond_json(out, document);
        } else {
            cJSON_Delete(document);
        }
    }
    if (state.ran) {
        api_grow_run_destroy(&state.run);
    }
    free(state.material);
    composer_style_destroy(state.style);
    cJSON_Delete(state.request);
    if (status != URSATZ_OK) {
        UrsatzError error = out->error;
        composer_response_destroy(out);
        out->error = error;
    }
    return status;
}

UrsatzStatus api_theme_validate(const KnowledgeSnapshot *knowledge, const CommonPracticeMaterials *materials, const cJSON *request,
                                  UrsatzError *error) {
    ThemeState state;
    memset(&state, 0, sizeof(state));
    state.setup.knowledge = knowledge;
    state.setup.materials = materials;
    ApiOutputOptions outputs;
    memset(&outputs, 0, sizeof(outputs));
    state.request = cJSON_Duplicate(request, true);
    UrsatzStatus status = state.request ? theme_prepare(&state, NULL, &outputs, error) : api_fail_alloc(error);
    if (status == URSATZ_OK) {
        /* Build the plan as Compose would, without seeding: the first refusal is the answer. */
        GrowRequest grow;
        Plan *plan = NULL;
        GrowRefusal refusal;
        api_grow_request_for(&state.setup, state.setup.seed, &grow);
        GrowResult built = grow_build_plan_checked(&grow, &plan, &refusal);
        if (built == GROW_OK) {
            plan_destroy(plan);
        } else if (built == GROW_ERR_ALLOC) {
            status = api_fail_alloc(error);
        } else {
            status = api_fail(error, URSATZ_ERR_UNSATISFIABLE, refusal.path, "%s", refusal.message);
        }
    }
    free(state.material);
    composer_style_destroy(state.style);
    cJSON_Delete(state.request);
    return status;
}
