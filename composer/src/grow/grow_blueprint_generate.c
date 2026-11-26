#include <ursatz_composer/grow_types.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <model/pitch/pitch_material.h>

#include <ursatz_composer/motive.h>
#include <ursatz_composer/motive_blueprint.h>
#include <ursatz_composer/theme_shape.h>

#include "grow/grow_internal.h"
#include "structure/plan_util.h"

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

/* An onset in whole notes from a bar's start as the own-harmony syntax writes it: in beats of the meter's beat unit ("2", "3/2"). */
static void onset_text(Rational time, const PlanMeter *meter, char *out, size_t size) {
    Rational beats = time;
    if (rational_mul_int(time, meter->beat_unit, &beats) != RATIONAL_OK) {
        beats = time;
    }
    if (beats.den == 1) {
        snprintf(out, size, "%lld", (long long)beats.num);
    } else {
        snprintf(out, size, "%lld/%lld", (long long)beats.num, (long long)beats.den);
    }
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
                onset_text(within, &request->meter, at, sizeof at);
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
            onset_text(within, meter, at, sizeof at);
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
