# Pieces from form templates

`piece_build_plan` (`<ursatz_composer/piece.h>`) plans a short piece from a named form template, and `compose_run`
(`<ursatz_composer/compose.h>`) fills every section of the plan with the grow stages. A piece is a row of theme sections
as [grow](grow.md) builds them (4 or 8 bars each), so every section can be heard with `form_hear`. Large forms (sonata,
rondo) are not offered: standard-form and the kernel do not analyze them. Back to the [docs index](README.md).

```c
PieceRequest request = {.form = FORM_TEMPLATE_TERNARY, .key = key, .meter = meter, .bars = 24, .motive = NULL};
PiecePlan *plan;
ComposeConfig config = {.seed = 7};
ComposeOutcome outcome;
if (piece_build_plan(&request, &plan) == PIECE_OK && compose_run(plan, &config, &outcome) == PIECE_OK) {
    /* plan holds the piece; outcome.sections say how each theme was heard */
}
```

## The request

| Field | Meaning |
| --- | --- |
| `form` | `FORM_TEMPLATE_BINARY`, `_ROUNDED_BINARY`, `_TERNARY`, `_VARIATIONS`, `_LOOP` or `_LAYERED`. `form_template_from_name` reads `binary`, `rounded-binary`, `ternary`, `variations`, `loop`, `layered`. |
| `key`, `meter` | The key and meter, as in grow. |
| `bars` | The total bars, one of the sizes below. 0 for the default. Any other size is `PIECE_ERR_BAD_SIZE`. |
| `count` | The variations or the layers, instead of `bars`. 0 for the default. |
| `motive` | The motive, or NULL to have one invented. |
| `has_accompaniment`, `accompaniment` | An accompaniment under every section, as in grow. A layered form has one in any case (pattern `ANY`). |
| `modulate`, `seed` | With `modulate`, the sections' keys and the cadences across them are planned from `seed` (see Keys and cadences). Without it every section is in the home key with the template's cadences, as before. |
| `given_melodies` | The given melodies ([middle-out.md](middle-out.md)) the plan is to take: the material holds a bar more for each. 0 for none. |

## The templates

Each section is a theme section: an `archetype`, a `theme_pairing`, a `cadence`, a `key` (home), the antecedent's second
`cadence`, a `section_letter`, and, for a returning section, a `return`. Section keys other than home and cadences
other than the table's are not planned yet.

| Template | Bars | Sections |
| --- | --- | --- |
| binary | 16 | A 8 (presentation + continuation, PAC), B 8 (presentation + continuation, PAC) |
| | 8 | A 4, B 4 |
| rounded binary | 16 | A 8 (PAC), B 4 (compound basic idea + continuation, HC), A' 4 (returns A, PAC) |
| | 24 | A 8, B 8, A' 8 |
| ternary | 24 | A 8 (antecedent + consequent, PAC), B 8 (presentation + continuation, PAC), A 8 (returns A, PAC) |
| | 12 | 4 each |
| variations | 8 x (1 + n) | Theme 8 (antecedent + consequent, PAC), Var 1 to n, each returning the theme; n 1 to 8, default 3 |
| loop | 8 | one section (presentation + continuation, HC), so the end leads back to the first bar |
| | 16 | A 8 ending IAC, B 8 ending HC |
| layered | 8 x layers | Layer 1 8 (presentation + continuation, HC), then each later layer returns Layer 1; layers 2 to 4, default 4 |

The return defaults: A' of the rounded binary and the second A of the ternary are `exact`, Var k is `ornament` for odd k
and `reharmonize` for even k, a layer is `exact`.

Pieces of other shapes, laid out in beats and not from these templates, are [piece structures](piece-structure.md):
sections of any length with a pickup and a cadence each, the loop of a game track. The templates here are unchanged.

## Returns

A section with a `return` is given the material of the section it returns by `form-return` (`form_return.h`) instead of
being grown afresh. It does so when the two are as long, in the same meter, and the key lane has the same shape over
both (the same keys cut at the same bars, in the same modes); the returning section is transposed by the key difference.
Any other return is grown afresh from its motive, its `return` stored only: the A' of the 16-bar rounded binary (4 bars
against A's 8) is one.

`compose_run` leaves such a section alone through the first six stages (grow-theme does not apply to it), then runs a
`returns` stage (seed + 7) after the texture. `form-return` copies the returned section's nodes and lane spans, shifted;
the variation changes the copy:

| Variation | What it does |
| --- | --- |
| `exact` | Nothing. The harmony, bass, melody and inner voices are the returned section's. |
| `ornament` | The melody keeps every note and gains passing and neighbor tones between them, off the strong beats, one added note a beat; candidates differ in which notes are ornamented. Harmony, bass and inner voices are the returned section's. |
| `reharmonize` | The harmony spans are reopened as the targets they were written from (shape and cadence kept), the bass and the inner voices with them; the melody is the returned one. The harmony, bass, melody and texture stages run again (seeds + 8 to + 11, no records kept) and write them. The chords are the harmony operators', which know nothing of the melody, so the returns are made again from the plan as it was, up to 12 attempts, until every strong-beat melody note is a tone of the chord under it (`form_return_melody_fits`) and, in the first six attempts, a chord differs from the returned one. |
| `reregister` | The melody and the inner voices an octave up or down, whichever keeps the melody inside the range of `MelodyFillParams` (`ComposeConfig.melody`, else the default 60 to 79). One candidate for each that fits, none when neither does: the section stays open and the run is stuck at the returns. |

`return_hear` (`return_hear.h`) hears the returns of a written plan: each section of the piece is heard with `form_hear`,
each theme found becomes an entry (the contour and the rhythm of its melody notes), and the kernel's thematic return
detection with its default configuration (windows of 6 events, similarity of at least 0.8) gives a pair for each later
theme resembling an earlier one. `ComposeOutcome` has `returns` (the pairs), `returns_planned`, `returns_heard` and
`returns_heard_as_planned`; each return planned adds 1 to the score's denominator, and heard, to its numerator. A return
not heard is reported, not rejected.

Deviations from the plan, and why:

- A reharmonized return keeps the melody and the harmony operators write the chords; the plan had the checker reject a
  candidate whose chords equal the source's in more than half the spans. The phrase model allows little variety (the
  predominant, V or V7, the length of a chord), so 5 of 41 chords of variation 2 differ from the theme's (12%) and a
  half is out of reach with these operators.
- The test of the hearing asserts a baseline instead of `WILL_FAIL`, as the other sweeps do.

Measured, seeds 1 to 10, C major 4/4, invented motive (`test_return_heard`):

| Template | Returns heard | Why not all |
| --- | --- | --- |
| rounded binary (24 bars) | 10 of 10 | |
| ternary (24 bars) | 3 of 10 | A is a period, and standard-form hears no theme in a period (see below), so the section has no entry in the detection |
| variations (3) | 6 of 30 | The same: the theme and every variation are periods |

The cause is the kernel's: the detection compares the exact contour and rhythm of the themes standard-form finds, and
grow never hears a period (0 of 40, [grow.md](grow.md)). A section without a heard theme can neither return nor be
returned in what is heard, whatever the plan copied.

## Keys and cadences

With `modulate`, `piece_build_plan` runs `piece_plan_keys` and `piece_plan_cadences` (`<ursatz_composer/piece_plan_keys.h>`)
with `seed`. A key is the home key or one of its closely related keys (`harmony_related_key_at`, kept only when
`harmony_key_is_closely_related`). A fixed share of seeds, one in three, stays home. Otherwise:

| Template | Keys |
| --- | --- |
| binary | A ends in V (major) or III (minor), B starts there and returns home. |
| rounded binary | The same for A and B; A' is home. |
| ternary | A home; B starts home and ends in IV, vi or V (major), III, iv or VI (minor); A home again. |
| variations | All home, except with three variations or more the middle one, wholly in the relative key (vi in major, III in minor). The parallel mode is not closely related and has no pivot, so it is not used. |
| loop, layered | Home. |

The key lane is Given and owned by the form. A section that changes key halfway has its first half in the key it starts
in and its second in the key it ends in (4 of 8 bars, 2 of 4); spans that follow each other in one key are one span, so
a plan in one key has one span, as it always had. The KEY constraint of a section that ends in another key is that key.

A key opens where a span starts after another. The harmony target that starts there has `starts_on` `pivot`
(`PLAN_HARMONY_ANCHOR_PIVOT`): `harmony-phrase-model` writes a pivot chord first, one `harmony_pivot_find` gives between
the two keys, as its numeral in the new key. The candidates are the pivots by `harmony_pivot_rank` (best first), without
a dominant, a diminished triad, or in minor the minor v; the first candidate opens on the best, the others on the
others. A predominant pivot is the phrase's predominant (only ii may follow IV). `harmony_phrase_checker` rejects a
candidate whose first chord is not `harmony_pivot_valid` and does not ask for an opening tonic. Because the target is in
the new key, the cadence the checker hears is heard in it. Only a phrase model has a pivot, so grow-theme opens a key
only in the second half of a section (a continuation or a consequent) and at the start of an antecedent; a presentation
or a compound basic idea never opens a key, and the planner never asks them to.

`piece_plan_cadences` sets each section's cadence in a closing form (binary, rounded binary, ternary, variations): the
last section PAC; a section that ends in another key PAC; a binary A that stays home HC, since it would otherwise close
in the home key before the piece does; every other section the template's. Loop and layered forms end open by design and
keep the template's cadences. `piece_check_cadences` checks the plan against `harmony_cadence_strength`: the last
cadence is a PAC, so the strongest; a section ending in another key ends in a PAC or IAC; no section before the last
closes in the home key with a PAC unless it is `closed` (ternary A, rounded binary A, the theme and every variation).

After a complete run, `compose_run` of a plan with several key spans runs `piece_check_keys`: every key is closely
related to the home key, and each key change a section ends in is classified from its length and what follows the
cadence (`harmony_key_change_classify`) and confirmed by the authentic cadences `form_hear` heard (an authentic cadence
must arrive inside the new key's span, `harmony_key_change_confirm`); it must be a modulation or a weak modulation, not
a tonicization. A run that fails the key or cadence check is rejected like a stuck one and starts again from the plan as
given with other seeds, up to the same 12 rounds; the check of the last run is in `ComposeOutcome.key_check`
and `.cadence_check`, with `key_rejected` set. No run of the sweeps below was rejected by it.

## Motives

The plan has a material section (marked `material`, so it is not part of the piece) with the motive node `m0`: the
motive as a Given melody span, or, when none is given, a Constrained one-bar span with an `invent_motive` constraint.
A section's basic idea takes its motive from `m0`. A section may carry a `motive_ref` to `m0` with a transform, and
`grow-theme` then adds the section's own motive node ("<section> motive") to the material and builds the basic idea
from it. The templates give B sections an inverted `m0`, and Var k the diminished (k odd) or interval-changed (k even)
`m0`, so contrasting sections sound different; returning sections use their source's. The material holds `m0` and five
bars for each section (`piece_material_bars`); each section's motives go after the ones already there.

## Inventing a motive

`motive-invent` (`motive_invent_register`) writes the motive of an `invent_motive` span: 3 to 6 notes filling the bar,
a rhythm from the meter's cells (one long note allowed at the end), a first note on scale degree 1, 3 or 5, steps and
leaps of at most a sixth, inside C4 to C6. The checker `motive_invent_checker` holds the melodic rules of
standard-counterpoint as hard rules and scores the motive's recognizability. When the rhythm of a section's motive
cannot be written, the section's motive is an interval change of `m0` instead.

## The compose run

| Stage | Operators | Checker | Seed |
| --- | --- | --- | --- |
| invent | `motive-invent` | `motive_invent_checker` | seed + 6 |
| structure | `grow-theme` | `grow_checker` | seed + 0 |
| motives | `motive-restate`, `motive-fragment`, `motive-rhythm` | `motive_develop_checker` | seed + 1 |
| harmony | `harmony-prolongation`, `harmony-phrase-model` | `harmony_prolong_checker` | seed + 2 |
| bass | `harmony-bass` | `harmony_phrase_checker` | seed + 3 |
| melody | `melody-fill` | `melody_fill_checker` | seed + 4 |
| texture | `accompaniment`, `voicing-satb` | `texture_checker` | seed + 5 |

Each stage is one `fill_run` over the whole plan. The seeds of the stages from structure to texture are grow's, so
`grow_run`, a compose run of one theme section, gives the same result as before. The invent stage runs first and only for
a plan with an `invent_motive` span. The retries are grow's: the melody stage runs up to 6 times, each from the next
seed, reopening only the melody of the sections it left unwritten, and a run still stuck starts again from a JSON
snapshot of the plan as given, up to 12 rounds. After a complete run every theme section is heard with `form_hear` over
its own span: `ComposeOutcome.sections` has what each was heard as, and `heard_as_requested` how many were as requested.
A layered form with fewer than 4 layers keeps the build-up order and makes its last layer everything.

## Layers

A section with a `layers` constraint renders only the lanes its mask names; a span of another lane of that section is
skipped with `RENDER_SKIP_LAYER`, which the command line names in its skip report. Every layer is grown in full, so
every layer has every lane and the mask decides what sounds: layer 1 the bass, layer 2 the bass and the accompaniment,
layer 3 adds the melody, layer 4 everything.

## Tests and measured results

Four tests for each template (`tests/test_form_templates.c`):

| Test | What it checks | Label |
| --- | --- | --- |
| `structure` | The sections tile the form with the table's letters, bars, pairings and cadences. | |
| `smoke` | The smallest size in C major 4/4 with an invented motive composes with one of seeds 1 to 5, and every section is heard. Under a second. | |
| `compose` | Seeds 1 to 5, C major 4/4 and A minor 3/4, with and without a given motive, every size (variations: 1, 2 and 5 variations, 16, 24 and 48 bars): the runs that complete and the sections heard are not below the baseline. | `slow` |
| `heard` | The same sweep: the sections heard as the form they ask for are not below the baseline. | `slow` |

CI runs `ctest -LE slow`. The slow sweeps run in the full `ctest` before a push. The sweeps are deterministic, so a
baseline is an exact measurement; a test fails when a count falls below it, passes when it holds or rises, and the
baseline is raised in `BASELINES` when a change lifts it. Returns that copy their source moved the baselines of B6: layered
and variations rose (their returns no longer get stuck on their own), and rounded binary's heard-as-requested and
ternary's completions fell by one (a return is heard as its source is, and the seeds the stages draw from changed).

Baselines, with grow's measured shares for the same sections for comparison:

| Template | Runs complete | Heard as requested | Share | Expected from grow's shares |
| --- | --- | --- | --- | --- |
| binary | 21 of 40 | 15 of 80 | 19% | 26 |
| rounded binary | 29 of 40 | 23 of 120 | 19% | 58 |
| ternary | 27 of 40 | 7 of 120 | 6% | 13 |
| loop | 15 of 40 | 12 of 60 | 20% | 39 |
| layered | 30 of 60 | 20 of 180 | 11% | 117 |
| variations | 45 of 60 | 0 of 220 | 0% | 0 |

Variations hear none, as grow does, since every section is a period and grow never hears one (0 of 40). Its runs get
stuck more the longer the form: in A minor 3/4 with a given motive none of the sizes completes.

### Keys

`tests/test_piece_keys.c`: every template, C major and A minor 4/4, an invented motive, seeds 1 to 10, with `modulate`.

| Test | What it checks | Label |
| --- | --- | --- |
| `plan` | The key lane tiles the piece, every key is closely related, a section's KEY constraint is the key it ends in, the cadences pass `piece_check_cadences`, loop and layered keep one key. 64 of the 120 plans change key (8 of 10 for each of the four templates that can). | |
| `smoke` | Binary in C major: the first of seeds 1 to 5 composes, changes key and passes the checks. | |
| `<template>` | The composed runs: every key change opens with a valid pivot, every planned modulation is a modulation with an authentic cadence heard in the new key, the last cadence is the strongest. Prints the share that completes. | `slow` |

Every completed run passes every check. The share of runs that complete is below 100% for every template but variations
(rounded binary is 65% because the checks hear the plan through the Analyzer: the key check reads the cadences the Analyzer
hears, which are not those of the planned chords), so the other five tests are `WILL_FAIL`, with the measured shares (the
stage is where the last round of a stuck run ended; before the Analyzer: binary 14 of 20, rounded binary 20, ternary 16):

| Template | Runs complete | Of them change key | Stuck at |
| --- | --- | --- | --- |
| binary | 11 of 20 (55%) | 9 | melody 5, bass 2, harmony 2 |
| rounded binary | 13 of 20 (65%) | 9 | melody 4, bass 2, texture 1 |
| ternary | 11 of 20 (55%) | 9 | melody 3, bass 3, harmony 2, structure 1 |
| variations | 20 of 20 (100%) | 16 | |
| loop | 10 of 20 (50%) | 0 | A minor only, all harmony |
| layered | 10 of 20 (50%) | 0 | A minor: harmony 10 |

Loop and layered have one key; they are stuck in A minor 4/4 without `modulate` too (B4's sweeps ran A minor in 3/4),
and are in the table because the test runs every template. For binary, 5 of the 7 stuck runs complete with the same seeds
without `modulate` (C major 4, 5, 8, 10; A minor 7), so a key change or a pivot chord is what leaves them stuck, at the
melody, bass or harmony stage; the other two (A minor 3 and 6) are stuck without it. The causes of the stuck runs of
ternary and variations were not investigated. None is a run the key check rejected.

## Test notes

Causes:

- Runs that get stuck are mostly half-cadence sentences (loop, layered, rounded binary 24 bars in A minor 3/4) and some
  4-bar minor themes. Grow cannot grow the same theme alone: `grow ... --cadence HC` fails for seeds 1 to 16.
- Low shares come from what grow does not hear: 4-bar themes are never heard, and periods are never heard
  (see [grow.md](grow.md)).
