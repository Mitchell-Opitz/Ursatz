#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>

#include <standard-harmony/harmony_chord.h>

#include <ursatz_composer/composer_api.h>
#include <ursatz_composer/explain.h>
#include <ursatz_composer/export.h>
#include <ursatz_composer/grow.h>
#include <ursatz_composer/harmony_phrase.h>
#include <ursatz_composer/motive.h>
#include <ursatz_composer/plan_json.h>
#include <ursatz_composer/render.h>

#include "api_grow.h"
#include "api_internal.h"
#include "api_vocabulary.h"
#include "grow/grow_internal.h"
#include "util/request_parse.h"

/*
 * The theme request (docs/theme-request.md, schema "ursatz-theme-request" version 2) as an API entry: the motives of the
 * request, its slots and ideas, the chords and the brief are read, the seeds are tried one after another, and the plan, the
 * result document, the MIDI file and the MusicXML are made in memory.
 */

#define THEME_MOTIVES 16
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
static ComposerStatus read_slots(ThemeRequest *theme, const cJSON *request, ComposerError *error) {
    const cJSON *slots = cJSON_GetObjectItemCaseSensitive(request, "slots");
    if (!cJSON_IsObject(slots)) {
        return api_fail(error, COMPOSER_ERR_REQUEST, "/slots", "the request needs \"slots\": {\"basic\": <motive id>, ...}");
    }
    for (const cJSON *entry = slots->child; entry; entry = entry->next) {
        bool known = false;
        for (int s = 0; s < GROW_SLOT_COUNT; s++) {
            known = known || strcmp(entry->string, grow_slot_name((GrowSlot)s)) == 0;
        }
        if (!known) {
            return api_fail(error, COMPOSER_ERR_REQUEST, "/slots",
                            "unknown slot '%s'; the slots are basic, contrasting, continuation and cadential", entry->string);
        }
    }
    for (int s = 0; s < GROW_SLOT_COUNT; s++) {
        const cJSON *named = cJSON_GetObjectItemCaseSensitive(slots, grow_slot_name((GrowSlot)s));
        char path[64];
        snprintf(path, sizeof path, "/slots/%s", grow_slot_name((GrowSlot)s));
        if (!named || cJSON_IsNull(named)) {
            if (s == GROW_SLOT_BASIC) {
                return api_fail(error, COMPOSER_ERR_REQUEST, "/slots/basic", "the request needs a basic slot");
            }
            continue;
        }
        if (!cJSON_IsString(named)) {
            return api_fail(error, COMPOSER_ERR_REQUEST, path, "slot '%s' must name a motive by its id", grow_slot_name((GrowSlot)s));
        }
        const RequestMotive *found = NULL;
        for (size_t m = 0; m < theme->motive_count; m++) {
            if (strcmp(theme->motives[m].id, named->valuestring) == 0) {
                found = &theme->motives[m];
            }
        }
        if (!found) {
            return api_fail(error, COMPOSER_ERR_REQUEST, path, "slot '%s' names the motive '%s', which the request does not have",
                            grow_slot_name((GrowSlot)s), named->valuestring);
        }
        theme->slot_motive[s] = found->json;
        theme->slot_motive_id[s] = found->id;
        theme->slot_motive_index[s] = found->index;
    }
    return COMPOSER_OK;
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
static ComposerStatus read_ideas(ThemeRequest *theme, const cJSON *request, ComposerError *error) {
    const cJSON *ideas = cJSON_GetObjectItemCaseSensitive(request, "ideas");
    if (!ideas || cJSON_IsNull(ideas)) {
        return COMPOSER_OK;
    }
    if (!cJSON_IsObject(ideas)) {
        return api_fail(error, COMPOSER_ERR_REQUEST, "/ideas", "\"ideas\" must be an object: {\"<idea name>\": <motive id>, ...}");
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
            return api_fail(error, COMPOSER_ERR_REQUEST, "/ideas", "unknown idea '%s' in \"ideas\"", entry->string);
        }
        if (cJSON_IsNull(entry)) {
            continue;
        }
        if (!cJSON_IsString(entry)) {
            return api_fail(error, COMPOSER_ERR_REQUEST, path, "idea '%s' must name a motive by its id", entry->string);
        }
        const RequestMotive *found = find_motive(theme, entry->valuestring);
        if (!found) {
            return api_fail(error, COMPOSER_ERR_REQUEST, path, "idea '%s' names the motive '%s', which the request does not have",
                            entry->string, entry->valuestring);
        }
        if (idea == GROW_IDEA_BASIC) {
            if (!theme->slot_motive[GROW_SLOT_BASIC] || theme->slot_motive[GROW_SLOT_BASIC] != found->json) {
                return api_fail(error, COMPOSER_ERR_REQUEST, path,
                                "the basic idea is the basic slot; leave it out of \"ideas\" or name the motive of the "
                                "basic slot");
            }
            continue;
        }
        theme->idea_motive[idea] = found->json;
        theme->idea_motive_id[idea] = found->id;
        theme->idea_motive_index[idea] = found->index;
    }
    return COMPOSER_OK;
}

/* Reads the motives, the slots and the ideas, and refuses saved forms and motive files, before anything is grown. */
static ComposerStatus read_motives(ThemeRequest *theme, const cJSON *request, ComposerError *error) {
    const cJSON *motives = cJSON_GetObjectItemCaseSensitive(request, "motives");
    if (!cJSON_IsArray(motives) || cJSON_GetArraySize(motives) < 1 || cJSON_GetArraySize(motives) > THEME_MOTIVES) {
        return api_fail(error, COMPOSER_ERR_REQUEST, "/motives", "the request needs \"motives\": 1 to %d motives",
                        THEME_MOTIVES);
    }
    const cJSON *item = NULL;
    size_t index = 0;
    cJSON_ArrayForEach(item, motives) {
        const char *id;
        char path[48];
        motive_path(path, sizeof path, index);
        if (!cJSON_IsObject(item)) {
            return api_fail(error, COMPOSER_ERR_REQUEST, path, "the request needs \"id\"");
        }
        if (!api_request_string(item, path, "id", true, &id, error)) {
            return COMPOSER_ERR_REQUEST;
        }
        for (size_t m = 0; m < theme->motive_count; m++) {
            if (strcmp(theme->motives[m].id, id) == 0) {
                return api_fail(error, COMPOSER_ERR_REQUEST, path, "the motive id '%s' is used twice", id);
            }
        }
        /* The caller reads a motive file; the request carries its content. */
        if (cJSON_GetObjectItemCaseSensitive(item, "path") || cJSON_GetObjectItemCaseSensitive(item, "file")) {
            const char *member = cJSON_GetObjectItemCaseSensitive(item, "path") ? "path" : "file";
            char member_path[64];
            snprintf(member_path, sizeof member_path, "%s/%s", path, member);
            return api_fail(error, COMPOSER_ERR_REQUEST, member_path,
                            "motive '%s': a motive carries its content (\"degrees\"), not a file; the caller reads the file", id);
        }
        theme->motives[theme->motive_count].json = item;
        theme->motives[theme->motive_count].id = id;
        theme->motives[theme->motive_count].index = index;
        theme->motive_count++;
        index++;
    }
    ComposerStatus status = read_slots(theme, request, error);
    if (status == COMPOSER_OK) {
        status = read_ideas(theme, request, error);
    }
    if (status != COMPOSER_OK) {
        return status;
    }
    /* Saved forms are not read (R-207): the motives stage invents the forms. */
    for (size_t m = 0; m < theme->motive_count; m++) {
        const cJSON *forms = cJSON_GetObjectItemCaseSensitive(theme->motives[m].json, "forms");
        if (forms && !cJSON_IsNull(forms)) {
            char path[64];
            snprintf(path, sizeof path, "/motives/%zu/forms", theme->motives[m].index);
            return api_fail(error, COMPOSER_ERR_REQUEST, path, "motive '%s': saved forms are no longer read; leave \"forms\" out",
                            theme->motives[m].id);
        }
    }
    return COMPOSER_OK;
}

/*
 * The chords: a list of {"bar", "text"}, at most one a bar, the text in the own-harmony syntax relative to the bar. They are
 * written into material->chords as Given chords from the theme's start, each lasting to the next chord of its bar or the
 * end of the bar.
 */
static ComposerStatus read_chords(GrowMaterial *material, const cJSON *request, const PlanMeter *meter, const AnalysisKey *key,
                                  ComposerError *error) {
    const cJSON *list = cJSON_GetObjectItemCaseSensitive(request, "chords");
    if (!list || cJSON_IsNull(list)) {
        return COMPOSER_OK;
    }
    if (!cJSON_IsArray(list)) {
        return api_fail(error, COMPOSER_ERR_REQUEST, "/chords",
                        "\"chords\" must be a list of {\"bar\": <number from 0>, \"text\": \"I V@3/2\"}");
    }
    const cJSON *bars_item = cJSON_GetObjectItemCaseSensitive(request, "bars");
    long bars = cJSON_IsNumber(bars_item) ? (long)bars_item->valuedouble : 8;
    Rational bar_length;
    if (rational_make(meter->beats_per_bar, (long long)meter->beat_unit, &bar_length) != RATIONAL_OK) {
        return api_fail(error, COMPOSER_ERR_REQUEST, "/meter", "the meter has no bar length");
    }
    bool seen[GROW_EXT_CHORDS / 2 + 1] = {false};
    const cJSON *entry = NULL;
    size_t index = 0;
    cJSON_ArrayForEach(entry, list) {
        char path[48];
        snprintf(path, sizeof path, "/chords/%zu", index++);
        const cJSON *bar_item = cJSON_IsObject(entry) ? cJSON_GetObjectItemCaseSensitive(entry, "bar") : NULL;
        const char *text;
        if (!cJSON_IsObject(entry) || !cJSON_IsNumber(bar_item) || !api_request_string(entry, path, "text", true, &text, error)) {
            return api_fail(error, COMPOSER_ERR_REQUEST, path,
                            "each of \"chords\" must be {\"bar\": <number from 0>, \"text\": \"I V@3/2\"}");
        }
        long bar = (long)bar_item->valuedouble;
        if ((double)bar != bar_item->valuedouble || bar < 0 || bar >= bars) {
            return api_fail(error, COMPOSER_ERR_REQUEST, path, "chords bar %g: the theme has bars 0 to %ld",
                            bar_item->valuedouble, bars - 1);
        }
        if (seen[bar]) {
            return api_fail(error, COMPOSER_ERR_REQUEST, path, "chords bar %ld: given twice", bar);
        }
        seen[bar] = true;
        MotiveSketchChord parsed[MOTIVE_MAX_CHORDS];
        size_t count = 0;
        MotiveError motive_error;
        if (motive_harmony_parse(text, meter, parsed, &count, &motive_error) != MOTIVE_OK) {
            return api_fail(error, COMPOSER_ERR_REQUEST, path, "chords bar %ld: '%s' at %s: %s", bar, text,
                            motive_error.location, motive_error.message);
        }
        if (count == 0 || count > 2) {
            return api_fail(error, COMPOSER_ERR_REQUEST, path, "chords bar %ld: write 1 or 2 chords, not %zu", bar, count);
        }
        if (material->chord_count + count > GROW_EXT_CHORDS) {
            return api_fail(error, COMPOSER_ERR_REQUEST, "/chords", "chords: too many chords");
        }
        Rational bar_start;
        if (rational_mul_int(bar_length, (long long)bar, &bar_start) != RATIONAL_OK) {
            return api_fail(error, COMPOSER_ERR_REQUEST, path, "chords bar %ld: the bar is out of range", bar);
        }
        for (size_t c = 0; c < count; c++) {
            const MotiveSketchChord *chord = &parsed[c];
            Rational half_beats;
            Rational end = bar_length;
            /* An onset is a multiple of half a beat of the beat unit, inside the bar. */
            if (rational_make(chord->onset.num * 2 * meter->beat_unit, chord->onset.den, &half_beats) != RATIONAL_OK ||
                half_beats.den != 1) {
                return api_fail(error, COMPOSER_ERR_REQUEST, path, "chords bar %ld: a chord starts on a beat or a half beat",
                                bar);
            }
            Rational cell;
            Rational in_cells;
            if (!given_cell_length(meter, &cell) ||
                rational_make(chord->onset.num * (long)cell.den, chord->onset.den * (long long)cell.num, &in_cells) !=
                    RATIONAL_OK ||
                in_cells.den != 1) {
                /* The harmony and bass operators write chords on the cells of the meter only (see docs/theme-request.md). */
                long beats = cell.num * (long)meter->beat_unit / (long)cell.den;
                return rational_cmp(cell, bar_length) == 0
                           ? api_fail(error, COMPOSER_ERR_REQUEST, path, "chords bar %ld: a bar of %d/%d takes one chord", bar,
                                      meter->beats_per_bar, meter->beat_unit)
                           : api_fail(error, COMPOSER_ERR_REQUEST, path,
                                      "chords bar %ld: in %d/%d a chord starts only every %ld beats (@%ld)", bar,
                                      meter->beats_per_bar, meter->beat_unit, beats, beats);
            }
            if (rational_cmp(chord->onset, bar_length) >= 0) {
                return api_fail(error, COMPOSER_ERR_REQUEST, path, "chords bar %ld: a chord starts inside the bar", bar);
            }
            if (chord->alteration != 0) {
                return api_fail(error, COMPOSER_ERR_REQUEST, path,
                                "chords bar %ld: altered chords are not given; write a diatonic numeral", bar);
            }
            HarmonyChord harmony;
            memset(&harmony, 0, sizeof(harmony));
            harmony.degree = chord->degree;
            harmony.inversion = chord->inversion;
            harmony.seventh = chord->seventh;
            size_t at = material->chord_count;
            if (!harmony_roman_format(key->mode, &harmony, material->chord_numerals[at])) {
                return api_fail(error, COMPOSER_ERR_REQUEST, path, "chords bar %ld: that chord does not exist in the key", bar);
            }
            if (c + 1 < count) {
                end = parsed[c + 1].onset;
            }
            GivenChord *given = &material->chords[at];
            given->numeral = material->chord_numerals[at];
            if (rational_add(bar_start, chord->onset, &given->onset) != RATIONAL_OK ||
                rational_sub(end, chord->onset, &given->length) != RATIONAL_OK) {
                return api_fail(error, COMPOSER_ERR_REQUEST, path, "chords bar %ld: the chord has no length", bar);
            }
            material->chord_count++;
        }
    }
    return COMPOSER_OK;
}

/*
 * A basic motif as long as the presentation or the antecedent fills that part whole: it leaves no contrasting or repeating
 * idea, so a contrasting slot or the motive of that idea is refused where it is given.
 */
static ComposerStatus refuse_idea_beside_part(const GrowMaterial *material, const ThemeRequest *theme,
                                              const GrowSetup *setup, ComposerError *error) {
    Motive basic;
    MotiveRelationStep step;
    if (!grow_part_basic_idea(&material->basic, grow_bar_length(&setup->meter), setup->bars / 2, setup->bars == 4 ? 1 : 2,
                              setup->pairing.beginning, &basic, &step)) {
        return COMPOSER_OK;
    }
    const char *part = setup->pairing.beginning == PLAN_THEME_BEGINNING_ANTECEDENT ? "antecedent" : "presentation";
    char path[96];
    if (theme->slot_motive[GROW_SLOT_CONTRASTING]) {
        return api_fail(error, COMPOSER_ERR_REQUEST, "/slots/contrasting",
                        "the basic motif fills the whole %s; leave the contrasting idea out", part);
    }
    for (int idea = GROW_IDEA_BASIC + 1; idea < GROW_IDEA_COUNT; idea++) {
        GrowPlace place;
        if (theme->idea_motive[idea] && grow_idea_place(&setup->pairing, (GrowIdea)idea, &place) &&
            place == GROW_PLACE_REPETITION) {
            snprintf(path, sizeof path, "/ideas/%.80s", grow_idea_name((GrowIdea)idea));
            return api_fail(error, COMPOSER_ERR_REQUEST, path,
                            "the basic motif fills the whole %s; leave the contrasting idea out", part);
        }
    }
    return COMPOSER_OK;
}

/* Fills the motives of the material once the key and the meter are known (the style has given the mode). */
static ComposerStatus load_material(GrowMaterial *material, const ThemeRequest *theme, const cJSON *request,
                                    const GrowSetup *setup, ComposerError *error) {
    char what[96];
    char path[48];
    ComposerStatus status;
    for (int s = 0; s < GROW_SLOT_COUNT; s++) {
        if (!theme->slot_motive[s]) {
            continue;
        }
        snprintf(what, sizeof(what), "motive '%s'", theme->slot_motive_id[s]);
        motive_path(path, sizeof path, theme->slot_motive_index[s]);
        Motive *target = s == GROW_SLOT_BASIC ? &material->basic : &material->slot_motives[s];
        status = api_read_degrees_motive(what, path, theme->slot_motive[s], NULL, &setup->meter, &setup->key, target, error);
        if (status != COMPOSER_OK) {
            return status;
        }
        if (target->accompaniment) {
            return api_fail(error, COMPOSER_ERR_REQUEST, path, "%s is of the accompaniment role; a slot takes a melodic motive",
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
            return api_fail(error, COMPOSER_ERR_REQUEST, path, "idea '%s' is not in this theme; the pairing has no %s",
                            grow_idea_name((GrowIdea)idea), grow_idea_name((GrowIdea)idea));
        }
        snprintf(what, sizeof(what), "motive '%s'", theme->idea_motive_id[idea]);
        status = api_read_degrees_motive(what, path, theme->idea_motive[idea], NULL, &setup->meter, &setup->key,
                                         &material->idea_motives[idea], error);
        if (status != COMPOSER_OK) {
            return status;
        }
        if (material->idea_motives[idea].accompaniment) {
            return api_fail(error, COMPOSER_ERR_REQUEST, path, "%s is of the accompaniment role; an idea takes a melodic motive",
                            what);
        }
        material->has_idea[idea] = true;
    }
    if (theme->accompaniment_motive) {
        status = api_read_degrees_motive("the accompaniment motive", "/accompaniment/motive", theme->accompaniment_motive,
                                         "accompaniment", &setup->meter, &setup->key, &material->accompaniment_motive, error);
        if (status != COMPOSER_OK) {
            return status;
        }
        material->has_accompaniment_motive = true;
    }
    status = refuse_idea_beside_part(material, theme, setup, error);
    return status != COMPOSER_OK ? status : read_chords(material, request, &setup->meter, &setup->key, error);
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
static ComposerStatus read_brief(ThemeRequest *theme, const cJSON *request, ThemeBrief *brief, ComposerError *error) {
    const char *into;
    const char *hybrid;
    const char *key;
    const char *mode;
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
        !api_request_string(request, "", "key", true, &key, error) || !api_request_string(request, "", "mode", true, &mode, error) ||
        !api_request_string(request, "", "meter", true, &meter, error) ||
        !api_request_string(request, "", "cadence", false, &cadence, error) ||
        !api_request_string(request, "", "first_cadence", false, &first_cadence, error) ||
        !api_request_string(request, "", "voicing", false, &voicing, error) ||
        !api_request_string(request, "", "style", false, &style, error) ||
        !api_request_string(request, "", "style_weight", false, &weight, error) ||
        !api_request_string(request, "", "blend", false, &brief->blend, error) ||
        !api_request_string(request, "", "blend_weight", false, &brief->blend_weight, error) ||
        !api_request_string(request, "", "blend_weights", false, &brief->blend_weights, error)) {
        return COMPOSER_ERR_REQUEST;
    }
    GrowBriefText *text = &brief->text;
    text->into = into;
    text->hybrid = hybrid;
    text->key = key;
    text->mode = mode;
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
            return api_fail(error, COMPOSER_ERR_REQUEST, "/bars", "\"bars\" in the request must be 4 or 8");
        }
        snprintf(brief->bars_text, sizeof(brief->bars_text), "%g", bars->valuedouble);
        text->bars = brief->bars_text;
    }
    const cJSON *accompaniment = cJSON_GetObjectItemCaseSensitive(request, "accompaniment");
    if (accompaniment && !cJSON_IsNull(accompaniment)) {
        const char *pattern;
        if (!cJSON_IsObject(accompaniment)) {
            return api_fail(error, COMPOSER_ERR_REQUEST, "/accompaniment", "\"accompaniment\" must be null or {\"pattern\": ...}");
        }
        if (!api_request_string(accompaniment, "/accompaniment", "pattern", true, &pattern, error)) {
            return COMPOSER_ERR_REQUEST;
        }
        if (strcmp(pattern, "motive") == 0) {
            theme->accompaniment_motive = cJSON_GetObjectItemCaseSensitive(accompaniment, "motive");
            if (!cJSON_IsObject(theme->accompaniment_motive)) {
                return api_fail(error, COMPOSER_ERR_REQUEST, "/accompaniment/motive",
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
            return api_fail(error, COMPOSER_ERR_REQUEST, "/voicing", "\"voicing\" in the request must be satb or chorale, not '%s'",
                            voicing);
        }
    }
    const cJSON *without = cJSON_GetObjectItemCaseSensitive(request, "without");
    if (without && !cJSON_IsNull(without) && !(cJSON_IsArray(without) && cJSON_GetArraySize(without) == 0)) {
        if (!style) {
            return api_fail(error, COMPOSER_ERR_REQUEST, "/without", "\"without\" in the request needs a \"style\"");
        }
        if (!cJSON_IsArray(without)) {
            return api_fail(error, COMPOSER_ERR_REQUEST, "/without", "\"without\" in the request must be a list of categories");
        }
        const cJSON *category = NULL;
        cJSON_ArrayForEach(category, without) {
            if (!cJSON_IsString(category)) {
                return api_fail(error, COMPOSER_ERR_REQUEST, "/without", "\"without\" in the request must be a list of categories");
            }
            bool known = false;
            for (size_t i = 0; i < text->without_count; i++) {
                known = known || strcmp(brief->without[i], category->valuestring) == 0;
            }
            if (known) {
                continue;
            }
            if (text->without_count >= WITHOUT_MAX) {
                return api_fail(error, COMPOSER_ERR_REQUEST, "/without", "\"without\" names at most %d style categories",
                                WITHOUT_MAX);
            }
            brief->without[text->without_count++] = category->valuestring;
        }
    }
    return COMPOSER_OK;
}

/* --- the outputs --------------------------------------------------------------------------------------- */

/* Adds, for each theme of the document, its plan (with "plans") and its explanation (with "explain"). */
static ComposerStatus add_theme_members(cJSON *document, const GrowRun *run, GrowDevelopment setup_development,
                                        const ApiOutputOptions *outputs, ComposerError *error) {
    cJSON *themes = cJSON_GetObjectItemCaseSensitive(document, "themes");
    for (size_t k = 0; k < run->made; k++) {
        cJSON *item = cJSON_GetArrayItem(themes, (int)k);
        const GrowTheme *theme = &run->themes[k];
        if (outputs->plans) {
            PlanJsonError detail;
            char *text = plan_to_json(theme->plan, &detail);
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
    return COMPOSER_OK;
}

/* The plan, the MIDI file and the MusicXML of the first theme. */
static ComposerStatus render_first(const GrowTheme *theme, const ApiOutputOptions *outputs, ComposerResponse *out) {
    PlanJsonError json_error;
    out->plan_json = plan_to_json(theme->plan, &json_error);
    if (!out->plan_json) {
        return api_fail(&out->error, COMPOSER_ERR_INTERNAL, "", "cannot write the plan: %s", json_error.message);
    }
    if (!outputs->midi && !outputs->musicxml) {
        return COMPOSER_OK;
    }
    RenderedPlan rendered;
    PlanExportError detail;
    if (plan_export_render(theme->plan, &rendered, &detail) != PLAN_EXPORT_OK) {
        return api_fail(&out->error, COMPOSER_ERR_REQUEST, outputs->midi ? "/midi" : "/musicxml", "cannot render the plan");
    }
    ComposerStatus status = COMPOSER_OK;
    if (outputs->midi &&
        rendered_plan_midi_bytes(&rendered, &outputs->export_options, &out->midi, &out->midi_size, &detail) != PLAN_EXPORT_OK) {
        status = api_fail(&out->error, COMPOSER_ERR_REQUEST, "/midi", "cannot write the MIDI file: %s", detail.message);
    }
    if (status == COMPOSER_OK && outputs->musicxml &&
        rendered_plan_musicxml_text(&rendered, &outputs->export_options, &out->musicxml, &detail) != PLAN_EXPORT_OK) {
        status = api_fail(&out->error, COMPOSER_ERR_REQUEST, "/musicxml", "cannot write the MusicXML: %s", detail.message);
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

static ComposerStatus theme_prepare(ThemeState *state, const ComposerJob *job, ApiOutputOptions *outputs, ComposerError *error) {
    const cJSON *request = state->request;
    const char *schema;
    const cJSON *version = cJSON_GetObjectItemCaseSensitive(request, "schema_version");
    if (!api_request_string(request, "", "schema", true, &schema, error)) {
        return COMPOSER_ERR_REQUEST;
    }
    if (strcmp(schema, SCHEMA_NAME) != 0) {
        return api_fail(error, COMPOSER_ERR_REQUEST, "/schema", "the request must have \"schema\": \"%s\"", SCHEMA_NAME);
    }
    if (!cJSON_IsNumber(version) || version->valuedouble != 2) {
        return api_fail(error, COMPOSER_ERR_REQUEST, "/schema_version",
                        "the request must have \"schema_version\": 2 (see docs/theme-request.md)");
    }
    ComposerStatus status = read_motives(&state->theme, request, error);
    if (status == COMPOSER_OK) {
        status = read_brief(&state->theme, request, &state->brief, error);
    }
    if (status == COMPOSER_OK) {
        status = api_read_output_options(request, outputs, error);
    }
    if (status != COMPOSER_OK) {
        return status;
    }

    GrowSetup *setup = &state->setup;
    bool present;
    bool check = false;
    unsigned long seed = 0;
    unsigned long suggest = 1;
    if (!api_member_bool(request, "check", &check, &present, error) ||
        !api_member_whole(request, "seed", 0, GROW_SEED_MAX, &seed, &present, error)) {
        return COMPOSER_ERR_REQUEST;
    }
    setup->seed = present ? seed : api_default_seed();
    if (!api_member_whole(request, "suggest", 1, GROW_MAX_SUGGESTIONS, &suggest, &present, error)) {
        return COMPOSER_ERR_REQUEST;
    }
    setup->suggest = suggest;
    setup->suggested = present;
    setup->check = check;
    setup->explain = outputs->explain != 0;
    setup->job = job;

    /*
     * The settings of the melody and of the development's fidelity minimums. The motif group is accepted, echoed and has
     * no effect: a theme's motives come from the request (docs/theme-request.md).
     */
    if (!api_read_preferences(request, &state->preferences, &state->has_preferences, error)) {
        return COMPOSER_ERR_REQUEST;
    }
    if (state->has_preferences) {
        setup->melody = &state->preferences.melody;
        setup->fidelity = &state->preferences.development;
        setup->continuation = &state->preferences.continuation;
        setup->basic_as_written = state->preferences.basic_as_written;
    }

    status = api_grow_read_brief(&state->brief.text, setup, error);
    if (status != COMPOSER_OK) {
        return status;
    }
    ComposerStyleWeights weights;
    memset(&weights, 0, sizeof(weights));
    status = api_read_blend(state->brief.style != NULL, state->brief.blend, state->brief.blend_weight,
                            state->brief.blend_weights, &weights, error);
    if (status != COMPOSER_OK) {
        return status;
    }
    /* The tempo of the request wins over the style's, and is the tempo of the files. */
    PlanExportOptions tempo_options = plan_export_options_default();
    status = api_read_tempo(request, &tempo_options, error);
    if (status != COMPOSER_OK) {
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
        if (status != COMPOSER_OK) {
            return status;
        }
    }
    setup->style = state->style;
    setup->without = state->brief.without;
    setup->without_count = state->brief.text.without_count;
    ComposerRatio option_tempo = {outputs->export_options.tempo_numerator, outputs->export_options.tempo_denominator};
    status = api_grow_set_tempo_and_mode(setup, has_tempo ? &option_tempo : NULL, error);
    if (status != COMPOSER_OK) {
        return status;
    }
    if (!has_tempo && setup->tempo.denominator > 0) {
        outputs->export_options.tempo_numerator = setup->tempo.numerator;
        outputs->export_options.tempo_denominator = setup->tempo.denominator;
    }

    state->material = calloc(1, sizeof(*state->material));
    if (!state->material) {
        return api_fail_alloc(error);
    }
    status = load_material(state->material, &state->theme, request, setup, error);
    if (status != COMPOSER_OK) {
        return status;
    }
    setup->motive = &state->material->basic;
    setup->accompaniment_motive = state->material->has_accompaniment_motive ? &state->material->accompaniment_motive : NULL;
    setup->material = state->material;
    return COMPOSER_OK;
}

ComposerStatus composer_api_theme(const char *request_json, const ComposerJob *job, ComposerResponse *out) {
    memset(out, 0, sizeof *out);
    ThemeState state;
    memset(&state, 0, sizeof(state));
    ApiOutputOptions outputs;
    ComposerStatus status = api_parse_request(request_json, &state.request, &out->error);
    if (status == COMPOSER_OK) {
        status = theme_prepare(&state, job, &outputs, &out->error);
    }
    if (status == COMPOSER_OK) {
        state.ran = true;
        status = api_grow_run(&state.setup, &state.run, &out->error);
    }
    if (status == COMPOSER_OK) {
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
        status = document ? add_theme_members(document, &state.run, state.setup.development, &outputs, &out->error) : api_fail_alloc(&out->error);
        if (status == COMPOSER_OK && state.has_preferences &&
            !cJSON_AddItemToObject(document, "preferences", composer_preferences_write(&state.preferences))) {
            status = api_fail_alloc(&out->error);
        }
        if (status == COMPOSER_OK) {
            status = render_first(&state.run.themes[0], &outputs, out);
        }
        if (status == COMPOSER_OK) {
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
    if (status != COMPOSER_OK) {
        ComposerError error = out->error;
        composer_response_destroy(out);
        out->error = error;
    }
    return status;
}

ComposerStatus api_theme_validate(const cJSON *request, ComposerError *error) {
    ThemeState state;
    memset(&state, 0, sizeof(state));
    ApiOutputOptions outputs;
    memset(&outputs, 0, sizeof(outputs));
    state.request = cJSON_Duplicate(request, true);
    ComposerStatus status = state.request ? theme_prepare(&state, NULL, &outputs, error) : api_fail_alloc(error);
    if (status == COMPOSER_OK) {
        /* Build the plan as Compose would, without seeding: the first refusal is the answer. */
        GrowRequest grow;
        PiecePlan *plan = NULL;
        GrowRefusal refusal;
        api_grow_request_for(&state.setup, state.setup.seed, &grow);
        GrowResult built = grow_build_plan_checked(&grow, &plan, &refusal);
        if (built == GROW_OK) {
            plan_destroy(plan);
        } else if (built == GROW_ERR_ALLOC) {
            status = api_fail_alloc(error);
        } else {
            status = api_fail(error, COMPOSER_ERR_UNSATISFIABLE, refusal.path, "%s", refusal.message);
        }
    }
    free(state.material);
    composer_style_destroy(state.style);
    cJSON_Delete(state.request);
    return status;
}
