#include <standard-harmony/harmony_cadence_kinds.h>
#include <stdio.h>
#include <string.h>

#include <ursatz_composer/composer_api.h>
#include <ursatz_composer/form_spec.h>
#include <ursatz_composer/theme_shape.h>

#include "api_grow.h"
#include "api_internal.h"
#include "api_piece.h"
#include "api_vocabulary.h"
#include "util/plan_util.h"

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

/* The degree a section's presentation sits over in the bass: only the dominant (5) is a choice (R-446). */
static bool add_pedal(cJSON *section, int pedal) {
    return pedal == 5 ? add_choice_member(section, "pedal", "dominant", "Dominant pedal")
                      : api_add(section, "pedal", cJSON_CreateNull());
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
static bool add_slots(cJSON *section, const PlanThemePairing *pairing) {
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

static bool add_keys(cJSON *section, const FormSpec *form, const FormSectionSpec *spec) {
    cJSON *keys = cJSON_CreateObject();
    if (!keys || !cJSON_AddStringToObject(keys, "start", key_name(form, spec->key_start)) ||
        !cJSON_AddStringToObject(keys, "end", key_name(form, spec->key_end))) {
        cJSON_Delete(keys);
        return false;
    }
    return api_add(section, "keys", keys);
}

/*
 * The construction of a section that has one (R-468), served as a theme's response gives it (api_grow_construction_json),
 * or "auto" when the Composer chooses it with the piece's motive. A section without one has no member.
 */
static bool add_construction(cJSON *section, const FormSectionSpec *spec) {
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

static bool add_section(cJSON *list, const FormSpec *form, const FormSpecSize *size, const FormSectionSpec *spec) {
    cJSON *section = cJSON_CreateObject();
    if (!section || !cJSON_AddItemToArray(list, section)) {
        cJSON_Delete(section);
        return false;
    }
    const ApiThemePart *beginning = &API_THEME_BEGINNINGS[spec->pairing.beginning];
    const ApiThemePart *ending = &API_THEME_ENDINGS[spec->pairing.ending];
    const HarmonyCadenceKind *kind = harmony_cadence_kind_find(spec->cadence_name);
    cJSON *cadence = cJSON_CreateObject();
    if (!cadence || !cJSON_AddStringToObject(cadence, "value", spec->cadence_name) ||
        !cJSON_AddStringToObject(cadence, "label", kind ? kind->label : spec->cadence_name) ||
        !cJSON_AddStringToObject(cadence, "core", plan_cadence_name(spec->cadence))) {
        cJSON_Delete(cadence);
        return false;
    }
    char motive[96];
    bool has_motive = spec->has_motive && motive_relation_step_text(&spec->motive, motive, sizeof motive);
    bool returns = spec->returns != FORM_SPEC_NO_RETURN && spec->returns < size->section_count;
    return cJSON_AddStringToObject(section, "name", spec->name) && cJSON_AddStringToObject(section, "letter", spec->letter) &&
           add_choice_member(section, "role", form_role_name(spec->role), form_role_label(spec->role)) &&
           add_number_or_null(section, "bars", spec->bars) && add_number_or_null(section, "beats", spec->beats) &&
           add_choice_member(section, "beginning", theme_shape_beginning(spec->pairing.beginning)->plan_name,
                             beginning->label) &&
           add_choice_member(section, "ending", theme_shape_ending(spec->pairing.ending)->plan_name, ending->label) &&
           api_add(section, "cadence", cadence) && cJSON_AddBoolToObject(section, "closed", spec->closed) &&
           add_keys(section, form, spec) && add_string_or_null(section, "motive", has_motive ? motive : NULL) &&
           add_string_or_null(section, "returns", returns ? size->sections[spec->returns].name : NULL) &&
           add_string_or_null(section, "variation", returns ? plan_return_variation_name(spec->variation) : NULL) &&
           add_layers(section, spec->layers) && add_pedal(section, spec->pedal) && add_slots(section, &spec->pairing) &&
           add_construction(section, spec);
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
        if (!role || !cJSON_AddStringToObject(role, "value", form_role_name(form->roles[i])) ||
            !cJSON_AddStringToObject(role, "label", form_role_label(form->roles[i])) ||
            !cJSON_AddBoolToObject(role, "required", form->roles[i] == FORM_ROLE_BODY) || !cJSON_AddItemToArray(roles, role)) {
            cJSON_Delete(role);
            return false;
        }
    }
    for (size_t i = 0; i < form->loop_role_count; i++) {
        if (!append_string(loop_roles, form_role_name(form->loop_roles[i]))) {
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
            if (!append_string(names, form_role_name(rule->roles[r]))) {
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
        !cJSON_AddStringToObject(object, "role", form_role_name(ending->role)) ||
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
    if (!settings || !cJSON_AddBoolToObject(settings, "modulate", form->region_count > 0) ||
        !cJSON_AddStringToObject(settings, "accompaniment", form->accompaniment_required ? "required" : "optional") ||
        !cJSON_AddBoolToObject(settings, "voicing", form->request_sections) ||
        !cJSON_AddBoolToObject(settings, "section_fills", form->request_sections)) {
        cJSON_Delete(settings);
        return false;
    }
    return api_add(form_json, "settings", settings);
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
           cJSON_AddBoolToObject(entry, "closing", form->closing) &&
           cJSON_AddStringToObject(entry, "sections", form->request_sections ? "request" : "sizes") &&
           add_versions(entry, form) && add_settings(entry, form) && add_regions(entry, form) && add_sizes(entry, form) &&
           add_request_members(entry, form);
}

ComposerStatus composer_api_forms(ComposerResponse *out) {
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
                       : api_fail(&out->error, COMPOSER_ERR_INTERNAL, "", "the built-in form '%s' does not parse",
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
