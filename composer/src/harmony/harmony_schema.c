#include <ursatz_composer/harmony_schema.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <galant-schemas/galant_degree.h>

#include "harmony/harmony_internal.h"

#define DEFAULT_PREFERENCE 250
#define PREFERENCE_LIMIT 1000000
/* Bass lines tried for each schema before it is given up on. */
#define BASS_TRIES 4

HarmonySchemaParams harmony_schema_params_default(void) {
    HarmonySchemaParams params = {HARMONY_SCHEMA_ALL, HARMONY_SCHEMA_ROLE_ALL, DEFAULT_PREFERENCE,
                                  {0, 0, 0, 0}};
    params.bass = harmony_bass_params_default();
    return params;
}

static HarmonySchemaParams params_of(void *context) {
    return context ? *(const HarmonySchemaParams *)context : harmony_schema_params_default();
}

/* --- from the framework's stages to chords ------------------------------ */

typedef struct {
    const char *figures;
    bool seventh;
    int inversion;
} Figure;

/* The figured bass the prototypes use; a suspension (7-6) is read as the chord it resolves to. */
static const Figure FIGURES[] = {{"5", false, 0},   {"3", false, 0},     {"4", false, 0},     {"6", false, 1},
                                 {"6/4", false, 2}, {"7", true, 0},      {"7/#", true, 0},    {"6/5", true, 1},
                                 {"4/3", true, 2},  {"6/4/3", true, 2},  {"4/2", true, 3},    {"6/4/2", true, 3},
                                 {"7/4/2", true, 3}};

static bool read_figures(const char *figures, bool *seventh, int *inversion) {
    const char *f = figures ? figures : "5";
    const char *dash = strrchr(f, '-');
    if (dash) {
        f = dash + 1;
    }
    while (*f == '#' || *f == 'b') {
        f++;
    }
    for (size_t i = 0; i < sizeof(FIGURES) / sizeof(FIGURES[0]); i++) {
        if (strcmp(f, FIGURES[i].figures) == 0) {
            *seventh = FIGURES[i].seventh;
            *inversion = FIGURES[i].inversion;
            return true;
        }
    }
    return false;
}

/* The scale degree a Roman numeral names, whatever its case; 0 for anything else. */
static int numeral_degree(const char *numeral, size_t length) {
    static const char *const NAMES[7] = {"i", "ii", "iii", "iv", "v", "vi", "vii"};
    char lower[8];
    if (length == 0 || length >= sizeof(lower)) {
        return 0;
    }
    for (size_t i = 0; i < length; i++) {
        lower[i] = (char)tolower((unsigned char)numeral[i]);
    }
    lower[length] = '\0';
    for (int degree = 0; degree < 7; degree++) {
        if (strcmp(lower, NAMES[degree]) == 0) {
            return degree + 1;
        }
    }
    return 0;
}

/* Whether the chord's bass member is what the framework's stage asks for, by the framework's own matching. */
static bool bass_accepts(const PlanKey *key, const PhraseChord *chord, const GalantDegree *wanted) {
    int pc;
    if (!harmony_chord_bass_pc(key, chord, &pc)) {
        return false;
    }
    PitchSpelling *pitch = harmony_spell_pitch(key, harmony_member_degree(key->mode, chord), 48 + pc);
    PitchSpelling *tonic = harmony_spell_tonic(key, 3);
    GalantPitchDegree actual;
    bool ok = pitch && tonic && galant_degree_of(pitch, tonic, &actual) && galant_degree_allows(wanted, &actual);
    pitch_spelling_destroy(pitch);
    pitch_spelling_destroy(tonic);
    return ok;
}

/* The chord of one stage, one cell long, or false when the data does not make one consistent chord. */
static bool derive_stage(const PlanKey *key, const GalantStage *stage, PhraseChord *out) {
    PlanKeyMode mode = key->mode;
    bool seventh;
    int inversion;
    if (!read_figures(stage->figures, &seventh, &inversion)) {
        return false;
    }
    if (strcmp(stage->numeral, "Cad.") == 0) {
        /* The cadential 6/4. */
        return inversion == 2 && harmony_make_chord(mode, 1, 2, false, true, 1, out) &&
               bass_accepts(key, out, &stage->bass);
    }
    const char *slash = strchr(stage->numeral, '/');
    if (slash) {
        HarmonyAppliedChord applied;
        memset(&applied, 0, sizeof(applied));
        memset(out, 0, sizeof(*out));
        if (strncmp(stage->numeral, "V", (size_t)(slash - stage->numeral)) == 0 && slash - stage->numeral == 1) {
            applied.quality = seventh ? HARMONY_APPLIED_DOMINANT_SEVENTH : HARMONY_APPLIED_MAJOR_TRIAD;
        } else if (slash - stage->numeral == 3 && strncmp(stage->numeral, "vii", 3) == 0) {
            applied.quality = seventh ? HARMONY_APPLIED_FULLY_DIMINISHED_SEVENTH : HARMONY_APPLIED_DIMINISHED_TRIAD;
        } else {
            return false;
        }
        applied.target = numeral_degree(slash + 1, strlen(slash + 1));
        if (!harmony_applied_quality_kind(applied.quality, &applied.kind) ||
            !harmony_applied_valid(harmony_mode_of(mode), &applied) ||
            !harmony_roman_format_applied_inverted(mode, &applied, inversion, out->roman)) {
            return false;
        }
        out->applied = true;
        out->applied_chord = applied;
        out->applied_inversion = inversion;
        out->function = PLAN_FUNCTION_DOMINANT;
        out->cells = 1;
        return bass_accepts(key, out, &stage->bass);
    }
    int degree = numeral_degree(stage->numeral, strlen(stage->numeral));
    if (degree == 0) {
        return false;
    }
    /* A lower-case vii in a minor key is the diminished chord on the raised seventh degree. */
    bool leading = mode == PLAN_MODE_MINOR && strcmp(stage->numeral, "vii") == 0;
    bool made = leading ? (!seventh && harmony_make_leading_tone(mode, inversion, 1, out))
                        : harmony_make_chord(mode, degree, inversion, seventh, false, 1, out);
    return made && bass_accepts(key, out, &stage->bass);
}

/* The modulating schemas change key, which is planned elsewhere. */
static bool modulates(GalantSchemaId id) {
    return id == GALANT_SCHEMA_MODULATING_PRINNER || id == GALANT_SCHEMA_MODULATING_PRINNER_WITH_DOMINANT;
}

static bool derive_schema(const PlanKey *key, const GalantPrototype *prototype, PhraseChord *chords) {
    if (modulates(prototype->id) || prototype->stage_count == 0 || prototype->stage_count > GALANT_MAX_STAGES) {
        return false;
    }
    for (size_t i = 0; i < prototype->stage_count; i++) {
        if (prototype->stages[i].numeral_dominant != NULL || !derive_stage(key, &prototype->stages[i], &chords[i])) {
            return false;
        }
    }
    return true;
}

bool harmony_schema_find(const char *name, GalantSchemaId *id) {
    if (!name || !id) {
        return false;
    }
    for (int candidate = 0; candidate < GALANT_SCHEMA_COUNT; candidate++) {
        if (strcmp(galant_schema_name((GalantSchemaId)candidate), name) == 0) {
            *id = (GalantSchemaId)candidate;
            return true;
        }
    }
    return false;
}

bool harmony_schema_available(GalantSchemaId id, PlanKeyMode mode) {
    const GalantPrototype *prototype = galant_prototype(id);
    PhraseChord chords[GALANT_MAX_STAGES];
    PlanKey key = {0, mode};
    return prototype && derive_schema(&key, prototype, chords);
}

/* --- from stages to cells ----------------------------------------------- */

/*
 * Gives each chord its cells: one for each stage, in order from the start of the span, the last running
 * to the end of its bar, and the first held one cell longer when the schema starts weak at a bar line
 * (its first chord a tonic). Each stage must fall on a cell of its own strength. False if it cannot.
 */
static bool layout_schema(const SkeletonContext *context, const GalantPrototype *prototype, PhraseChord *chords,
                          size_t *total) {
    size_t cpb = context->metre.cells_per_bar;
    size_t n = prototype->stage_count;
    size_t start = context->metre.first_cell % cpb;
    size_t lead = 0;
    bool strong_first = prototype->stages[0].strength == BEAT_STRENGTH_STRONG;
    if (start != (strong_first ? 0 : cpb - 1)) {
        if (strong_first || start != 0 || chords[0].function != PLAN_FUNCTION_TONIC) {
            return false;
        }
        lead = cpb - 1;
    }
    for (size_t i = 0; i < n; i++) {
        bool strong_cell = (start + lead + i) % cpb == 0;
        if ((prototype->stages[i].strength == BEAT_STRENGTH_STRONG) != strong_cell) {
            return false;
        }
        chords[i].cells = i == 0 ? 1 + lead : 1;
    }
    size_t end = (start + lead + n) % cpb;
    if (end != 0) {
        chords[n - 1].cells += cpb - end;
    }
    *total = 0;
    for (size_t i = 0; i < n; i++) {
        *total += chords[i].cells;
    }
    return true;
}

/* --- the operator -------------------------------------------------------- */

static bool schema_applies(const PiecePlan *plan, FillTarget target, void *context) {
    SkeletonContext read;
    (void)context;
    return target.kind == FILL_TARGET_LANE_SPAN && harmony_skeleton_read(plan, (LaneSpanId)target.id, &read) &&
           read.metre.cells_per_bar == 2;
}

static unsigned role_of(SchemaFunction function) {
    switch (function) {
    case SCHEMA_FUNCTION_OPENING_GAMBIT:
        return HARMONY_SCHEMA_ROLE_OPENING;
    case SCHEMA_FUNCTION_CONTINUATION:
    case SCHEMA_FUNCTION_PRE_CADENTIAL:
        return HARMONY_SCHEMA_ROLE_CONTINUATION;
    case SCHEMA_FUNCTION_CADENCE:
        return HARMONY_SCHEMA_ROLE_CADENCE;
    default:
        return 0;
    }
}

/* Whether the role fits where the target is. */
static bool role_fits(unsigned role, const SkeletonContext *context, size_t remainder) {
    switch (role) {
    case HARMONY_SCHEMA_ROLE_OPENING:
        return context->at_start;
    case HARMONY_SCHEMA_ROLE_CONTINUATION:
        return !context->at_start;
    case HARMONY_SCHEMA_ROLE_CADENCE:
        return remainder == 0 && context->has_cadence;
    default:
        return false;
    }
}

static bool schema_generate(const PiecePlan *plan, FillTarget target, FillRng *rng, void *context,
                            FillCandidateList *out) {
    HarmonySchemaParams params_value = params_of(context);
    const HarmonySchemaParams *params = &params_value;
    SkeletonContext read;
    if (target.kind != FILL_TARGET_LANE_SPAN || !harmony_skeleton_read(plan, (LaneSpanId)target.id, &read)) {
        return false;
    }
    SkeletonCandidate *skeleton = malloc(sizeof(*skeleton));
    if (!skeleton) {
        return false;
    }
    bool ok = true;
    for (int id = 0; ok && id < GALANT_SCHEMA_COUNT; id++) {
        const GalantPrototype *prototype = galant_prototype((GalantSchemaId)id);
        unsigned role = prototype ? role_of(prototype->function) : 0;
        if (!prototype || !(params->schemas & HARMONY_SCHEMA_BIT(id)) || !(params->roles & role)) {
            continue;
        }
        memset(skeleton, 0, sizeof(*skeleton));
        skeleton->schema = id;
        skeleton->context = read;
        size_t total;
        if (!derive_schema(&read.key, prototype, skeleton->chords) ||
            !layout_schema(&read, prototype, skeleton->chords, &total) || total > read.metre.cells) {
            continue;
        }
        skeleton->count = prototype->stage_count;
        skeleton->remainder_cells = read.metre.cells - total;
        skeleton->preference = params->preference < PREFERENCE_LIMIT ? params->preference : PREFERENCE_LIMIT;
        if (!role_fits(role, &read, skeleton->remainder_cells)) {
            continue;
        }

        /* A bass the melodic rules accept, and a phrase standard-harmony accepts, or no candidate for this schema. */
        for (int attempt = 0; ok && attempt < BASS_TRIES; attempt++) {
            PhraseVerdict verdict;
            if (!harmony_skeleton_bass(skeleton, &params->bass, rng)) {
                break;
            }
            if (!harmony_skeleton_judge(skeleton, &verdict)) {
                ok = false;
            } else if (!verdict.hard) {
                SkeletonCandidate *payload = malloc(sizeof(*payload));
                char label[FILL_LABEL_MAX];
                if (!payload) {
                    ok = false;
                    break;
                }
                *payload = *skeleton;
                snprintf(label, sizeof(label), "schema:%s", galant_schema_name((GalantSchemaId)id));
                ok = fill_candidates_add(out, label, payload, free);
                break;
            }
        }
    }
    free(skeleton);
    return ok;
}

static PlanResult schema_apply(PiecePlan *plan, FillTarget target, const FillCandidate *candidate, void *context) {
    const SkeletonCandidate *skeleton = candidate ? candidate->payload : NULL;
    (void)context;
    if (target.kind != FILL_TARGET_LANE_SPAN || !skeleton || skeleton->context.target != (LaneSpanId)target.id) {
        return PLAN_ERR_NULL;
    }
    return harmony_skeleton_apply(plan, skeleton);
}

FillResult harmony_schema_register(FillOperatorRegistry *registry, const HarmonySchemaParams *params) {
    FillOperator operator_;
    memset(&operator_, 0, sizeof(operator_));
    operator_.generator.name = HARMONY_SCHEMA_OPERATOR_NAME;
    operator_.generator.supported_inputs = "PiecePlan harmony lane span with the chords before it";
    operator_.generator.supported_outputs = "PiecePlan harmony and bass lane spans";
    operator_.generator.configuration_description = "enabled schemas, enabled roles, preference, bass parameters";
    operator_.generator.constraints_description = "galant-schemas prototypes, standard-harmony, standard-counterpoint";
    operator_.generator.deterministic = true;
    operator_.generator.version = "1";
    operator_.kind = FILL_OPERATOR_FILL;
    operator_.applies = schema_applies;
    operator_.generate = schema_generate;
    operator_.apply = schema_apply;
    operator_.context = (void *)params;
    return fill_registry_register(registry, &operator_);
}
