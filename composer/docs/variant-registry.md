# Variant registry

```c
#include <ursatz_composer/variant_registry.h>
```

The variant registry lists every way a motive can be made into a variant, one table row each. Every row applies a library motive relation: the first 9 rows are the development operations of `motive_develop` (`motive_develop.h`), which calls the relation of the same name, and the rhythm, interval, harmony and melody-adaptation rows that follow convert the motive to a sketch themselves. Each row has the category and the citation of Schoenberg's *Fundamentals of Musical Composition* it belongs to, and the `relation` it maps to: the relation of the same name in the Ursatz library (`analysis/motive_relation.h`), with the row's parameters in the relation's order. A variant is made by a chain of up to 4 operations applied from a seed, and comes with a record of how it was made and what it kept and changed. The [variants command](cli-variants.md) is built on this; the library works on its own.

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
| `harmony-inversion` | Harmony inversion | harmony | ch. III, Exs. 25a, b | `chord`, `inversion` | full |
| `harmony-append` | Harmony append | harmony | ch. III, Exs. 25c to i | `degree`, `seventh` | full |
| `harmony-insert` | Harmony insert | harmony | ch. III, Ex. 26 | `chord`, `degree` | full |
| `harmony-substitute` | Harmony substitute | harmony | ch. III, Exs. 27a to i | `chord`, `degree`, `then` | full |
| `passing-harmony` | Passing harmony | melody-adaptation | ch. III, Ex. 29 | `chord` | full |
| `counter-line` | Counter line | melody-adaptation | ch. III, Ex. 29 | none | full |
| `dominant-form` | Dominant form | harmony | ch. V, the dominant form | none | full |
| `liquidate-features` | Liquidate features | intervals | ch. VIII, Ex. 52 | `steps`, `shorten` | full |

Categories: `exact`, `rhythm`, `intervals`, `harmony`, `melody-adaptation`.

The first 9 rows call `motive_develop`, which converts the motive to a sketch, calls `motive_relation_apply` and converts the result back (`motive_sketch_bridge.h`); every later row does the same through `relation_apply`, and both read the library's result with `motive_sketch_result`. Augment and diminish scale the pickup with the notes. An operation refuses what its function refuses (an unchanged result, a tonal step with no key, a rhythm that cannot be written in the meter, a relation that does not apply). A feature row (`repeat-feature`, `shift-feature`) needs a labelled feature and is refused with `repeat-feature needs a labelled feature` on a motive that has none; a random chain only draws one for a motive that has a feature. `change-meter` is seldom usable (ch. III), so a random chain never draws it. A contour motive is only its ranks, so after an operation its result is normalized to its own contour ranks by the library (`motive_sketch_contour_normalize`).

The harmony rows and the dominant form take a full motive with a key and are refused without one (`no key`). A motive without chords of its own is given its best implied chord at 0 first, so its result has chords. Each row changes the chords and then adapts the melody: a note on a beat whose chord changed that is not a chord tone moves to the nearest chord tone, keeping the rhythm and as much of the contour as it can. A variant made by one of these rows keeps the result's chords as its own harmony, even a single chord that its melody also implies. `passing-harmony` puts a chord between two chords (it needs two, and a random chain draws it only for a motive with two or more of its own); `dominant-form` turns I into V, I-V and any other harmony starting on I into V-I and I-V-I into V-I-V, moving the melody a fourth down (a fifth up when that is out of range) with rhythm and contour kept, and is refused when the first chord is not I. `counter-line` works on an accompaniment motive only (`--role accompaniment`): runs of one repeated pitch inside a chord become a line moving between its tones. On a melodic motive it is refused with `counter-line needs an accompaniment motive`, and a random chain draws it only for an accompaniment.

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
| `chord` | int | 0 to the chord count less 1 (the one implied chord when the motive has none of its own) | The 0-based chord a harmony row changes: inverted, inserted after, replaced, or the first of the two a passing chord goes between. |
| `inversion` | int | 0 to 3 | `harmony-inversion`: root position, first, second or third inversion (third needs a seventh chord). |
| `degree` | int | 1 to 7 | The scale degree of the chord a harmony row adds or puts in place. |
| `seventh` | bool | 0 or 1 | `harmony-append`: the added chord is a seventh chord. |
| `then` | int | 1 to 7 | `harmony-substitute`: a second chord that follows the first in the same time. Optional: never drawn. |

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

`liquidate-features` thins a full motive with a key by its characteristic features (R-144): each step replaces the heaviest feature of two notes or more by one note on the chord tone nearest its first note, lasting the feature's span. A feature's weight is its leaps (steps of 3 or more, semitones 3 or more without a scale) plus its notes whose duration occurs only once in the motive; equal weights go to the later one. A motive without labelled features takes each beat group (notes from one beat onset to the next) as a feature, `f1`, `f2` in order. `steps` is 1 to 3; `shorten` also diminishes the result 1:2, the unit halving as in ch. VIII, Ex. 52. It is refused when no feature has two notes or more (`not applicable`), and, as it chooses chord tones, without a key (`no key`); a rhythm motive has no chord tones, so the row takes the full form only. The variant keeps the chord the source implied (or its own chords) as its own harmony under the thinned melody, as the harmony rows do.

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
  - harmony is kept when the best implied chord's degree is the source's (a full motive with a key only). When either motive has chords of its own it is kept when the chords of both have the same degrees, alterations, inversions, sevenths and onsets.
  - A dimension the motive's form does not have is in neither list: a rhythm motive has only the rhythm, a contour motive only the contour.
- `harmony`: the result's own harmony as text (`I IV@2`), empty when it has none.
- `recognizability`: the total of `motive_recognizability` against the source, as a fraction.
- `remoteness_num` and `remoteness_den`: how far the result is from the basic motive, the tree root (R-142), set by `variant_set_remoteness(basic, form, depth, &record)`. It is the recognizer's distance (cost over cost plus 4) from the basic motive, not from the form's parent, and 1/1 when no chain within `depth` is found. `remoteness_den` is 0 until it is set.
- `functions` and `function_count`: what `variant_functions` found, below.
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
  "harmony": "I IV@2",
  "functions": ["continuation"],
  "derivation": "Inverted about A4 in the key's scale. Kept the rhythm; changed the intervals and the contour.",
  "recognizability": 0.6429,
  "remoteness": 0.3333
}
```

A parameter value is a boolean, a number (an int or a MIDI pitch), a position name or a ratio text (`"2:1"`). `harmony` is written only when the result has chords of its own. `remoteness` is a number to 4 places, written only once `variant_set_remoteness` has set it. `functions` is `[]` until `variant_functions` has filled the record's `functions`; the `variants` command fills both.

## Remoteness and functions

`variant_functions(basic, form, &record, out, &count)` tags a form with the functions it suits in a phrase built from `basic` (R-143, Schoenberg ch. IV, V and VIII), several allowed, in this order:

| Function | When |
| --- | --- |
| `beginning` | the form's first chord is I, its rhythm is the basic motive's (not scaled) and its remoteness is at most 1/2 |
| `continuation` | the form is shorter than the basic motive, or the record's chain holds `fragment`, `liquidate`, `liquidate-features`, `condense` or `repeat-feature` |
| `contrast` | its remoteness is over 1/2, or its first chord is not I |
| `cadential` | its last chord is I or V, its last note is degree 1, 3 or 5 and no note is longer than the last |

A chord is the form's own harmony, else its best implied chord. A form without a key has no chord, so it is never `beginning` or `cadential`, and its first chord does not make it `contrast`. A rhythm or contour form is only `continuation` or `contrast`, by length and remoteness. The dominant form starts on V, so it is a contrast, not a beginning. Remoteness is the record's, or searched when the record has none. Remoteness over 3/4 is penalized as a preference when phrases are built (ch. IV, "remote forms endanger comprehensibility"); nothing here refuses it.

## Recognizing a variant

`variant_recognize(source, target, &chain, &recognition)` is the reverse: it finds the chain of rows that turns `source` into `target`, `found`, its `distance` as a fraction, `kept`, `changed` and the derivation. Each library relation step becomes the registry step of the row whose `relation` it is, every parameter set; only steps the registry can replay are searched. See [docs/cli-recognize.md](cli-recognize.md).

## Adding an entry

Add one row to the `OPERATIONS` table in `src/motive/variant_registry.c`, with its parameters, an `apply` function that returns the `motive_develop` result (a library relation goes through `relation_apply`), a `describe` function that returns its phrase, the forms it takes, its category and citation and the library relation it maps to. Chains, records, JSON, the random chains and the tests that walk the registry read the table; nothing else changes.
