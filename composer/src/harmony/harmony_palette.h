#ifndef URSATZ_COMPOSER_HARMONY_PALETTE_H
#define URSATZ_COMPOSER_HARMONY_PALETTE_H

#include <stdbool.h>
#include <stddef.h>

#include <model/pitch/pitch_material.h>
#include <theory/knowledge/schema_entry.h>
#include <theory/tonality/modal_palette.h>
#include <theory/tonality/spelled_key.h>
#include <theory/vocabulary/cadence_type.h>

/*
 * The palette operator (R-766): one chord for each unit of a harmony plan, drawn from the chords of one or two modes on the key's tonic, with
 * K5 progression schemas placed, chords given, brightness and tension shapes asked for and a cadence fixed. It draws candidate sequences from a
 * seeded stream and keeps the one of least cost (see harmony_palette.c); it knows no chord text, which the caller writes and reads back.
 */

#define HARMONY_PALETTE_UNITS_MAX 256
#define HARMONY_PALETTE_SPANS_MAX 16
#define HARMONY_PALETTE_CANDIDATES 64

typedef enum {
    HARMONY_POOL_FIRST,  /* the key's material's chords */
    HARMONY_POOL_SECOND, /* the `with` material's chords */
    HARMONY_POOL_SHARED, /* the chords both hold */
    HARMONY_POOL_COLOR,  /* the `with` material's chords that carry a tone the key's material lacks */
    HARMONY_POOL_ANY     /* the union of the first and the second */
} HarmonyPalettePool;

typedef enum { HARMONY_SHAPE_NONE, HARMONY_SHAPE_RISE, HARMONY_SHAPE_FALL, HARMONY_SHAPE_HOLD, HARMONY_SHAPE_PEAK, HARMONY_SHAPE_STEP } HarmonyPaletteShape;

typedef struct {
    HarmonyPaletteShape shape;
    int at; /* the bar (from 1) a peak or a step is at */
} HarmonyPaletteArc;

/* A run of units (a bar is `units_per_bar` of them) with the pool its open units draw from, and a schema placed on it, if any. */
typedef struct {
    size_t first_unit;
    size_t unit_count;
    HarmonyPalettePool pool;
    const SchemaEntry *schema; /* a K5 progression schema, or NULL */
    bool loop;
} HarmonyPaletteSpan;

typedef struct {
    size_t unit;
    PaletteChord chord;
} HarmonyPaletteGiven;

typedef struct {
    const SpelledKey *key;
    const PitchMaterial *second; /* NULL for none */
    const PitchMaterial *major;  /* the major material, which the numerals of a schema are read against */
    long bars;
    int units_per_bar;
    HarmonyPaletteSpan spans[HARMONY_PALETTE_SPANS_MAX];
    size_t span_count;
    HarmonyPaletteGiven given[HARMONY_PALETTE_UNITS_MAX];
    size_t given_count;
    HarmonyPaletteArc brightness;
    HarmonyPaletteArc tension;
    bool has_cadence;
    CadenceType cadence; /* PAC or HC when has_cadence */
    PaletteChord tonic;    /* the chord a PAC ends on */
    PaletteChord approach; /* the chord before it (a PAC) or the last chord (an HC) */
    unsigned long seed;
} HarmonyPaletteRequest;

typedef struct {
    size_t count;
    PaletteChord chords[HARMONY_PALETTE_UNITS_MAX];
    bool fixed[HARMONY_PALETTE_UNITS_MAX]; /* given, placed by a schema or fixed by the cadence */
    long cost;
} HarmonyPaletteChoice;

/*
 * The chords of suggestion `suggestion` (0 for the first), one for each unit: bars times units_per_bar. The pools are the triads of the two
 * materials without diminished and augmented chords and without the material's avoided degrees. Given chords stand as given, then the schema
 * stages (a rotation drawn among the schema's rotations, each stage its units, repeated while `loop` and cut at the span's end); a non-loop schema
 * longer than its span, or a pool with no chord for an open unit, is false. 64 candidate sequences are drawn from a stream seeded with the seed
 * plus 13 plus 7919 times the suggestion, each open unit uniform from its pool, a unit redrawn up to 16 times when its root is a tritone from the
 * one before; the cost of a candidate is 10 for each pair of bars against each shape (see harmony_palette.c), 2 for each pair of equal neighbouring
 * open units and the Series 2 rank of each neighbouring root interval; the cadence fixes the last units. The least cost wins, the earlier on a tie;
 * suggestion k is the least-cost candidate of its own draw that differs from the suggestions before it (the least when none does).
 */
bool harmony_palette_draw(const HarmonyPaletteRequest *request, size_t suggestion, HarmonyPaletteChoice *out);

/* The brightness or tension value of a chord in root position, one note each from C4 up, over the key's tonic: the measure the cost reads. */
bool harmony_palette_chord_value(const SpelledKey *key, const PaletteChord *chord, bool tension, long *numerator, long *denominator);

#endif
