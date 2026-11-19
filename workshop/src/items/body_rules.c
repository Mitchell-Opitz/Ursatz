#include "store_internal.h"
#include "composition/composer_client.h"

/* --- body validation ----------------------------------------------------- */

#define LABEL_MAX_CHARS 8
#define NOTES_MAX_CHARS 4000
#define HARMONY_MAX_CHARS 200

UrsatzStatus store_invalid_body(UrsatzError *error, const char *message) {
    workshop_error_text(error, message);
    return workshop_fail(URSATZ_ERR_REQUEST, error ? message : "invalid body");
}

static bool in_set(const cJSON *value, const char *const *set) {
    if (!cJSON_IsString(value)) {
        return false;
    }
    for (; *set; set++) {
        if (strcmp(value->valuestring, *set) == 0) {
            return true;
        }
    }
    return false;
}

static bool is_whole_number(const cJSON *value, double minimum) {
    return cJSON_IsNumber(value) && value->valuedouble >= minimum && value->valuedouble == (double)(long long)value->valuedouble;
}

static size_t utf8_length(const char *text) {
    size_t chars = 0;
    for (; *text; text++) {
        if (((unsigned char)*text & 0xC0) != 0x80) {
            chars++;
        }
    }
    return chars;
}

/* A member that is absent or null. */
bool store_is_absent_or_null(const cJSON *object, const char *name) {
    const cJSON *value = cJSON_GetObjectItemCaseSensitive(object, name);
    return !value || cJSON_IsNull(value);
}

/* digits with an optional "/digits": "4/4" when meter is set (both parts 1 to 99), else "0", "1/2", "3". */
static bool is_rational(const char *text, bool meter) {
    const char *slash = strchr(text, '/');
    size_t left = slash ? (size_t)(slash - text) : strlen(text);
    size_t right = slash ? strlen(slash + 1) : 0;
    if (left == 0 || left > (meter ? 2 : 9) || (meter && !slash) || (slash && (right == 0 || right > (meter ? 2 : 9)))) {
        return false;
    }
    bool denominator_nonzero = !slash;
    for (const char *c = text; *c; c++) {
        if (*c == '/' && c == slash) {
            continue;
        }
        if (!isdigit((unsigned char)*c)) {
            return false;
        }
        if (slash && c > slash && *c != '0') {
            denominator_nonzero = true;
        }
    }
    if (meter && text[0] == '0') {
        return false;
    }
    return denominator_nonzero;
}

static UrsatzStatus validate_uses(const cJSON *body, UrsatzError *error) {
    const cJSON *uses = cJSON_GetObjectItemCaseSensitive(body, "uses");
    if (!cJSON_IsArray(uses)) {
        return store_invalid_body(error, "uses must be an array");
    }
    const cJSON *use = NULL;
    cJSON_ArrayForEach(use, uses) {
        const cJSON *item_id = cJSON_GetObjectItemCaseSensitive(use, "item_id");
        const cJSON *copy = cJSON_GetObjectItemCaseSensitive(use, "copy");
        if (!cJSON_IsObject(use) || !cJSON_IsString(item_id) || !store_is_valid_id(item_id->valuestring) ||
            !is_whole_number(cJSON_GetObjectItemCaseSensitive(use, "version"), 1) || (copy && !cJSON_IsObject(copy))) {
            return store_invalid_body(error, "each use needs item_id, version and, if present, an object copy");
        }
    }
    return URSATZ_OK;
}

static bool is_string_list_from(const cJSON *value, const char *const *set) {
    if (!cJSON_IsArray(value)) {
        return false;
    }
    const cJSON *entry = NULL;
    cJSON_ArrayForEach(entry, value) {
        if (!in_set(entry, set)) {
            return false;
        }
    }
    return true;
}

static UrsatzStatus validate_derived(const cJSON *derived, UrsatzError *error) {
    static const char *const kinds[] = {"exact", "motive-form", "variant", NULL};
    static const char *const dimensions[] = {"rhythm", "intervals", "harmony", "contour", NULL};
    static const char *const functions[] = {"beginning", "continuation", "contrast", "cadential", NULL};
    static const char *const flags[] = {"approved", "rejected", "unflagged", NULL};
    const cJSON *standalone = cJSON_GetObjectItemCaseSensitive(derived, "standalone");
    const cJSON *source_item = cJSON_GetObjectItemCaseSensitive(derived, "source_item");
    const cJSON *seed = cJSON_GetObjectItemCaseSensitive(derived, "seed");
    if (!cJSON_IsObject(derived)) {
        return store_invalid_body(error, "derived must be null or an object");
    }
    if (!cJSON_IsString(source_item) || !store_is_valid_id(source_item->valuestring) ||
        !is_whole_number(cJSON_GetObjectItemCaseSensitive(derived, "source_version"), 1)) {
        return store_invalid_body(error, "derived needs source_item and source_version");
    }
    if (!cJSON_IsArray(cJSON_GetObjectItemCaseSensitive(derived, "operations"))) {
        return store_invalid_body(error, "derived operations must be an array");
    }
    if (seed && !cJSON_IsNull(seed) && !is_whole_number(seed, 0)) {
        return store_invalid_body(error, "derived seed must be null or a whole number");
    }
    if (!in_set(cJSON_GetObjectItemCaseSensitive(derived, "kind"), kinds)) {
        return store_invalid_body(error, "derived kind must be exact, motive-form or variant");
    }
    if (!is_string_list_from(cJSON_GetObjectItemCaseSensitive(derived, "kept"), dimensions) ||
        !is_string_list_from(cJSON_GetObjectItemCaseSensitive(derived, "changed"), dimensions)) {
        return store_invalid_body(error, "derived kept and changed must list rhythm, intervals, harmony or contour");
    }
    if (!is_string_list_from(cJSON_GetObjectItemCaseSensitive(derived, "functions"), functions)) {
        return store_invalid_body(error, "derived functions must list beginning, continuation, contrast or cadential");
    }
    const cJSON *harmony = cJSON_GetObjectItemCaseSensitive(derived, "harmony");
    if (harmony && !cJSON_IsNull(harmony) &&
        (!cJSON_IsString(harmony) || utf8_length(harmony->valuestring) > HARMONY_MAX_CHARS)) {
        return store_invalid_body(error, "derived harmony must be null or a string of at most 200 characters");
    }
    const cJSON *remoteness = cJSON_GetObjectItemCaseSensitive(derived, "remoteness");
    if (remoteness && !cJSON_IsNull(remoteness) &&
        (!cJSON_IsNumber(remoteness) || remoteness->valuedouble < 0 || remoteness->valuedouble > 1)) {
        return store_invalid_body(error, "derived remoteness must be null or a number from 0 to 1");
    }
    if (!in_set(cJSON_GetObjectItemCaseSensitive(derived, "flag"), flags)) {
        return store_invalid_body(error, "derived flag must be approved, rejected or unflagged");
    }
    if (standalone && !cJSON_IsBool(standalone)) {
        return store_invalid_body(error, "derived standalone must be true or false");
    }
    return URSATZ_OK;
}

#define TONE_OVERRIDES_MAX 64
#define TONE_ROLE_MAX 24

/* motif.tone_overrides is absent, null or an object of "<note>": "<role>" (1-based note); the role is the Composer's
 * word (R-276), so a save keeps its type and size and validate marks a role the Composer does not read. */
static UrsatzStatus validate_tone_overrides(const cJSON *overrides, UrsatzError *error) {
    if (!overrides || cJSON_IsNull(overrides)) {
        return URSATZ_OK;
    }
    if (!cJSON_IsObject(overrides) || cJSON_GetArraySize(overrides) > TONE_OVERRIDES_MAX) {
        return store_invalid_body(error, "motif tone_overrides must be an object of at most 64 notes");
    }
    const cJSON *entry = NULL;
    cJSON_ArrayForEach(entry, overrides) {
        const char *name = entry->string ? entry->string : "";
        char *end = NULL;
        long note = strtol(name, &end, 10);
        if (name[0] == '\0' || *end != '\0' || name[0] == '+' || name[0] == '-' || note < 1 || note > TONE_OVERRIDES_MAX ||
            !cJSON_IsString(entry) || strlen(entry->valuestring) > TONE_ROLE_MAX) {
            return store_invalid_body(error,
                                "motif tone_overrides must map a note number from 1 to 64 to a tone type");
        }
    }
    return URSATZ_OK;
}

static UrsatzStatus validate_motif(const cJSON *motif, UrsatzError *error) {
    static const char *const roles[] = {"melodic", "accompaniment", NULL};
    static const char *const forms[] = {"full", "rhythm", "contour", NULL};
    if (!cJSON_IsObject(motif)) {
        return store_invalid_body(error, "a motif body needs a motif object");
    }
    if (!in_set(cJSON_GetObjectItemCaseSensitive(motif, "role"), roles)) {
        return store_invalid_body(error, "motif role must be melodic or accompaniment");
    }
    if (!in_set(cJSON_GetObjectItemCaseSensitive(motif, "form"), forms)) {
        return store_invalid_body(error, "motif form must be full, rhythm or contour");
    }
    const cJSON *notes = cJSON_GetObjectItemCaseSensitive(motif, "notes");
    if (!cJSON_IsString(notes) || notes->valuestring[0] == '\0' || utf8_length(notes->valuestring) > NOTES_MAX_CHARS) {
        return store_invalid_body(error, "motif notes must be a non-empty string of at most 4000 characters");
    }
    const cJSON *meter = cJSON_GetObjectItemCaseSensitive(motif, "meter");
    if (!cJSON_IsString(meter) || !is_rational(meter->valuestring, true)) {
        return store_invalid_body(error, "motif meter must look like 4/4");
    }
    /* A motif is in C: its material, or the old key text. */
    const cJSON *key = cJSON_GetObjectItemCaseSensitive(motif, "key");
    if (key && !cJSON_IsNull(key) && !cJSON_IsString(key)) {
        return store_invalid_body(error, "motif key must be null or a string");
    }
    const cJSON *material = cJSON_GetObjectItemCaseSensitive(motif, "material");
    if (material && !cJSON_IsNull(material) && !cJSON_IsString(material)) {
        return store_invalid_body(error, "motif material must be null or a string");
    }
    const cJSON *pickup = cJSON_GetObjectItemCaseSensitive(motif, "pickup");
    if (!cJSON_IsString(pickup) || !is_rational(pickup->valuestring, false)) {
        return store_invalid_body(error, "motif pickup must be beats as n or n/d");
    }
    const cJSON *features = cJSON_GetObjectItemCaseSensitive(motif, "features");
    if (!cJSON_IsArray(features)) {
        return store_invalid_body(error, "motif features must be an array");
    }
    const cJSON *feature = NULL;
    cJSON_ArrayForEach(feature, features) {
        const cJSON *label = cJSON_GetObjectItemCaseSensitive(feature, "label");
        const cJSON *first = cJSON_GetObjectItemCaseSensitive(feature, "first");
        const cJSON *last = cJSON_GetObjectItemCaseSensitive(feature, "last");
        if (!cJSON_IsObject(feature) || !cJSON_IsString(label) || utf8_length(label->valuestring) < 1 ||
            utf8_length(label->valuestring) > LABEL_MAX_CHARS || !is_whole_number(first, 0) ||
            !is_whole_number(last, 0) || first->valuedouble > last->valuedouble) {
            return store_invalid_body(error, "each feature needs a label of 1 to 8 characters and 0 <= first <= last");
        }
    }
    const cJSON *harmony = cJSON_GetObjectItemCaseSensitive(motif, "harmony");
    if (harmony && !cJSON_IsNull(harmony) &&
        (!cJSON_IsString(harmony) || utf8_length(harmony->valuestring) > HARMONY_MAX_CHARS)) {
        return store_invalid_body(error, "motif harmony must be null or a string of at most 200 characters");
    }
    UrsatzStatus tones = validate_tone_overrides(cJSON_GetObjectItemCaseSensitive(motif, "tone_overrides"), error);
    if (tones != URSATZ_OK) {
        return tones;
    }
    const cJSON *analysis = cJSON_GetObjectItemCaseSensitive(motif, "analysis");
    if (analysis && !cJSON_IsNull(analysis) && !cJSON_IsObject(analysis)) {
        return store_invalid_body(error, "motif analysis must be null or an object");
    }
    const cJSON *degrees = cJSON_GetObjectItemCaseSensitive(motif, "degrees");
    if (degrees && !cJSON_IsNull(degrees) &&
        (!cJSON_IsString(degrees) || utf8_length(degrees->valuestring) > NOTES_MAX_CHARS)) {
        return store_invalid_body(error, "motif degrees must be null or a string of at most 4000 characters");
    }
    return store_is_absent_or_null(motif, "derived")
               ? URSATZ_OK
               : validate_derived(cJSON_GetObjectItemCaseSensitive(motif, "derived"), error);
}

static const char *const kThemePools[] = {"approved-and-unflagged", "approved", "none", NULL};
static const char *const kThemeMotiveCategories[] = {"motive-degrees", "motive-intervals", "motive-rhythm",
                                                     "motive-nct-figure", NULL};
/* The members a placeholder theme (form null or a string, slots of anything) does not have. */
static const char *const kThemeNewMembers[] = {"ideas", "chords", "accompaniment", "brief", "style_weight",
                                               "without", "tempo", "check",         "seed",  "result", NULL};

#define THEME_BRIEF_MAX_SCOPES 16
#define THEME_BRIEF_MAX_PIECES 5000
#define THEME_BRIEF_MAX_STYLES 16
#define THEME_WITHOUT_MAX 4
#define THEME_IDEAS_MAX 10
#define THEME_CHORD_BARS 8
#define THEME_CHORD_TEXT_MAX 64

static bool is_string_or_null(const cJSON *object, const char *name) {
    const cJSON *value = cJSON_GetObjectItemCaseSensitive(object, name);
    return !value || cJSON_IsNull(value) || cJSON_IsString(value);
}

/* A member that is absent, null or one of set. */
static bool is_in_set_or_null(const cJSON *object, const char *name, const char *const *set) {
    return store_is_absent_or_null(object, name) || in_set(cJSON_GetObjectItemCaseSensitive(object, name), set);
}

#define THEME_FORM_WORD_MAX 40

/* The form words are the Composer's (R-276): a save keeps their type and size, and validate marks a value the Composer
 * does not read, as compose refuses it with its path. */
static UrsatzStatus validate_theme_form(const cJSON *form, UrsatzError *error) {
    static const char *const words[] = {"into", "hybrid", "material", "mode", "meter", "cadence", "first_cadence", NULL};
    for (const char *const *word = words; *word; word++) {
        const cJSON *value = cJSON_GetObjectItemCaseSensitive(form, *word);
        if (!is_string_or_null(form, *word) || (cJSON_IsString(value) && strlen(value->valuestring) > THEME_FORM_WORD_MAX)) {
            return store_invalid_body(error, "theme form into, hybrid, material, meter, cadence and first_cadence must be text");
        }
    }
    if (cJSON_GetObjectItemCaseSensitive(form, "key")) {
        return store_invalid_body(error, "themes are in C; remove key");
    }
    const cJSON *bars = cJSON_GetObjectItemCaseSensitive(form, "bars");
    if (!store_is_absent_or_null(form, "bars") && !cJSON_IsNumber(bars)) {
        return store_invalid_body(error, "theme form bars must be a number");
    }
    /* The construction (Composer API 1.18) is the Composer's: a save keeps only that it is an object or null. */
    const cJSON *construction = cJSON_GetObjectItemCaseSensitive(form, "construction");
    if (!store_is_absent_or_null(form, "construction") && !cJSON_IsObject(construction)) {
        return store_invalid_body(error, "theme form construction must be an object or null");
    }
    return URSATZ_OK;
}

/* {slot, item_id, version}: a reference to a version of another item. */
static bool theme_reference_ok(const cJSON *reference) {
    const cJSON *item_id = cJSON_GetObjectItemCaseSensitive(reference, "item_id");
    return cJSON_IsObject(reference) && cJSON_IsString(item_id) && store_is_valid_id(item_id->valuestring) &&
           is_whole_number(cJSON_GetObjectItemCaseSensitive(reference, "version"), 1);
}

static UrsatzStatus validate_theme_slots(const cJSON *slots, UrsatzError *error) {
    bool seen[THEME_IDEAS_MAX] = {false};
    const cJSON *slot = NULL;
    cJSON_ArrayForEach(slot, slots) {
        const cJSON *name = cJSON_GetObjectItemCaseSensitive(slot, "slot");
        int index = cJSON_IsString(name) ? workshop_composer_vocabulary_index("theme_slots", name->valuestring) : -1;
        if (index < 0 || index >= THEME_IDEAS_MAX || !theme_reference_ok(slot)) {
            return store_invalid_body(error, "each theme slot needs slot, item_id and version");
        }
        if (seen[index]) {
            return store_invalid_body(error, "each theme slot may be named once");
        }
        seen[index] = true;
    }
    int required = workshop_composer_vocabulary_required_index("theme_slots");
    if (required >= 0 && required < THEME_IDEAS_MAX && seen[required]) {
        return URSATZ_OK;
    }
    const cJSON *required_entry = cJSON_GetArrayItem(workshop_composer_vocabulary_list("theme_slots"), required);
    char text[96];
    snprintf(text, sizeof(text), "a theme needs a %s slot", common_json_string(required_entry, "value"));
    return store_invalid_body(error, text);
}

/* [{idea, item_id, version}]: a motif version for a single idea, each idea named once. */
static UrsatzStatus validate_theme_ideas(const cJSON *ideas, UrsatzError *error) {
    bool seen[THEME_IDEAS_MAX] = {false};
    if (!cJSON_IsArray(ideas) || cJSON_GetArraySize(ideas) > THEME_IDEAS_MAX) {
        return store_invalid_body(error, "theme ideas must be a list of at most 10");
    }
    const cJSON *entry = NULL;
    cJSON_ArrayForEach(entry, ideas) {
        const cJSON *name = cJSON_GetObjectItemCaseSensitive(entry, "idea");
        int index = cJSON_IsString(name) ? workshop_composer_vocabulary_index("theme_ideas", name->valuestring) : -1;
        if (index < 0 || index >= THEME_IDEAS_MAX || !theme_reference_ok(entry)) {
            return store_invalid_body(error, "each theme idea needs idea, item_id and version");
        }
        if (seen[index]) {
            return store_invalid_body(error, "each theme idea may be named once");
        }
        seen[index] = true;
    }
    return URSATZ_OK;
}

/* [{bar, text}]: the chords given for a bar, bar 0 to 7, each bar once; the Composer checks the syntax. */
static UrsatzStatus validate_theme_chords(const cJSON *chords, UrsatzError *error) {
    bool seen[THEME_CHORD_BARS] = {false};
    if (!cJSON_IsArray(chords) || cJSON_GetArraySize(chords) > THEME_CHORD_BARS) {
        return store_invalid_body(error, "theme chords must be a list of at most 8 bars");
    }
    const cJSON *entry = NULL;
    cJSON_ArrayForEach(entry, chords) {
        const cJSON *bar = cJSON_GetObjectItemCaseSensitive(entry, "bar");
        const cJSON *text = cJSON_GetObjectItemCaseSensitive(entry, "text");
        if (!cJSON_IsObject(entry) || !is_whole_number(bar, 0) || bar->valuedouble >= THEME_CHORD_BARS ||
            !cJSON_IsString(text) || utf8_length(text->valuestring) > THEME_CHORD_TEXT_MAX) {
            return store_invalid_body(error, "each theme chord needs a bar from 0 to 7 and a text of at most 64 characters");
        }
        if (seen[(int)bar->valuedouble]) {
            return store_invalid_body(error, "each theme chord bar may be named once");
        }
        seen[(int)bar->valuedouble] = true;
    }
    return URSATZ_OK;
}

static UrsatzStatus validate_theme_accompaniment(const cJSON *accompaniment, UrsatzError *error) {
    static const char *const patterns[] = {"block", "arpeggiated", "alberti", "oom-pah", "any", NULL};
    const cJSON *pattern = cJSON_GetObjectItemCaseSensitive(accompaniment, "pattern");
    if (cJSON_IsObject(accompaniment) && cJSON_IsString(pattern) && strcmp(pattern->valuestring, "motive") == 0) {
        return theme_reference_ok(accompaniment)
                   ? URSATZ_OK
                   : store_invalid_body(error, "a motive accompaniment needs item_id and version");
    }
    return cJSON_IsObject(accompaniment) && in_set(pattern, patterns)
               ? URSATZ_OK
               : store_invalid_body(error, "theme accompaniment pattern must be block, arpeggiated, alberti, oom-pah, any or motive");
}

static bool theme_id_list_ok(const cJSON *brief, const char *name, size_t max) {
    const cJSON *list = cJSON_GetObjectItemCaseSensitive(brief, name);
    if (!list) {
        return true;
    }
    if (!cJSON_IsArray(list) || (size_t)cJSON_GetArraySize(list) > max) {
        return false;
    }
    const cJSON *id = NULL;
    cJSON_ArrayForEach(id, list) {
        if (!cJSON_IsString(id) || !store_is_valid_id(id->valuestring)) {
            return false;
        }
    }
    return true;
}

/* {scopes, piece_ids, style_ids} as the styles route takes a brief: at most 16 scopes, 5000 pieces and 16 styles. */
static bool theme_brief_ok(const cJSON *brief) {
    static const char *const kinds[] = {"all", "collection", "filter", "subject", "project", NULL};
    const cJSON *scopes = cJSON_GetObjectItemCaseSensitive(brief, "scopes");
    if (!cJSON_IsObject(brief) || (scopes && (!cJSON_IsArray(scopes) || cJSON_GetArraySize(scopes) > THEME_BRIEF_MAX_SCOPES)) ||
        !theme_id_list_ok(brief, "piece_ids", THEME_BRIEF_MAX_PIECES) ||
        !theme_id_list_ok(brief, "style_ids", THEME_BRIEF_MAX_STYLES)) {
        return false;
    }
    const cJSON *scope = NULL;
    cJSON_ArrayForEach(scope, scopes) {
        const cJSON *filter = cJSON_GetObjectItemCaseSensitive(scope, "filter");
        if (!cJSON_IsObject(scope) || !in_set(cJSON_GetObjectItemCaseSensitive(scope, "kind"), kinds) ||
            !is_string_or_null(scope, "collection_id") || !is_string_or_null(scope, "subject_id") ||
            !is_string_or_null(scope, "project_id") ||
            (filter && !cJSON_IsNull(filter) &&
             (!cJSON_IsObject(filter) || !is_string_or_null(filter, "composer") || !is_string_or_null(filter, "source") ||
              !is_string_or_null(filter, "tag")))) {
            return false;
        }
    }
    int entries = (scopes ? cJSON_GetArraySize(scopes) : 0) +
                  cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(brief, "piece_ids")) +
                  cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(brief, "style_ids"));
    return entries > 0;
}

/* True when the theme is a placeholder: form null, absent or a string, no new member, and no slot written as an object with a slot name. */
bool store_theme_is_placeholder(const cJSON *theme) {
    const cJSON *form = cJSON_GetObjectItemCaseSensitive(theme, "form");
    if (cJSON_IsObject(form)) {
        return false;
    }
    for (const char *const *name = kThemeNewMembers; *name; name++) {
        if (cJSON_GetObjectItemCaseSensitive(theme, *name)) {
            return false;
        }
    }
    const cJSON *slot = NULL;
    cJSON_ArrayForEach(slot, cJSON_GetObjectItemCaseSensitive(theme, "slots")) {
        if (cJSON_IsObject(slot) && cJSON_GetObjectItemCaseSensitive(slot, "slot")) {
            return false;
        }
    }
    return true;
}

static UrsatzStatus validate_theme(const cJSON *theme, UrsatzError *error) {
    const cJSON *form = cJSON_GetObjectItemCaseSensitive(theme, "form");
    const cJSON *slots = cJSON_GetObjectItemCaseSensitive(theme, "slots");
    if (!cJSON_IsObject(theme) || (form && !cJSON_IsNull(form) && !cJSON_IsString(form) && !cJSON_IsObject(form)) ||
        !cJSON_IsArray(slots)) {
        return store_invalid_body(error, "a theme body needs a theme object with form and slots");
    }
    if (cJSON_GetObjectItemCaseSensitive(theme, "pool")) {
        return store_invalid_body(error, "themes have no pool");
    }
    if (store_theme_is_placeholder(theme)) {
        return URSATZ_OK;
    }
    UrsatzStatus result = cJSON_IsObject(form) ? validate_theme_form(form, error)
                                                          : URSATZ_OK;
    if (result == URSATZ_OK) {
        result = validate_theme_slots(slots, error);
    }
    if (result != URSATZ_OK) {
        return result;
    }
    const cJSON *ideas = cJSON_GetObjectItemCaseSensitive(theme, "ideas");
    if (ideas && validate_theme_ideas(ideas, error) != URSATZ_OK) {
        return workshop_fail(URSATZ_ERR_REQUEST, "invalid body");
    }
    const cJSON *chords = cJSON_GetObjectItemCaseSensitive(theme, "chords");
    if (chords && validate_theme_chords(chords, error) != URSATZ_OK) {
        return workshop_fail(URSATZ_ERR_REQUEST, "invalid body");
    }
    const cJSON *accompaniment = cJSON_GetObjectItemCaseSensitive(theme, "accompaniment");
    if (!store_is_absent_or_null(theme, "accompaniment") &&
        validate_theme_accompaniment(accompaniment, error) != URSATZ_OK) {
        return workshop_fail(URSATZ_ERR_REQUEST, "invalid body");
    }
    if (!store_is_absent_or_null(theme, "brief") && !theme_brief_ok(cJSON_GetObjectItemCaseSensitive(theme, "brief"))) {
        return store_invalid_body(error, "theme brief needs scopes, piece_ids or style_ids within the limits");
    }
    const cJSON *weight = cJSON_GetObjectItemCaseSensitive(theme, "style_weight");
    if (!store_is_absent_or_null(theme, "style_weight") &&
        (!cJSON_IsString(weight) || !is_rational(weight->valuestring, false))) {
        return store_invalid_body(error, "theme style_weight must be n or n/d");
    }
    const cJSON *without = cJSON_GetObjectItemCaseSensitive(theme, "without");
    if (without && (!is_string_list_from(without, kThemeMotiveCategories) ||
                    cJSON_GetArraySize(without) > THEME_WITHOUT_MAX)) {
        return store_invalid_body(error, "theme without lists up to four motive categories");
    }
    const cJSON *tempo = cJSON_GetObjectItemCaseSensitive(theme, "tempo");
    if (!store_is_absent_or_null(theme, "tempo") && !is_whole_number(tempo, 0)) {
        return store_invalid_body(error, "theme tempo must be null or a whole number");
    }
    const cJSON *check = cJSON_GetObjectItemCaseSensitive(theme, "check");
    if (check && !cJSON_IsBool(check)) {
        return store_invalid_body(error, "theme check must be true or false");
    }
    const cJSON *seed = cJSON_GetObjectItemCaseSensitive(theme, "seed");
    if (!store_is_absent_or_null(theme, "seed") && !is_whole_number(seed, 0)) {
        return store_invalid_body(error, "theme seed must be null or a whole number");
    }
    const cJSON *result_member = cJSON_GetObjectItemCaseSensitive(theme, "result");
    if (result_member && !cJSON_IsNull(result_member) && !cJSON_IsObject(result_member)) {
        return store_invalid_body(error, "theme result must be null or an object");
    }
    return URSATZ_OK;
}

/* --- piece bodies -------------------------------------------------------- */

/* The members a placeholder piece (structure null, a string, an array or an object without sections) does not have. */
static const char *const kPieceNewMembers[] = {"motive", "pool",  "accompaniment", "brief", "style_weight", "without",
                                               "tempo",  "check", "seed",          "result", NULL};

#define PIECE_SECTIONS_MAX 32
#define PIECE_ROLES_MAX 8
#define PIECE_LETTER_MAX 8
#define PIECE_TEXT_MAX 1024
#define PIECE_WORD_MAX 40
#define PIECE_CELLS_MAX 8

/* True when the piece is a placeholder: no new member and a structure that is not an object with a list of sections. */
bool store_piece_is_placeholder(const cJSON *piece) {
    const cJSON *structure = cJSON_GetObjectItemCaseSensitive(piece, "structure");
    if (cJSON_IsObject(structure) && (cJSON_IsArray(cJSON_GetObjectItemCaseSensitive(structure, "sections")) ||
                                      cJSON_IsString(cJSON_GetObjectItemCaseSensitive(structure, "form")))) {
        return false;
    }
    for (const char *const *name = kPieceNewMembers; *name; name++) {
        if (cJSON_GetObjectItemCaseSensitive(piece, *name)) {
            return false;
        }
    }
    return true;
}

/* An id like s0, s12 or end. */
bool store_piece_section_id_ok(const char *id) {
    if (strcmp(id, "end") == 0) {
        return true;
    }
    if (id[0] != 's' || id[1] == '\0' || strlen(id) > PIECE_ID_MAX) {
        return false;
    }
    for (const char *c = id + 1; *c; c++) {
        if (!isdigit((unsigned char)*c)) {
            return false;
        }
    }
    return true;
}

/* {kind, ...}: null or empty, a motif, a theme version or a progression. */
static UrsatzStatus validate_piece_fill(const cJSON *fill, UrsatzError *error) {
    static const char *const kinds[] = {"empty", "motif", "theme", "progression", NULL};
    if (!fill || cJSON_IsNull(fill)) {
        return URSATZ_OK;
    }
    const cJSON *kind = cJSON_GetObjectItemCaseSensitive(fill, "kind");
    if (!cJSON_IsObject(fill) || !in_set(kind, kinds)) {
        return store_invalid_body(error, "a section fill must be null or have a kind of empty, motif, theme or progression");
    }
    if (strcmp(kind->valuestring, "motif") == 0 || strcmp(kind->valuestring, "theme") == 0) {
        if (!theme_reference_ok(fill)) {
            return store_invalid_body(error, "a motif or theme fill needs item_id and version");
        }
    }
    if (strcmp(kind->valuestring, "motif") == 0) {
        const cJSON *into = cJSON_GetObjectItemCaseSensitive(fill, "into");
        const cJSON *hybrid = cJSON_GetObjectItemCaseSensitive(fill, "hybrid");
        if (!is_string_or_null(fill, "into") || !is_string_or_null(fill, "hybrid") ||
            (cJSON_IsString(into) && strlen(into->valuestring) > PIECE_WORD_MAX) ||
            (cJSON_IsString(hybrid) && strlen(hybrid->valuestring) > PIECE_WORD_MAX)) {
            return store_invalid_body(error, "a motif fill into and hybrid must be text");
        }
        /* The construction (Composer API 1.18) is "auto" or the Composer's object; the Composer checks what is in it. */
        const cJSON *construction = cJSON_GetObjectItemCaseSensitive(fill, "construction");
        if (!store_is_absent_or_null(fill, "construction") && !cJSON_IsObject(construction) &&
            !(cJSON_IsString(construction) && strcmp(construction->valuestring, "auto") == 0)) {
            return store_invalid_body(error, "a motif fill construction must be \"auto\" or an object");
        }
    }
    if (strcmp(kind->valuestring, "progression") == 0) {
        const cJSON *numerals = cJSON_GetObjectItemCaseSensitive(fill, "numerals");
        const cJSON *cells = cJSON_GetObjectItemCaseSensitive(fill, "cells");
        if (!cJSON_IsString(numerals) || numerals->valuestring[0] == '\0' || strlen(numerals->valuestring) > PIECE_TEXT_MAX) {
            return store_invalid_body(error, "a progression fill needs numerals as text");
        }
        if (!store_is_absent_or_null(fill, "cells") && (!is_whole_number(cells, 1) || cells->valuedouble > PIECE_CELLS_MAX)) {
            return store_invalid_body(error, "a progression fill cells must be 1 to 8");
        }
    }
    return URSATZ_OK;
}

/* A section's region (R-610): null or absent, "home", {tonic, material} or {relation, anchor}, each member text. */
static UrsatzStatus validate_piece_region(const cJSON *section, UrsatzError *error) {
    const cJSON *region = cJSON_GetObjectItemCaseSensitive(section, "region");
    if (store_is_absent_or_null(section, "region") || (cJSON_IsString(region) && strcmp(region->valuestring, "home") == 0)) {
        return URSATZ_OK;
    }
    bool key = cJSON_IsObject(region) && cJSON_IsString(cJSON_GetObjectItemCaseSensitive(region, "tonic")) &&
               cJSON_IsString(cJSON_GetObjectItemCaseSensitive(region, "material"));
    bool relation = cJSON_IsObject(region) && cJSON_IsString(cJSON_GetObjectItemCaseSensitive(region, "relation")) &&
                    cJSON_IsString(cJSON_GetObjectItemCaseSensitive(region, "anchor"));
    const cJSON *member = NULL;
    bool long_text = false;
    cJSON_ArrayForEach(member, region) {
        long_text = long_text || !cJSON_IsString(member) || strlen(member->valuestring) > PIECE_WORD_MAX;
    }
    if ((!key && !relation) || long_text) {
        return store_invalid_body(error, "a section region must be home, {tonic, material} or {relation, anchor}");
    }
    return URSATZ_OK;
}

/* A section's harmony (R-610): null or absent, {progression, cells}, {schema} or {chords: [{bar, text}]}; the Composer checks the text. */
static UrsatzStatus validate_piece_harmony(const cJSON *section, UrsatzError *error) {
    const cJSON *harmony = cJSON_GetObjectItemCaseSensitive(section, "harmony");
    if (store_is_absent_or_null(section, "harmony")) {
        return URSATZ_OK;
    }
    const cJSON *progression = cJSON_GetObjectItemCaseSensitive(harmony, "progression");
    const cJSON *schema = cJSON_GetObjectItemCaseSensitive(harmony, "schema");
    const cJSON *chords = cJSON_GetObjectItemCaseSensitive(harmony, "chords");
    if (!cJSON_IsObject(harmony) || (cJSON_GetArraySize(harmony) != (cJSON_IsString(progression) && !store_is_absent_or_null(harmony, "cells") ? 2 : 1))) {
        return store_invalid_body(error, "a section harmony is {progression, cells}, {schema} or {chords}");
    }
    if (progression) {
        const cJSON *cells = cJSON_GetObjectItemCaseSensitive(harmony, "cells");
        if (!cJSON_IsString(progression) || progression->valuestring[0] == '\0' || strlen(progression->valuestring) > PIECE_TEXT_MAX ||
            (!store_is_absent_or_null(harmony, "cells") && (!is_whole_number(cells, 1) || cells->valuedouble > PIECE_CELLS_MAX))) {
            return store_invalid_body(error, "a harmony progression needs numerals as text and cells of 1 to 8");
        }
    } else if (schema) {
        if (!cJSON_IsString(schema) || schema->valuestring[0] == '\0' || strlen(schema->valuestring) > PIECE_WORD_MAX) {
            return store_invalid_body(error, "a harmony schema must be a name");
        }
    } else if (cJSON_IsArray(chords) && cJSON_GetArraySize(chords) >= 1 && cJSON_GetArraySize(chords) <= 16) {
        const cJSON *chord = NULL;
        cJSON_ArrayForEach(chord, chords) {
            const cJSON *text = cJSON_GetObjectItemCaseSensitive(chord, "text");
            if (!cJSON_IsObject(chord) || !is_whole_number(cJSON_GetObjectItemCaseSensitive(chord, "bar"), 1) || !cJSON_IsString(text) ||
                text->valuestring[0] == '\0' || strlen(text->valuestring) > PIECE_WORD_MAX) {
                return store_invalid_body(error, "each harmony chord needs a bar from 1 and text");
            }
        }
    } else {
        return store_invalid_body(error, "a section harmony is {progression, cells}, {schema} or {chords}");
    }
    return URSATZ_OK;
}

/* The words of a form's list (roles or sizes members) joined by ", ": member (value or label) of each entry. */
static void join_members(const cJSON *list, const char *member, char *out, size_t out_size) {
    size_t used = 0;
    out[0] = '\0';
    const cJSON *entry = NULL;
    cJSON_ArrayForEach(entry, list) {
        const char *text = common_json_string(entry, member);
        if (text && used + strlen(text) + 3 < out_size) {
            used += (size_t)snprintf(out + used, out_size - used, "%s%s", used ? ", " : "", text);
        }
    }
}

/* The key, material (or the old mode) and meter of a structure: the Composer's words (R-276), a save keeps their type and size. */
static UrsatzStatus validate_piece_words(const cJSON *structure, UrsatzError *error) {
    static const char *const words[] = {"key", "material", "mode", "meter", NULL};
    for (const char *const *word = words; *word; word++) {
        const cJSON *value = cJSON_GetObjectItemCaseSensitive(structure, *word);
        if (!is_string_or_null(structure, *word) || (cJSON_IsString(value) && strlen(value->valuestring) > PIECE_WORD_MAX)) {
            return store_invalid_body(error, "piece key, material and meter must be text");
        }
    }
    return URSATZ_OK;
}

/* modulate is a boolean, and true only for a form whose settings allow it. */
static UrsatzStatus validate_piece_modulate(const cJSON *structure, const cJSON *form, UrsatzError *error) {
    const char *label = common_json_string(form, "label");
    const cJSON *modulate = cJSON_GetObjectItemCaseSensitive(structure, "modulate");
    if (!store_is_absent_or_null(structure, "modulate")) {
        const cJSON *allowed = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(form, "settings"), "modulate");
        if (!cJSON_IsBool(modulate)) {
            return store_invalid_body(error, "piece modulate must be true or false");
        }
        if (cJSON_IsTrue(modulate) && !cJSON_IsTrue(allowed)) {
            char message[128];
            snprintf(message, sizeof(message), "a %s piece does not modulate", label);
            return store_invalid_body(error, message);
        }
    }
    return URSATZ_OK;
}

/* A form whose sections are the request's own (the game loop): each part is one of the form's roles, in their order,
 * with every required role present. */
static UrsatzStatus validate_piece_sections(const cJSON *structure, const cJSON *form, UrsatzError *error) {
    const cJSON *sections = cJSON_GetObjectItemCaseSensitive(structure, "sections");
    const cJSON *roles = cJSON_GetObjectItemCaseSensitive(form, "roles");
    int role_count = cJSON_GetArraySize(roles);
    if (!cJSON_IsArray(sections)) {
        return store_invalid_body(error, "a piece structure needs key, material, meter and sections");
    }
    if (role_count < 1 || role_count > PIECE_ROLES_MAX) {
        return store_invalid_body(error, "the Composer gave no roles for this form");
    }
    const char *form_label = common_json_string(form, "label");
    const cJSON *bars = cJSON_GetObjectItemCaseSensitive(structure, "bars");
    const cJSON *count_member = cJSON_GetObjectItemCaseSensitive(structure, "count");
    if ((!store_is_absent_or_null(structure, "bars") && !(cJSON_IsNumber(bars) && bars->valuedouble == 0)) ||
        (!store_is_absent_or_null(structure, "count") && !(cJSON_IsNumber(count_member) && count_member->valuedouble == 0))) {
        char message[128];
        snprintf(message, sizeof(message), "a %s piece has no size: bars and count are for a form with sizes", form_label);
        return store_invalid_body(error, message);
    }
    UrsatzStatus modulate = validate_piece_modulate(structure, form, error);
    if (modulate != URSATZ_OK) {
        return modulate;
    }
    char values[160];
    char labels[160];
    join_members(roles, "value", values, sizeof(values));
    join_members(roles, "label", labels, sizeof(labels));
    int count = cJSON_GetArraySize(sections);
    if (count < 1 || count > PIECE_SECTIONS_MAX) {
        return store_invalid_body(error, "a piece has 1 to 32 sections");
    }
    double beats_max = workshop_composer_limit("piece_beats");
    int last_part = -1;
    bool present[PIECE_ROLES_MAX] = {false};
    const cJSON *section = NULL;
    cJSON_ArrayForEach(section, sections) {
        const cJSON *id = cJSON_GetObjectItemCaseSensitive(section, "id");
        const cJSON *part = cJSON_GetObjectItemCaseSensitive(section, "part");
        const cJSON *letter = cJSON_GetObjectItemCaseSensitive(section, "letter");
        const cJSON *beats = cJSON_GetObjectItemCaseSensitive(section, "beats");
        int part_index = -1;
        int index = 0;
        const cJSON *role = NULL;
        cJSON_ArrayForEach(role, roles) {
            const char *value = common_json_string(role, "value");
            if (cJSON_IsString(part) && value && strcmp(value, part->valuestring) == 0) {
                part_index = index;
            }
            index++;
        }
        if (!cJSON_IsObject(section) || !cJSON_IsString(id) || !store_piece_section_id_ok(id->valuestring) || part_index < 0) {
            char message[320];
            snprintf(message, sizeof(message), "each section needs an id like s0 or end and a part of %s", values);
            return store_invalid_body(error, message);
        }
        for (const cJSON *earlier = sections->child; earlier != section; earlier = earlier->next) {
            if (strcmp(cJSON_GetObjectItemCaseSensitive(earlier, "id")->valuestring, id->valuestring) == 0) {
                return store_invalid_body(error, "each section id may be used once");
            }
        }
        if (part_index < last_part) {
            char message[320];
            snprintf(message, sizeof(message), "sections must come in the order %s", labels);
            return store_invalid_body(error, message);
        }
        last_part = part_index;
        present[part_index] = true;
        if (!store_is_absent_or_null(section, "letter") &&
            (!cJSON_IsString(letter) || letter->valuestring[0] == '\0' || utf8_length(letter->valuestring) > PIECE_LETTER_MAX)) {
            return store_invalid_body(error, "a section letter must be 1 to 8 characters");
        }
        if (!is_whole_number(beats, 1) || (beats_max >= 1 && beats->valuedouble > beats_max)) {
            char message[96];
            snprintf(message, sizeof(message), "a section has 1 to %.0f beats", beats_max);
            return store_invalid_body(error, message);
        }
        const cJSON *pickup = cJSON_GetObjectItemCaseSensitive(section, "pickup");
        if (!store_is_absent_or_null(section, "pickup") && (!cJSON_IsString(pickup) || !is_rational(pickup->valuestring, false))) {
            return store_invalid_body(error, "a section pickup must be beats as n or n/d");
        }
        const cJSON *cadence = cJSON_GetObjectItemCaseSensitive(section, "cadence");
        if (!is_string_or_null(section, "cadence") || (cJSON_IsString(cadence) && strlen(cadence->valuestring) > PIECE_WORD_MAX)) {
            return store_invalid_body(error, "a section cadence must be null or text");
        }
        const cJSON *function = cJSON_GetObjectItemCaseSensitive(section, "function");
        if (!is_string_or_null(section, "function") || (cJSON_IsString(function) && strlen(function->valuestring) > PIECE_WORD_MAX)) {
            return store_invalid_body(error, "a section function must be null or text");
        }
        UrsatzStatus filled = validate_piece_fill(cJSON_GetObjectItemCaseSensitive(section, "fill"), error);
        if (filled != URSATZ_OK) {
            return filled;
        }
        UrsatzStatus placed = validate_piece_region(section, error);
        placed = placed == URSATZ_OK ? validate_piece_harmony(section, error) : placed;
        if (placed != URSATZ_OK) {
            return placed;
        }
    }
    int index = 0;
    const cJSON *role = NULL;
    cJSON_ArrayForEach(role, roles) {
        const cJSON *required = cJSON_GetObjectItemCaseSensitive(role, "required");
        if (cJSON_IsTrue(required) && !present[index]) {
            char message[160];
            snprintf(message, sizeof(message), "a piece needs at least one %s section", common_json_string(role, "label"));
            return store_invalid_body(error, message);
        }
        index++;
    }
    return URSATZ_OK;
}

/* The overlays of a sized form (R-610): each names a section of the size the piece picks, once, and has a region, a harmony and a
 * fill; the Composer checks what the region and the harmony say. */
static UrsatzStatus validate_piece_overlays(const cJSON *structure, const cJSON *form, const cJSON *overlays, double want_bars,
                                                       double want_count, UrsatzError *error) {
    (void)structure;
    if (store_is_absent_or_null(structure, "sections")) {
        return URSATZ_OK;
    }
    const cJSON *picked = NULL;
    const cJSON *size = NULL;
    cJSON_ArrayForEach(size, cJSON_GetObjectItemCaseSensitive(form, "sizes")) {
        const cJSON *size_bars = cJSON_GetObjectItemCaseSensitive(size, "bars");
        const cJSON *size_count = cJSON_GetObjectItemCaseSensitive(size, "count");
        bool match = want_bars != 0 || want_count != 0
                         ? (cJSON_IsNumber(size_bars) && cJSON_IsNumber(size_count) && (want_bars == 0 || size_bars->valuedouble == want_bars) &&
                            (want_count == 0 || size_count->valuedouble == want_count))
                         : cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(size, "default"));
        picked = picked ? picked : (match ? size : NULL);
    }
    const char *label = common_json_string(form, "label");
    const cJSON *overlay = NULL;
    cJSON_ArrayForEach(overlay, overlays) {
        const cJSON *name = cJSON_GetObjectItemCaseSensitive(overlay, "name");
        bool found = false;
        const cJSON *section = NULL;
        cJSON_ArrayForEach(section, cJSON_GetObjectItemCaseSensitive(picked, "sections")) {
            found = found || (cJSON_IsString(name) && strcmp(common_json_string(section, "name"), name->valuestring) == 0);
        }
        if (!cJSON_IsObject(overlay) || !cJSON_IsString(name) || !found) {
            char message[256];
            snprintf(message, sizeof(message), "an overlay names a section of the %s form at this size, not '%s'", label ? label : "sized",
                     cJSON_IsString(name) ? name->valuestring : "");
            return store_invalid_body(error, message);
        }
        for (const cJSON *earlier = overlays->child; earlier != overlay; earlier = earlier->next) {
            if (strcmp(common_json_string(earlier, "name"), name->valuestring) == 0) {
                return store_invalid_body(error, "each overlay names a section once");
            }
        }
        UrsatzStatus result = validate_piece_fill(cJSON_GetObjectItemCaseSensitive(overlay, "fill"), error);
        result = result == URSATZ_OK ? validate_piece_region(overlay, error) : result;
        result = result == URSATZ_OK ? validate_piece_harmony(overlay, error) : result;
        if (result != URSATZ_OK) {
            return result;
        }
    }
    return URSATZ_OK;
}

/* A form with sizes: no sections, bars and count (absent is 0) that pick one of the form's sizes, 0 and 0 being the
 * default, and modulate only where the form allows it. */
static UrsatzStatus validate_piece_size(const cJSON *structure, const cJSON *form, UrsatzError *error) {
    const char *label = common_json_string(form, "label");
    char message[256];
    const cJSON *overlays = cJSON_GetObjectItemCaseSensitive(structure, "sections");
    if (!store_is_absent_or_null(structure, "sections") && (!cJSON_IsArray(overlays) || cJSON_GetArraySize(overlays) > PIECE_SECTIONS_MAX)) {
        snprintf(message, sizeof(message), "a %s piece takes its sections from its form: sections lists the overlays", label ? label : "sized");
        return store_invalid_body(error, message);
    }
    const cJSON *bars = cJSON_GetObjectItemCaseSensitive(structure, "bars");
    const cJSON *count = cJSON_GetObjectItemCaseSensitive(structure, "count");
    if ((!store_is_absent_or_null(structure, "bars") && !is_whole_number(bars, 0)) ||
        (!store_is_absent_or_null(structure, "count") && !is_whole_number(count, 0))) {
        return store_invalid_body(error, "piece bars and count must be whole numbers");
    }
    double want_bars = cJSON_IsNumber(bars) ? bars->valuedouble : 0;
    double want_count = cJSON_IsNumber(count) ? count->valuedouble : 0;
    if (want_bars != 0 || want_count != 0) {
        /* A size is picked by its bars, by its count, or by both: the one not given is not compared. */
        bool found = false;
        const cJSON *size = NULL;
        cJSON_ArrayForEach(size, cJSON_GetObjectItemCaseSensitive(form, "sizes")) {
            const cJSON *size_bars = cJSON_GetObjectItemCaseSensitive(size, "bars");
            const cJSON *size_count = cJSON_GetObjectItemCaseSensitive(size, "count");
            found = found || (cJSON_IsNumber(size_bars) && cJSON_IsNumber(size_count) &&
                              (want_bars == 0 || size_bars->valuedouble == want_bars) &&
                              (want_count == 0 || size_count->valuedouble == want_count));
        }
        if (!found) {
            snprintf(message, sizeof(message), "a %s piece has no size of %.0f bars and %.0f count (0 is not given)",
                     label ? label : "sized", want_bars, want_count);
            return store_invalid_body(error, message);
        }
    }
    UrsatzStatus overlaid = validate_piece_overlays(structure, form, overlays, want_bars, want_count, error);
    return overlaid == URSATZ_OK ? validate_piece_modulate(structure, form, error) : overlaid;
}

/* structure.form names a form of the Composer (absent is game-loop); the form says whether the structure lists its
 * own sections or is a size. */
static UrsatzStatus validate_piece_structure(const cJSON *structure, UrsatzError *error) {
    if (!cJSON_IsObject(structure)) {
        return store_invalid_body(error, "a piece structure needs key, material, meter and sections");
    }
    const cJSON *form_name = cJSON_GetObjectItemCaseSensitive(structure, "form");
    if (!store_is_absent_or_null(structure, "form") && !cJSON_IsString(form_name)) {
        return store_invalid_body(error, "a piece form must be text");
    }
    const char *name = cJSON_IsString(form_name) ? form_name->valuestring : "game-loop";
    const cJSON *form = workshop_composer_form(name);
    if (!form) {
        return store_invalid_body(error, "a piece form must be one the Composer builds");
    }
    UrsatzStatus words = validate_piece_words(structure, error);
    if (words != URSATZ_OK) {
        return words;
    }
    const char *sections = common_json_string(form, "sections");
    if (sections && strcmp(sections, "request") == 0) {
        return validate_piece_sections(structure, form, error);
    }
    return validate_piece_size(structure, form, error);
}

static UrsatzStatus validate_piece(const cJSON *piece, UrsatzError *error) {
    const cJSON *structure = cJSON_GetObjectItemCaseSensitive(piece, "structure");
    if (!cJSON_IsObject(piece) || (structure && !cJSON_IsNull(structure) && !cJSON_IsString(structure) &&
                                   !cJSON_IsObject(structure) && !cJSON_IsArray(structure))) {
        return store_invalid_body(error, "a piece body needs a piece object with a structure");
    }
    if (store_piece_is_placeholder(piece)) {
        return URSATZ_OK;
    }
    UrsatzStatus result = validate_piece_structure(structure, error);
    if (result != URSATZ_OK) {
        return result;
    }
    if (!store_is_absent_or_null(piece, "motive") && !theme_reference_ok(cJSON_GetObjectItemCaseSensitive(piece, "motive"))) {
        return store_invalid_body(error, "a piece motive needs item_id and version");
    }
    if (!is_in_set_or_null(piece, "pool", kThemePools)) {
        return store_invalid_body(error, "piece pool must be approved-and-unflagged, approved or none");
    }
    const cJSON *accompaniment = cJSON_GetObjectItemCaseSensitive(piece, "accompaniment");
    const char *pattern = common_json_string(accompaniment, "pattern");
    if (!store_is_absent_or_null(piece, "accompaniment") &&
        ((pattern && strcmp(pattern, "motive") == 0) ||
         validate_theme_accompaniment(accompaniment, error) != URSATZ_OK)) {
        return store_invalid_body(error, "piece accompaniment pattern must be block, arpeggiated, alberti, oom-pah or any");
    }
    if (!store_is_absent_or_null(piece, "brief") && !theme_brief_ok(cJSON_GetObjectItemCaseSensitive(piece, "brief"))) {
        return store_invalid_body(error, "piece brief needs scopes, piece_ids or style_ids within the limits");
    }
    const cJSON *weight = cJSON_GetObjectItemCaseSensitive(piece, "style_weight");
    if (!store_is_absent_or_null(piece, "style_weight") && (!cJSON_IsString(weight) || !is_rational(weight->valuestring, false))) {
        return store_invalid_body(error, "piece style_weight must be n or n/d");
    }
    const cJSON *without = cJSON_GetObjectItemCaseSensitive(piece, "without");
    if (without && (!is_string_list_from(without, kThemeMotiveCategories) || cJSON_GetArraySize(without) > THEME_WITHOUT_MAX)) {
        return store_invalid_body(error, "piece without lists up to four motive categories");
    }
    const cJSON *tempo = cJSON_GetObjectItemCaseSensitive(piece, "tempo");
    if (!store_is_absent_or_null(piece, "tempo") && !is_whole_number(tempo, 0)) {
        return store_invalid_body(error, "piece tempo must be null or a whole number");
    }
    const cJSON *check = cJSON_GetObjectItemCaseSensitive(piece, "check");
    if (check && !cJSON_IsBool(check)) {
        return store_invalid_body(error, "piece check must be true or false");
    }
    if (!store_is_absent_or_null(piece, "seed") && !is_whole_number(cJSON_GetObjectItemCaseSensitive(piece, "seed"), 0)) {
        return store_invalid_body(error, "piece seed must be null or a whole number");
    }
    const cJSON *result_member = cJSON_GetObjectItemCaseSensitive(piece, "result");
    if (result_member && !cJSON_IsNull(result_member) && !cJSON_IsObject(result_member)) {
        return store_invalid_body(error, "piece result must be null or an object");
    }
    return URSATZ_OK;
}

UrsatzStatus workshop_material_body_validate(const char *type, const cJSON *body, UrsatzError *error) {
    workshop_error_text(error, NULL);
    if (!store_prefix_for_type(type)) {
        return workshop_fail(URSATZ_ERR_REQUEST, "invalid type");
    }
    if (!cJSON_IsObject(body)) {
        return store_invalid_body(error, "a version body must be an object");
    }
    UrsatzStatus result = validate_uses(body, error);
    if (result != URSATZ_OK) {
        return result;
    }
    static const char *const members[] = {"motif", "theme", "piece"};
    for (size_t i = 0; i < sizeof(members) / sizeof(members[0]); i++) {
        if (strcmp(members[i], type) != 0 && cJSON_GetObjectItemCaseSensitive(body, members[i])) {
            return store_invalid_body(error, "a body must not carry another type's member");
        }
    }
    const cJSON *own = cJSON_GetObjectItemCaseSensitive(body, type);
    if (strcmp(type, "motif") == 0) {
        return validate_motif(own, error);
    }
    return strcmp(type, "theme") == 0 ? validate_theme(own, error) : validate_piece(own, error);
}
