#include <ursatz_composer/grow_types.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>

#include <model/pitch/pitch_arithmetic.h>
#include <model/pitch/pitch_material.h>

#include <ursatz_composer/motive.h>
#include <ursatz_composer/motive_blueprint.h>
#include <ursatz_composer/theme_shape.h>

#include "grow/grow_internal.h"
#include "structure/plan_util.h"
#include "util/util.h"

/*
 * The blueprint generator (R-751, docs/blueprint-generator.md): from the motif and the K8 slot expectations of the sentence or the period
 * (R-749), the chords and the rhythm jobs of an 8-bar theme, drawn for a seed with the blueprint preferences (R-750); then, once the
 * rhythms are drawn, the climax and the sounds (grow_blueprint_generate_marks).
 */

#define GEN_BARS 8
#define GEN_ATTEMPTS 16
#define GEN_CELLS_MAX 128

static GrowResult refuse(GrowRefusal *refusal, GrowResult result, const char *path, const char *format, ...)
    __attribute__((format(printf, 4, 5)));

static GrowResult refuse(GrowRefusal *refusal, GrowResult result, const char *path, const char *format, ...) {
    if (refusal && refusal->message[0] == '\0') {
        va_list args;
        va_start(args, format);
        vsnprintf(refusal->message, sizeof refusal->message, format, args);
        va_end(args);
        snprintf(refusal->path, sizeof refusal->path, "%s", path);
        refusal->result = result;
    }
    return result;
}

/* The ideas of the theme type over the 8 bars: a unit of the type is a bar. */
typedef struct {
    const ThemeType *type;
    size_t first[THEME_TYPE_IDEA_MAX];
    size_t bars[THEME_TYPE_IDEA_MAX];
} Layout;

static bool layout_of(const GrowRequest *request, Layout *out) {
    bool sentence = request->pairing.beginning == SECTION_BEGINNING_PRESENTATION && request->pairing.ending == SECTION_ENDING_CONTINUATION;
    bool period = request->pairing.beginning == SECTION_BEGINNING_ANTECEDENT && request->pairing.ending == SECTION_ENDING_CONSEQUENT;
    if (!request->knowledge || (!sentence && !period)) {
        return false;
    }
    out->type = sentence ? knowledge_snapshot_theme_type_of(request->knowledge, "presentation", "continuation")
                         : knowledge_snapshot_theme_type_of(request->knowledge, "antecedent", "consequent");
    if (!out->type) {
        return false;
    }
    size_t at = 0;
    for (size_t i = 0; i < out->type->idea_count; i++) {
        out->first[i] = at;
        out->bars[i] = (size_t)out->type->ideas[i].units;
        at += out->bars[i];
    }
    return at == GEN_BARS;
}

static bool is_basic_slot(const ThemeTypeIdea *idea) {
    return strcmp(idea->slot, "basic") == 0;
}

/* --------------------------------------------------------------------------------------------- the chords */

/* The cells of a bar, each a chord may start on, and the length of one. */
typedef struct {
    size_t cells;
    Rational bar;
    Rational cell;
} Cells;

static bool cells_of(const PlanMeter *meter, Cells *out) {
    return harmony_rhythm_cells_per_bar(meter, &out->cells) && out->cells > 0 && plan_meter_bar_length(meter, &out->bar) == PLAN_OK &&
           rational_div_int(out->bar, (long long)out->cells, &out->cell) == RATIONAL_OK;
}

/* The index of the motive's chord sounding at the time (the last whose onset is not after it); -1 for a motive with none. */
static long chord_index_at(const Motive *motive, Rational time) {
    long found = -1;
    for (size_t i = 0; i < motive->chord_count; i++) {
        if (rational_cmp(motive->chords[i].onset, time) <= 0) {
            found = (long)i;
        }
    }
    return found;
}

/*
 * The basic idea's chords as the texts of its bars (R-751): at each cell of a bar the chord sounding at its start, a cell with the chord of
 * the cell before adding nothing; a motive with no harmony is "I" in each bar; a motive of one bar gives its second bar the chord sounding
 * at its end.
 */
static void basic_texts(const GrowRequest *request, const Cells *cells, size_t bars, char texts[][GROW_BLUEPRINT_TEXT]) {
    const Motive *motive = request->motive;
    Rational motive_bars = {0, 1};
    bool one_bar = rational_div(motive->length, cells->bar, &motive_bars) == RATIONAL_OK && rational_cmp(motive_bars, (Rational){1, 1}) <= 0;
    for (size_t bar = 0; bar < bars; bar++) {
        char *text = texts[bar];
        text[0] = '\0';
        if (motive->chord_count == 0) {
            snprintf(text, GROW_BLUEPRINT_TEXT, "I");
            continue;
        }
        long previous = -2;
        for (size_t cell = 0; cell < cells->cells; cell++) {
            Rational start;
            Rational within;
            long index;
            if (rational_mul_int(cells->bar, (long long)bar, &start) != RATIONAL_OK ||
                rational_mul_int(cells->cell, (long long)cell, &within) != RATIONAL_OK || rational_add(start, within, &start) != RATIONAL_OK) {
                break;
            }
            if (one_bar && bar >= 1) {
                index = -1; /* the chord sounding at the motive's end */
                for (size_t c = 0; c < motive->chord_count; c++) {
                    if (rational_cmp(motive->chords[c].onset, motive->length) < 0) {
                        index = (long)c;
                    }
                }
            } else {
                index = chord_index_at(motive, start);
            }
            if (index < 0 || index == previous) {
                continue;
            }
            char numeral[MOTIVE_CHORD_NUMERAL_MAX];
            if (!motive_chord_numeral(&motive->chords[index], numeral, sizeof numeral)) {
                snprintf(numeral, sizeof numeral, "I");
            }
            size_t used = strlen(text);
            if (cell == 0 || used == 0) {
                snprintf(text + used, GROW_BLUEPRINT_TEXT - used, "%s%s", used ? " " : "", numeral);
            } else {
                char at[24];
                grow_blueprint_beats_text(within, &request->meter, at, sizeof at);
                snprintf(text + used, GROW_BLUEPRINT_TEXT - used, " %s@%s", numeral, at);
            }
            previous = index;
            if (one_bar && bar >= 1) {
                break;
            }
        }
        if (text[0] == '\0') {
            snprintf(text, GROW_BLUEPRINT_TEXT, "I");
        }
    }
}

/*
 * A template's bar text (a chord text of K8, its "@k" the k-th cell of the bar) written for the meter: false when the bar has more chords
 * than cells or a chord at a cell the meter lacks.
 */
static bool template_text(const char *source, const Cells *cells, const PlanMeter *meter, char *out, size_t size) {
    char copy[GROW_BLUEPRINT_TEXT];
    size_t chords = 0;
    out[0] = '\0';
    snprintf(copy, sizeof copy, "%s", source);
    for (char *token = strtok(copy, " "); token; token = strtok(NULL, " ")) {
        char *mark = strchr(token, '@');
        size_t used = strlen(out);
        chords++;
        if (mark) {
            long cell = strtol(mark + 1, NULL, 10);
            Rational within;
            char at[24];
            if (cell < 2 || (size_t)cell > cells->cells || rational_mul_int(cells->cell, cell - 1, &within) != RATIONAL_OK) {
                return false;
            }
            *mark = '\0';
            grow_blueprint_beats_text(within, meter, at, sizeof at);
            snprintf(out + used, size - used, "%s%s@%s", used ? " " : "", token, at);
        } else {
            snprintf(out + used, size - used, "%s%s", used ? " " : "", token);
        }
    }
    return chords > 0 && chords <= cells->cells;
}

/* The semitones above the tonic of degree 1 to 7 of the key's material; the minor key's raised leading tone is degree 7 of the major scale. */
static int scale_pc(const SpelledKey *key, int degree, bool raised_seventh) {
    int steps = pitch_material_step(key->material, degree);
    if (degree == 7 && raised_seventh && pitch_material_third(key->material) == 3) {
        return 11;
    }
    return steps < 0 ? 0 : steps;
}

/* The bass of a chord of a bar's text as (scale degree, pitch class above the tonic); the minor key's V, V7 and their inversions take the raised seventh degree. */
typedef struct {
    int degree;
    int pc;
} Bass;

/* The chords of a text in order, their basses. False for a text the own-harmony syntax does not read. */
static bool basses_of(const char *text, const PlanMeter *meter, const SpelledKey *key, Bass *out, size_t capacity, size_t *count) {
    MotiveSketchChord chords[MOTIVE_MAX_CHORDS];
    size_t chord_count = 0;
    MotiveError detail;
    if (motive_harmony_parse(text, meter, chords, &chord_count, &detail) != MOTIVE_OK) {
        return false;
    }
    for (size_t i = 0; i < chord_count && *count < capacity; i++) {
        int root = chords[i].degree;
        int bass = (root - 1 + 2 * chords[i].inversion) % 7 + 1;
        out[*count].degree = bass;
        out[*count].pc = scale_pc(key, bass, root == 5) + (bass == root || root != 5 ? chords[i].alteration : 0);
        out[*count].pc = (out[*count].pc % 12 + 12) % 12;
        (*count)++;
    }
    return true;
}

/* Whether two basses make an augmented or a diminished interval (read by quality: a perfect interval is 0, 5 or 7 semitones, another 1 to 4 or 8 to 11 is major or minor). */
static bool bass_clash(const Bass *a, const Bass *b) {
    int generic = ((b->degree - a->degree) % 7 + 7) % 7 + 1;
    int semitones = ((b->pc - a->pc) % 12 + 12) % 12;
    switch (generic) {
    case 1:
        return semitones != 0;
    case 4:
        return semitones != 5;
    case 5:
        return semitones != 7;
    case 2:
        return semitones != 1 && semitones != 2;
    case 3:
        return semitones != 3 && semitones != 4;
    case 6:
        return semitones != 8 && semitones != 9;
    default:
        return semitones != 10 && semitones != 11;
    }
}

/* Whether the texts of the 8 bars, in order, have no clash between neighbouring basses. */
static bool basses_fit(char texts[][GROW_BLUEPRINT_TEXT], const PlanMeter *meter, const SpelledKey *key) {
    Bass basses[GEN_BARS * 8];
    size_t count = 0;
    for (size_t bar = 0; bar < GEN_BARS; bar++) {
        if (texts[bar][0] && !basses_of(texts[bar], meter, key, basses, sizeof basses / sizeof basses[0], &count)) {
            return false;
        }
    }
    for (size_t i = 0; i + 1 < count; i++) {
        if (bass_clash(&basses[i], &basses[i + 1])) {
            return false;
        }
    }
    return true;
}

/* One alternative of an idea, the chord texts of its bars written for the meter; false when it does not fit the meter's cells. */
static bool alternative_texts(const ThemeHarmonyTemplate *template, const ThemeTypeIdea *idea, const Cells *cells, const PlanMeter *meter,
                              char out[][GROW_BLUEPRINT_TEXT]) {
    if (template->restate) {
        return true;
    }
    for (size_t unit = 0; unit < template->chord_count && unit < (size_t)idea->units; unit++) {
        if (!template_text(template->chords[unit], cells, meter, out[unit], GROW_BLUEPRINT_TEXT)) {
            return false;
        }
    }
    return template->chord_count == (size_t)idea->units;
}

static GrowResult draw_chords(const GrowRequest *request, const Layout *layout, const Cells *cells, FillRng *rng, GrowBlueprint *out,
                              GrowRefusal *refusal) {
    char basic[4][GROW_BLUEPRINT_TEXT];
    size_t basic_bars = layout->bars[0] < 4 ? layout->bars[0] : 4;
    basic_texts(request, cells, basic_bars, basic);
    for (size_t attempt = 0; attempt < GEN_ATTEMPTS; attempt++) {
        char texts[GEN_BARS][GROW_BLUEPRINT_TEXT];
        memset(texts, 0, sizeof texts);
        for (size_t bar = 0; bar < basic_bars; bar++) {
            snprintf(texts[bar], GROW_BLUEPRINT_TEXT, "%s", basic[bar]);
        }
        for (size_t i = 1; i < layout->type->idea_count; i++) {
            const ThemeTypeIdea *idea = &layout->type->ideas[i];
            const ThemeTypeExpect *expect = &idea->expect;
            char drawn[THEME_EXPECT_HARMONY_MAX][4][GROW_BLUEPRINT_TEXT];
            size_t fits[THEME_EXPECT_HARMONY_MAX];
            size_t fit_count = 0;
            for (size_t k = 0; k < expect->harmony_count && idea->units <= 4; k++) {
                memset(drawn[k], 0, sizeof drawn[k]);
                if (alternative_texts(&expect->harmony[k], idea, cells, &request->meter, drawn[k])) {
                    fits[fit_count++] = k;
                }
            }
            if (fit_count == 0) {
                continue;
            }
            size_t pick = fits[fill_rng_below(rng, fit_count)];
            for (size_t unit = 0; unit < (size_t)idea->units; unit++) {
                const char *text = expect->harmony[pick].restate ? basic[unit < basic_bars ? unit : 0] : drawn[pick][unit];
                snprintf(texts[layout->first[i] + unit], GROW_BLUEPRINT_TEXT, "%s", text);
            }
        }
        if (basses_fit(texts, &request->meter, &request->key)) {
            for (size_t bar = 0; bar < GEN_BARS; bar++) {
                snprintf(out->bars[bar].chords, GROW_BLUEPRINT_TEXT, "%s", texts[bar]);
            }
            return GROW_OK;
        }
    }
    return refuse(refusal, GROW_ERR_STUCK, "/blueprint/bars", "no blueprint chords fit the bass");
}

/* --------------------------------------------------------------------------------------------- the rhythms */

/* What a bar's rhythm is known to be before the draw: a motif bar or a cell bar, and whether it ends with exactly a half note. */
typedef struct {
    bool known;
    bool ends_half;
} BarInfo;

static bool is_half(Rational duration) {
    return duration.num == 1 && duration.den == 2;
}

/* The bars of the motive's rhythm as far as the basic idea goes: whether each ends with exactly a half note. */
static void motive_info(const GrowRequest *request, const Cells *cells, BarInfo *info, size_t bars) {
    const Motive *motive = request->motive;
    for (size_t bar = 0; bar < bars; bar++) {
        Rational start;
        Rational end;
        if (rational_mul_int(cells->bar, (long long)bar, &start) != RATIONAL_OK || rational_add(start, cells->bar, &end) != RATIONAL_OK) {
            return;
        }
        for (size_t i = 0; i < motive->note_count; i++) {
            if (rational_cmp(motive->notes[i].onset, start) >= 0 && rational_cmp(motive->notes[i].onset, end) < 0) {
                info[bar].known = true;
                info[bar].ends_half = is_half(motive->notes[i].duration);
            }
        }
    }
}

/* The length of a beat in whole notes: the beat unit, or three of them in a compound meter. */
static Rational beat_length(const PlanMeter *meter) {
    long long den = meter->beat_unit;
    long long num = 1;
    if (meter->beat_unit == 8 && meter->beats_per_bar % 3 == 0) {
        num = 3;
    }
    return (Rational){num, den};
}

static unsigned pattern_job_of(unsigned theme_job) {
    return theme_job == THEME_JOB_RUNNING ? PATTERN_JOB_RUNNING
           : theme_job == THEME_JOB_LONG ? PATTERN_JOB_LONG
           : theme_job == THEME_JOB_SYNCOPATED ? PATTERN_JOB_SYNCOPATED
                                               : PATTERN_JOB_CADENCE;
}

/* The blueprint job a theme job of K8 draws (running to contrast), the index into BlueprintPreferences.jobs. */
static int blueprint_job_of(unsigned theme_job) {
    return theme_job == THEME_JOB_RUNNING ? BLUEPRINT_JOB_RUNNING
           : theme_job == THEME_JOB_LONG ? BLUEPRINT_JOB_LONG
           : theme_job == THEME_JOB_SYNCOPATED ? BLUEPRINT_JOB_SYNCOPATED
           : theme_job == THEME_JOB_VARIANT ? BLUEPRINT_JOB_VARIANT
                                            : BLUEPRINT_JOB_CONTRAST;
}

static BlueprintFunction function_of(const ThemeTypeIdea *idea) {
    const char *id = idea->function->id;
    return strstr(id, "fragment") ? BLUEPRINT_FUNCTION_FRAGMENT
           : strstr(id, "contrasting") ? BLUEPRINT_FUNCTION_CONTRASTING
           : strstr(id, "cadential") ? BLUEPRINT_FUNCTION_CADENTIAL
                                     : BLUEPRINT_FUNCTION_BASIC;
}

/* The index of a draw in proportion to the weights; `count` when every weight is 0. */
static size_t draw_weighted(FillRng *rng, const unsigned *weights, size_t count) {
    unsigned total = 0;
    for (size_t i = 0; i < count; i++) {
        total += weights[i];
    }
    if (total == 0) {
        return count;
    }
    unsigned at = (unsigned)fill_rng_below(rng, total);
    for (size_t i = 0; i < count; i++) {
        if (at < weights[i]) {
            return i;
        }
        at -= weights[i];
    }
    return count - 1;
}

/*
 * A K11 cell of the meter with the job, each weighted 1, or 1 + long_weak when it holds a note of a beat or longer that starts off the
 * downbeat; a cell whose first note is exactly a half note is skipped when the bar before is known and ends with one. NULL when there
 * is none. `line` is the cell's rhythm.
 */
static const PatternEntry *draw_cell(const GrowRequest *request, unsigned pattern_job, const Cells *cells, const BarInfo *before,
                                     unsigned long_weak, bool commit, FillRng *rng, GrowBlueprintLine *lines, GrowBlueprintLine *line) {
    const PatternEntry *entries[GEN_CELLS_MAX];
    unsigned weights[GEN_CELLS_MAX];
    size_t count = grow_blueprint_cells(request->knowledge, &request->meter, pattern_job, entries, GEN_CELLS_MAX);
    size_t usable = 0;
    Rational beat = beat_length(&request->meter);
    for (size_t c = 0; c < count; c++) {
        weights[c] = 0;
        if (!grow_blueprint_cell_line(entries[c], request->knowledge, &request->meter, cells->bar, &lines[c])) {
            continue;
        }
        if (is_half(lines[c].notes[0].duration) && before && before->known && before->ends_half) {
            continue;
        }
        weights[c] = 1;
        for (size_t n = 1; n < lines[c].count; n++) {
            if (rational_cmp(lines[c].notes[n].duration, beat) >= 0 && lines[c].notes[n].offset.num > 0) {
                weights[c] = 1 + long_weak;
            }
        }
        usable++;
    }
    if (usable == 0) {
        return NULL;
    }
    if (!commit) {
        return entries[0];
    }
    size_t pick = draw_weighted(rng, weights, count);
    if (pick >= count) {
        return NULL;
    }
    *line = lines[pick];
    return entries[pick];
}

static GrowResult draw_jobs(const GrowRequest *request, const Layout *layout, const Cells *cells, const BlueprintPreferences *preferences,
                            FillRng *rng, GrowBlueprint *out, GrowRefusal *refusal, GrowBlueprintLine *lines) {
    BarInfo info[GEN_BARS];
    memset(info, 0, sizeof info);
    motive_info(request, cells, info, layout->bars[0]);
    for (size_t i = 1; i < layout->type->idea_count; i++) {
        const ThemeTypeIdea *idea = &layout->type->ideas[i];
        if (is_basic_slot(idea)) {
            for (size_t unit = 0; unit < (size_t)idea->units && unit < layout->bars[0]; unit++) {
                info[layout->first[i] + unit] = info[unit];
            }
        }
    }
    for (size_t i = 1; i < layout->type->idea_count; i++) {
        const ThemeTypeIdea *idea = &layout->type->ideas[i];
        const ThemeTypeExpect *expect = &idea->expect;
        BlueprintFunction function = function_of(idea);
        if (is_basic_slot(idea) || expect->job_count == 0) {
            continue;
        }
        for (size_t unit = 0; unit < (size_t)idea->units; unit++) {
            size_t bar = layout->first[i] + unit;
            unsigned bits = expect->jobs[unit];
            GrowBlueprintBar *this = &out->bars[bar];
            const BarInfo *before = bar > 0 ? &info[bar - 1] : NULL;
            GrowBlueprintLine line;
            char path[48];
            snprintf(path, sizeof path, "/blueprint/bars/%zu/job", bar);
            if (bits == THEME_JOB_LIQUIDATION) {
                this->job = GROW_JOB_LIQUIDATION;
                info[bar] = bar > 0 ? info[bar - 1] : (BarInfo){false, false};
                continue;
            }
            unsigned chosen = 0;
            if (bits == THEME_JOB_CADENCE) {
                chosen = THEME_JOB_CADENCE;
            } else {
                unsigned jobs[5];
                unsigned weights[5];
                size_t candidates = 0;
                for (unsigned bit = THEME_JOB_VARIANT; bit <= THEME_JOB_SYNCOPATED; bit <<= 1) {
                    if ((bits & bit) == 0) {
                        continue;
                    }
                    unsigned weight = preferences->jobs[function][blueprint_job_of(bit)];
                    bool cell_job = bit != THEME_JOB_VARIANT && bit != THEME_JOB_CONTRAST;
                    if (weight == 0 || (cell_job && !draw_cell(request, pattern_job_of(bit), cells, before, preferences->sounds[BLUEPRINT_SOUND_LONG_WEAK], false, rng, lines, &line))) {
                        continue;
                    }
                    jobs[candidates] = bit;
                    weights[candidates++] = weight;
                }
                size_t pick = draw_weighted(rng, weights, candidates);
                if (pick < candidates) {
                    chosen = jobs[pick];
                } else {
                    chosen = bits & (~bits + 1u); /* the unit's first listed job: the lowest bit */
                }
            }
            if (chosen == THEME_JOB_VARIANT || chosen == THEME_JOB_CONTRAST) {
                this->job = chosen == THEME_JOB_VARIANT ? GROW_JOB_VARIANT : GROW_JOB_CONTRAST;
                info[bar] = (BarInfo){false, false};
            } else if (chosen == THEME_JOB_MOTIF || chosen == THEME_JOB_LIQUIDATION) {
                this->job = chosen == THEME_JOB_MOTIF ? GROW_JOB_MOTIF : GROW_JOB_LIQUIDATION;
                info[bar] = (BarInfo){false, false};
            } else {
                const PatternEntry *cell = draw_cell(request, pattern_job_of(chosen), cells, before, preferences->sounds[BLUEPRINT_SOUND_LONG_WEAK], true, rng, lines, &line);
                if (!cell) {
                    char meter[16];
                    snprintf(meter, sizeof meter, "%d/%d", request->meter.beats_per_bar, request->meter.beat_unit);
                    return refuse(refusal, GROW_ERR_BAD_REQUEST, path, "no rhythm for bar %zu in %s", bar, meter);
                }
                this->job = GROW_JOB_ENTRY;
                this->job_entry = cell;
                info[bar].known = true;
                info[bar].ends_half = is_half(line.notes[line.count - 1].duration);
            }
        }
    }
    return GROW_OK;
}

static GrowResult draw_rhythms(const GrowRequest *request, const Layout *layout, const Cells *cells, const BlueprintPreferences *preferences,
                               FillRng *rng, GrowBlueprint *out, GrowRefusal *refusal) {
    GrowBlueprintLine *lines = malloc(GEN_CELLS_MAX * sizeof(*lines));
    if (!lines) {
        return GROW_ERR_ALLOC;
    }
    GrowResult result = draw_jobs(request, layout, cells, preferences, rng, out, refusal, lines);
    free(lines);
    return result;
}

GrowResult grow_blueprint_generate(const GrowRequest *request, const BlueprintPreferences *preferences, FillRng *rng, GrowBlueprint *out,
                                   GrowRefusal *refusal) {
    BlueprintPreferences defaults;
    Layout layout;
    Cells cells;
    memset(out, 0, sizeof(*out));
    if (!request || !request->motive || !rng) {
        return GROW_ERR_NULL;
    }
    if (!preferences) {
        blueprint_preferences_default(&defaults);
        preferences = &defaults;
    }
    if (!layout_of(request, &layout) || request->bars != GEN_BARS || !cells_of(&request->meter, &cells)) {
        return refuse(refusal, GROW_ERR_BAD_REQUEST, "/development", "a blueprint is written for an 8-bar sentence or period in 4/4, 3/4 or 6/8");
    }
    out->bar_count = GEN_BARS;
    GrowResult chords = draw_chords(request, &layout, &cells, rng, out, refusal);
    if (chords != GROW_OK) {
        return chords;
    }
    return draw_rhythms(request, &layout, &cells, preferences, rng, out, refusal);
}

/* --------------------------------------------------------------------------------------------- the climax and the sounds */

#define CLIMAX_ABOVE_MOTIF 8
#define SOUND_NONE_WEIGHT 2

/* A bar's chords as the own-harmony syntax parsed them, and the tones of one. */
typedef struct {
    Motive *keyed; /* a motive with the request's key, for the chord tones */
    const GrowRequest *request;
} Reading;

static size_t read_chords(const Reading *reading, const char *text, MotiveSketchChord *out) {
    size_t count = 0;
    MotiveError detail;
    if (motive_harmony_parse(text, &reading->request->meter, out, &count, &detail) != MOTIVE_OK) {
        return 0;
    }
    return count;
}

/* The index of the chord sounding at the offset (the last whose onset is not after it). */
static size_t sounding(const MotiveSketchChord *chords, size_t count, Rational offset) {
    size_t at = 0;
    for (size_t i = 0; i < count; i++) {
        if (rational_cmp(chords[i].onset, offset) <= 0) {
            at = i;
        }
    }
    return at;
}

/* The notes of the key: the tonic's pitch class, and whether a pitch class is of the chord. */
static int tonic_pc(const GrowRequest *request) {
    return spelled_key_tonic_pitch_class(&request->key);
}

/* The pitch of a pitch class: the octave nearest the mean, moved by octaves into [low, high]; -1 when none lies there. */
static int place_pc(int pc, int mean, int low, int high) {
    int pitch = pc + 12 * ((mean - pc + (mean >= pc ? 6 : -6)) / 12);
    while (pitch > high) {
        pitch -= 12;
    }
    while (pitch < low) {
        pitch += 12;
    }
    return pitch >= low && pitch <= high ? pitch : -1;
}

/* What the marks know of the theme while they are placed. */
typedef struct {
    const GrowRequest *request;
    const BlueprintPreferences *preferences;
    const GrowBlueprintDraw *drawn;
    const Layout *layout;
    GrowBlueprint *out;
    Motive keyed;
    int mean;      /* the mean pitch of the basic motive's notes */
    int low;       /* the lowest an anchor may lie */
    int high;      /* the highest: one below the climax */
    Rational beat; /* a beat of the meter */
    Cells cells;
    bool taken[GEN_BARS];
    bool once[BLUEPRINT_SOUND_COUNT];
} Marks;

/* Whether an anchor or an event at the offset of a bar could still be added. */
static bool room(const GrowBlueprintBar *bar, size_t anchors, size_t events) {
    return bar->anchor_count + anchors <= GROW_BLUEPRINT_ANCHORS && bar->event_count + events <= GROW_BLUEPRINT_EVENTS;
}

static void add_anchor(GrowBlueprintBar *bar, Rational offset, int midi) {
    bar->anchors[bar->anchor_count].offset = offset;
    bar->anchors[bar->anchor_count++].midi = midi;
}

static void add_event(GrowBlueprintBar *bar, Rational offset, PlanEventType type) {
    bar->events[bar->event_count].offset = offset;
    bar->events[bar->event_count++].type = type;
}

/* The index of the note of the bar's drawn rhythm that starts at the offset; -1 for none. */
static long note_at_offset(const Marks *marks, size_t bar, Rational offset) {
    const GrowBlueprintLine *line = &marks->drawn->rhythm[bar];
    for (size_t i = 0; marks->drawn->known[bar] && i < line->count; i++) {
        if (rational_cmp(line->notes[i].offset, offset) == 0) {
            return (long)i;
        }
    }
    return -1;
}

/* The longest note of the bar's rhythm, the first of equals, from `from` on; -1 when there is none. */
static long longest_note(const Marks *marks, size_t bar, Rational from) {
    const GrowBlueprintLine *line = &marks->drawn->rhythm[bar];
    long best = -1;
    for (size_t i = 0; marks->drawn->known[bar] && i < line->count; i++) {
        if (rational_cmp(line->notes[i].offset, from) >= 0 && (best < 0 || rational_cmp(line->notes[i].duration, line->notes[best].duration) > 0)) {
            best = (long)i;
        }
    }
    return best;
}

/* The chord tones of a parsed chord, as the pitch classes of the sounding chord. */
static size_t tones_of(Marks *marks, const MotiveSketchChord *chord, int pcs[MOTIVE_SKETCH_CHORD_TONES_MAX]) {
    return motive_sketch_chord_tones(&marks->keyed, chord, pcs);
}

/* Whether the chord is a major (4) or a minor (3) triad: the interval of its third; 0 for another. */
static int triad_third(Marks *marks, const MotiveSketchChord *chord) {
    int pcs[MOTIVE_SKETCH_CHORD_TONES_MAX];
    size_t count = tones_of(marks, chord, pcs);
    if (count != 3 || chord->inversion != 0 || chord->seventh || chord->color || chord->alteration != 0) {
        return 0;
    }
    int third = ((pcs[1] - pcs[0]) % 12 + 12) % 12;
    int fifth = ((pcs[2] - pcs[0]) % 12 + 12) % 12;
    return fifth == 7 && (third == 3 || third == 4) ? third : 0;
}

/* Rewrites the first chord of the bar's text with a suffix, the numeral in the case of its quality ("I" and "add9" in minor: "iadd9"). */
static void suffix_first_chord(char *text, int third, const char *suffix) {
    char rest[GROW_BLUEPRINT_TEXT - 24];
    size_t length = strcspn(text, " ");
    char numeral[16];
    snprintf(numeral, sizeof numeral, "%.*s", (int)(length < 15 ? length : 15), text);
    snprintf(rest, sizeof rest, "%s", text + length);
    for (char *c = numeral; third == 3 && *c; c++) {
        *c = (char)((*c >= 'A' && *c <= 'Z') ? *c + ('a' - 'A') : *c);
    }
    snprintf(text, GROW_BLUEPRINT_TEXT, "%s%s%s", numeral, suffix, rest);
}

/* The step above a pitch class in the key's natural scale, in semitones. */
static int step_above(const Marks *marks, int pc) {
    int rel = ((pc - tonic_pc(marks->request)) % 12 + 12) % 12;
    for (int up = 1; up <= 3; up++) {
        if (pitch_material_contains(marks->request->key.material, rel + up)) {
            return up;
        }
    }
    return 2;
}

/* The pitch of the chord tone nearest the mean pitch (placed by the octave rule, with `above` semitones of room under the top), the chord's pcs given. */
static int nearest_tone(const Marks *marks, const int pcs[], size_t count, int above, int *pc_out) {
    int best = -1;
    for (size_t i = 0; i < count; i++) {
        int pitch = place_pc(pcs[i], marks->mean, marks->low, marks->high - above);
        if (pitch >= 0 && (best < 0 || abs(pitch - marks->mean) < abs(best - marks->mean))) {
            best = pitch;
            *pc_out = pcs[i];
        }
    }
    return best;
}

/* An appoggiatura on the bar's first note, resolving to the second (the same chord span): the chord tone c nearest the mean, the first note a scale step above it. */
static bool sound_appoggiatura(Marks *marks, size_t bar) {
    GrowBlueprintBar *this = &marks->out->bars[bar];
    const GrowBlueprintLine *line = &marks->drawn->rhythm[bar];
    MotiveSketchChord chords[MOTIVE_MAX_CHORDS];
    size_t count = read_chords(&(Reading){&marks->keyed, marks->request}, this->chords, chords);
    int pcs[MOTIVE_SKETCH_CHORD_TONES_MAX];
    if (count == 0 || !marks->drawn->known[bar] || line->count < 2 || !room(this, 2, 1) ||
        sounding(chords, count, line->notes[1].offset) != sounding(chords, count, line->notes[0].offset)) {
        return false;
    }
    size_t members = tones_of(marks, &chords[sounding(chords, count, line->notes[0].offset)], pcs);
    int pc = 0;
    int tone = nearest_tone(marks, pcs, members, 3, &pc);
    int step = step_above(marks, pc);
    if (tone < 0 || tone + step > marks->high) {
        return false;
    }
    add_anchor(this, line->notes[0].offset, tone + step);
    add_anchor(this, line->notes[1].offset, tone);
    add_event(this, line->notes[0].offset, PLAN_EVENT_APPOGGIATURA);
    return true;
}

/* A ninth on the downbeat over a major or minor triad in root position held a beat or more: the chord gains add9, the first note the ninth, the second the root. */
static bool sound_ninth(Marks *marks, size_t bar) {
    GrowBlueprintBar *this = &marks->out->bars[bar];
    const GrowBlueprintLine *line = &marks->drawn->rhythm[bar];
    MotiveSketchChord chords[MOTIVE_MAX_CHORDS];
    size_t count = read_chords(&(Reading){&marks->keyed, marks->request}, this->chords, chords);
    if (count == 0 || !marks->drawn->known[bar] || line->count < 1 || !room(this, 2, 1) || line->notes[0].offset.num != 0 ||
        rational_cmp(line->notes[0].duration, marks->beat) < 0) {
        return false;
    }
    int third = triad_third(marks, &chords[0]);
    int pcs[MOTIVE_SKETCH_CHORD_TONES_MAX];
    if (third == 0 || tones_of(marks, &chords[0], pcs) < 1) {
        return false;
    }
    int root = place_pc(pcs[0], marks->mean, marks->low, marks->high - 2);
    if (root < 0) {
        return false;
    }
    add_anchor(this, line->notes[0].offset, root + 2);
    add_event(this, line->notes[0].offset, PLAN_EVENT_APPOGGIATURA);
    if (line->count > 1 && sounding(chords, count, line->notes[1].offset) == 0) {
        add_anchor(this, line->notes[1].offset, root);
    }
    suffix_first_chord(this->chords, third, "add9");
    return true;
}

/* A major seventh on the bar's longest note: over I or IV in major, III or VI in minor in root position, the chord gains maj7. */
static bool sound_major_seventh(Marks *marks, size_t bar) {
    GrowBlueprintBar *this = &marks->out->bars[bar];
    MotiveSketchChord chords[MOTIVE_MAX_CHORDS];
    size_t count = read_chords(&(Reading){&marks->keyed, marks->request}, this->chords, chords);
    bool minor = pitch_material_third(marks->request->key.material) == 3;
    long note = longest_note(marks, bar, (Rational){0, 1});
    if (count == 0 || note < 0 || !room(this, 1, 0)) {
        return false;
    }
    int degree = chords[0].degree;
    int third = triad_third(marks, &chords[0]);
    bool fits = minor ? (degree == 3 || degree == 6) : (degree == 1 || degree == 4);
    int pcs[MOTIVE_SKETCH_CHORD_TONES_MAX];
    if (!fits || third != 4 || tones_of(marks, &chords[0], pcs) < 1) {
        return false;
    }
    int seventh = place_pc((pcs[0] + 11) % 12, marks->mean, marks->low, marks->high);
    if (seventh < 0) {
        return false;
    }
    add_anchor(this, marks->drawn->rhythm[bar].notes[note].offset, seventh);
    suffix_first_chord(this->chords, third, "maj7");
    return true;
}

/* A suspended fourth over a dominant that fills the bar: Vsus4 for the first cell, the note there the tonic, the first note of the second cell the leading tone. */
static bool sound_sus4(Marks *marks, size_t bar) {
    GrowBlueprintBar *this = &marks->out->bars[bar];
    const GrowBlueprintLine *line = &marks->drawn->rhythm[bar];
    MotiveSketchChord chords[MOTIVE_MAX_CHORDS];
    size_t count = read_chords(&(Reading){&marks->keyed, marks->request}, this->chords, chords);
    if (count != 1 || chords[0].degree != 5 || chords[0].inversion != 0 || chords[0].alteration != 0 || marks->cells.cells < 2 ||
        !marks->drawn->known[bar] || !room(this, 2, 0) || note_at_offset(marks, bar, (Rational){0, 1}) < 0) {
        return false;
    }
    Rational second = marks->cells.cell;
    long next = -1;
    for (size_t i = 0; i < line->count && next < 0; i++) {
        if (rational_cmp(line->notes[i].offset, second) >= 0 && rational_cmp(line->notes[i].offset, marks->cells.bar) < 0) {
            next = (long)i;
        }
    }
    int tonic = place_pc(tonic_pc(marks->request), marks->mean, marks->low, marks->high);
    int leading = place_pc((tonic_pc(marks->request) + 11) % 12, marks->mean, marks->low, marks->high);
    if (next < 0 || tonic < 0 || leading < 0) {
        return false;
    }
    char at[24];
    grow_blueprint_beats_text(second, &marks->request->meter, at, sizeof at);
    char text[GROW_BLUEPRINT_TEXT];
    size_t length = strcspn(this->chords, " ");
    snprintf(text, sizeof text, "%.*ssus4 %.*s@%s", (int)length, this->chords, (int)length, this->chords, at);
    snprintf(this->chords, GROW_BLUEPRINT_TEXT, "%s", text);
    add_anchor(this, (Rational){0, 1}, tonic);
    add_anchor(this, line->notes[next].offset, leading);
    return true;
}

/* The leading tone on the bar's last note over V or V7, the final tonic a semitone above it on the next bar's first note. */
static bool sound_leading_tone(Marks *marks, size_t bar) {
    GrowBlueprintBar *this = &marks->out->bars[bar];
    const GrowBlueprintLine *line = &marks->drawn->rhythm[bar];
    MotiveSketchChord chords[MOTIVE_MAX_CHORDS];
    size_t count = read_chords(&(Reading){&marks->keyed, marks->request}, this->chords, chords);
    if (count == 0 || !marks->drawn->known[bar] || line->count == 0 || bar + 1 >= marks->out->bar_count || !marks->drawn->known[bar + 1] ||
        marks->drawn->rhythm[bar + 1].count == 0 || marks->taken[bar + 1] || !room(this, 1, 0) || !room(&marks->out->bars[bar + 1], 1, 0)) {
        return false;
    }
    const PlanNote *last = &line->notes[line->count - 1];
    const MotiveSketchChord *chord = &chords[sounding(chords, count, last->offset)];
    int leading = place_pc((tonic_pc(marks->request) + 11) % 12, marks->mean, marks->low, marks->high - 1);
    if (chord->degree != 5 || chord->alteration != 0 || leading < 0) {
        return false;
    }
    add_anchor(this, last->offset, leading);
    add_anchor(&marks->out->bars[bar + 1], marks->drawn->rhythm[bar + 1].notes[0].offset, leading + 1);
    marks->taken[bar + 1] = true;
    return true;
}

static bool sound_realize(Marks *marks, unsigned kind, size_t bar) {
    switch (kind) {
    case THEME_SOUND_NINTH_DOWNBEAT:
        return sound_ninth(marks, bar);
    case THEME_SOUND_SUS4_DOMINANT:
        return sound_sus4(marks, bar);
    case THEME_SOUND_APPOGGIATURA:
        return sound_appoggiatura(marks, bar);
    case THEME_SOUND_MAJOR_SEVENTH:
        return sound_major_seventh(marks, bar);
    default:
        return sound_leading_tone(marks, bar);
    }
}

static BlueprintSound sound_index(unsigned kind) {
    return kind == THEME_SOUND_NINTH_DOWNBEAT ? BLUEPRINT_SOUND_NINTH_DOWNBEAT
           : kind == THEME_SOUND_SUS4_DOMINANT ? BLUEPRINT_SOUND_SUS4_DOMINANT
           : kind == THEME_SOUND_APPOGGIATURA ? BLUEPRINT_SOUND_APPOGGIATURA
           : kind == THEME_SOUND_MAJOR_SEVENTH ? BLUEPRINT_SOUND_MAJOR_SEVENTH
                                               : BLUEPRINT_SOUND_LEADING_TONE;
}

/* The sounds of the K8 places in bar order, the basic-slot ideas left out. */
static void place_sounds(Marks *marks, FillRng *rng) {
    for (size_t i = 1; i < marks->layout->type->idea_count; i++) {
        const ThemeTypeIdea *idea = &marks->layout->type->ideas[i];
        if (is_basic_slot(idea)) {
            continue;
        }
        for (size_t unit = 0; unit < (size_t)idea->units; unit++) {
            size_t bar = marks->layout->first[i] + unit;
            unsigned allowed = idea->expect.sounds[unit];
            unsigned kinds[5];
            unsigned weights[6];
            size_t count = 0;
            for (unsigned bit = THEME_SOUND_NINTH_DOWNBEAT; bit <= THEME_SOUND_LEADING_TONE; bit <<= 1) {
                BlueprintSound index = sound_index(bit);
                bool repeats = bit == THEME_SOUND_APPOGGIATURA;
                if ((allowed & bit) == 0 || (!repeats && marks->once[index]) || marks->preferences->sounds[index] == 0) {
                    continue;
                }
                kinds[count] = bit;
                weights[count++] = marks->preferences->sounds[index];
            }
            if (marks->taken[bar] || count == 0) {
                continue;
            }
            weights[count] = SOUND_NONE_WEIGHT;
            size_t pick = draw_weighted(rng, weights, count + 1);
            if (pick < count && sound_realize(marks, kinds[pick], bar)) {
                marks->once[sound_index(kinds[pick])] = true;
                marks->taken[bar] = true;
            }
        }
    }
}

/* The climax bars of the K8 units: a bar index each, with the weight the preferences give (m6 the bar 5, m7 the bar 6). */
static size_t climax_bars(const Marks *marks, size_t bars[], unsigned weights[]) {
    size_t count = 0;
    for (size_t i = 0; i < marks->layout->type->idea_count; i++) {
        const ThemeTypeIdea *idea = &marks->layout->type->ideas[i];
        for (size_t unit = 0; unit < (size_t)idea->units; unit++) {
            if ((idea->expect.climax >> unit) & 1u) {
                size_t bar = marks->layout->first[i] + unit;
                bars[count] = bar;
                weights[count++] = bar == 5 ? marks->preferences->climax[0] : bar == 6 ? marks->preferences->climax[1] : 1;
            }
        }
    }
    return count;
}

GrowResult grow_blueprint_generate_marks(const GrowRequest *request, const BlueprintPreferences *preferences, const GrowBlueprintDraw *drawn,
                                         FillRng *rng, GrowBlueprint *out, int *melody_low, int *melody_high, GrowRefusal *refusal) {
    BlueprintPreferences defaults;
    Layout layout;
    Marks *marks = NULL;
    if (!request || !request->motive || !drawn || !rng || !out || !melody_low || !melody_high) {
        return GROW_ERR_NULL;
    }
    if (!preferences) {
        blueprint_preferences_default(&defaults);
        preferences = &defaults;
    }
    if (!layout_of(request, &layout) || request->motive->note_count == 0) {
        return refuse(refusal, GROW_ERR_BAD_REQUEST, "/development", "a blueprint is written for an 8-bar sentence or period");
    }
    marks = calloc(1, sizeof(*marks));
    if (!marks) {
        return GROW_ERR_ALLOC;
    }
    marks->request = request;
    marks->preferences = preferences;
    marks->drawn = drawn;
    marks->layout = &layout;
    marks->out = out;
    marks->beat = beat_length(&request->meter);
    motive_init(&marks->keyed, request->meter);
    motive_set_key(&marks->keyed, &request->key, MOTIVE_SKETCH_KEY_GIVEN);
    if (!cells_of(&request->meter, &marks->cells)) {
        free(marks);
        return refuse(refusal, GROW_ERR_BAD_REQUEST, "/development", "a blueprint is written in 4/4, 3/4 or 6/8");
    }
    int highest = -1;
    int lowest = 128;
    long total = 0;
    for (size_t i = 0; i < request->motive->note_count; i++) {
        int pitch = motive_pitch(request->motive, i);
        highest = pitch > highest ? pitch : highest;
        lowest = pitch < lowest ? pitch : lowest;
        total += pitch;
    }
    marks->mean = (int)(total / (long)request->motive->note_count);
    /* The climax. */
    size_t bars[THEME_TYPE_IDEA_MAX * 2];
    unsigned weights[THEME_TYPE_IDEA_MAX * 2];
    size_t count = climax_bars(marks, bars, weights);
    GrowResult result = GROW_OK;
    if (count == 0) {
        result = refuse(refusal, GROW_ERR_BAD_REQUEST, "/blueprint/climax", "the theme type has no climax unit");
    } else {
        size_t pick = draw_weighted(rng, weights, count);
        size_t bar = bars[pick < count ? pick : count - 1];
        long note = longest_note(marks, bar, marks->beat);
        note = note >= 0 ? note : (drawn->known[bar] && drawn->rhythm[bar].count > 0 ? 0 : -1);
        MotiveSketchChord chords[MOTIVE_MAX_CHORDS];
        size_t chord_count = note >= 0 ? read_chords(&(Reading){&marks->keyed, request}, out->bars[bar].chords, chords) : 0;
        if (note < 0 || chord_count == 0) {
            result = refuse(refusal, GROW_ERR_STUCK, "/blueprint/climax", "no note of bar %zu can hold the climax", bar);
        } else {
            Rational at = drawn->rhythm[bar].notes[note].offset;
            int pcs[MOTIVE_SKETCH_CHORD_TONES_MAX];
            size_t members = tones_of(marks, &chords[sounding(chords, chord_count, at)], pcs);
            int climax = -1;
            for (int midi = highest + CLIMAX_ABOVE_MOTIF; climax < 0 && midi <= *melody_high; midi++) {
                for (size_t m = 0; m < members; m++) {
                    climax = util_pitch_class(midi) == (pcs[m] % 12 + 12) % 12 ? midi : climax;
                }
            }
            if (climax < 0) {
                result = refuse(refusal, GROW_ERR_STUCK, "/blueprint/climax", "no chord tone of bar %zu lies %d to %d semitones above the motif",
                                bar, CLIMAX_ABOVE_MOTIF, *melody_high - highest);
            } else {
                out->has_climax = true;
                out->climax_bar = bar;
                out->climax_at = at;
                out->climax_midi = climax;
                util_pitch_name(climax, util_key_prefers_flats(tonic_pc(request), request->key.material), out->climax_name, sizeof out->climax_name);
                int span = (int)preferences->range > climax - lowest ? (int)preferences->range : climax - lowest;
                *melody_high = climax;
                *melody_low = climax - span;
                marks->high = climax - 1;
                marks->low = *melody_low;
                place_sounds(marks, rng);
            }
        }
    }
    free(marks);
    return result;
}

/* --------------------------------------------------------------------------------------------- the echo */

/* Whether the bar belongs to a basic-slot idea of a sentence or a period: a restatement of the motif, which a request's blueprint gives no rhythm. */
static bool restates_motif(const SectionPairing *pairing, size_t bar) {
    if (pairing->beginning == SECTION_BEGINNING_PRESENTATION) {
        return bar < 4;
    }
    return bar < 2 || (bar >= 4 && bar < 6);
}

/* The rhythm text of one bar's line ("x:q x:e x:e x:h"), NULL when it cannot be written; the caller frees it. */
static char *rhythm_text_of(const GrowBlueprintLine *line, const PlanMeter *meter) {
    Motive motive;
    motive_init(&motive, *meter);
    motive.form = MOTIVE_SKETCH_FORM_RHYTHM;
    for (size_t i = 0; i < line->count && i < MOTIVE_MAX_NOTES; i++) {
        MotiveSketchNote *note = &motive.notes[motive.note_count++];
        note->pitch = MOTIVE_SKETCH_PITCH_UNKNOWN;
        note->onset = line->notes[i].offset;
        note->duration = line->notes[i].duration;
        rational_add(note->onset, note->duration, &motive.length);
    }
    return motive_to_rhythm_text(&motive);
}

cJSON *grow_blueprint_write(const GrowBlueprint *blueprint, const GrowBlueprintLine rhythm[], const bool known[], const SectionPairing *pairing,
                            const PlanMeter *meter, const SpelledKey *key) {
    static const char *const EVENTS[] = {"appoggiatura", "suspension", "passing", "neighbor", "escape", "anticipation", "retardation"};
    bool flats = util_key_prefers_flats(spelled_key_tonic_pitch_class(key), key->material);
    cJSON *root = cJSON_CreateObject();
    cJSON *bars = root ? cJSON_AddArrayToObject(root, "bars") : NULL;
    bool ok = root && bars;
    for (size_t b = 0; ok && b < blueprint->bar_count; b++) {
        const GrowBlueprintBar *bar = &blueprint->bars[b];
        cJSON *item = cJSON_CreateObject();
        ok = item && cJSON_AddItemToArray(bars, item);
        if (!ok) {
            cJSON_Delete(item);
            break;
        }
        ok = !bar->chords[0] || cJSON_AddStringToObject(item, "chords", bar->chords);
        if (ok && known[b] && !restates_motif(pairing, b)) {
            char *text = rhythm_text_of(&rhythm[b], meter);
            ok = text && cJSON_AddStringToObject(item, "rhythm", text);
            free(text);
        }
        cJSON *anchors = ok && bar->anchor_count > 0 ? cJSON_AddArrayToObject(item, "anchors") : NULL;
        for (size_t a = 0; ok && anchors && a < bar->anchor_count; a++) {
            char at[24];
            char name[16];
            cJSON *anchor = cJSON_CreateObject();
            grow_blueprint_beats_text(bar->anchors[a].offset, meter, at, sizeof at);
            pitch_name_of_midi(bar->anchors[a].midi, flats, name, sizeof name);
            ok = anchor && cJSON_AddStringToObject(anchor, "at", at) && cJSON_AddStringToObject(anchor, "pitch", name) &&
                 cJSON_AddItemToArray(anchors, anchor);
            if (!ok) {
                cJSON_Delete(anchor);
            }
        }
        cJSON *events = ok && bar->event_count > 0 ? cJSON_AddArrayToObject(item, "events") : NULL;
        for (size_t e = 0; ok && events && e < bar->event_count; e++) {
            char at[24];
            cJSON *event = cJSON_CreateObject();
            grow_blueprint_beats_text(bar->events[e].offset, meter, at, sizeof at);
            ok = event && cJSON_AddStringToObject(event, "at", at) && cJSON_AddStringToObject(event, "type", EVENTS[bar->events[e].type]) &&
                 cJSON_AddItemToArray(events, event);
            if (!ok) {
                cJSON_Delete(event);
            }
        }
    }
    if (ok && blueprint->has_climax) {
        char at[24];
        cJSON *climax = cJSON_AddObjectToObject(root, "climax");
        grow_blueprint_beats_text(blueprint->climax_at, meter, at, sizeof at);
        ok = climax && cJSON_AddNumberToObject(climax, "bar", (double)blueprint->climax_bar) && cJSON_AddStringToObject(climax, "at", at) &&
             cJSON_AddStringToObject(climax, "pitch", blueprint->climax_name);
    }
    if (!ok) {
        cJSON_Delete(root);
        return NULL;
    }
    return root;
}
