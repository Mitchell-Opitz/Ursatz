# Variant registry

```c
#include <ursatz_composer/variant_registry.h>
```

The variant registry lists every way a motive can be made into a variant, one table row each. The 9 rows are the development operations of `motive_develop` (`motive_develop.h`), each with the category and the citation of Schoenberg's *Fundamentals of Musical Composition* it belongs to. A variant is made by a chain of up to 4 operations applied from a seed, and comes with a record of how it was made and what it kept and changed. The [variants command](cli-variants.md) is built on this; the library works on its own.

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

Categories: `exact`, `rhythm`, `intervals`, `harmony`, `melody-adaptation`. Harmony has no row until P14.

`apply` calls `motive_develop`, so an operation refuses what `motive_develop` refuses (an unchanged result, a tonal step with no key, a rhythm that cannot be written in the meter). A contour motive is only its ranks, so after an operation its result is read back from its own contour.

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
| `ratio` | ratio | 2:1, 3:1, 3:2 (augment); 1:2, 1:3, 2:3 (diminish) | The factor durations are multiplied by. |

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

`variant_registry_to_json()` writes `{"operations": [{"name", "label", "category", "citation", "params": [{"name", "kind", "min", "max", "description", "optional"}], "forms": []}]}`.

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

Add one row to the `OPERATIONS` table in `src/motive/variant_registry.c`, with its parameters, an `apply` function that returns the `motive_develop` result, a `describe` function that returns its phrase, the forms it takes and its category and citation. Chains, records, JSON, the random chains and the tests that walk the registry read the table; nothing else changes.
