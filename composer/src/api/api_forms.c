#include <theory/harmony/harmony_cadence_kinds.h>
#include <stdio.h>
#include <string.h>

#include <plan/section_json.h>

#include <ursatz_composer/composer_api.h>
#include <ursatz_composer/form_spec.h>
#include <ursatz_composer/theme_shape.h>

#include "api_construction.h"
#include "api_internal.h"
#include "api_piece.h"
#include "api_vocabulary.h"
#include "structure/plan_util.h"

/*
 * composer_api_forms (R-428): the built-in forms as the Pieces view needs them: the sections of every size with their roles,
 * lengths, cadences, keys and the slots of their themes, and what a form whose sections the request gives (the game loop)
 * allows. Everything comes from the parsed documents and the vocabulary tables; nothing is written again here.
 */

/* The lanes a layers mask names, in the order the form reader lists them. */
static const PlanLaneKind LAYER_LANES[] = {PLAN_LANE_BASS, PLAN_LANE_INNER_VOICE, PLAN_LANE_MELODY};

/* Adds {"value", "label"} under name. */
static bool add_choice_member(cJSON *object, const char *name, const char *value, const char *label) {
    cJSON *choice = cJSON_CreateObject();
    if (!choice || !cJSON_AddStringToObject(choice, "value", value) || !cJSON_AddStringToObject(choice, "label", label)) {
        cJSON_Delete(choice);
        return false;
    }
    return api_add(object, name, choice);
}

static bool add_string_or_null(cJSON *object, const char *name, const char *text) {
    return api_add(object, name, text ? cJSON_CreateString(text) : cJSON_CreateNull());
}

static bool add_number_or_null(cJSON *object, const char *name, size_t value) {
    return api_add(object, name, value > 0 ? cJSON_CreateNumber((double)value) : cJSON_CreateNull());
}

static bool append_string(cJSON *list, const char *text) {
    cJSON *item = cJSON_CreateString(text);
    if (!item || !cJSON_AddItemToArray(list, item)) {
        cJSON_Delete(item);
        return false;
    }
    return true;
}

/* The name of the key a section starts or ends in: "home", or the region's. */
static const char *key_name(const FormSpec *form, int key) {
    return key == FORM_SPEC_HOME ? "home" : form->regions[key].name;
}

/* The slots of a theme with this pairing, each with the ideas the pairing's beginning and ending give it. */
static bool add_slots(cJSON *section, const SectionPairing *pairing) {
    cJSON *slots = cJSON_AddArrayToObject(section, "slots");
    if (!slots) {
        return false;
    }
    for (int slot = 0; slot < GROW_SLOT_COUNT; slot++) {
        cJSON *entry = NULL;
        cJSON *ideas = NULL;
        for (size_t i = 0; i < theme_shape_idea_count(); i++) {
            const ThemeShapeIdea *row = theme_shape_idea_at(i);
            int half = row->in_beginning ? (int)pairing->beginning : (int)pairing->ending;
            if ((int)row->slot != slot || row->half != half) {
                continue;
            }
            if (!entry) {
                entry = cJSON_CreateObject();
                if (!entry || !cJSON_AddStringToObject(entry, "slot", theme_shape_slot((GrowSlot)slot)->name) ||
                    !(ideas = cJSON_AddArrayToObject(entry, "ideas")) || !cJSON_AddItemToArray(slots, entry)) {
                    cJSON_Delete(entry);
                    return false;
                }
            }
            if (!append_string(ideas, row->name)) {
                return false;
            }
        }
    }
    return true;
}

static bool add_layers(cJSON *section, unsigned mask) {
    if (mask == 0) {
        return api_add(section, "layers", cJSON_CreateString("all"));
    }
    cJSON *list = cJSON_AddArrayToObject(section, "layers");
    for (size_t i = 0; list && i < sizeof LAYER_LANES / sizeof LAYER_LANES[0]; i++) {
        if ((mask & PLAN_LAYER_BIT(LAYER_LANES[i])) && !append_string(list, plan_lane_name(LAYER_LANES[i]))) {
            return false;
        }
    }
    return list != NULL;
}

/*
 * The construction of a section that has one (R-468), served as a theme's response gives it (api_grow_construction_json),
 * or "auto" when the Composer chooses it with the piece's motive. A section without one has no member.
 */
static bool add_construction(cJSON *section, const SectionDescription *spec) {
    if (!spec->has_construction) {
        return true;
    }
    if (spec->construction_auto) {
        return cJSON_AddStringToObject(section, "construction", "auto") != NULL;
    }
    char text[192];
    grow_construction_describe(&spec->construction, &spec->pairing, text, sizeof text);
    return api_add(section, "construction", api_grow_construction_json(&spec->construction, &spec->pairing, text));
}

/* The motive of variation of a figurate return (R-497): null for no figure, else {"kind", "rhythm", "harmony", "change_at", "climax"}. */
static bool add_figure(cJSON *section, const SectionDescription *spec) {
    if (!spec->has_figure) {
        return api_add(section, "figure", cJSON_CreateNull());
    }
    cJSON *figure = cJSON_CreateObject();
    if (!figure || !cJSON_AddStringToObject(figure, "kind", section_figure_kind_name(spec->figure.kind)) ||
        !cJSON_AddStringToObject(figure, "rhythm", section_figure_rhythm_name(spec->figure.rhythm)) ||
        !cJSON_AddStringToObject(figure, "harmony", spec->figure.reharmonize ? "reharmonized" : "kept") ||
        !api_add(figure, "change_at", spec->figure.change_at > 0 ? cJSON_CreateNumber((double)spec->figure.change_at) : cJSON_CreateNull()) ||
        !cJSON_AddBoolToObject(figure, "climax", spec->figure.climax)) {
        cJSON_Delete(figure);
        return false;
    }
    return api_add(section, "figure", figure);
}

/* A member of the codec's object copied under its name, or null when the codec left it out (it holds only what is set). */
static bool copy_or_null(cJSON *to, const cJSON *base, const char *name) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(base, name);
    return api_add(to, name, item ? cJSON_Duplicate(item, true) : cJSON_CreateNull());
}

/* The label of a choice member: the vocabulary's word for the value the codec wrote. */
typedef struct {
    const char *member;
    const char *(*label)(const SectionDescription *spec);
} ChoiceRow;

static const char *role_label(const SectionDescription *spec) {
    return section_role_label(spec->role);
}

static const char *beginning_label(const SectionDescription *spec) {
    return API_THEME_BEGINNINGS[spec->pairing.beginning].label;
}

static const char *ending_label(const SectionDescription *spec) {
    return API_THEME_ENDINGS[spec->pairing.ending].label;
}

static const char *pedal_label(const SectionDescription *spec) {
    (void)spec;
    return "Dominant pedal";
}

static const char *connective_label(const SectionDescription *spec) {
    return spec->connective == SECTION_CONNECTIVE_ALWAYS ? "Always" : "Sometimes";
}

/* The members served as {"value", "label"}, or null when the section has none: a liquidation, retransition and coda have no beginning or ending. */
static const ChoiceRow CHOICES[] = {{"role", role_label},       {"beginning", beginning_label}, {"ending", ending_label},
                                    {"pedal", pedal_label},     {"connective", connective_label}};

static bool add_choice_row(cJSON *section, const cJSON *base, const SectionDescription *spec, const ChoiceRow *row) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(base, row->member);
    if (!cJSON_IsString(item)) {
        return api_add(section, row->member, cJSON_CreateNull());
    }
    return add_choice_member(section, row->member, item->valuestring, row->label(spec));
}

static const ChoiceRow *choice_row(const char *member) {
    for (size_t i = 0; i < sizeof(CHOICES) / sizeof(CHOICES[0]); i++) {
        if (strcmp(CHOICES[i].member, member) == 0) {
            return &CHOICES[i];
        }
    }
    return NULL;
}

/* The key a section starts or ends in, "home" when the codec left it out. */
static bool add_key_member(cJSON *keys, const cJSON *base_keys, const char *name) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(base_keys, name);
    return cJSON_AddStringToObject(keys, name, cJSON_IsString(item) ? item->valuestring : "home") != NULL;
}

/* The text of a member of a section object, or "" when it is not text. */
static const char *text_of(const cJSON *object, const char *name) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);
    return cJSON_IsString(item) ? item->valuestring : "";
}

static bool flag_of(const cJSON *object, const char *name) {
    return cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(object, name));
}

/* The sentence of a section's figure: the kind and the rhythm, then reharmonized, the root change at a bar and the climax as they apply, and the
   parallel mode when the section is in it; "" for a section with no figure. */
static void figure_label(const cJSON *section, char *out, size_t size) {
    const cJSON *figure = cJSON_GetObjectItemCaseSensitive(section, "figure");
    out[0] = '\0';
    if (!cJSON_IsObject(figure)) {
        return;
    }
    const char *kind = text_of(figure, "kind");
    const char *rhythm = text_of(figure, "rhythm");
    size_t used = (size_t)snprintf(out, size, "%s", strcmp(kind, "neighbor") == 0 ? "neighbor tones" : strcmp(kind, "scale") == 0 ? "scale line" : strcmp(kind, "broken-chord") == 0 ? "broken chord" : kind);
    if (rhythm[0] != '\0' && used < size) {
        used += (size_t)snprintf(out + used, size - used, " in %s", rhythm);
    }
    const cJSON *change = cJSON_GetObjectItemCaseSensitive(figure, "change_at");
    if (strcmp(text_of(figure, "harmony"), "reharmonized") == 0 && used < size) {
        used += (size_t)snprintf(out + used, size - used, ", reharmonized");
    }
    if (cJSON_IsNumber(change) && used < size) {
        used += (size_t)snprintf(out + used, size - used, ", root changed at bar %d", change->valueint);
    }
    if (flag_of(figure, "climax") && used < size) {
        used += (size_t)snprintf(out + used, size - used, ", climax");
    }
    if (strcmp(text_of(section, "mode"), "parallel") == 0 && used < size) {
        snprintf(out + used, size - used, ", parallel mode");
    }
}

/*
 * The sentences a section is shown as (API 4.2): labels {role, return, keys, figure}. role is the role's label, then " (independent)" for an
 * independent transition and " (lyric)" for a lyric subordinate theme; return is "" for a section that returns none, else the returned name,
 * then " (<variation>)" or " (setting: <setting>)"; keys is "<start> to <end>" then " (tonic major)" or " (home mode)"; figure is the figure in words.
 */
static bool add_labels(cJSON *section) {
    cJSON *labels = cJSON_CreateObject();
    if (!labels || !cJSON_AddItemToObject(section, "labels", labels)) {
        cJSON_Delete(labels);
        return false;
    }
    char text[256];
    const cJSON *role = cJSON_GetObjectItemCaseSensitive(section, "role");
    snprintf(text, sizeof text, "%s%s%s", text_of(role, "label"), flag_of(section, "independent") ? " (independent)" : "",
             flag_of(section, "lyric") ? " (lyric)" : "");
    bool ok = cJSON_AddStringToObject(labels, "role", text) != NULL;
    const char *returns = text_of(section, "returns");
    const char *variation = text_of(section, "variation");
    const char *setting = text_of(section, "setting");
    if (returns[0] == '\0') {
        text[0] = '\0';
    } else if (variation[0] == '\0') {
        snprintf(text, sizeof text, "%s", returns);
    } else if (strcmp(variation, "setting") == 0 && setting[0] != '\0') {
        snprintf(text, sizeof text, "%s (setting: %s)", returns, setting);
    } else {
        snprintf(text, sizeof text, "%s (%s)", returns, variation);
    }
    ok = ok && cJSON_AddStringToObject(labels, "return", text);
    const cJSON *keys = cJSON_GetObjectItemCaseSensitive(section, "keys");
    const char *mode = text_of(section, "mode");
    snprintf(text, sizeof text, "%s to %s%s", text_of(keys, "start"), text_of(keys, "end"),
             strcmp(mode, "major") == 0 ? " (tonic major)" : strcmp(mode, "home") == 0 ? " (home mode)" : "");
    ok = ok && cJSON_AddStringToObject(labels, "keys", text);
    figure_label(section, text, sizeof text);
    return ok && cJSON_AddStringToObject(labels, "figure", text);
}

/*
 * A form's section as the Pieces view needs it: the members of the section codec (the document's own, written by
 * section_to_json for a form), then the choices as {"value", "label"} and the members that always have a value. bars and
 * beats are both there, null for the one the section does not give.
 */
static bool add_section(cJSON *list, const FormSpec *form, const FormSpecSize *size, const SectionDescription *spec) {
    cJSON *base = section_to_json(spec, SECTION_USE_FORM);
    cJSON *section = cJSON_CreateObject();
    if (!base || !section || !cJSON_AddItemToArray(list, section)) {
        cJSON_Delete(base);
        cJSON_Delete(section);
        return false;
    }
    const HarmonyCadenceKind *kind = harmony_cadence_kind_find(spec->cadence_name);
    cJSON *cadence = cJSON_CreateObject();
    if (!cadence || !cJSON_AddStringToObject(cadence, "value", spec->cadence_name) ||
        !cJSON_AddStringToObject(cadence, "label", kind ? kind->label : spec->cadence_name) ||
        !cJSON_AddStringToObject(cadence, "core", plan_cadence_name(spec->cadence))) {
        cJSON_Delete(cadence);
        cJSON_Delete(base);
        return false;
    }
    bool returns = form_spec_section_index(size, spec->returns) != FORM_SPEC_NO_RETURN;
    cJSON *keys = cJSON_CreateObject();
    const cJSON *base_keys = cJSON_GetObjectItemCaseSensitive(base, "keys");
    bool ok = keys && add_key_member(keys, base_keys, "start") && add_key_member(keys, base_keys, "end");
    ok = ok && copy_or_null(section, base, "name") && copy_or_null(section, base, "letter") &&
         add_choice_row(section, base, spec, choice_row("role")) && add_number_or_null(section, "bars", spec->bars) &&
         add_number_or_null(section, "beats", spec->beats) && add_choice_row(section, base, spec, choice_row("beginning")) &&
         add_choice_row(section, base, spec, choice_row("ending")) && api_add(section, "cadence", cadence) &&
         cJSON_AddBoolToObject(section, "closed", spec->closed) && api_add(section, "keys", keys) &&
         copy_or_null(section, base, "motive") && copy_or_null(section, base, "returns") &&
         add_string_or_null(section, "variation", returns ? section_return_variation_name(spec->variation) : NULL) &&
         copy_or_null(section, base, "setting");
    ok = ok && add_layers(section, spec->layers) && add_choice_row(section, base, spec, choice_row("pedal")) &&
         (spec->has_pairing ? add_slots(section, &spec->pairing) : api_add(section, "slots", cJSON_CreateNull())) &&
         add_construction(section, spec) && cJSON_AddBoolToObject(section, "leads_in", spec->leads_in) &&
         add_choice_row(section, base, spec, choice_row("connective")) && add_figure(section, spec) &&
         copy_or_null(section, base, "mode") && cJSON_AddBoolToObject(section, "independent", spec->function.independent) &&
         cJSON_AddBoolToObject(section, "lyric", spec->function.lyric) && add_labels(section);
    cJSON_Delete(base);
    (void)form;
    return ok;
}

static bool add_sizes(cJSON *form_json, const FormSpec *form) {
    cJSON *sizes = cJSON_AddArrayToObject(form_json, "sizes");
    for (size_t i = 0; sizes && i < form->size_count; i++) {
        const FormSpecSize *size = &form->sizes[i];
        cJSON *entry = cJSON_CreateObject();
        if (!entry || !cJSON_AddItemToArray(sizes, entry)) {
            cJSON_Delete(entry);
            return false;
        }
        cJSON *sections = NULL;
        if (!cJSON_AddNumberToObject(entry, "bars", (double)size->bars) ||
            !cJSON_AddNumberToObject(entry, "count", (double)size->count) ||
            !cJSON_AddBoolToObject(entry, "default", size->is_default) ||
            !(sections = cJSON_AddArrayToObject(entry, "sections"))) {
            return false;
        }
        for (size_t k = 0; k < size->section_count; k++) {
            if (!add_section(sections, form, size, &size->sections[k])) {
                return false;
            }
        }
        /* "3 variations, 32 bars" for a size with a count (the noun is the role of its last section), else "16 bars" */
        const cJSON *last = cJSON_GetArrayItem(sections, (int)size->section_count - 1);
        char noun[64] = "section";
        const char *role_text = text_of(cJSON_GetObjectItemCaseSensitive(last, "role"), "label");
        if (role_text[0] != '\0') {
            size_t n = 0;
            for (; role_text[n] && n < sizeof(noun) - 1; n++) {
                noun[n] = (char)((role_text[n] >= 'A' && role_text[n] <= 'Z') ? role_text[n] + 32 : role_text[n]);
            }
            noun[n] = '\0';
        }
        char label[128];
        if (size->count > 0) {
            snprintf(label, sizeof label, "%d %s%s, %d bars", (int)size->count, noun, size->count == 1 ? "" : "s", (int)size->bars);
        } else {
            snprintf(label, sizeof label, "%d bars", (int)size->bars);
        }
        if (!cJSON_AddStringToObject(entry, "label", label)) {
            return false;
        }
    }
    return sizes != NULL;
}

/* The members of a form whose sections the request gives; for a sized form they are [], [], null and null. */
static bool add_request_members(cJSON *form_json, const FormSpec *form) {
    cJSON *roles = cJSON_AddArrayToObject(form_json, "roles");
    cJSON *loop_roles = cJSON_AddArrayToObject(form_json, "loop_roles");
    if (!roles || !loop_roles) {
        return false;
    }
    if (!form->request_sections) {
        return cJSON_AddNullToObject(form_json, "cadences") && cJSON_AddNullToObject(form_json, "default_ending");
    }
    for (size_t i = 0; i < form->role_count; i++) {
        cJSON *role = cJSON_CreateObject();
        if (!role || !cJSON_AddStringToObject(role, "value", section_role_name(form->roles[i])) ||
            !cJSON_AddStringToObject(role, "label", section_role_label(form->roles[i])) ||
            !cJSON_AddBoolToObject(role, "required", form->roles[i] == SECTION_ROLE_BODY) || !cJSON_AddItemToArray(roles, role)) {
            cJSON_Delete(role);
            return false;
        }
    }
    for (size_t i = 0; i < form->loop_role_count; i++) {
        if (!append_string(loop_roles, section_role_name(form->loop_roles[i]))) {
            return false;
        }
    }
    cJSON *cadences = cJSON_CreateObject();
    cJSON *last = NULL;
    if (!cadences || !cJSON_AddStringToObject(cadences, "default", plan_cadence_name(form->default_cadence)) ||
        !(last = cJSON_AddArrayToObject(cadences, "last")) || !api_add(form_json, "cadences", cadences)) {
        if (!last) {
            cJSON_Delete(cadences);
        }
        return false;
    }
    for (size_t i = 0; i < form->last_cadence_count; i++) {
        const FormCadenceRule *rule = &form->last_cadences[i];
        cJSON *entry = cJSON_CreateObject();
        cJSON *names = NULL;
        if (!entry || !(names = cJSON_AddArrayToObject(entry, "roles")) || !cJSON_AddItemToArray(last, entry)) {
            cJSON_Delete(entry);
            return false;
        }
        for (size_t r = 0; r < rule->role_count; r++) {
            if (!append_string(names, section_role_name(rule->roles[r]))) {
                return false;
            }
        }
        if (!cJSON_AddStringToObject(entry, "cadence", plan_cadence_name(rule->cadence))) {
            return false;
        }
    }
    if (!form->has_default_ending) {
        return cJSON_AddNullToObject(form_json, "default_ending") != NULL;
    }
    const FormDefaultEnding *ending = &form->default_ending;
    cJSON *object = cJSON_CreateObject();
    if (!object || !cJSON_AddStringToObject(object, "id", ending->id) || !cJSON_AddStringToObject(object, "letter", ending->letter) ||
        !cJSON_AddStringToObject(object, "role", section_role_name(ending->role)) ||
        !cJSON_AddNumberToObject(object, "bars", (double)ending->bars) ||
        !cJSON_AddStringToObject(object, "cadence", plan_cadence_name(ending->cadence))) {
        cJSON_Delete(object);
        return false;
    }
    return api_add(form_json, "default_ending", object);
}

static bool add_versions(cJSON *form_json, const FormSpec *form) {
    const char *names[2];
    const char *labels[2];
    size_t count = api_piece_version_names(form, names, labels, 2);
    cJSON *versions = cJSON_AddArrayToObject(form_json, "versions");
    for (size_t i = 0; versions && i < count; i++) {
        cJSON *version = cJSON_CreateObject();
        if (!version || !cJSON_AddStringToObject(version, "value", names[i]) ||
            !cJSON_AddStringToObject(version, "label", labels[i]) ||
            !cJSON_AddBoolToObject(version, "loop", strcmp(names[i], "loop") == 0) ||
            (strcmp(names[i], "ending") == 0 && !cJSON_AddStringToObject(version, "after", "loop")) ||
            !cJSON_AddItemToArray(versions, version)) {
            cJSON_Delete(version);
            return false;
        }
    }
    return versions != NULL;
}

static bool add_settings(cJSON *form_json, const FormSpec *form) {
    cJSON *settings = cJSON_CreateObject();
    if (!settings || !cJSON_AddBoolToObject(settings, "modulate", form->region_count > 0 && !form->modulates) ||
        !cJSON_AddStringToObject(settings, "accompaniment", form->accompaniment_required ? "required" : "optional") ||
        !cJSON_AddBoolToObject(settings, "voicing", form->request_sections) ||
        !cJSON_AddBoolToObject(settings, "section_fills", form->request_sections)) {
        cJSON_Delete(settings);
        return false;
    }
    return api_add(form_json, "settings", settings);
}

/* The meters a piece of the form may be in, as "3/4" ([] for any), and its tempo (null for none). */
static bool add_meters_and_tempo(cJSON *form_json, const FormSpec *form) {
    cJSON *meters = cJSON_AddArrayToObject(form_json, "meters");
    for (size_t i = 0; meters && i < form->meter_count; i++) {
        char text[24];
        snprintf(text, sizeof text, "%d/%d", form->meters[i].beats_per_bar, form->meters[i].beat_unit);
        if (!append_string(meters, text)) {
            return false;
        }
    }
    return meters && api_add(form_json, "tempo", form->tempo > 0 ? cJSON_CreateNumber((double)form->tempo) : cJSON_CreateNull());
}

/* The groups of sections a form writes out twice, by name (the sections are the same in every size). */
static bool add_repeats(cJSON *form_json, const FormSpec *form) {
    cJSON *groups = cJSON_AddArrayToObject(form_json, "repeats");
    const FormSpecSize *size = form->size_count > 0 ? &form->sizes[0] : NULL;
    for (size_t i = 0; groups && i < form->repeat_count; i++) {
        cJSON *group = cJSON_CreateArray();
        if (!group || !cJSON_AddItemToArray(groups, group)) {
            cJSON_Delete(group);
            return false;
        }
        for (size_t k = 0; size && k < form->repeats[i].count; k++) {
            size_t index = form->repeats[i].first + k;
            if (index >= size->section_count || !append_string(group, size->sections[index].name)) {
                return false;
            }
        }
    }
    return groups != NULL;
}

/* The parts of a compound form: null, or [{name, form {value, label} or null, key, motive, again, repeats, bars}]. */
static bool add_parts(cJSON *form_json, const FormSpec *form) {
    if (form->part_count == 0) {
        return api_add(form_json, "parts", cJSON_CreateNull());
    }
    cJSON *parts = cJSON_AddArrayToObject(form_json, "parts");
    for (size_t i = 0; parts && i < form->part_count; i++) {
        const FormPart *part = &form->parts[i];
        cJSON *entry = cJSON_CreateObject();
        if (!entry || !cJSON_AddItemToArray(parts, entry)) {
            cJSON_Delete(entry);
            return false;
        }
        if (!cJSON_AddStringToObject(entry, "name", part->name)) {
            return false;
        }
        FormSpec *built = NULL;
        bool has_form = part->form[0] != '\0' && form_spec_builtin(part->form, &built) == FORM_SPEC_OK;
        bool ok = has_form ? add_choice_member(entry, "form", part->form, built->label) : api_add(entry, "form", cJSON_CreateNull());
        form_spec_destroy(built);
        bool again = part->again != FORM_SPEC_NO_RETURN && part->again < form->part_count;
        if (!ok || !cJSON_AddStringToObject(entry, "key", key_name(form, part->key)) ||
            !cJSON_AddStringToObject(entry, "motive", part->new_motive ? "new" : "piece") ||
            !add_string_or_null(entry, "again", again ? form->parts[part->again].name : NULL) ||
            !cJSON_AddBoolToObject(entry, "repeats", part->repeats) ||
            !api_add(entry, "bars", part->bars > 0 ? cJSON_CreateNumber((double)part->bars) : cJSON_CreateNull())) {
            return false;
        }
    }
    return parts != NULL;
}

static bool add_regions(cJSON *form_json, const FormSpec *form) {
    cJSON *regions = cJSON_AddArrayToObject(form_json, "regions");
    for (size_t i = 0; regions && i < form->region_count; i++) {
        if (!append_string(regions, form->regions[i].name)) {
            return false;
        }
    }
    return regions != NULL;
}

static bool add_form(cJSON *list, const FormSpec *form) {
    cJSON *entry = cJSON_CreateObject();
    if (!entry || !cJSON_AddItemToArray(list, entry)) {
        cJSON_Delete(entry);
        return false;
    }
    return cJSON_AddStringToObject(entry, "name", form->name) && cJSON_AddStringToObject(entry, "label", form->label) &&
           cJSON_AddBoolToObject(entry, "closing", form->closing) && cJSON_AddBoolToObject(entry, "modulates", form->modulates) &&
           cJSON_AddStringToObject(entry, "sections", form->request_sections ? "request" : "sizes") &&
           add_versions(entry, form) && add_settings(entry, form) && add_regions(entry, form) && add_meters_and_tempo(entry, form) &&
           add_repeats(entry, form) && add_parts(entry, form) && cJSON_AddBoolToObject(entry, "performed", form_spec_performed(form)) &&
           add_sizes(entry, form) && add_request_members(entry, form);
}

UrsatzStatus composer_api_forms(ComposerResponse *out) {
    memset(out, 0, sizeof *out);
    cJSON *root = cJSON_CreateObject();
    cJSON *list = root ? cJSON_AddArrayToObject(root, "forms") : NULL;
    if (!list) {
        cJSON_Delete(root);
        return api_fail_alloc(&out->error);
    }
    for (size_t i = 0; i < form_spec_builtin_count(); i++) {
        FormSpec *form = NULL;
        FormSpecResult result = form_spec_builtin(form_spec_builtin_name(i), &form);
        if (result != FORM_SPEC_OK) {
            cJSON_Delete(root);
            return result == FORM_SPEC_ERR_ALLOC
                       ? api_fail_alloc(&out->error)
                       : api_fail(&out->error, URSATZ_ERR_INTERNAL, "", "the built-in form '%s' does not parse",
                                  form_spec_builtin_name(i));
        }
        bool added = add_form(list, form);
        form_spec_destroy(form);
        if (!added) {
            cJSON_Delete(root);
            return api_fail_alloc(&out->error);
        }
    }
    return api_respond_json(out, root);
}
