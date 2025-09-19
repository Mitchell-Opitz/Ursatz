# Variant registry

```c
#include <ursatz_composer/variant_registry.h>
```

The variant registry lists every way a motive can be made into a variant, one table row each. The first 9 rows are the development operations of `motive_develop` (`motive_develop.h`); the rhythm and interval rows that follow apply the library's motive relations. Each row has the category and the citation of Schoenberg's *Fundamentals of Musical Composition* it belongs to, and the `relation` it maps to: the relation of the same name in the Ursatz library (`analysis/motive_relation.h`), with the row's parameters in the relation's order. A variant is made by a chain of up to 4 operations applied from a seed, and comes with a record of how it was made and what it kept and changed. The [variants command](cli-variants.md) is built on this; the library works on its own.

## Operations

| Name | Label | Category | Citation | Parameters | Forms |
| --- | --- | --- | --- | --- | --- |
| `exact` | Exact | exact | Schoenberg ch. III, Ex. 14 | none | full, rhythm, contour |
| `transpose` | Transpose | melody-adaptation | ch. III, Exs. 14, 28 | `tonal`, `by`, `degree` | full |
| `invert` | Invert | intervals | ch. III, Exs. 14, 19 | `axis`, `tonal` | full, contour |
| `retrograde` | Retrograde | intervals | ch. III, Exs. 14, 19 | none | full, rhythm, contour |
| `interval-change` | Interval change | intervals | ch. III, Ex. 21 | `interval`, `amount`, `keep-contour` | full |
| `fragment` | Fragment | intervals | ch. III, Ex. 21; ch. VIII, Ex. 52 | `position`, `length`, `min` | full, rhythm, contour |
| `liquidate` | Liquidate | intervals | ch. VIII, Ex. 52 | `drop`, `keep-rhythm`, `min` | full, rhythm |
| `augment` | Augment | rhythm | ch. III, Exs. 14, 17 | `ratio` | full, rhythm |
| `diminish` | Diminish | rhythm | ch. III, Exs. 14, 17 | `ratio` | full, rhythm |
| `note-repeat` | Note repeat | rhythm | ch. III, Exs. 17h, i, k, l, n | `note`, `count` | full, rhythm |
| `rhythm-repeat` | Rhythm repeat | rhythm | ch. III, Exs. 17l, m, 18e | `first`, `length`, `target` | full, rhythm |
| `rhythm-shift` | Rhythm shift | rhythm | ch. III, Ex. 23 | `offset` | full, rhythm |
| `add-upbeat` | Add upbeat | rhythm | ch. III, Ex. 22 | `count`, `approach` | full, rhythm |
| `change-meter` | Change meter | rhythm | ch. III, Ex. 24 | `num`, `den` | full, rhythm |
| `add-interval` | Add interval | intervals | ch. III, Ex. 21 | `interval`, `steps` | full, contour |
| `omit-interval` | Omit interval | intervals | ch. III, Ex. 21 | `interval` | full, contour |
| `ancillary-fill` | Ancillary fill | intervals | ch. III, Exs. 18, 20 | `interval`, `kind` | full |
| `condense` | Condense | intervals | ch. III, Ex. 21; ch. VIII, Ex. 52 | `first`, `last`, `ratio` | full, rhythm |
| `repeat-feature` | Repeat feature | intervals | ch. III, Exs. 20h, 22a, b, d | `feature`, `times`, `steps` | full, contour |
| `shift-feature` | Shift feature | intervals | ch. III, Ex. 23 | `feature`, `offset` | full |

Categories: `exact`, `rhythm`, `intervals`, `harmony`, `melody-adaptation`. Harmony has no row until P14.

The first 9 rows call `motive_develop`; every later row converts the motive to a sketch, calls `motive_relation_apply` and converts the result back (`motive_sketch_bridge.h`). An operation refuses what its function refuses (an unchanged result, a tonal step with no key, a rhythm that cannot be written in the meter, a relation that does not apply). A feature row (`repeat-feature`, `shift-feature`) needs a labelled feature and is refused with `repeat-feature needs a labelled feature` on a motive that has none; a random chain only draws one for a motive that has a feature. `change-meter` is seldom usable (ch. III), so a random chain never draws it. A contour motive is only its ranks, so after an operation its result is read back from its own contour.

### Parameters

The parameters are the `develop` command's options without the dashes.

| Parameter | Kind | Range drawn | Meaning |
| --- | --- | --- | --- |
| `tonal` | bool | 0 or 1 | Move or mirror in steps of the key's scale instead of semitones. Drawn as 0 without a key. |
| `by` | int | -12 to 12, not 0 | How far to transpose: semitones, or steps when tonal. |
| `degree` | int | 1 to 7 | Transpose so the first note sits on this degree, the nearest way, tonal. Optional: never drawn. |
| `axis` | pitch | one of the motive's pitches | The pitch mirrored about. The first note when not set. |
| `interval` | int | 0 to the note count less 1 | The 1-based interval changed; 0 for every one. |
| `amount` | int | -6 to 6, not 0 | Semitones to widen by, negative to narrow. |
| `keep-contour` | bool | 0 or 1 | Refuse a result that changes a direction. |
| `position` | position | leading, trailing, middle | Where a fragment is taken from. |
| `length` | int | 1 to the note count less 1 | How many notes a fragment has. |
| `drop` | int | 1 to the note count less 1 | How many notes to drop, the weakest first. |
| `keep-rhythm` | bool | 0 or 1 | A dropped note's time stays as a rest. |
| `min` | int | 1 to 16 | The fewest notes a fragment or liquidation may have, 2 when not set. Optional: never drawn. |
| `ratio` | ratio | 2:1, 3:1, 3:2 (augment); 1:2, 1:3, 2:3 (diminish); 1:2, 1:3, 2:3 (condense) | The factor durations are multiplied by. |
| `note` | int | 0 to the note count less 1 | The 0-based note `note-repeat` splits. |
| `count` | int | 1 to 3 (`add-upbeat`), 2 to 4 (`note-repeat`) | How many equal notes a note becomes; how many upbeat notes lead in. |
| `first`, `last`, `target` | int | 0 to the note count less 1 | 0-based notes: the first note whose durations `rhythm-repeat` copies and the note they start on; the first and last note `condense` compresses. |
| `offset` | ratio | 1:2, 1:3, 2:3, 1:4, 3:4 | How far later, in whole notes: `rhythm-shift` in the bar, `shift-feature` for the feature. |
| `approach` | int | -1 or 1 | The upbeat steps in from below (1) or above (-1). |
| `num`, `den` | int | 1 to 16 | The new meter of `change-meter`; `den` must be 1, 2, 4, 8 or 16. |
| `steps` | int | -7 to 7, not 0 | Scale steps: from the note before the interval for `add-interval`; each repeat of `repeat-feature` is moved by it. |
| `kind` | int | 0 or 1 | `ancillary-fill`: 0 passing notes, 1 a neighbor note. |
| `feature` | int | 0 to the feature count less 1 | The 0-based labelled feature. |
| `times` | int | 1 or 2 | How many times `repeat-feature` repeats the feature. |

## Chains

A `VariantChain` is up to `VARIANT_CHAIN_MAX` (4) steps. A step is an operation name and its set parameters; a parameter not set is open.

```c
VariantChain chain = {0};
variant_step_init(&chain.steps[0], "invert");
variant_step_set(&chain.steps[0], "tonal", 1, 0);   /* axis stays open */
variant_step_init(&chain.steps[1], "augment");      /* ratio stays open */
chain.step_count = 2;

Motive variant;
VariantRecord record;
VariantResult result = variant_apply_chain(&source, &chain, 42, &variant, &record);
```

`variant_apply_chain` applies the steps in order, each to the result of the one before. The open parameters of every step are drawn from one `FillRng` seeded once with the seed, in step order and in the order of the operation's parameters. A step the operation refuses is drawn again, up to 16 draws. The same chain and seed give the same variant; a reroll is the same chain with another seed.

`variant_random_chain(source, seed, &chain)` draws 1 or 2 different operations that the source's form takes (never `exact`), all parameters open.

A chain that cannot be applied returns one of `VARIANT_ERR_EMPTY`, `VARIANT_ERR_UNKNOWN_OP`, `VARIANT_ERR_FORM`, `VARIANT_ERR_PARAM` or `VARIANT_ERR_REFUSED`, and the record's `failed_step` and `error` say which step and why: `interval-change does not apply to a contour motive`.

## Record

`VariantRecord` holds:

- `seed`, and for each step the operation, every resolved parameter and which ones were open (drawn).
- `kind`: `exact` when nothing changed, otherwise `motive-form`. `variant` is set only by Michelle in the GUI.
- `kept` and `changed`: dimension names, in the order rhythm, intervals, contour, harmony.
  - rhythm is kept when the durations and onsets are the source's times one common factor (`rhythm_scaled` says the factor is not 1).
  - intervals are kept when every diatonic step (semitones without a key) is the source's.
  - contour is kept when every direction is the source's.
  - harmony is kept when the best implied chord's degree is the source's (a full motive with a key only). When either motive has chords of its own it is kept when the chords of both have the same degrees at the same onsets.
  - A dimension the motive's form does not have is in neither list: a rhythm motive has only the rhythm, a contour motive only the contour.
- `recognizability`: the total of `motive_recognizability` against the source, as a fraction.
- `derivation`: each step's phrase, joined by "then", and what was kept and changed: "Inverted about D4 in the key's scale, then augmented 2:1. Kept the rhythm's proportions; changed the intervals, the contour and the harmony."

## JSON

`variant_registry_to_json()` writes `{"operations": [{"name", "label", "category", "citation", "relation", "params": [{"name", "kind", "min", "max", "description", "optional"}], "forms": []}]}`.

`variant_record_to_json(&record)` writes:

```json
{
  "seed": 42,
  "chain": {"steps": [{"op": "invert", "params": {"axis": 69, "tonal": true}, "open": ["axis"]}]},
  "kind": "motive-form",
  "kept": ["rhythm"],
  "changed": ["intervals", "contour"],
  "functions": [],
  "derivation": "Inverted about A4 in the key's scale. Kept the rhythm; changed the intervals and the contour.",
  "recognizability": 0.6429
}
```

A parameter value is a boolean, a number (an int or a MIDI pitch), a position name or a ratio text (`"2:1"`). `functions` stays `[]` until the function tags of P14.

## Adding an entry

Add one row to the `OPERATIONS` table in `src/motive/variant_registry.c`, with its parameters, an `apply` function that returns the `motive_develop` result (a library relation goes through `relation_apply`), a `describe` function that returns its phrase, the forms it takes, its category and citation and the library relation it maps to. Chains, records, JSON, the random chains and the tests that walk the registry read the table; nothing else changes.
