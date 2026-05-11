#include "theme.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>

#include "cli_options.h"
#include "grow.h"

#include <ursatz_composer/fill_record.h>
#include <ursatz_composer/grow.h>
#include <ursatz_composer/grow_form.h>
#include <ursatz_composer/motive.h>

/* At most this many motives in a request (one for each slot), forms for each, and arguments for grow. */
#define THEME_MOTIVES 4
#define THEME_FORMS 64
#define THEME_ARGS 80
#define THEME_ROWS 16

#define SCHEMA_NAME "ursatz-theme-request"

typedef struct {
    const cJSON *json;
    const char *id;
    bool referenced;
} RequestMotive;

typedef struct {
    GrowExt base; /* first: run_grow_ext gets a pointer to it */
    cJSON *root;
    RequestMotive motives[THEME_MOTIVES];
    size_t motive_count;
    const cJSON *slot_motive[GROW_SLOT_COUNT]; /* the request motive of each slot, NULL for none */
    const char *slot_motive_id[GROW_SLOT_COUNT];
    const cJSON *accompaniment_motive; /* the "motive" member of an accompaniment of pattern "motive" */
    GrowPoolForm *pool;
    Motive *pool_motives;
    size_t pool_total; /* forms the request holds, for the storage */
} ThemeExt;

static const char *const SLOT_KEYS[GROW_SLOT_COUNT] = {"basic", "contrasting", "continuation", "cadential"};

/* --- the request --------------------------------------------------------------- */

static char *read_whole_file(const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        return NULL;
    }
    char *text = NULL;
    if (fseek(file, 0, SEEK_END) == 0) {
        long length = ftell(file);
        if (length >= 0 && fseek(file, 0, SEEK_SET) == 0) {
            text = malloc((size_t)length + 1);
            if (text && fread(text, 1, (size_t)length, file) == (size_t)length) {
                text[length] = '\0';
            } else {
                free(text);
                text = NULL;
            }
        }
    }
    fclose(file);
    return text;
}

/* A member that is a string (or null / absent when optional). Returns false, with the error printed, for any other type. */
static bool member_string(const cJSON *object, const char *name, bool required, const char **out) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);
    *out = NULL;
    if (!item || cJSON_IsNull(item)) {
        if (required) {
            cli_fail(CLI_EXIT_USAGE, "the request needs \"%s\"", name);
            return false;
        }
        return true;
    }
    if (!cJSON_IsString(item)) {
        cli_fail(CLI_EXIT_USAGE, "\"%s\" in the request must be a string", name);
        return false;
    }
    *out = item->valuestring;
    return true;
}

/* A pickup written as a string ("0", "1/2") or a number of beats; formats a number into text. */
static bool member_pickup(const cJSON *object, char *text, size_t size, const char **out) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, "pickup");
    *out = NULL;
    if (!item || cJSON_IsNull(item)) {
        return true;
    }
    if (cJSON_IsString(item)) {
        *out = item->valuestring;
        return true;
    }
    if (cJSON_IsNumber(item)) {
        snprintf(text, size, "%g", item->valuedouble);
        *out = text;
        return true;
    }
    cli_fail(CLI_EXIT_USAGE, "\"pickup\" in the request must be a string or a number of beats");
    return false;
}

/* "0", "1", "1/2": beats of the beat unit, as whole notes. */
static bool parse_pickup(const char *text, int beat_unit, PlanTime *out) {
    char *end = NULL;
    long numerator = strtol(text, &end, 10);
    long denominator = 1;
    if (end == text || numerator < 0) {
        return false;
    }
    if (*end == '/') {
        const char *rest = end + 1;
        denominator = strtol(rest, &end, 10);
        if (end == rest || denominator < 1) {
            return false;
        }
    }
    return *end == '\0' && plan_time_make(numerator, (unsigned long)(denominator * beat_unit), out) == PLAN_OK;
}

/*
 * Reads a motive of the request, a degree list in the key (R-135), with its pickup, harmony, features and role. `role` is
 * the role to force (NULL for the motive's own member). Returns 0 or the status of the error printed.
 */
static int read_motive(const char *what, const cJSON *object, const char *role, const PlanMeter *meter,
                       const PlanKey *key, Motive *out) {
    const char *degrees;
    const char *harmony;
    const char *form;
    const char *role_text;
    const char *pickup;
    char pickup_text[32];
    if (!member_string(object, "degrees", true, &degrees) || !member_string(object, "harmony", false, &harmony) ||
        !member_string(object, "form", false, &form) || !member_string(object, "role", false, &role_text) ||
        !member_pickup(object, pickup_text, sizeof(pickup_text), &pickup)) {
        return CLI_EXIT_USAGE;
    }
    if (form && strcmp(form, "full") != 0) {
        return cli_fail(CLI_EXIT_USAGE, "%s: form must be \"full\", not \"%s\"", what, form);
    }
    MotiveOptions options;
    memset(&options, 0, sizeof(options));
    options.has_meter = true;
    options.meter = *meter;
    options.has_key = true;
    options.key = *key;
    if (pickup) {
        if (!parse_pickup(pickup, meter->beat_unit, &options.pickup)) {
            return cli_fail(CLI_EXIT_USAGE, "%s: pickup '%s' is not a number of beats; write 0, 1 or 1/2", what, pickup);
        }
        options.has_pickup = true;
    }
    MotiveError error;
    if (harmony && harmony[0]) {
        if (motive_harmony_parse(harmony, meter, options.harmony, &options.harmony_count, &error) != MOTIVE_OK) {
            return cli_fail(CLI_EXIT_USAGE, "%s: harmony '%s' at %s: %s", what, harmony, error.location, error.message);
        }
        options.has_harmony = true;
    }
    const cJSON *features = cJSON_GetObjectItemCaseSensitive(object, "features");
    if (features && !cJSON_IsNull(features)) {
        if (!cJSON_IsArray(features) || cJSON_GetArraySize(features) > MOTIVE_MAX_FEATURES) {
            return cli_fail(CLI_EXIT_USAGE, "%s: \"features\" must be a list of at most %d features", what,
                            MOTIVE_MAX_FEATURES);
        }
        const cJSON *item = NULL;
        cJSON_ArrayForEach(item, features) {
            const cJSON *label = cJSON_GetObjectItemCaseSensitive(item, "label");
            const cJSON *first = cJSON_GetObjectItemCaseSensitive(item, "first");
            const cJSON *last = cJSON_GetObjectItemCaseSensitive(item, "last");
            if (!cJSON_IsString(label) || strlen(label->valuestring) > MOTIVE_FEATURE_LABEL_MAX || !cJSON_IsNumber(first) ||
                !cJSON_IsNumber(last) || first->valuedouble < 0 || last->valuedouble < first->valuedouble) {
                return cli_fail(CLI_EXIT_USAGE, "%s: a feature is {\"label\", \"first\", \"last\"} with 0 <= first <= last", what);
            }
            MotiveFeature *feature = &options.features[options.feature_count++];
            snprintf(feature->label, sizeof(feature->label), "%s", label->valuestring);
            feature->first = (size_t)first->valuedouble;
            feature->last = (size_t)last->valuedouble;
        }
        options.has_features = options.feature_count > 0;
    }
    const char *wanted_role = role ? role : role_text;
    if (wanted_role) {
        if (!motive_role_parse(wanted_role, &options.role)) {
            return cli_fail(CLI_EXIT_USAGE, "%s: role must be melodic or accompaniment, not '%s'", what, wanted_role);
        }
        options.has_role = true;
    }
    if (motive_from_degrees_text(degrees, &options, out, &error) != MOTIVE_OK) {
        return cli_fail(CLI_EXIT_USAGE, "%s: cannot read the degrees '%s'%s%s: %s", what, degrees,
                        error.location[0] ? " at " : "", error.location, error.message);
    }
    return 0;
}

/* The bits of the functions a form lists; false, with the error printed, for a name that is not one. */
static bool read_functions(const char *what, const cJSON *functions, unsigned *out) {
    *out = 0;
    if (!functions || cJSON_IsNull(functions)) {
        return true;
    }
    if (!cJSON_IsArray(functions)) {
        cli_fail(CLI_EXIT_USAGE, "%s: \"functions\" must be a list", what);
        return false;
    }
    const cJSON *item = NULL;
    cJSON_ArrayForEach(item, functions) {
        const char *name = cJSON_IsString(item) ? item->valuestring : "";
        unsigned bit = strcmp(name, "beginning") == 0      ? GROW_FUNCTION_BEGINNING
                       : strcmp(name, "continuation") == 0 ? GROW_FUNCTION_CONTINUATION
                       : strcmp(name, "contrast") == 0     ? GROW_FUNCTION_CONTRAST
                       : strcmp(name, "cadential") == 0    ? GROW_FUNCTION_CADENTIAL
                                                           : 0;
        if (bit == 0) {
            cli_fail(CLI_EXIT_USAGE, "%s: function must be beginning, continuation, contrast or cadential, not '%s'", what, name);
            return false;
        }
        *out |= bit;
    }
    return true;
}

/* The slots: each names a motive of the request, no motive twice, "basic" required. */
static int read_slots(ThemeExt *ext, const cJSON *request) {
    const cJSON *slots = cJSON_GetObjectItemCaseSensitive(request, "slots");
    if (!cJSON_IsObject(slots)) {
        return cli_fail(CLI_EXIT_USAGE, "the request needs \"slots\": {\"basic\": <motive id>, ...}");
    }
    for (const cJSON *entry = slots->child; entry; entry = entry->next) {
        bool known = false;
        for (int s = 0; s < GROW_SLOT_COUNT; s++) {
            known = known || strcmp(entry->string, SLOT_KEYS[s]) == 0;
        }
        if (!known) {
            return cli_fail(CLI_EXIT_USAGE, "unknown slot '%s'; the slots are basic, contrasting, continuation and cadential",
                            entry->string);
        }
    }
    for (int s = 0; s < GROW_SLOT_COUNT; s++) {
        const cJSON *named = cJSON_GetObjectItemCaseSensitive(slots, SLOT_KEYS[s]);
        if (!named || cJSON_IsNull(named)) {
            if (s == GROW_SLOT_BASIC) {
                return cli_fail(CLI_EXIT_USAGE, "the request needs a basic slot");
            }
            continue;
        }
        if (!cJSON_IsString(named)) {
            return cli_fail(CLI_EXIT_USAGE, "slot '%s' must name a motive by its id", SLOT_KEYS[s]);
        }
        RequestMotive *found = NULL;
        for (size_t m = 0; m < ext->motive_count; m++) {
            if (strcmp(ext->motives[m].id, named->valuestring) == 0) {
                found = &ext->motives[m];
            }
        }
        if (!found) {
            return cli_fail(CLI_EXIT_USAGE, "slot '%s' names the motive '%s', which the request does not have", SLOT_KEYS[s],
                            named->valuestring);
        }
        if (found->referenced) {
            return cli_fail(CLI_EXIT_USAGE, "the motive '%s' is in two slots; a motive is in at most one", found->id);
        }
        found->referenced = true;
        ext->slot_motive[s] = found->json;
        ext->slot_motive_id[s] = found->id;
    }
    return 0;
}

/* Reads the motives and checks the forms; rejected forms are refused here, before anything is grown. */
static int read_motives(ThemeExt *ext, const cJSON *request) {
    const cJSON *motives = cJSON_GetObjectItemCaseSensitive(request, "motives");
    if (!cJSON_IsArray(motives) || cJSON_GetArraySize(motives) < 1 || cJSON_GetArraySize(motives) > THEME_MOTIVES) {
        return cli_fail(CLI_EXIT_USAGE, "the request needs \"motives\": 1 to %d motives", THEME_MOTIVES);
    }
    const cJSON *item = NULL;
    cJSON_ArrayForEach(item, motives) {
        const char *id;
        if (!cJSON_IsObject(item) || !member_string(item, "id", true, &id)) {
            return CLI_EXIT_USAGE;
        }
        for (size_t m = 0; m < ext->motive_count; m++) {
            if (strcmp(ext->motives[m].id, id) == 0) {
                return cli_fail(CLI_EXIT_USAGE, "the motive id '%s' is used twice", id);
            }
        }
        ext->motives[ext->motive_count].json = item;
        ext->motives[ext->motive_count].id = id;
        ext->motive_count++;
    }
    int status = read_slots(ext, request);
    if (status != 0) {
        return status;
    }
    for (int s = 0; s < GROW_SLOT_COUNT; s++) {
        if (!ext->slot_motive[s]) {
            continue;
        }
        const cJSON *forms = cJSON_GetObjectItemCaseSensitive(ext->slot_motive[s], "forms");
        if (!forms || cJSON_IsNull(forms)) {
            continue;
        }
        if (!cJSON_IsArray(forms) || cJSON_GetArraySize(forms) > THEME_FORMS) {
            return cli_fail(CLI_EXIT_USAGE, "the forms of motive '%s' must be a list of at most %d", ext->slot_motive_id[s],
                            THEME_FORMS);
        }
        const cJSON *form = NULL;
        cJSON_ArrayForEach(form, forms) {
            const char *flag;
            const char *item_id;
            if (!cJSON_IsObject(form) || !member_string(form, "item", true, &item_id) ||
                !member_string(form, "flag", true, &flag)) {
                return CLI_EXIT_USAGE;
            }
            if (strcmp(flag, "rejected") == 0) {
                return cli_fail(CLI_EXIT_USAGE, "the form '%s' of motive '%s' is rejected; leave it out of the request", item_id,
                                ext->slot_motive_id[s]);
            }
            if (strcmp(flag, "approved") != 0 && strcmp(flag, "unflagged") != 0) {
                return cli_fail(CLI_EXIT_USAGE, "the flag of form '%s' must be approved or unflagged, not '%s'", item_id, flag);
            }
            ext->pool_total++;
        }
    }
    return 0;
}

static int theme_load(GrowExt *base, const PlanMeter *meter, const PlanKey *key) {
    ThemeExt *ext = (ThemeExt *)base;
    char what[96];
    int status;
    for (int s = 0; s < GROW_SLOT_COUNT; s++) {
        if (!ext->slot_motive[s]) {
            continue;
        }
        snprintf(what, sizeof(what), "motive '%s'", ext->slot_motive_id[s]);
        Motive *target = s == GROW_SLOT_BASIC ? &ext->base.basic : &ext->base.slot_motives[s];
        status = read_motive(what, ext->slot_motive[s], NULL, meter, key, target);
        if (status != 0) {
            return status;
        }
        if (target->role != MOTIVE_ROLE_MELODIC) {
            return cli_fail(CLI_EXIT_USAGE, "%s is of the accompaniment role; a slot takes a melodic motive", what);
        }
        ext->base.has_slot[s] = s != GROW_SLOT_BASIC;
    }
    if (ext->accompaniment_motive) {
        status = read_motive("the accompaniment motive", ext->accompaniment_motive, "accompaniment", meter, key,
                             &ext->base.accompaniment_motive);
        if (status != 0) {
            return status;
        }
        ext->base.has_accompaniment_motive = true;
    }
    if (ext->pool_total == 0) {
        return 0;
    }
    ext->pool = calloc(ext->pool_total, sizeof(*ext->pool));
    ext->pool_motives = calloc(ext->pool_total, sizeof(*ext->pool_motives));
    if (!ext->pool || !ext->pool_motives) {
        return cli_fail(CLI_EXIT_UNSATISFIABLE, "out of memory");
    }
    size_t count = 0;
    for (int s = 0; s < GROW_SLOT_COUNT; s++) {
        const cJSON *forms = ext->slot_motive[s] ? cJSON_GetObjectItemCaseSensitive(ext->slot_motive[s], "forms") : NULL;
        const cJSON *form = NULL;
        cJSON_ArrayForEach(form, forms) {
            const char *item_id = cJSON_GetObjectItemCaseSensitive(form, "item")->valuestring;
            const char *flag = cJSON_GetObjectItemCaseSensitive(form, "flag")->valuestring;
            const cJSON *chain = cJSON_GetObjectItemCaseSensitive(form, "chain");
            snprintf(what, sizeof(what), "form '%s'", item_id);
            status = read_motive(what, form, NULL, meter, key, &ext->pool_motives[count]);
            if (status != 0) {
                return status;
            }
            GrowPoolForm *entry = &ext->pool[count];
            entry->motive = &ext->pool_motives[count];
            entry->slot_motive_of = (GrowSlot)s;
            entry->approved = strcmp(flag, "approved") == 0;
            snprintf(entry->item, sizeof(entry->item), "%s", item_id);
            snprintf(entry->chain, sizeof(entry->chain), "%s", cJSON_IsString(chain) ? chain->valuestring : "");
            if (!read_functions(what, cJSON_GetObjectItemCaseSensitive(form, "functions"), &entry->functions)) {
                return CLI_EXIT_USAGE;
            }
            count++;
        }
    }
    ext->base.pool = ext->pool;
    ext->base.pool_count = count;
    return 0;
}

/* --- the output ------------------------------------------------------------------ */

static void print_number(const ComposerRatio *ratio) {
    if (ratio->denominator == 1) {
        printf("%lu", ratio->numerator);
    } else {
        printf("%g", (double)ratio->numerator / (double)ratio->denominator);
    }
}

static void theme_print_brief(const GrowExt *base, const GrowBrief *brief, bool json) {
    (void)base;
    if (json) {
        printf("\"brief\":{\"tempo\":{\"qpm\":");
        if (brief->tempo.denominator > 0) {
            print_number(&brief->tempo);
        } else {
            printf("null");
        }
        printf(",\"from\":\"%s\"},\"mode\":{\"mode\":\"%s\",\"from\":\"%s\"},\"density\":", brief->tempo_from,
               brief->minor ? "minor" : "major", brief->mode_from);
        if (brief->has_density) {
            print_number(&brief->density);
        } else {
            printf("null");
        }
        printf(",\"without\":[");
        for (size_t i = 0; i < brief->without_count; i++) {
            printf("%s", i ? "," : "");
            cli_print_json_string(brief->without[i]);
        }
        printf("]}");
        return;
    }
    printf("brief: mode %s (%s)", brief->minor ? "minor" : "major", brief->mode_from);
    if (brief->tempo.denominator > 0) {
        printf(", tempo ");
        print_number(&brief->tempo);
        printf(" (%s)", brief->tempo_from);
    }
    if (brief->has_density) {
        printf(", density ");
        print_number(&brief->density);
        printf(" attacks per beat");
    }
    if (brief->without_count > 0) {
        printf(", without");
        for (size_t i = 0; i < brief->without_count; i++) {
            printf("%s %s", i ? "," : "", brief->without[i]);
        }
    }
    printf("\n");
}

/* The slot an idea has the function of, by the idea's name (grow_form.c has the same table). */
static GrowSlot idea_slot(const char *name) {
    static const struct {
        const char *name;
        GrowSlot slot;
    } NAMES[] = {{"basic idea", GROW_SLOT_BASIC},
                 {"repetition", GROW_SLOT_BASIC},
                 {"consequent basic idea", GROW_SLOT_BASIC},
                 {"contrasting idea", GROW_SLOT_CONTRASTING},
                 {"consequent contrasting idea", GROW_SLOT_CONTRASTING},
                 {"fragment 1", GROW_SLOT_CONTINUATION},
                 {"fragment 2", GROW_SLOT_CONTINUATION},
                 {"cadential", GROW_SLOT_CADENTIAL},
                 {"cadential 1", GROW_SLOT_CADENTIAL},
                 {"cadential 2", GROW_SLOT_CADENTIAL}};
    for (size_t i = 0; name && i < sizeof(NAMES) / sizeof(NAMES[0]); i++) {
        if (strcmp(name, NAMES[i].name) == 0) {
            return NAMES[i].slot;
        }
    }
    return GROW_SLOT_BASIC;
}

typedef struct {
    const char *idea;
    GrowSlot slot;
    GrowSlot origin; /* the slot whose motive the idea is made from */
    const char *source;
    char item[64];
    char chain[256];
    long connection; /* percent, -1 for none */
} Row;

/* The percent in a form's label ("... 87%: ..."), else -1; and where the text before it ends. */
static long label_percent(const char *label, size_t *before) {
    const char *mark = strstr(label, "%:");
    if (!mark) {
        return -1;
    }
    const char *start = mark;
    while (start > label && start[-1] >= '0' && start[-1] <= '9') {
        start--;
    }
    if (start == mark) {
        return -1;
    }
    *before = (size_t)(start - label);
    while (*before > 0 && label[*before - 1] == ' ') {
        (*before)--;
    }
    return strtol(start, NULL, 10);
}

/* The step of the motives stage that wrote the motive node, else NULL. */
static const FillConsidered *chosen_for(const GrowOutcome *outcome, NodeId node) {
    const FillRecord *record = outcome->records[GROW_STAGE_MOTIVES];
    for (size_t i = 0; record && i < record->step_count; i++) {
        const FillStep *step = &record->steps[i];
        if (step->target.kind == FILL_TARGET_NODE && step->target.id == node && step->chosen >= 0) {
            return &step->considered[step->chosen];
        }
    }
    return NULL;
}

static void describe_row(const ThemeExt *ext, const GrowOutcome *outcome, NodeId motive_node, Row *row) {
    const FillConsidered *chosen = chosen_for(outcome, motive_node);
    row->source = "motive";
    row->connection = -1;
    if (!chosen) {
        return;
    }
    size_t before = 0;
    row->connection = label_percent(chosen->label, &before);
    if (strcmp(chosen->operator_name, GROW_FORM_OPERATOR_NAME) != 0) {
        row->source = "operator";
        snprintf(row->chain, sizeof(row->chain), "%s", chosen->label);
    } else if (strncmp(chosen->label, "pool ", 5) == 0) {
        row->source = "pool";
        const char *item = chosen->label + 5;
        size_t length = strcspn(item, " ");
        snprintf(row->item, sizeof(row->item), "%.*s", (int)length, item);
        for (size_t i = 0; i < ext->base.pool_count; i++) {
            if (strcmp(ext->pool[i].item, row->item) == 0) {
                snprintf(row->chain, sizeof(row->chain), "%s", ext->pool[i].chain);
            }
        }
    } else if (strncmp(chosen->label, "exact ", 6) == 0) {
        row->source = "motive";
    } else {
        row->source = "registry";
        snprintf(row->chain, sizeof(row->chain), "%.*s", (int)(before < sizeof(row->chain) - 1 ? before : sizeof(row->chain) - 1),
                 chosen->label);
    }
}

/* The ideas of the theme, in order, with the motive each is made from and how its motive was made. */
static size_t collect_rows(const ThemeExt *ext, const PiecePlan *plan, const GrowOutcome *outcome, Row *rows) {
    NodeId theme = cli_find_node(plan, "theme");
    size_t count = 0;
    if (theme == PLAN_NODE_NONE) {
        return 0;
    }
    PlanSpan span = plan_node_span(plan, theme);
    for (NodeId id = 1; id <= plan_node_count(plan) && count < THEME_ROWS; id++) {
        if (plan_node_kind(plan, id) != PLAN_NODE_IDEA || !plan_span_contains(span, plan_node_span(plan, id))) {
            continue;
        }
        LaneSpanId lanes[16];
        size_t found = plan_node_lane_spans(plan, id, lanes, 16);
        for (size_t i = 0; i < found && i < 16; i++) {
            if (plan_lane_span_lane(plan, lanes[i]) != PLAN_LANE_MELODY || plan_lane_span_voice(plan, lanes[i]) != 0) {
                continue;
            }
            for (size_t k = 0; k < plan_lane_span_constraint_count(plan, lanes[i]); k++) {
                const PlanConstraint *constraint = plan_lane_span_constraint_at(plan, lanes[i], k);
                if (constraint->kind != PLAN_CONSTRAINT_MOTIVE_REF) {
                    continue;
                }
                Row *row = &rows[count];
                memset(row, 0, sizeof(*row));
                row->idea = plan_node_name(plan, id);
                row->slot = idea_slot(row->idea);
                row->origin = GROW_SLOT_BASIC;
                /* The idea's melody asks for its motive node (m1 to m4); that node is made from m0 or a slot's node. */
                NodeId made_from = constraint->params.motive.motive;
                for (size_t c = 0; c < plan_node_constraint_count(plan, made_from); c++) {
                    const PlanConstraint *ref = plan_node_constraint_at(plan, made_from, c);
                    if (ref->kind == PLAN_CONSTRAINT_MOTIVE_REF) {
                        made_from = ref->params.motive.motive;
                    }
                }
                const char *node_name = plan_node_name(plan, made_from);
                /* The slot motives are the material nodes s1 (contrasting), s2 (continuation) and s3 (cadential). */
                for (GrowSlot own = GROW_SLOT_CONTRASTING; node_name && own < GROW_SLOT_COUNT; own++) {
                    if (node_name[0] == 's' && node_name[1] == (char)('0' + own) && node_name[2] == '\0') {
                        row->origin = own;
                    }
                }
                const char *direct = plan_node_name(plan, constraint->params.motive.motive);
                if (direct && strcmp(direct, "m0") == 0) {
                    row->source = "motive"; /* the basic idea is the basic motive */
                    row->connection = -1;
                } else {
                    describe_row(ext, outcome, constraint->params.motive.motive, row);
                }
                count++;
                break;
            }
            break;
        }
    }
    return count;
}

static void theme_print_slots(const GrowExt *base, const PiecePlan *plan, const GrowOutcome *outcome, bool json) {
    const ThemeExt *ext = (const ThemeExt *)base;
    Row rows[THEME_ROWS];
    size_t count = collect_rows(ext, plan, outcome, rows);
    if (json) {
        printf("\"slots\":[");
        for (size_t i = 0; i < count; i++) {
            const char *motive = ext->slot_motive_id[rows[i].origin];
            printf("%s{\"idea\":", i ? "," : "");
            cli_print_json_string(rows[i].idea ? rows[i].idea : "");
            printf(",\"slot\":\"%s\",\"motive\":", grow_slot_name(rows[i].slot));
            cli_print_json_string(motive ? motive : "");
            printf(",\"source\":\"%s\",\"item\":", rows[i].source);
            if (rows[i].item[0]) {
                cli_print_json_string(rows[i].item);
            } else {
                printf("null");
            }
            printf(",\"chain\":");
            if (rows[i].chain[0]) {
                cli_print_json_string(rows[i].chain);
            } else {
                printf("null");
            }
            printf(",\"connection\":");
            if (rows[i].connection >= 0) {
                printf("%ld", rows[i].connection);
            } else {
                printf("null");
            }
            printf("}");
        }
        printf("]");
        return;
    }
    printf("\nslots (the motive each idea is made from)\n");
    for (size_t i = 0; i < count; i++) {
        const char *motive = ext->slot_motive_id[rows[i].origin];
        printf("  %s: %s slot, motive %s, %s", rows[i].idea ? rows[i].idea : "?", grow_slot_name(rows[i].slot),
               motive ? motive : "?", rows[i].source);
        if (rows[i].item[0]) {
            printf(" %s", rows[i].item);
        }
        if (rows[i].chain[0]) {
            printf(" (%s)", rows[i].chain);
        }
        if (rows[i].connection >= 0) {
            printf(", connection %ld%%", rows[i].connection);
        }
        printf("\n");
    }
}

/* --- the command ------------------------------------------------------------------ */

typedef struct {
    bool help;
    bool check;
    bool plain_midi;
    const char *input;
    const char *seed;
    const char *suggest;
    const char *out;
    const char *midi;
    const char *musicxml;
    const char *explain;
    const char *format;
} Options;

static void print_help(FILE *out) {
    fprintf(out, "Usage:\n%s\n\n", THEME_USAGE_LINE + 7);
    fprintf(out, "Grows a theme from the motives of a request (JSON, schema %s): up to four motives, one for each slot of the\n", SCHEMA_NAME);
    fprintf(out, "theme (basic, contrasting, continuation and cadential), saved forms of them to prefer (approved over\n");
    fprintf(out, "unflagged, the registry's forms invented when none fits), and the brief: key, mode, meter, cadences, an\n");
    fprintf(out, "accompaniment or a voicing, and a style that gives the tempo, the mode and the note density. See\n");
    fprintf(out, "docs/theme-request.md.\n\n");
    fprintf(out, "Options (as grow's):\n");
    fprintf(out, "  --seed <n>           whole number from 0; the same seed gives the same output (default: from the clock)\n");
    fprintf(out, "  --suggest <n>        write n different themes (1 to 16); with files, theme k goes to <name>-k.<extension>\n");
    fprintf(out, "  --check              analyze the theme with the Ursatz Analyzer and pass over a seed whose analysis contradicts\n");
    fprintf(out, "                       the plan\n");
    fprintf(out, "  --out <file.json>    write the plan as JSON\n");
    fprintf(out, "  --midi <file>        write the melody, the bass and the accompaniment as a MIDI file (the tempo of the brief)\n");
    fprintf(out, "  --musicxml <file>    write them as MusicXML\n");
    fprintf(out, "  --plain-midi         MIDI: no track names and no section markers\n");
    fprintf(out, "  --explain <file>     write why each candidate was chosen or rejected; a pool form is named \"pool <item>\"\n");
    fprintf(out, "  --format <text|json> how to print the theme on stdout (default: text), with the slots and the brief\n");
    fprintf(out, "  --help               this text\n\n");
    fprintf(out, "Exit status: 0 success; 1 usage or an invalid request; 2 the request cannot be satisfied; 3 an output file\n");
    fprintf(out, "could not be written.\n");
}

static int read_options(int argc, char **argv, Options *options) {
    memset(options, 0, sizeof(*options));
    for (int i = 2; i < argc; i++) {
        const char *arg = argv[i];
        const char **target = NULL;
        if (strcmp(arg, "--help") == 0) {
            options->help = true;
        } else if (strcmp(arg, "--check") == 0) {
            options->check = true;
        } else if (strcmp(arg, "--plain-midi") == 0) {
            options->plain_midi = true;
        } else if (strcmp(arg, "--seed") == 0) {
            target = &options->seed;
        } else if (strcmp(arg, "--suggest") == 0) {
            target = &options->suggest;
        } else if (strcmp(arg, "--out") == 0) {
            target = &options->out;
        } else if (strcmp(arg, "--midi") == 0) {
            target = &options->midi;
        } else if (strcmp(arg, "--musicxml") == 0) {
            target = &options->musicxml;
        } else if (strcmp(arg, "--explain") == 0) {
            target = &options->explain;
        } else if (strcmp(arg, "--format") == 0) {
            target = &options->format;
        } else if (arg[0] == '-' && arg[1] != '\0') {
            return cli_fail(CLI_EXIT_USAGE, "unknown option '%s' (see ursatz-composer theme --help)", arg);
        } else if (!options->input) {
            options->input = arg;
        } else {
            return cli_fail(CLI_EXIT_USAGE, "unexpected argument '%s'; the input is one request file", arg);
        }
        if (target) {
            if (i + 1 >= argc) {
                return cli_fail(CLI_EXIT_USAGE, "%s needs a value", arg);
            }
            if (*target) {
                return cli_fail(CLI_EXIT_USAGE, "%s given twice", arg);
            }
            *target = argv[++i];
        }
    }
    return 0;
}

typedef struct {
    const char *items[THEME_ARGS];
    size_t count;
} Args;

static bool args_add(Args *args, const char *text) {
    if (args->count >= THEME_ARGS) {
        return false;
    }
    args->items[args->count++] = text;
    return true;
}

static bool args_pair(Args *args, const char *name, const char *value) {
    return args_add(args, name) && args_add(args, value);
}

/* grow's arguments for the request: the brief, and the options passed through. */
static int build_args(ThemeExt *ext, const cJSON *request, const Options *options, Args *args) {
    const char *into;
    const char *hybrid;
    const char *key;
    const char *mode;
    const char *meter;
    const char *cadence;
    const char *first_cadence;
    const char *voicing;
    const char *style;
    const char *tempo;
    const char *weight;
    if (!member_string(request, "into", true, &into) || !member_string(request, "hybrid", false, &hybrid) ||
        !member_string(request, "key", true, &key) || !member_string(request, "mode", true, &mode) ||
        !member_string(request, "meter", true, &meter) || !member_string(request, "cadence", false, &cadence) ||
        !member_string(request, "first_cadence", false, &first_cadence) ||
        !member_string(request, "voicing", false, &voicing) || !member_string(request, "style", false, &style) ||
        !member_string(request, "style_weight", false, &weight)) {
        return CLI_EXIT_USAGE;
    }
    const cJSON *tempo_item = cJSON_GetObjectItemCaseSensitive(request, "tempo");
    static char tempo_text[32];
    tempo = NULL;
    if (tempo_item && !cJSON_IsNull(tempo_item)) {
        if (cJSON_IsString(tempo_item)) {
            tempo = tempo_item->valuestring;
        } else if (cJSON_IsNumber(tempo_item)) {
            snprintf(tempo_text, sizeof(tempo_text), "%g", tempo_item->valuedouble);
            tempo = tempo_text;
        } else {
            return cli_fail(CLI_EXIT_USAGE, "\"tempo\" in the request must be a number of quarter notes per minute");
        }
    }
    bool ok = args_add(args, "ursatz-composer") && args_add(args, "grow") && args_pair(args, "--into", into) &&
              args_pair(args, "--key", key) && args_pair(args, "--mode", mode) && args_pair(args, "--meter", meter);
    if (hybrid) {
        ok = ok && args_pair(args, "--hybrid", hybrid);
    }
    static char bars_text[16];
    const cJSON *bars = cJSON_GetObjectItemCaseSensitive(request, "bars");
    if (bars && !cJSON_IsNull(bars)) {
        if (!cJSON_IsNumber(bars)) {
            return cli_fail(CLI_EXIT_USAGE, "\"bars\" in the request must be 4 or 8");
        }
        snprintf(bars_text, sizeof(bars_text), "%g", bars->valuedouble);
        ok = ok && args_pair(args, "--bars", bars_text);
    }
    if (cadence) {
        ok = ok && args_pair(args, "--cadence", cadence);
    }
    if (first_cadence) {
        ok = ok && args_pair(args, "--first-cadence", first_cadence);
    }
    const cJSON *accompaniment = cJSON_GetObjectItemCaseSensitive(request, "accompaniment");
    if (accompaniment && !cJSON_IsNull(accompaniment)) {
        const char *pattern;
        if (!cJSON_IsObject(accompaniment) || !member_string(accompaniment, "pattern", true, &pattern)) {
            return CLI_EXIT_USAGE;
        }
        if (strcmp(pattern, "motive") == 0) {
            ext->accompaniment_motive = cJSON_GetObjectItemCaseSensitive(accompaniment, "motive");
            if (!cJSON_IsObject(ext->accompaniment_motive)) {
                return cli_fail(CLI_EXIT_USAGE, "an accompaniment of pattern \"motive\" needs a \"motive\" member");
            }
            ext->base.accompaniment_motive_given = true;
        }
        ok = ok && args_pair(args, "--accompaniment", pattern);
    }
    if (voicing) {
        if (strcmp(voicing, "satb") == 0) {
            ok = ok && args_add(args, "--voicing");
        } else if (strcmp(voicing, "chorale") == 0) {
            ok = ok && args_pair(args, "--texture", "chorale");
        } else {
            return cli_fail(CLI_EXIT_USAGE, "\"voicing\" in the request must be satb or chorale, not '%s'", voicing);
        }
    }
    if (style) {
        ok = ok && args_pair(args, "--style", style);
        if (weight) {
            ok = ok && args_pair(args, "--style-weight", weight);
        }
    }
    const cJSON *without = cJSON_GetObjectItemCaseSensitive(request, "without");
    if (without && !cJSON_IsNull(without) && !(cJSON_IsArray(without) && cJSON_GetArraySize(without) == 0)) {
        if (!style) {
            return cli_fail(CLI_EXIT_USAGE, "\"without\" in the request needs a \"style\"");
        }
        if (!cJSON_IsArray(without)) {
            return cli_fail(CLI_EXIT_USAGE, "\"without\" in the request must be a list of categories");
        }
        const cJSON *category = NULL;
        cJSON_ArrayForEach(category, without) {
            if (!cJSON_IsString(category)) {
                return cli_fail(CLI_EXIT_USAGE, "\"without\" in the request must be a list of categories");
            }
            ok = ok && args_pair(args, "--style-without", category->valuestring);
        }
    }
    if (tempo) {
        ok = ok && args_pair(args, "--tempo", tempo);
    }
    if (options->seed) {
        ok = ok && args_pair(args, "--seed", options->seed);
    }
    if (options->suggest) {
        ok = ok && args_pair(args, "--suggest", options->suggest);
    }
    if (options->check) {
        ok = ok && args_add(args, "--check");
    }
    if (options->plain_midi) {
        ok = ok && args_add(args, "--plain-midi");
    }
    if (options->out) {
        ok = ok && args_pair(args, "--out", options->out);
    }
    if (options->midi) {
        ok = ok && args_pair(args, "--midi", options->midi);
    }
    if (options->musicxml) {
        ok = ok && args_pair(args, "--musicxml", options->musicxml);
    }
    if (options->explain) {
        ok = ok && args_pair(args, "--explain", options->explain);
    }
    if (options->format) {
        ok = ok && args_pair(args, "--format", options->format);
    }
    return ok ? 0 : cli_fail(CLI_EXIT_USAGE, "the request has too many style categories to leave out");
}

int run_theme(int argc, char **argv) {
    Options options;
    int status = read_options(argc, argv, &options);
    if (status != 0) {
        return status;
    }
    if (options.help) {
        print_help(stdout);
        return 0;
    }
    if (!options.input) {
        return cli_fail(CLI_EXIT_USAGE, "theme needs a request file\n%s", THEME_USAGE_LINE);
    }
    char *text = read_whole_file(options.input);
    if (!text) {
        return cli_fail(CLI_EXIT_USAGE, "cannot read the request '%s'", options.input);
    }
    ThemeExt ext;
    memset(&ext, 0, sizeof(ext));
    ext.root = cJSON_Parse(text);
    free(text);
    if (!ext.root || !cJSON_IsObject(ext.root)) {
        cJSON_Delete(ext.root);
        return cli_fail(CLI_EXIT_USAGE, "the request '%s' is not a JSON object", options.input);
    }
    const char *schema;
    const cJSON *version = cJSON_GetObjectItemCaseSensitive(ext.root, "schema_version");
    if (!member_string(ext.root, "schema", true, &schema) || strcmp(schema, SCHEMA_NAME) != 0 ||
        !cJSON_IsNumber(version) || version->valuedouble != 1) {
        status = cJSON_GetObjectItemCaseSensitive(ext.root, "schema") ? cli_fail(CLI_EXIT_USAGE,
                     "the request must have \"schema\": \"%s\" and \"schema_version\": 1", SCHEMA_NAME)
                                                                      : CLI_EXIT_USAGE;
        cJSON_Delete(ext.root);
        return status;
    }
    Args args;
    memset(&args, 0, sizeof(args));
    ext.base.load = theme_load;
    ext.base.print_theme = theme_print_slots;
    ext.base.print_brief = theme_print_brief;
    status = read_motives(&ext, ext.root);
    if (status == 0) {
        status = build_args(&ext, ext.root, &options, &args);
    }
    if (status == 0) {
        status = run_grow_ext((int)args.count, (char **)args.items, &ext.base);
    }
    free(ext.pool);
    free(ext.pool_motives);
    cJSON_Delete(ext.root);
    return status;
}
