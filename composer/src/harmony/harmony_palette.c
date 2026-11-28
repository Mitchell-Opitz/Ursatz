#include "harmony/harmony_palette.h"

#include <string.h>

#include <analysis/dimension/dimension.h>
#include <theory/harmony/chord_value.h>
#include <theory/tonality/modal_chords.h>
#include <theory/tonality/roman_numeral_grammar.h>
#include <ursatz_composer/fill_rng.h>

/*
 * The palette operator (R-766). The cost of a candidate, lower is better:
 *   - 10 for each pair of bars against each shape. The value of a bar is the mean over its units of the chord's measure (brightness over the key's
 *     tonic with no material, or tension: harmony_palette_chord_value). `rise` counts each pair of neighbouring bars whose value drops, `fall` each
 *     that rises, `hold` each that changes, `peak` the drops up to the bar `at` and the rises from it, `step` each pair (a bar before `at`, a bar
 *     at or after it) whose earlier value is not below the later;
 *   - 2 for each pair of neighbouring open units with equal chords;
 *   - the Series 2 rank (chord_value.h) of the interval from each root to the next, a repeat adding nothing.
 */

#define SPAN_UNITS_MAX 64
#define POOL_MAX 16
#define REDRAWS 16
#define TONES 3

typedef struct {
    PaletteChord list[POOL_MAX];
    size_t count;
} Pool;

typedef struct {
    Pool pools[5];
} Pools;

static bool same_chord(const PaletteChord *a, const PaletteChord *b) {
    return a->root_semitones == b->root_semitones && a->quality == b->quality;
}

static void add_chord(Pool *pool, const PaletteChord *chord) {
    for (size_t i = 0; i < pool->count; i++) {
        if (same_chord(&pool->list[i], chord)) {
            return;
        }
    }
    if (pool->count < POOL_MAX) {
        pool->list[pool->count++] = *chord;
    }
}

/* Whether the material avoids the chord's degree (its root is the step of that degree). */
static bool avoided(const PitchMaterial *material, const PaletteChord *chord) {
    for (int degree = 1; degree <= 7; degree++) {
        if (pitch_material_step(material, degree) == chord->root_semitones && pitch_material_avoids(material, degree)) {
            return true;
        }
    }
    return false;
}

static bool usable(const PitchMaterial *material, const PaletteChord *chord) {
    return (chord->quality == STANDARD_TONALITY_TRIAD_MAJOR || chord->quality == STANDARD_TONALITY_TRIAD_MINOR) && !avoided(material, chord);
}

static bool build_pools(const HarmonyPaletteRequest *request, Pools *pools) {
    PaletteChord palette[2][PALETTE_DEGREES];
    size_t counts[2];
    memset(pools, 0, sizeof(*pools));
    if (!modal_palette(request->key->material, request->second, palette, counts)) {
        return false;
    }
    for (size_t c = 0; c < counts[0]; c++) {
        if (usable(request->key->material, &palette[0][c])) {
            add_chord(&pools->pools[HARMONY_POOL_FIRST], &palette[0][c]);
            add_chord(&pools->pools[HARMONY_POOL_ANY], &palette[0][c]);
            if (palette[0][c].shared) {
                add_chord(&pools->pools[HARMONY_POOL_SHARED], &palette[0][c]);
            }
        }
    }
    for (size_t c = 0; request->second != NULL && c < counts[1]; c++) {
        if (usable(request->second, &palette[1][c])) {
            add_chord(&pools->pools[HARMONY_POOL_SECOND], &palette[1][c]);
            add_chord(&pools->pools[HARMONY_POOL_ANY], &palette[1][c]);
            if (palette[1][c].color) {
                add_chord(&pools->pools[HARMONY_POOL_COLOR], &palette[1][c]);
            }
        }
    }
    return true;
}

/* A chord of an OMT numeral against the major scale of the center (the stage of a schema). */
static bool chord_of_numeral(const HarmonyPaletteRequest *request, const char *text, PaletteChord *out) {
    StandardTonalityNumeralText numeral;
    if (!standard_tonality_numeral_parse(text, &numeral) || numeral.has_target) {
        return false;
    }
    memset(out, 0, sizeof(*out));
    out->degree = numeral.degree;
    out->alteration = numeral.alteration;
    switch (numeral.quality) {
    case STANDARD_TONALITY_NUMERAL_MAJOR:
    case STANDARD_TONALITY_NUMERAL_DOMINANT_SEVENTH:
    case STANDARD_TONALITY_NUMERAL_MAJOR_SEVENTH:
        out->quality = STANDARD_TONALITY_TRIAD_MAJOR;
        break;
    case STANDARD_TONALITY_NUMERAL_MINOR:
    case STANDARD_TONALITY_NUMERAL_MINOR_SEVENTH:
        out->quality = STANDARD_TONALITY_TRIAD_MINOR;
        break;
    case STANDARD_TONALITY_NUMERAL_AUGMENTED:
        out->quality = STANDARD_TONALITY_TRIAD_AUGMENTED;
        break;
    default:
        out->quality = STANDARD_TONALITY_TRIAD_DIMINISHED;
        break;
    }
    int step = pitch_material_step(request->major, numeral.degree);
    if (step < 0) {
        return false;
    }
    out->root_semitones = ((step + numeral.alteration) % 12 + 12) % 12;
    return standard_tonality_numeral_format(&numeral, out->numeral, sizeof(out->numeral));
}

/* Series 2 rank of the interval from one root up to the next, 0 for a repeat. */
static int interval_rank(int from, int to) {
    int semitones = ((to - from) % 12 + 12) % 12;
    for (int i = 0; semitones != 0 && i < CHORD_VALUE_RANKS; i++) {
        if (CHORD_VALUE_SERIES_TWO[i].semitones == semitones) {
            return CHORD_VALUE_SERIES_TWO[i].rank;
        }
    }
    return 0;
}

bool harmony_palette_chord_value(const SpelledKey *key, const PaletteChord *chord, bool tension, long *numerator, long *denominator) {
    int triad[3];
    if (!standard_tonality_triad_quality_semitones(chord->quality, triad)) {
        return false;
    }
    int tonic = spelled_key_tonic_pitch_class(key);
    int base = 60 + (tonic + chord->root_semitones) % 12;
    DimensionNote notes[TONES];
    for (int i = 0; i < TONES; i++) {
        notes[i].onset = rational_ratio(0, 1);
        notes[i].duration = rational_ratio(1, 1);
        notes[i].pitch = base + triad[i];
    }
    DimensionSpan span = {notes, TONES, rational_ratio(1, 1), rational_ratio(1, 4), !tension, tonic, NULL};
    Rational value;
    if (dimension_measure(tension ? DIMENSION_TENSION : DIMENSION_BRIGHTNESS, &span, &value) != DIMENSION_OK) {
        return false;
    }
    *numerator = (long)value.num;
    *denominator = (long)value.den;
    return true;
}

static double unit_value(const HarmonyPaletteRequest *request, const PaletteChord *chord, bool tension) {
    long n;
    long d;
    return harmony_palette_chord_value(request->key, chord, tension, &n, &d) && d != 0 ? (double)n / (double)d : 0.0;
}

/* The cost of the bars' values against one shape. */
static long shape_cost(const double *bar, size_t bars, HarmonyPaletteArc arc) {
    long cost = 0;
    if (arc.shape == HARMONY_SHAPE_STEP) {
        for (size_t i = 0; i < bars; i++) {
            for (size_t j = i + 1; j < bars; j++) {
                bool straddles = (int)i + 1 < arc.at && (int)j + 1 >= arc.at;
                cost += straddles && bar[i] >= bar[j] ? 10 : 0;
            }
        }
        return cost;
    }
    for (size_t i = 0; arc.shape != HARMONY_SHAPE_NONE && i + 1 < bars; i++) {
        bool drop = bar[i + 1] < bar[i];
        bool rise = bar[i + 1] > bar[i];
        bool before = (int)i + 2 <= arc.at; /* the pair ends at or before the peak */
        switch (arc.shape) {
        case HARMONY_SHAPE_RISE:
            cost += drop ? 10 : 0;
            break;
        case HARMONY_SHAPE_FALL:
            cost += rise ? 10 : 0;
            break;
        case HARMONY_SHAPE_HOLD:
            cost += drop || rise ? 10 : 0;
            break;
        case HARMONY_SHAPE_PEAK:
            cost += (before ? drop : rise) ? 10 : 0;
            break;
        default:
            break;
        }
    }
    return cost;
}

static long cost_of(const HarmonyPaletteRequest *request, const HarmonyPaletteChoice *choice) {
    size_t units = choice->count;
    size_t per_bar = (size_t)request->units_per_bar;
    size_t bars = units / per_bar;
    long cost = 0;
    double bright[HARMONY_PALETTE_UNITS_MAX];
    double tense[HARMONY_PALETTE_UNITS_MAX];
    for (size_t b = 0; b < bars; b++) {
        double sum_bright = 0;
        double sum_tense = 0;
        for (size_t u = 0; u < per_bar; u++) {
            const PaletteChord *chord = &choice->chords[b * per_bar + u];
            sum_bright += request->brightness.shape != HARMONY_SHAPE_NONE ? unit_value(request, chord, false) : 0;
            sum_tense += request->tension.shape != HARMONY_SHAPE_NONE ? unit_value(request, chord, true) : 0;
        }
        bright[b] = sum_bright / (double)per_bar;
        tense[b] = sum_tense / (double)per_bar;
    }
    cost += shape_cost(bright, bars, request->brightness) + shape_cost(tense, bars, request->tension);
    for (size_t i = 1; i < units; i++) {
        if (!choice->fixed[i] && !choice->fixed[i - 1] && same_chord(&choice->chords[i], &choice->chords[i - 1])) {
            cost += 2;
        }
        cost += interval_rank(choice->chords[i - 1].root_semitones, choice->chords[i].root_semitones);
    }
    return cost;
}

/* The fixed units: given chords, then the stages of each schema (the rotation drawn from rng). False for a schema that does not fit its span. */
static bool place_fixed(const HarmonyPaletteRequest *request, FillRng *rng, HarmonyPaletteChoice *choice) {
    for (size_t g = 0; g < request->given_count; g++) {
        if (request->given[g].unit >= choice->count) {
            return false;
        }
        choice->chords[request->given[g].unit] = request->given[g].chord;
        choice->fixed[request->given[g].unit] = true;
    }
    for (size_t s = 0; s < request->span_count; s++) {
        const HarmonyPaletteSpan *span = &request->spans[s];
        const SchemaEntry *schema = span->schema;
        if (schema == NULL) {
            continue;
        }
        size_t total = 0;
        for (size_t k = 0; k < schema->stage_count; k++) {
            total += schema->stages[k].units == 0 ? 1 : schema->stages[k].units;
        }
        if (total == 0 || (!span->loop && total > span->unit_count)) {
            return false;
        }
        size_t rotation = schema->rotation_count > 0 ? schema->rotations[fill_rng_below(rng, schema->rotation_count)] : 0;
        size_t stage = rotation < schema->stage_count ? rotation : 0;
        size_t left = schema->stages[stage].units == 0 ? 1 : schema->stages[stage].units;
        size_t placed = 0;
        for (size_t u = 0; u < span->unit_count; u++) {
            size_t at = span->first_unit + u;
            if (at >= choice->count) {
                return false;
            }
            if (!span->loop && placed == total) {
                break;
            }
            PaletteChord chord;
            if (!choice->fixed[at]) {
                if (!chord_of_numeral(request, schema->stages[stage].chord, &chord)) {
                    return false;
                }
                choice->chords[at] = chord;
                choice->fixed[at] = true;
            }
            placed++;
            if (--left == 0) {
                stage = (stage + 1) % schema->stage_count;
                left = schema->stages[stage].units == 0 ? 1 : schema->stages[stage].units;
            }
        }
    }
    return true;
}

static void apply_cadence(const HarmonyPaletteRequest *request, HarmonyPaletteChoice *choice) {
    if (!request->has_cadence || choice->count == 0) {
        return;
    }
    size_t last = choice->count - 1;
    if (request->cadence == CADENCE_TYPE_PERFECT_AUTHENTIC) {
        if (!choice->fixed[last]) {
            choice->chords[last] = request->tonic;
        }
        if (last > 0 && !choice->fixed[last - 1]) {
            choice->chords[last - 1] = request->approach;
        }
    } else if (!choice->fixed[last]) {
        choice->chords[last] = request->approach;
    }
}

static bool same_sequence(const HarmonyPaletteChoice *a, const HarmonyPaletteChoice *b) {
    for (size_t i = 0; i < a->count; i++) {
        if (!same_chord(&a->chords[i], &b->chords[i])) {
            return false;
        }
    }
    return a->count == b->count;
}

/* The pool of an open unit: its span's, else any. */
static HarmonyPalettePool pool_of(const HarmonyPaletteRequest *request, size_t unit) {
    for (size_t s = 0; s < request->span_count; s++) {
        if (unit >= request->spans[s].first_unit && unit < request->spans[s].first_unit + request->spans[s].unit_count) {
            return request->spans[s].pool;
        }
    }
    return HARMONY_POOL_ANY;
}

bool harmony_palette_draw(const HarmonyPaletteRequest *request, size_t suggestion, HarmonyPaletteChoice *out) {
    Pools pools;
    if (request == NULL || out == NULL || request->key == NULL || request->key->material == NULL || request->bars < 1 || request->units_per_bar < 1 ||
        (size_t)request->bars * (size_t)request->units_per_bar > HARMONY_PALETTE_UNITS_MAX || request->major == NULL || !build_pools(request, &pools)) {
        return false;
    }
    size_t units = (size_t)request->bars * (size_t)request->units_per_bar;
    HarmonyPaletteChoice winners[16];
    size_t winner_count = 0;
    for (size_t k = 0; k <= suggestion && k < 16; k++) {
        FillRng rng;
        fill_rng_seed(&rng, request->seed + 13 + 7919 * (unsigned long)k);
        HarmonyPaletteChoice best;
        bool have_best = false;
        HarmonyPaletteChoice least;
        bool have_least = false;
        for (size_t candidate = 0; candidate < HARMONY_PALETTE_CANDIDATES; candidate++) {
            HarmonyPaletteChoice choice;
            memset(&choice, 0, sizeof(choice));
            choice.count = units;
            if (!place_fixed(request, &rng, &choice)) {
                return false;
            }
            for (size_t u = 0; u < units; u++) {
                if (choice.fixed[u]) {
                    continue;
                }
                const Pool *pool = &pools.pools[pool_of(request, u)];
                if (pool->count == 0) {
                    return false;
                }
                PaletteChord drawn = pool->list[fill_rng_below(&rng, pool->count)];
                for (int again = 0; again < REDRAWS && u > 0 && ((drawn.root_semitones - choice.chords[u - 1].root_semitones) % 12 + 12) % 12 == 6; again++) {
                    drawn = pool->list[fill_rng_below(&rng, pool->count)];
                }
                choice.chords[u] = drawn;
            }
            apply_cadence(request, &choice);
            choice.cost = cost_of(request, &choice);
            bool distinct = true;
            for (size_t w = 0; w < winner_count; w++) {
                distinct = distinct && !same_sequence(&choice, &winners[w]);
            }
            if (!have_least || choice.cost < least.cost) {
                least = choice;
                have_least = true;
            }
            if (distinct && (!have_best || choice.cost < best.cost)) {
                best = choice;
                have_best = true;
            }
        }
        winners[winner_count++] = have_best ? best : least;
    }
    *out = winners[winner_count - 1];
    return true;
}
