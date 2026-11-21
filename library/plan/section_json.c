#include "plan/section_json_internal.h"

#include <ctype.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "theory/harmony/harmony_cadence_kinds.h"

#define COUNT_OF(array) (sizeof(array) / sizeof((array)[0]))

#define USE_F SECTION_USE_FORM
#define USE_P SECTION_USE_PLAN
#define USE_R SECTION_USE_REQUEST
#define USE_O SECTION_USE_OVERLAY

/* ------------------------------------------------------------- reading helpers */

static bool fail(Reader *reader, const char *path, const char *format, ...) {
    if (!reader->failed && reader->error) {
        va_list args;
        snprintf(reader->error->path, sizeof(reader->error->path), "%s", path);
        va_start(args, format);
        vsnprintf(reader->error->message, sizeof(reader->error->message), format, args);
        va_end(args);
    }
    reader->failed = true;
    return false;
}

/* base + "/" + the key, with "~" and "/" escaped as a JSON pointer does. */
static void join(char *out, size_t cap, const char *base, const char *key) {
    size_t n = (size_t)snprintf(out, cap, "%s/", base);
    if (n >= cap) {
        n = cap - 1;
    }
    for (; *key && n + 2 < cap; key++) {
        if (*key == '~') {
            out[n++] = '~';
            out[n++] = '0';
        } else if (*key == '/') {
            out[n++] = '~';
            out[n++] = '1';
        } else {
            out[n++] = *key;
        }
    }
    out[n] = '\0';
}

static void join_index(char *out, size_t cap, const char *base, size_t index) {
    snprintf(out, cap, "%s/%zu", base, index);
}

/* A string copied into out (1 to cap - 1 characters). */
static bool read_text(Reader *reader, const cJSON *item, const char *path, char *out, size_t cap) {
    if (!cJSON_IsString(item) || !item->valuestring) {
        return fail(reader, path, "must be a string");
    }
    if (item->valuestring[0] == '\0' || strlen(item->valuestring) >= cap) {
        return fail(reader, path, "must be 1 to %zu characters", cap - 1);
    }
    snprintf(out, cap, "%s", item->valuestring);
    return true;
}

static bool read_bool(Reader *reader, const cJSON *item, const char *path, bool *out) {
    if (!cJSON_IsBool(item)) {
        return fail(reader, path, "must be true or false");
    }
    *out = cJSON_IsTrue(item);
    return true;
}

/* A whole number in [min, max]. */
static bool read_whole(Reader *reader, const cJSON *item, const char *path, long min, long max, long *out) {
    if (!cJSON_IsNumber(item) || item->valuedouble != (double)(long)item->valuedouble) {
        return fail(reader, path, "must be a whole number");
    }
    if (item->valuedouble < (double)min || item->valuedouble > (double)max) {
        return fail(reader, path, "must be from %ld to %ld", min, max);
    }
    *out = (long)item->valuedouble;
    return true;
}

/* A string member of the choices a find function knows. */
#define READ_NAMED(reader, item, path, find, type, out, what)                       \
    do {                                                                            \
        if (!cJSON_IsString((item)) || !(item)->valuestring) {                      \
            return fail((reader), (path), "must be a string");                      \
        }                                                                           \
        if (!find((item)->valuestring, (out))) {                                    \
            return fail((reader), (path), "unknown " what " \"%s\"", (item)->valuestring); \
        }                                                                           \
    } while (0)

/* Refuses a member an object does not allow, and one it has twice. */
static bool check_members(Reader *reader, const cJSON *object, const char *path, const char *const *allowed, size_t count) {
    for (const cJSON *child = object->child; child; child = child->next) {
        char at[96];
        bool known = false;
        join(at, sizeof(at), path, child->string ? child->string : "");
        for (size_t i = 0; i < count; i++) {
            known = known || (child->string && strcmp(child->string, allowed[i]) == 0);
        }
        if (!known) {
            return fail(reader, at, "unknown member");
        }
        for (const cJSON *other = child->next; other; other = other->next) {
            if (other->string && strcmp(other->string, child->string) == 0) {
                return fail(reader, at, "member given twice");
            }
        }
    }
    return true;
}

/* ------------------------------------------------------------- the members: reading */

static bool read_name(Reader *r, const cJSON *item, SectionDescription *d) {
    return read_text(r, item, "/name", d->name, sizeof(d->name));
}

static bool read_id(Reader *r, const cJSON *item, SectionDescription *d) {
    return read_text(r, item, "/id", d->id, sizeof(d->id));
}

static bool read_letter(Reader *r, const cJSON *item, SectionDescription *d) {
    return read_text(r, item, "/letter", d->letter, sizeof(d->letter));
}

static bool read_role(Reader *r, const cJSON *item, SectionDescription *d) {
    READ_NAMED(r, item, "/role", section_role_find, SectionRole, &d->role, "role");
    d->has_role = true;
    return true;
}

static bool read_bars(Reader *r, const cJSON *item, SectionDescription *d) {
    long bars;
    if (!read_whole(r, item, "/bars", 1, 4096, &bars)) {
        return false;
    }
    d->bars = (size_t)bars;
    return true;
}

static bool read_beats(Reader *r, const cJSON *item, SectionDescription *d) {
    long beats;
    if (!read_whole(r, item, "/beats", 1, 4096, &beats)) {
        return false;
    }
    d->beats = (size_t)beats;
    return true;
}

/* A pickup: a whole number, or text "n" or "n/d". */
static bool read_pickup(Reader *r, const cJSON *item, SectionDescription *d) {
    long long num = 0;
    long long den = 1;
    if (cJSON_IsNumber(item)) {
        if (item->valuedouble != (double)(long)item->valuedouble || item->valuedouble < 0 || item->valuedouble > 4096) {
            return fail(r, "/pickup", "must be a whole number from 0 to 4096 or a fraction such as \"1/2\"");
        }
        num = (long long)item->valuedouble;
    } else if (cJSON_IsString(item) && item->valuestring) {
        char *end = NULL;
        const char *text = item->valuestring;
        if (!isdigit((unsigned char)text[0])) {
            return fail(r, "/pickup", "must be a whole number or a fraction such as \"1/2\"");
        }
        num = strtoll(text, &end, 10);
        if (*end == '/') {
            if (!isdigit((unsigned char)end[1])) {
                return fail(r, "/pickup", "must be a whole number or a fraction such as \"1/2\"");
            }
            den = strtoll(end + 1, &end, 10);
        }
        if (*end != '\0' || den < 1 || num > 1000000 || den > 1000000) {
            return fail(r, "/pickup", "must be a whole number or a fraction such as \"1/2\"");
        }
    } else {
        return fail(r, "/pickup", "must be a whole number or a fraction such as \"1/2\"");
    }
    if (rational_make(num, den, &d->pickup) != RATIONAL_OK) {
        return fail(r, "/pickup", "not a fraction");
    }
    d->has_pickup = true;
    return true;
}

static bool read_beginning(Reader *r, const cJSON *item, SectionDescription *d) {
    READ_NAMED(r, item, "/beginning", section_beginning_find, SectionBeginning, &d->pairing.beginning, "beginning");
    r->seen_beginning = true;
    return true;
}

static bool read_ending(Reader *r, const cJSON *item, SectionDescription *d) {
    READ_NAMED(r, item, "/ending", section_ending_find, SectionEnding, &d->pairing.ending, "ending");
    r->seen_ending = true;
    return true;
}

static const struct {
    const char *abbreviation;
    CadenceType type;
} CADENCE_ABBREVIATIONS[] = {{"PAC", CADENCE_TYPE_PERFECT_AUTHENTIC},
                             {"IAC", CADENCE_TYPE_IMPERFECT_AUTHENTIC},
                             {"HC", CADENCE_TYPE_HALF},
                             {"DC", CADENCE_TYPE_DECEPTIVE},
                             {"PC", CADENCE_TYPE_PLAGAL}};

/* A cadence: the name of a kind, or an abbreviation in any case. *named is the kind's name, or "" for an abbreviation. */
static bool read_cadence_text(Reader *r, const cJSON *item, const char *path, CadenceType *core, char *named, size_t cap) {
    if (!cJSON_IsString(item) || !item->valuestring) {
        return fail(r, path, "must be a string");
    }
    const HarmonyCadenceKind *kind = harmony_cadence_kind_find(item->valuestring);
    if (kind && kind->core != CADENCE_TYPE_UNKNOWN && strlen(item->valuestring) < cap) {
        *core = kind->core;
        snprintf(named, cap, "%s", item->valuestring);
        return true;
    }
    for (size_t i = 0; i < COUNT_OF(CADENCE_ABBREVIATIONS); i++) {
        const char *a = CADENCE_ABBREVIATIONS[i].abbreviation;
        const char *s = item->valuestring;
        size_t k = 0;
        while (a[k] && s[k] && toupper((unsigned char)s[k]) == a[k]) {
            k++;
        }
        if (a[k] == '\0' && s[k] == '\0') {
            *core = CADENCE_ABBREVIATIONS[i].type;
            named[0] = '\0';
            return true;
        }
    }
    return fail(r, path, "unknown cadence \"%.40s\"", item->valuestring);
}

static bool read_cadence(Reader *r, const cJSON *item, SectionDescription *d) {
    if (!read_cadence_text(r, item, "/cadence", &d->cadence, d->cadence_name, sizeof(d->cadence_name))) {
        return false;
    }
    d->has_cadence = true;
    return true;
}

static bool read_first_cadence(Reader *r, const cJSON *item, SectionDescription *d) {
    char ignored[SECTION_CADENCE_NAME_MAX];
    if (!read_cadence_text(r, item, "/first_cadence", &d->first_cadence, ignored, sizeof(ignored))) {
        return false;
    }
    d->has_first_cadence = true;
    return true;
}

static bool read_closed(Reader *r, const cJSON *item, SectionDescription *d) {
    return read_bool(r, item, "/closed", &d->closed);
}

/* A region: "home", a form region name (a form or a plan), {tonic, material} or {relation, anchor}. */
static bool read_region(Reader *r, const cJSON *item, const char *path, SectionRegion *out) {
    static const char *const KEY_MEMBERS[] = {"tonic", "material"};
    static const char *const RELATION_MEMBERS[] = {"relation", "anchor"};
    char at[96];
    memset(out, 0, sizeof(*out));
    if (cJSON_IsString(item) && item->valuestring) {
        if (strcmp(item->valuestring, "home") == 0) {
            out->kind = SECTION_REGION_HOME;
            return true;
        }
        if (r->use != USE_F && r->use != USE_P) {
            return fail(r, path, "must be \"home\", {\"tonic\", \"material\"} or {\"relation\", \"anchor\"}");
        }
        if (item->valuestring[0] == '\0' || strlen(item->valuestring) >= sizeof(out->form_region)) {
            return fail(r, path, "must be 1 to %zu characters", sizeof(out->form_region) - 1);
        }
        out->kind = SECTION_REGION_FORM;
        snprintf(out->form_region, sizeof(out->form_region), "%s", item->valuestring);
        return true;
    }
    if (!cJSON_IsObject(item)) {
        return fail(r, path, "must be \"home\", a region name, {\"tonic\", \"material\"} or {\"relation\", \"anchor\"}");
    }
    if (cJSON_GetObjectItemCaseSensitive(item, "tonic")) {
        if (!check_members(r, item, path, KEY_MEMBERS, COUNT_OF(KEY_MEMBERS))) {
            return false;
        }
        const cJSON *material = cJSON_GetObjectItemCaseSensitive(item, "material");
        join(at, sizeof(at), path, "material");
        if (!material) {
            return fail(r, at, "required member is missing");
        }
        join(at, sizeof(at), path, "tonic");
        if (!read_text(r, cJSON_GetObjectItemCaseSensitive(item, "tonic"), at, out->tonic, sizeof(out->tonic))) {
            return false;
        }
        join(at, sizeof(at), path, "material");
        out->kind = SECTION_REGION_KEY;
        return read_text(r, material, at, out->material, sizeof(out->material));
    }
    if (cJSON_GetObjectItemCaseSensitive(item, "relation")) {
        if (!check_members(r, item, path, RELATION_MEMBERS, COUNT_OF(RELATION_MEMBERS))) {
            return false;
        }
        join(at, sizeof(at), path, "relation");
        if (!read_text(r, cJSON_GetObjectItemCaseSensitive(item, "relation"), at, out->relation, sizeof(out->relation))) {
            return false;
        }
        out->kind = SECTION_REGION_RELATION;
        const cJSON *anchor = cJSON_GetObjectItemCaseSensitive(item, "anchor");
        if (anchor) {
            join(at, sizeof(at), path, "anchor");
            if (!cJSON_IsString(anchor) || !anchor->valuestring) {
                return fail(r, at, "must be \"home\", \"previous\" or the name of a section");
            }
            if (strcmp(anchor->valuestring, "home") == 0) {
                out->anchor = SECTION_ANCHOR_HOME;
            } else if (strcmp(anchor->valuestring, "previous") == 0) {
                out->anchor = SECTION_ANCHOR_PREVIOUS;
            } else {
                out->anchor = SECTION_ANCHOR_SECTION;
                return read_text(r, anchor, at, out->anchor_section, sizeof(out->anchor_section));
            }
        }
        return true;
    }
    return fail(r, path, "a key needs a tonic and a material, a relation needs its name");
}

static bool read_keys(Reader *r, const cJSON *item, SectionDescription *d) {
    static const char *const KEY_MEMBERS[] = {"start", "end"};
    const cJSON *start;
    const cJSON *end;
    if (!cJSON_IsObject(item)) {
        return fail(r, "/keys", "must be an object");
    }
    if (!check_members(r, item, "/keys", KEY_MEMBERS, COUNT_OF(KEY_MEMBERS))) {
        return false;
    }
    start = cJSON_GetObjectItemCaseSensitive(item, "start");
    end = cJSON_GetObjectItemCaseSensitive(item, "end");
    return (!start || read_region(r, start, "/keys/start", &d->key_start)) && (!end || read_region(r, end, "/keys/end", &d->key_end));
}

static bool read_region_member(Reader *r, const cJSON *item, SectionDescription *d) {
    if (!read_region(r, item, "/region", &d->key_end)) {
        return false;
    }
    d->key_start = d->key_end;
    return true;
}

static bool read_motive(Reader *r, const cJSON *item, SectionDescription *d) {
    return read_text(r, item, "/motive", d->motive, sizeof(d->motive));
}

static bool read_returns(Reader *r, const cJSON *item, SectionDescription *d) {
    return read_text(r, item, "/returns", d->returns, sizeof(d->returns));
}

static bool read_variation(Reader *r, const cJSON *item, SectionDescription *d) {
    READ_NAMED(r, item, "/variation", section_return_variation_find, SectionReturnVariation, &d->variation, "variation");
    return true;
}

static bool read_setting(Reader *r, const cJSON *item, SectionDescription *d) {
    READ_NAMED(r, item, "/setting", section_setting_find, SectionSetting, &d->setting, "setting");
    return true;
}

static bool read_mode(Reader *r, const cJSON *item, SectionDescription *d) {
    READ_NAMED(r, item, "/mode", section_mode_find, SectionModeShift, &d->mode, "mode");
    return true;
}

static bool read_layers(Reader *r, const cJSON *item, SectionDescription *d) {
    unsigned mask = 0;
    if (cJSON_IsString(item) && item->valuestring && strcmp(item->valuestring, "all") == 0) {
        d->layers = SECTION_LAYERS_ALL;
        return true;
    }
    if (!cJSON_IsArray(item) || cJSON_GetArraySize(item) < 1) {
        return fail(r, "/layers", "must be \"all\" or a list of lanes");
    }
    for (int i = 0; i < cJSON_GetArraySize(item); i++) {
        const cJSON *lane = cJSON_GetArrayItem(item, i);
        char at[96];
        SectionLane found;
        join_index(at, sizeof(at), "/layers", (size_t)i);
        if (!cJSON_IsString(lane) || !lane->valuestring) {
            return fail(r, at, "must be a lane name");
        }
        bool known = section_lane_find(lane->valuestring, &found);
        if (known && r->use == USE_F && found != SECTION_LANE_BASS && found != SECTION_LANE_INNER_VOICE &&
            found != SECTION_LANE_MELODY) {
            known = false;
        }
        if (!known) {
            return fail(r, at, "unknown lane \"%.40s\"%s", lane->valuestring,
                        r->use == USE_F ? " (bass, inner_voice, melody)" : "");
        }
        mask |= SECTION_LAYER_BIT(found);
    }
    d->layers = mask;
    return true;
}

static bool read_pedal(Reader *r, const cJSON *item, SectionDescription *d) {
    if (!cJSON_IsString(item) || !item->valuestring) {
        return fail(r, "/pedal", "must be a string");
    }
    if (strcmp(item->valuestring, "dominant") != 0) {
        return fail(r, "/pedal", "unknown pedal \"%.40s\"", item->valuestring);
    }
    d->pedal = 5;
    return true;
}

static bool read_leads_in(Reader *r, const cJSON *item, SectionDescription *d) {
    return read_bool(r, item, "/leads_in", &d->leads_in);
}

static bool read_connective(Reader *r, const cJSON *item, SectionDescription *d) {
    READ_NAMED(r, item, "/connective", section_connective_find, SectionConnective, &d->connective, "connective");
    return true;
}

static bool read_connective_written(Reader *r, const cJSON *item, SectionDescription *d) {
    return read_bool(r, item, "/connective_written", &d->connective_written);
}

static bool read_extension(Reader *r, const cJSON *item, const char *path, SectionExtension *out) {
    static const char *const MEMBERS[] = {"kind", "idea", "bars", "varied"};
    char at[96];
    long bars;
    memset(out, 0, sizeof(*out));
    if (!cJSON_IsObject(item)) {
        return fail(r, path, "must be {\"kind\", \"idea\", \"bars\", \"varied\"}");
    }
    if (!check_members(r, item, path, MEMBERS, COUNT_OF(MEMBERS))) {
        return false;
    }
    const cJSON *kind = cJSON_GetObjectItemCaseSensitive(item, "kind");
    join(at, sizeof(at), path, "kind");
    if (!kind) {
        return fail(r, at, "required member is missing");
    }
    if (!cJSON_IsString(kind) || !kind->valuestring || !section_extension_kind_find(kind->valuestring, &out->kind)) {
        return fail(r, at, "must be repeat, stretch, addition or evaded");
    }
    const cJSON *idea = cJSON_GetObjectItemCaseSensitive(item, "idea");
    join(at, sizeof(at), path, "idea");
    if (idea && !cJSON_IsNull(idea)) {
        SectionIdea found;
        if (!cJSON_IsString(idea) || !idea->valuestring || !section_idea_find(idea->valuestring, &found)) {
            return fail(r, at, "must be the name of an idea");
        }
        out->idea = (unsigned char)found;
    } else if (out->kind == SECTION_EXTENSION_REPEAT || out->kind == SECTION_EXTENSION_STRETCH) {
        return fail(r, at, "a %s needs the idea it follows", section_extension_kind_name(out->kind));
    }
    const cJSON *bars_item = cJSON_GetObjectItemCaseSensitive(item, "bars");
    if (bars_item) {
        join(at, sizeof(at), path, "bars");
        if (!read_whole(r, bars_item, at, 0, 4, &bars)) {
            return false;
        }
        out->bars = (unsigned char)bars;
    }
    const cJSON *varied = cJSON_GetObjectItemCaseSensitive(item, "varied");
    if (varied) {
        join(at, sizeof(at), path, "varied");
        return read_bool(r, varied, at, &out->varied);
    }
    return true;
}

/* "auto" (a form) or {ideas: {<idea name>: bars}, extensions: [{kind, idea, bars, varied}], reason}. */
static bool read_construction(Reader *r, const cJSON *item, SectionDescription *d) {
    static const char *const MEMBERS[] = {"ideas", "extensions", "reason"};
    char at[96];
    SectionConstruction *c = &d->construction;
    d->has_construction = true;
    if (cJSON_IsString(item) && item->valuestring && strcmp(item->valuestring, "auto") == 0) {
        d->construction_auto = true;
        return true;
    }
    if (!cJSON_IsObject(item)) {
        return fail(r, "/construction", "must be \"auto\" or {\"ideas\": {...}, \"extensions\": [...]}");
    }
    if (!check_members(r, item, "/construction", MEMBERS, COUNT_OF(MEMBERS))) {
        return false;
    }
    const cJSON *ideas = cJSON_GetObjectItemCaseSensitive(item, "ideas");
    if (!ideas) {
        return fail(r, "/construction/ideas", "required member is missing");
    }
    if (!cJSON_IsObject(ideas)) {
        return fail(r, "/construction/ideas", "must be an object of idea names and bars, for example {\"basic idea\": 2}");
    }
    for (const cJSON *member = ideas->child; member; member = member->next) {
        SectionIdea idea;
        long bars;
        join(at, sizeof(at), "/construction/ideas", member->string ? member->string : "");
        if (!member->string || !section_idea_find(member->string, &idea)) {
            return fail(r, at, "unknown idea");
        }
        if (!cJSON_IsNumber(member) || member->valuedouble != (double)(long)member->valuedouble || member->valuedouble < 0 ||
            member->valuedouble > 16) {
            return fail(r, at, "must be a whole number of bars from 0 to 16");
        }
        bars = (long)member->valuedouble;
        c->idea_bars[idea] = (unsigned char)bars;
    }
    const cJSON *extensions = cJSON_GetObjectItemCaseSensitive(item, "extensions");
    if (extensions) {
        if (!cJSON_IsArray(extensions)) {
            return fail(r, "/construction/extensions", "must be a list");
        }
        if (cJSON_GetArraySize(extensions) > SECTION_CONSTRUCTION_EXTENSIONS) {
            return fail(r, "/construction/extensions", "at most %d extensions", SECTION_CONSTRUCTION_EXTENSIONS);
        }
        for (int k = 0; k < cJSON_GetArraySize(extensions); k++) {
            join_index(at, sizeof(at), "/construction/extensions", (size_t)k);
            if (!read_extension(r, cJSON_GetArrayItem(extensions, k), at, &c->extensions[k])) {
                return false;
            }
            c->extension_count = (size_t)k + 1;
        }
    }
    const cJSON *reason = cJSON_GetObjectItemCaseSensitive(item, "reason");
    if (reason) {
        if (r->use != USE_P) {
            return fail(r, "/construction/reason", "\"reason\" is not given here");
        }
        READ_NAMED(r, reason, "/construction/reason", section_construction_reason_find, SectionConstructionReason, &c->reason,
                   "reason");
    }
    return true;
}

/* {kind, rhythm, harmony, change_at, climax}; harmony is "kept", or "reharmonized" (the default in the parallel mode). */
static bool read_figure(Reader *r, const cJSON *item, SectionDescription *d) {
    static const char *const MEMBERS[] = {"kind", "rhythm", "harmony", "change_at", "climax"};
    SectionFigure *f = &d->figure;
    char at[96];
    if (!cJSON_IsObject(item)) {
        return fail(r, "/figure", "must be an object");
    }
    if (!check_members(r, item, "/figure", MEMBERS, COUNT_OF(MEMBERS))) {
        return false;
    }
    const cJSON *kind = cJSON_GetObjectItemCaseSensitive(item, "kind");
    const cJSON *rhythm = cJSON_GetObjectItemCaseSensitive(item, "rhythm");
    if (!kind) {
        return fail(r, "/figure/kind", "required member is missing");
    }
    if (!rhythm) {
        return fail(r, "/figure/rhythm", "required member is missing");
    }
    READ_NAMED(r, kind, "/figure/kind", section_figure_kind_find, SectionFigureKind, &f->kind, "kind");
    READ_NAMED(r, rhythm, "/figure/rhythm", section_figure_rhythm_find, SectionFigureRhythm, &f->rhythm, "rhythm");
    f->reharmonize = d->mode == SECTION_MODE_PARALLEL;
    const cJSON *harmony = cJSON_GetObjectItemCaseSensitive(item, "harmony");
    if (harmony) {
        if (!cJSON_IsString(harmony) || !harmony->valuestring) {
            return fail(r, "/figure/harmony", "must be a string");
        }
        if (strcmp(harmony->valuestring, "kept") == 0) {
            f->reharmonize = false;
        } else if (strcmp(harmony->valuestring, "reharmonized") == 0) {
            f->reharmonize = true;
        } else {
            return fail(r, "/figure/harmony", "unknown harmony \"%.40s\"", harmony->valuestring);
        }
    }
    const cJSON *change_at = cJSON_GetObjectItemCaseSensitive(item, "change_at");
    if (change_at && !cJSON_IsNull(change_at)) {
        long bar;
        snprintf(at, sizeof(at), "/figure/change_at");
        if (!read_whole(r, change_at, at, 1, 4096, &bar)) {
            return false;
        }
        f->change_at = (unsigned)bar;
    }
    const cJSON *climax = cJSON_GetObjectItemCaseSensitive(item, "climax");
    if (climax && !read_bool(r, climax, "/figure/climax", &f->climax)) {
        return false;
    }
    d->has_figure = true;
    return true;
}

static bool read_function(Reader *r, const cJSON *item, SectionDescription *d) {
    READ_NAMED(r, item, "/function", section_function_kind_find, SectionFunctionKind, &d->function.function, "function");
    d->has_function = true;
    return true;
}

static bool read_independent(Reader *r, const cJSON *item, SectionDescription *d) {
    return read_bool(r, item, "/independent", &d->function.independent);
}

static bool read_lyric(Reader *r, const cJSON *item, SectionDescription *d) {
    return read_bool(r, item, "/lyric", &d->function.lyric);
}

static bool read_free_phrase(Reader *r, const cJSON *item, SectionDescription *d) {
    return read_bool(r, item, "/free_phrase", &d->free_phrase);
}

/* {progression: numerals, cells}, {schema: name} or {chords: [{bar, text}]}. */
static bool read_harmony(Reader *r, const cJSON *item, SectionDescription *d) {
    static const char *const MEMBERS[] = {"progression", "cells", "schema", "chords"};
    SectionHarmony *h = &d->harmony;
    long number;
    if (!cJSON_IsObject(item)) {
        return fail(r, "/harmony", "must be {\"progression\"}, {\"schema\"} or {\"chords\"}");
    }
    if (!check_members(r, item, "/harmony", MEMBERS, COUNT_OF(MEMBERS))) {
        return false;
    }
    const cJSON *progression = cJSON_GetObjectItemCaseSensitive(item, "progression");
    const cJSON *schema = cJSON_GetObjectItemCaseSensitive(item, "schema");
    const cJSON *chords = cJSON_GetObjectItemCaseSensitive(item, "chords");
    const cJSON *cells = cJSON_GetObjectItemCaseSensitive(item, "cells");
    if ((progression != NULL) + (schema != NULL) + (chords != NULL) != 1) {
        return fail(r, "/harmony", "give a progression, a schema or chords");
    }
    if (cells && !progression) {
        return fail(r, "/harmony/cells", "cells go with a progression");
    }
    if (progression) {
        h->kind = SECTION_HARMONY_PROGRESSION;
        if (!read_text(r, progression, "/harmony/progression", h->numerals, sizeof(h->numerals))) {
            return false;
        }
        if (cells) {
            if (!read_whole(r, cells, "/harmony/cells", 0, 16, &number)) {
                return false;
            }
            h->cells = (unsigned)number;
        }
        return true;
    }
    if (schema) {
        h->kind = SECTION_HARMONY_SCHEMA;
        return read_text(r, schema, "/harmony/schema", h->schema, sizeof(h->schema));
    }
    h->kind = SECTION_HARMONY_CHORDS;
    if (!cJSON_IsArray(chords)) {
        return fail(r, "/harmony/chords", "must be a list of {\"bar\", \"text\"}");
    }
    if (cJSON_GetArraySize(chords) > SECTION_CHORD_BARS_MAX) {
        return fail(r, "/harmony/chords", "must hold 1 to %d bars", SECTION_CHORD_BARS_MAX);
    }
    for (int i = 0; i < cJSON_GetArraySize(chords); i++) {
        static const char *const CHORD_MEMBERS[] = {"bar", "text"};
        const cJSON *chord = cJSON_GetArrayItem(chords, i);
        char at[96];
        char member_at[96];
        join_index(at, sizeof(at), "/harmony/chords", (size_t)i);
        if (!cJSON_IsObject(chord)) {
            return fail(r, at, "must be {\"bar\", \"text\"}");
        }
        if (!check_members(r, chord, at, CHORD_MEMBERS, COUNT_OF(CHORD_MEMBERS))) {
            return false;
        }
        const cJSON *bar = cJSON_GetObjectItemCaseSensitive(chord, "bar");
        const cJSON *text = cJSON_GetObjectItemCaseSensitive(chord, "text");
        join(member_at, sizeof(member_at), at, "bar");
        if (!bar) {
            return fail(r, member_at, "required member is missing");
        }
        if (!read_whole(r, bar, member_at, 1, 4096, &number)) {
            return false;
        }
        h->chords[i].bar = (unsigned)number;
        join(member_at, sizeof(member_at), at, "text");
        if (!text) {
            return fail(r, member_at, "required member is missing");
        }
        if (!read_text(r, text, member_at, h->chords[i].text, sizeof(h->chords[i].text))) {
            return false;
        }
        h->chord_count = (size_t)i + 1;
    }
    return true;
}

/* ------------------------------------------------------------- the members: writing */

static cJSON *w_string(const char *text) {
    return cJSON_CreateString(text);
}

static cJSON *w_true(void) {
    return cJSON_CreateTrue();
}

static cJSON *w_whole(size_t value) {
    return cJSON_CreateNumber((double)value);
}

static bool add(cJSON *object, const char *key, cJSON *item) {
    if (!item) {
        return false;
    }
    cJSON_AddItemToObject(object, key, item);
    return true;
}

static bool push(cJSON *array, cJSON *item) {
    if (!item) {
        return false;
    }
    cJSON_AddItemToArray(array, item);
    return true;
}

static cJSON *w_name(const SectionDescription *d, SectionUse use) {
    (void)use;
    return w_string(d->name);
}

static cJSON *w_id(const SectionDescription *d, SectionUse use) {
    (void)use;
    return w_string(d->id);
}

static cJSON *w_letter(const SectionDescription *d, SectionUse use) {
    (void)use;
    return w_string(d->letter);
}

static cJSON *w_role(const SectionDescription *d, SectionUse use) {
    (void)use;
    return w_string(section_role_name(d->role));
}

static cJSON *w_bars(const SectionDescription *d, SectionUse use) {
    (void)use;
    return w_whole(d->bars);
}

static cJSON *w_beats(const SectionDescription *d, SectionUse use) {
    (void)use;
    return w_whole(d->beats);
}

static cJSON *w_pickup(const SectionDescription *d, SectionUse use) {
    char text[64];
    (void)use;
    if (d->pickup.den == 1) {
        return cJSON_CreateNumber((double)d->pickup.num);
    }
    snprintf(text, sizeof(text), "%lld/%lld", d->pickup.num, d->pickup.den);
    return w_string(text);
}

static cJSON *w_beginning(const SectionDescription *d, SectionUse use) {
    (void)use;
    return w_string(section_beginning_name(d->pairing.beginning));
}

static cJSON *w_ending(const SectionDescription *d, SectionUse use) {
    (void)use;
    return w_string(section_ending_name(d->pairing.ending));
}

static cJSON *cadence_json(CadenceType core, const char *named) {
    const HarmonyCadenceKind *kind = harmony_cadence_kind_of(core);
    return w_string(named && named[0] ? named : kind ? kind->name : "unknown");
}

static cJSON *w_cadence(const SectionDescription *d, SectionUse use) {
    (void)use;
    return cadence_json(d->cadence, d->cadence_name);
}

static cJSON *w_first_cadence(const SectionDescription *d, SectionUse use) {
    (void)use;
    return cadence_json(d->first_cadence, NULL);
}

static cJSON *w_closed(const SectionDescription *d, SectionUse use) {
    (void)d;
    (void)use;
    return w_true();
}

static cJSON *region_json(const SectionRegion *region) {
    cJSON *object;
    switch (region->kind) {
    case SECTION_REGION_HOME:
        return w_string("home");
    case SECTION_REGION_FORM:
        return w_string(region->form_region);
    case SECTION_REGION_KEY:
        object = cJSON_CreateObject();
        if (!object || !add(object, "tonic", w_string(region->tonic)) || !add(object, "material", w_string(region->material))) {
            cJSON_Delete(object);
            return NULL;
        }
        return object;
    case SECTION_REGION_RELATION:
        object = cJSON_CreateObject();
        if (!object || !add(object, "relation", w_string(region->relation)) ||
            (region->anchor == SECTION_ANCHOR_PREVIOUS && !add(object, "anchor", w_string("previous"))) ||
            (region->anchor == SECTION_ANCHOR_SECTION && !add(object, "anchor", w_string(region->anchor_section)))) {
            cJSON_Delete(object);
            return NULL;
        }
        return object;
    case SECTION_REGION_NONE:
        break;
    }
    return NULL;
}

static cJSON *w_keys(const SectionDescription *d, SectionUse use) {
    cJSON *object = cJSON_CreateObject();
    (void)use;
    if (!object || (d->key_start.kind != SECTION_REGION_NONE && !add(object, "start", region_json(&d->key_start))) ||
        (d->key_end.kind != SECTION_REGION_NONE && !add(object, "end", region_json(&d->key_end)))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

static cJSON *w_region(const SectionDescription *d, SectionUse use) {
    (void)use;
    return region_json(&d->key_end);
}

static cJSON *w_motive(const SectionDescription *d, SectionUse use) {
    (void)use;
    return w_string(d->motive);
}

static cJSON *w_returns(const SectionDescription *d, SectionUse use) {
    (void)use;
    return w_string(d->returns);
}

static cJSON *w_variation(const SectionDescription *d, SectionUse use) {
    (void)use;
    return w_string(section_return_variation_name(d->variation));
}

static cJSON *w_setting(const SectionDescription *d, SectionUse use) {
    (void)use;
    return w_string(section_setting_name(d->setting));
}

static cJSON *w_mode(const SectionDescription *d, SectionUse use) {
    (void)use;
    return w_string(section_mode_name(d->mode));
}

static cJSON *w_layers(const SectionDescription *d, SectionUse use) {
    (void)use;
    if (d->layers == SECTION_LAYERS_ALL) {
        return w_string("all");
    }
    cJSON *list = cJSON_CreateArray();
    for (int lane = 0; list && lane < SECTION_LANE_COUNT; lane++) {
        if ((d->layers & SECTION_LAYER_BIT(lane)) && !push(list, w_string(section_lane_name((SectionLane)lane)))) {
            cJSON_Delete(list);
            return NULL;
        }
    }
    return list;
}

static cJSON *w_pedal(const SectionDescription *d, SectionUse use) {
    (void)d;
    (void)use;
    return w_string("dominant");
}

static cJSON *w_leads_in(const SectionDescription *d, SectionUse use) {
    (void)d;
    (void)use;
    return w_true();
}

static cJSON *w_connective(const SectionDescription *d, SectionUse use) {
    (void)use;
    return w_string(section_connective_name(d->connective));
}

static cJSON *w_connective_written(const SectionDescription *d, SectionUse use) {
    (void)d;
    (void)use;
    return w_true();
}

static cJSON *w_construction(const SectionDescription *d, SectionUse use) {
    const SectionConstruction *c = &d->construction;
    cJSON *object;
    cJSON *ideas;
    if (d->construction_auto) {
        return w_string("auto");
    }
    object = cJSON_CreateObject();
    ideas = cJSON_CreateObject();
    if (!object || !ideas) {
        cJSON_Delete(object);
        cJSON_Delete(ideas);
        return NULL;
    }
    for (int idea = 0; idea < SECTION_CONSTRUCTION_IDEAS; idea++) {
        if (c->idea_bars[idea] != 0 && !add(ideas, section_idea_name((SectionIdea)idea), w_whole(c->idea_bars[idea]))) {
            goto fail;
        }
    }
    if (!add(object, "ideas", ideas)) {
        cJSON_Delete(ideas);
        goto fail;
    }
    if (c->extension_count > 0) {
        cJSON *list = cJSON_CreateArray();
        if (!add(object, "extensions", list)) {
            goto fail;
        }
        for (size_t k = 0; k < c->extension_count; k++) {
            const SectionExtension *x = &c->extensions[k];
            cJSON *extension = cJSON_CreateObject();
            if (!extension || !push(list, extension) || !add(extension, "kind", w_string(section_extension_kind_name(x->kind))) ||
                !add(extension, "idea", w_string(section_idea_name((SectionIdea)x->idea))) ||
                !add(extension, "bars", w_whole(x->bars)) || !add(extension, "varied", x->varied ? cJSON_CreateTrue() : cJSON_CreateFalse())) {
                goto fail;
            }
        }
    }
    if (use == USE_P && c->reason != SECTION_CONSTRUCTION_GIVEN &&
        !add(object, "reason", w_string(section_construction_reason_name(c->reason)))) {
        goto fail;
    }
    return object;
fail:
    cJSON_Delete(object);
    return NULL;
}

static cJSON *w_figure(const SectionDescription *d, SectionUse use) {
    const SectionFigure *f = &d->figure;
    cJSON *object = cJSON_CreateObject();
    (void)use;
    if (!object || !add(object, "kind", w_string(section_figure_kind_name(f->kind))) ||
        !add(object, "rhythm", w_string(section_figure_rhythm_name(f->rhythm))) ||
        (f->reharmonize != (d->mode == SECTION_MODE_PARALLEL) &&
         !add(object, "harmony", w_string(f->reharmonize ? "reharmonized" : "kept"))) ||
        (f->change_at != 0 && !add(object, "change_at", w_whole(f->change_at))) ||
        (f->climax && !add(object, "climax", w_true()))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

static cJSON *w_function(const SectionDescription *d, SectionUse use) {
    (void)use;
    return w_string(section_function_kind_name(d->function.function));
}

static cJSON *w_independent(const SectionDescription *d, SectionUse use) {
    (void)d;
    (void)use;
    return w_true();
}

static cJSON *w_lyric(const SectionDescription *d, SectionUse use) {
    (void)d;
    (void)use;
    return w_true();
}

static cJSON *w_free_phrase(const SectionDescription *d, SectionUse use) {
    (void)d;
    (void)use;
    return w_true();
}

static cJSON *w_harmony(const SectionDescription *d, SectionUse use) {
    const SectionHarmony *h = &d->harmony;
    cJSON *object = cJSON_CreateObject();
    (void)use;
    if (!object) {
        return NULL;
    }
    switch (h->kind) {
    case SECTION_HARMONY_PROGRESSION:
        if (!add(object, "progression", w_string(h->numerals)) || (h->cells != 0 && !add(object, "cells", w_whole(h->cells)))) {
            goto fail;
        }
        break;
    case SECTION_HARMONY_SCHEMA:
        if (!add(object, "schema", w_string(h->schema))) {
            goto fail;
        }
        break;
    case SECTION_HARMONY_CHORDS: {
        cJSON *list = cJSON_CreateArray();
        if (!add(object, "chords", list)) {
            goto fail;
        }
        for (size_t i = 0; i < h->chord_count; i++) {
            cJSON *chord = cJSON_CreateObject();
            if (!chord || !push(list, chord) || !add(chord, "bar", w_whole(h->chords[i].bar)) ||
                !add(chord, "text", w_string(h->chords[i].text))) {
                goto fail;
            }
        }
        break;
    }
    case SECTION_HARMONY_NONE:
        break;
    }
    return object;
fail:
    cJSON_Delete(object);
    return NULL;
}

/* ------------------------------------------------------------- presence */

static bool p_name(const SectionDescription *d) { return d->name[0] != '\0'; }
static bool p_id(const SectionDescription *d) { return d->id[0] != '\0'; }
static bool p_letter(const SectionDescription *d) { return d->letter[0] != '\0'; }
static bool p_role(const SectionDescription *d) { return d->has_role; }
static bool p_bars(const SectionDescription *d) { return d->bars != 0; }
static bool p_beats(const SectionDescription *d) { return d->beats != 0; }
static bool p_pickup(const SectionDescription *d) { return d->has_pickup; }
static bool p_pairing(const SectionDescription *d) { return d->has_pairing; }
static bool p_cadence(const SectionDescription *d) { return d->has_cadence; }
static bool p_first_cadence(const SectionDescription *d) { return d->has_first_cadence; }
static bool p_closed(const SectionDescription *d) { return d->closed; }
static bool p_keys(const SectionDescription *d) {
    return d->key_start.kind != SECTION_REGION_NONE || d->key_end.kind != SECTION_REGION_NONE;
}
static bool p_region(const SectionDescription *d) { return d->key_end.kind != SECTION_REGION_NONE; }
static bool p_motive(const SectionDescription *d) { return d->motive[0] != '\0'; }
static bool p_returns(const SectionDescription *d) { return d->returns[0] != '\0'; }
static bool p_variation(const SectionDescription *d) { return d->variation != SECTION_RETURN_EXACT; }
static bool p_setting(const SectionDescription *d) { return d->setting != SECTION_SETTING_NONE; }
static bool p_mode(const SectionDescription *d) { return d->mode != SECTION_MODE_NONE; }
static bool p_layers(const SectionDescription *d) { return d->layers != 0; }
static bool p_pedal(const SectionDescription *d) { return d->pedal != 0; }
static bool p_leads_in(const SectionDescription *d) { return d->leads_in; }
static bool p_connective(const SectionDescription *d) { return d->connective != SECTION_CONNECTIVE_NONE; }
static bool p_connective_written(const SectionDescription *d) { return d->connective_written; }
static bool p_construction(const SectionDescription *d) { return d->has_construction; }
static bool p_figure(const SectionDescription *d) { return d->has_figure; }
static bool p_function(const SectionDescription *d) { return d->has_function; }
static bool p_independent(const SectionDescription *d) { return d->function.independent; }
static bool p_lyric(const SectionDescription *d) { return d->function.lyric; }
static bool p_free_phrase(const SectionDescription *d) { return d->free_phrase; }
static bool p_harmony(const SectionDescription *d) { return d->harmony.kind != SECTION_HARMONY_NONE; }

/* ------------------------------------------------------------- the table */

/* One row per member, in the order they are written. Nothing else in this file names a member. */
const Member section_json_members[] = {
    {"name", USE_F | USE_O, p_name, read_name, w_name},
    {"id", USE_P | USE_R, p_id, read_id, w_id},
    {"letter", USE_F | USE_P | USE_R, p_letter, read_letter, w_letter},
    {"role", USE_F | USE_P | USE_R, p_role, read_role, w_role},
    {"bars", USE_F | USE_R, p_bars, read_bars, w_bars},
    {"beats", USE_F | USE_R, p_beats, read_beats, w_beats},
    {"pickup", USE_R, p_pickup, read_pickup, w_pickup},
    {"beginning", USE_F | USE_P, p_pairing, read_beginning, w_beginning},
    {"ending", USE_F | USE_P, p_pairing, read_ending, w_ending},
    {"cadence", USE_F | USE_P | USE_R, p_cadence, read_cadence, w_cadence},
    {"first_cadence", USE_P, p_first_cadence, read_first_cadence, w_first_cadence},
    {"closed", USE_F | USE_P, p_closed, read_closed, w_closed},
    {"keys", USE_F | USE_P, p_keys, read_keys, w_keys},
    {"region", USE_R | USE_O, p_region, read_region_member, w_region},
    {"motive", USE_F, p_motive, read_motive, w_motive},
    {"returns", USE_F | USE_P, p_returns, read_returns, w_returns},
    {"variation", USE_F | USE_P, p_variation, read_variation, w_variation},
    {"setting", USE_F | USE_P, p_setting, read_setting, w_setting},
    {"mode", USE_F | USE_P, p_mode, read_mode, w_mode},
    {"layers", USE_F | USE_P, p_layers, read_layers, w_layers},
    {"pedal", USE_F | USE_P, p_pedal, read_pedal, w_pedal},
    {"leads_in", USE_F | USE_P, p_leads_in, read_leads_in, w_leads_in},
    {"connective", USE_F | USE_P, p_connective, read_connective, w_connective},
    {"connective_written", USE_P, p_connective_written, read_connective_written, w_connective_written},
    {"construction", USE_F | USE_P, p_construction, read_construction, w_construction},
    {"figure", USE_F | USE_P, p_figure, read_figure, w_figure},
    {"function", USE_P | USE_R, p_function, read_function, w_function},
    {"independent", USE_F | USE_P, p_independent, read_independent, w_independent},
    {"lyric", USE_F | USE_P, p_lyric, read_lyric, w_lyric},
    {"free_phrase", USE_P, p_free_phrase, read_free_phrase, w_free_phrase},
    {"harmony", USE_P | USE_R | USE_O, p_harmony, read_harmony, w_harmony},
};

const size_t section_json_member_total = COUNT_OF(section_json_members);

static const Member *member_find(const char *name) {
    for (size_t i = 0; name && i < section_json_member_total; i++) {
        if (strcmp(section_json_members[i].name, name) == 0) {
            return &section_json_members[i];
        }
    }
    return NULL;
}

static bool skipped(const char *name, const char *const *skip, size_t skip_count) {
    for (size_t i = 0; skip && name && i < skip_count; i++) {
        if (strcmp(skip[i], name) == 0) {
            return true;
        }
    }
    return false;
}

/* ------------------------------------------------------------- the entry points */

/* In a form, the role of a transition, retransition, subordinate theme or coda sets the function of that name. */
static void set_implied_function(SectionDescription *d) {
    static const struct {
        SectionRole role;
        SectionFunctionKind function;
    } IMPLIED[] = {{SECTION_ROLE_TRANSITION, SECTION_FUNCTION_TRANSITION},
                   {SECTION_ROLE_RETRANSITION, SECTION_FUNCTION_RETRANSITION},
                   {SECTION_ROLE_SUBORDINATE, SECTION_FUNCTION_SUBORDINATE},
                   {SECTION_ROLE_CODA, SECTION_FUNCTION_CODA}};
    for (size_t i = 0; d->has_role && i < COUNT_OF(IMPLIED); i++) {
        if (d->role == IMPLIED[i].role) {
            d->has_function = true;
            d->function.function = IMPLIED[i].function;
        }
    }
}

bool section_from_json(const cJSON *object, SectionUse use, const char *const *skip, size_t skip_count,
                       SectionDescription *out, SectionError *error) {
    Reader reader = {use, error, false, false, false};
    section_description_init(out);
    if (!cJSON_IsObject(object)) {
        return fail(&reader, "", "must be an object");
    }
    /* every member is known, allowed for the use and given once */
    for (const cJSON *child = object->child; child; child = child->next) {
        char at[96];
        const Member *member;
        if (skipped(child->string, skip, skip_count)) {
            continue;
        }
        join(at, sizeof(at), "", child->string ? child->string : "");
        member = member_find(child->string);
        if (!member) {
            return fail(&reader, at, "unknown member \"%.60s\"", child->string ? child->string : "");
        }
        if (!(member->uses & (unsigned)use)) {
            return fail(&reader, at, "\"%.60s\" is not given here", child->string);
        }
        for (const cJSON *other = child->next; other; other = other->next) {
            if (other->string && strcmp(other->string, child->string) == 0) {
                return fail(&reader, at, "member given twice");
            }
        }
    }
    for (size_t i = 0; i < section_json_member_total; i++) {
        const cJSON *item;
        if ((section_json_members[i].uses & (unsigned)use) == 0 || skipped(section_json_members[i].name, skip, skip_count)) {
            continue;
        }
        item = cJSON_GetObjectItemCaseSensitive(object, section_json_members[i].name);
        if (item && !section_json_members[i].read(&reader, item, out)) {
            return false;
        }
    }
    /* a pairing is both halves; one alone is refused after the rules that would refuse the section anyway (a liquidation has none) */
    const bool half = reader.seen_beginning != reader.seen_ending;
    if (use == USE_F && out->has_role && section_role_is_operator(out->role) && (reader.seen_beginning || reader.seen_ending)) {
        return fail(&reader, reader.seen_beginning ? "/beginning" : "/ending", "a %s has no beginning or ending",
                    section_role_name(out->role));
    }
    out->has_pairing = reader.seen_beginning || reader.seen_ending;
    if (use == USE_F) {
        set_implied_function(out);
    }
    if (!section_description_check(out, use, error)) {
        return false;
    }
    if (half) {
        return fail(&reader, reader.seen_beginning ? "/ending" : "/beginning", "required member is missing");
    }
    return true;
}

cJSON *section_to_json(const SectionDescription *description, SectionUse use) {
    cJSON *object = cJSON_CreateObject();
    if (!object) {
        return NULL;
    }
    for (size_t i = 0; i < section_json_member_total; i++) {
        if ((section_json_members[i].uses & (unsigned)use) == 0 || !section_json_members[i].present(description)) {
            continue;
        }
        /* a form's function comes from its role, so it is not written */
        if (!add(object, section_json_members[i].name, section_json_members[i].write(description, use))) {
            cJSON_Delete(object);
            return NULL;
        }
    }
    return object;
}
