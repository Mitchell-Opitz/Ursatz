#include "store_internal.h"

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

/*
 * What the store checks of a body is what it owns (R-649): the body is an object with its uses and exactly the member of its type, ids and
 * item references are ids and versions, derived records and lineage, the brief the GUI keeps, and the sizes of the text members. What the words
 * mean (a role, a form, a meter, a key, a section, a chord, a pool, a tempo) is the Composer's: a draft may hold any word (R-276), and validate
 * and compose say where the Composer refuses one.
 */

#define TEXT_WORD_MAX 80
#define MEMBERS_MAX 64
#define THEME_BRIEF_MAX_SCOPES 16
#define THEME_BRIEF_MAX_PIECES 5000
#define THEME_BRIEF_MAX_STYLES 16
#define LIST_MAX 64
#define TEXT_MAX 4000

static bool is_string_or_null(const cJSON *object, const char *name) {
    const cJSON *value = cJSON_GetObjectItemCaseSensitive(object, name);
    return !value || cJSON_IsNull(value) || cJSON_IsString(value);
}

/* A text member within the byte cap, or absent or null. */
static bool text_within(const cJSON *object, const char *name, size_t max_chars) {
    const cJSON *value = cJSON_GetObjectItemCaseSensitive(object, name);
    return !value || cJSON_IsNull(value) || (cJSON_IsString(value) && utf8_length(value->valuestring) <= max_chars);
}

/* The members of object that are text are within the cap (a form, a region, a harmony: the words the Composer reads). */
static bool all_text_within(const cJSON *object, size_t max_chars) {
    const cJSON *member = NULL;
    cJSON_ArrayForEach(member, object) {
        if (cJSON_IsString(member) && utf8_length(member->valuestring) > max_chars) {
            return false;
        }
    }
    return true;
}

/* {item_id, version}: a reference to a version of another item. */
static bool reference_ok(const cJSON *reference) {
    const cJSON *item_id = cJSON_GetObjectItemCaseSensitive(reference, "item_id");
    return cJSON_IsObject(reference) && cJSON_IsString(item_id) && store_is_valid_id(item_id->valuestring) &&
           is_whole_number(cJSON_GetObjectItemCaseSensitive(reference, "version"), 1);
}

/* A list of references (slots, ideas): at most LIST_MAX, each an object with item_id and version. */
static bool references_ok(const cJSON *list) {
    if (!cJSON_IsArray(list) || cJSON_GetArraySize(list) > LIST_MAX) {
        return false;
    }
    const cJSON *entry = NULL;
    cJSON_ArrayForEach(entry, list) {
        if (!reference_ok(entry) || !all_text_within(entry, TEXT_WORD_MAX)) {
            return false;
        }
    }
    return true;
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

/* The members a placeholder theme (form null or a string, slots of anything) does not have. */
static const char *const kThemeNewMembers[] = {"ideas", "chords", "accompaniment", "brief", "style_weight",
                                               "without", "tempo", "check",         "seed",  "result", NULL};
/* The members a placeholder piece (structure null, a string, an array or an object without sections) does not have. */
static const char *const kPieceNewMembers[] = {"motive", "pool",  "accompaniment", "brief", "style_weight", "without",
                                               "tempo",  "check", "seed",          "result", NULL};

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


static UrsatzStatus validate_motif(const cJSON *motif, UrsatzError *error) {
    if (!cJSON_IsObject(motif)) {
        return store_invalid_body(error, "a motif body needs a motif object");
    }
    if (!all_text_within(motif, TEXT_MAX) || !text_within(motif, "notes", NOTES_MAX_CHARS) ||
        !is_string_or_null(motif, "notes") || !text_within(motif, "harmony", HARMONY_MAX_CHARS) ||
        !text_within(motif, "degrees", NOTES_MAX_CHARS)) {
        return store_invalid_body(error, "a motif's text members must be text of at most 4000 characters (harmony 200)");
    }
    for (const char *const *word = (const char *const[]){"role", "form", "meter", "key", "material", "pickup", NULL}; *word; word++) {
        const cJSON *value = cJSON_GetObjectItemCaseSensitive(motif, *word);
        if (value && !cJSON_IsNull(value) && (!cJSON_IsString(value) || utf8_length(value->valuestring) > TEXT_WORD_MAX)) {
            return store_invalid_body(error, "a motif's role, form, meter, key, material and pickup must be text");
        }
    }
    const cJSON *features = cJSON_GetObjectItemCaseSensitive(motif, "features");
    if (features && !cJSON_IsNull(features) && (!cJSON_IsArray(features) || cJSON_GetArraySize(features) > MEMBERS_MAX)) {
        return store_invalid_body(error, "motif features must be an array");
    }
    const cJSON *overrides = cJSON_GetObjectItemCaseSensitive(motif, "tone_overrides");
    if (overrides && !cJSON_IsNull(overrides) && (!cJSON_IsObject(overrides) || cJSON_GetArraySize(overrides) > MEMBERS_MAX)) {
        return store_invalid_body(error, "motif tone_overrides must be an object of at most 64 notes");
    }
    const cJSON *analysis = cJSON_GetObjectItemCaseSensitive(motif, "analysis");
    if (analysis && !cJSON_IsNull(analysis) && !cJSON_IsObject(analysis)) {
        return store_invalid_body(error, "motif analysis must be null or an object");
    }
    return store_is_absent_or_null(motif, "derived")
               ? URSATZ_OK
               : validate_derived(cJSON_GetObjectItemCaseSensitive(motif, "derived"), error);
}

/* The members the theme and the piece share: the references, the brief the GUI keeps and the sizes of the rest; what the Composer reads is its own. */
static UrsatzStatus validate_request_members(const cJSON *object, const char *what, UrsatzError *error) {
    static const char *const kReferences[] = {"slots", "ideas", NULL};
    for (const char *const *name = kReferences; *name; name++) {
        const cJSON *list = cJSON_GetObjectItemCaseSensitive(object, *name);
        if (list && !cJSON_IsNull(list) && (strcmp(*name, "slots") == 0 || strcmp(*name, "ideas") == 0) && !references_ok(list)) {
            char message[96];
            snprintf(message, sizeof(message), "each %s %s needs item_id and version", what, *name);
            return store_invalid_body(error, message);
        }
    }
    const cJSON *motive = cJSON_GetObjectItemCaseSensitive(object, "motive");
    if (motive && !cJSON_IsNull(motive) && !reference_ok(motive)) {
        return store_invalid_body(error, "a piece motive needs item_id and version");
    }
    const cJSON *accompaniment = cJSON_GetObjectItemCaseSensitive(object, "accompaniment");
    if (accompaniment && !cJSON_IsNull(accompaniment) &&
        (!cJSON_IsObject(accompaniment) || !all_text_within(accompaniment, TEXT_WORD_MAX) ||
         (strcmp(common_json_string(accompaniment, "pattern") ? common_json_string(accompaniment, "pattern") : "", "motive") == 0 &&
          !reference_ok(accompaniment)))) {
        return store_invalid_body(error, "an accompaniment needs a pattern and, for a motive, item_id and version");
    }
    if (!store_is_absent_or_null(object, "brief") && !theme_brief_ok(cJSON_GetObjectItemCaseSensitive(object, "brief"))) {
        char message[96];
        snprintf(message, sizeof(message), "%s brief needs scopes, piece_ids or style_ids within the limits", what);
        return store_invalid_body(error, message);
    }
    const cJSON *chords = cJSON_GetObjectItemCaseSensitive(object, "chords");
    if (chords && !cJSON_IsNull(chords) && (!cJSON_IsArray(chords) || cJSON_GetArraySize(chords) > LIST_MAX)) {
        return store_invalid_body(error, "chords must be a list");
    }
    const cJSON *result_member = cJSON_GetObjectItemCaseSensitive(object, "result");
    if (result_member && !cJSON_IsNull(result_member) && !cJSON_IsObject(result_member)) {
        char message[96];
        snprintf(message, sizeof(message), "%s result must be null or an object", what);
        return store_invalid_body(error, message);
    }
    return URSATZ_OK;
}

static UrsatzStatus validate_theme(const cJSON *theme, UrsatzError *error) {
    const cJSON *form = cJSON_GetObjectItemCaseSensitive(theme, "form");
    const cJSON *slots = cJSON_GetObjectItemCaseSensitive(theme, "slots");
    if (!cJSON_IsObject(theme) || (form && !cJSON_IsNull(form) && !cJSON_IsString(form) && !cJSON_IsObject(form)) || !cJSON_IsArray(slots)) {
        return store_invalid_body(error, "a theme body needs a theme object with form and slots");
    }
    if (store_theme_is_placeholder(theme)) {
        return URSATZ_OK;
    }
    if (cJSON_IsObject(form) && !all_text_within(form, TEXT_WORD_MAX)) {
        return store_invalid_body(error, "the words of a theme form must be text of at most 80 characters");
    }
    return validate_request_members(theme, "theme", error);
}

static UrsatzStatus validate_piece(const cJSON *piece, UrsatzError *error) {
    const cJSON *structure = cJSON_GetObjectItemCaseSensitive(piece, "structure");
    if (!cJSON_IsObject(piece) ||
        (structure && !cJSON_IsNull(structure) && !cJSON_IsString(structure) && !cJSON_IsObject(structure) && !cJSON_IsArray(structure))) {
        return store_invalid_body(error, "a piece body needs a piece object with a structure");
    }
    if (store_piece_is_placeholder(piece)) {
        return URSATZ_OK;
    }
    const cJSON *sections = cJSON_IsObject(structure) ? cJSON_GetObjectItemCaseSensitive(structure, "sections") : NULL;
    if (sections && !cJSON_IsNull(sections) && (!cJSON_IsArray(sections) || cJSON_GetArraySize(sections) > MEMBERS_MAX)) {
        return store_invalid_body(error, "piece sections must be a list");
    }
    /* A fill that names a motif or a theme names it by reference: the store resolves the uses from them. */
    const cJSON *section = NULL;
    cJSON_ArrayForEach(section, cJSON_IsArray(sections) ? sections : NULL) {
        const cJSON *fill = cJSON_GetObjectItemCaseSensitive(section, "fill");
        const char *kind = cJSON_IsObject(fill) ? common_json_string(fill, "kind") : NULL;
        if (kind && (strcmp(kind, "motif") == 0 || strcmp(kind, "theme") == 0) && !reference_ok(fill)) {
            return store_invalid_body(error, "a motif or theme fill needs item_id and version");
        }
    }
    return validate_request_members(piece, "piece", error);
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
