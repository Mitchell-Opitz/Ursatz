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
| `reduce-to-frame` | Reduce to frame | melody-adaptation | ch. II and III | none | full |
| `add-non-chord-tone` | Add non-chord tone | melody-adaptation | ch. II and III | `after`, `type` | full |
| `remove-non-chord-tone` | Remove non-chord tone | melody-adaptation | ch. II and III | `note` | full |
| `change-non-chord-tone` | Change non-chord tone | melody-adaptation | ch. II and III | `note`, `type` | full |
| `reharmonize-frame` | Reharmonize frame | melody-adaptation | ch. II and III | `degree`, `seventh` | full |

Categories: `exact`, `rhythm`, `intervals`, `harmony`, `melody-adaptation`.

The motive is the library's `MotiveSketch` (`typedef MotiveSketch Motive`), so no conversion stands between a row and the library. The first 9 rows call `motive_develop`, which applies `motive_relation_apply` to the motive and takes the notes, meter, pickup, harmony, features and role of the result; every later row does the same through `relation_apply`, and both read the library's result through `motive_relation_develop` (`src/motive/motive_develop.c`). A full motive with a key and no harmony of its own is given its best implied chord for the relation, and the result leaves that chord out unless the row changes the harmony. Augment and diminish scale the pickup with the notes. An operation refuses what its function refuses (an unchanged result, a tonal step with no key, a rhythm that cannot be written in the meter, a relation that does not apply). A feature row (`repeat-feature`, `shift-feature`) needs a labelled feature and is refused with `repeat-feature needs a labelled feature` on a motive that has none; a random chain only draws one for a motive that has a feature. `change-meter` is seldom usable (ch. III), so a random chain never draws it. A contour motive is only its ranks, so after an operation its result is normalized to its own contour ranks by the library (`motive_sketch_contour_normalize`).

The frame rows work on the motive's tone labels (see [the motive command](cli-motive.md)), against its own harmony or the base harmony proposed from its notes. `reduce-to-frame` takes out every non-chord and unclassified note, giving its time to the note before it (a leading one's to the next kept note). `add-non-chord-tone` puts a figure of `type` (0 passing, 1 neighbor, 2 double neighbor, 3 changing tone, 4 suspension, 5 anticipation, 6 appoggiatura, 7 escape tone, 8 retardation, 9 pedal; the numbers of `NonChordTone`) between chord tone `after` (0-based) and the next note, taking half of the earlier note's duration. `remove-non-chord-tone` takes out the non-chord or unclassified note `note`, and `change-non-chord-tone` moves it (and, for a double neighbor or changing tone, its partner) to form `type` between the same chord tones. `reharmonize-frame` replaces every chord by the one chord of `degree` (`seventh` makes it a seventh chord) at 0 and adapts the melody as the harmony rows do; its variant keeps that chord as its own harmony. A row is refused when the figure cannot be formed (`unchanged`, `not applicable`), or when the merged durations cannot be written (`not expressible`). The open parameters of `add-non-chord-tone`, `remove-non-chord-tone` and `change-non-chord-tone` are drawn only from the notes and types the motive accepts: every combination is tried and one that applies is chosen, so a step with an applicable combination is never refused. A random chain draws `reduce-to-frame`, `remove-non-chord-tone` and `change-non-chord-tone` only for a motive that has a non-chord or unclassified note.

The harmony rows, `reharmonize-frame` and the dominant form take a full motive with a key and are refused without one (`no key`). A motive without chords of its own is given its best implied chord at 0 first, so its result has chords. Each row changes the chords and then adapts the melody: a note on a beat whose chord changed that is not a chord tone moves to the nearest chord tone, keeping the rhythm and as much of the contour as it can. The open parameters of a harmony row (and of `reharmonize-frame`) are drawn from the combinations that move at least one pitch (`motive_relation_harmony_changes_notes`) when there are any, else from those that apply. A variant made by one of these rows keeps the result's chords as its own harmony, even a single chord that its melody also implies. `passing-harmony` puts a chord between two chords (it needs two, and a random chain draws it only for a motive with two or more of its own); `dominant-form` turns I into V, I-V and any other harmony starting on I into V-I and I-V-I into V-I-V, moving the melody a fourth down (a fifth up when that is out of range) with rhythm and contour kept, and is refused when the first chord is not I. `counter-line` works on an accompaniment motive only (`--role accompaniment`): runs of one repeated pitch inside a chord become a line moving between its tones. On a melodic motive it is refused with `counter-line needs an accompaniment motive`, and a random chain draws it only for an accompaniment.

The parameters of each operation are in [variant-parameters.md](variant-parameters.md).

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

`variant_leaves_harmony(source, variant)` marks a variant that does not outline its harmony (R-197): more than half of its strong-beat notes (notes on a beat of the meter) are non-chord or unclassified against its base harmony, or it has a note that does not resolve and the source had none more. The record carries it as `leaves_harmony`, and the `variants` command adds the warning `Does not outline its harmony` to the variant's `analysis.warnings`. The variant is kept either way.

`variant_random_chain(source, seed, &chain)` draws 1 or 2 different operations that the source's form takes (never `exact`), all parameters open. `variant_random_chain_of(source, seed, min_steps, max_steps, &chain)` draws the same way with a chosen length (1 to 4, false outside it); `variant_random_chain` is the 1 to 2 case. The `variants` command's `--distance` asks for 1 step (close), 2 (medium) or 3 to 4 (far) and keeps the variants whose remoteness lies in the band: at most 1/3, over 1/3 up to 2/3, over 2/3.

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
