# Pieces from form templates

`piece_build_plan` (`<ursatz_composer/piece.h>`) plans a short piece from a named form template, and `compose_run`
(`<ursatz_composer/compose.h>`) fills every section of the plan with the grow stages. A piece is a row of theme sections
as [grow](grow.md) builds them (4 or 8 bars each), so every section can be heard with `form_hear`. Large forms (sonata,
rondo) are not offered: standard-form and the kernel do not analyze them.

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
and `reharmonize` for even k, a layer is `exact`. A `return` is stored only: a returning section is grown afresh from
its motive, not copied from its source.

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

## Heard results

Measured with `test_form_templates heard <template>`: seeds 1 to 5, C major 4/4 and A minor 3/4, with and without a
given motive. Sections heard as requested, against the number grow's own measured shares give for the same sections:

| Template | Heard as requested | Share | Expected from grow's shares |
| --- | --- | --- | --- |
| binary | 15 of 80 | 19% | 26 |
| rounded binary | 24 of 120 | 20% | 58 |
| ternary | 7 of 120 | 6% | 13 |
| loop | 12 of 60 | 20% | 39 |
| layered | 14 of 180 | 8% | 117 |
| variations | 0 of 880 | 0% | 0 |

Variations: 0 of 880 sections heard, which is also what grow gives, since every section is a period and grow never hears one (0 of 40). Its share test passes. Runs get stuck more the longer the form: in C major 4/4 every run completes up to 48 bars, in A minor 3/4 with a given motive none does at any size.

The `_compose` tests of all six templates and the `_heard` tests of the other five are `WILL_FAIL`, because runs get stuck and the shares are below what grow's give; the
`structure` tests (sections tile the form with the table's letters, bars, pairings and cadences) pass.

Causes:

- Runs that get stuck are mostly half-cadence sentences (loop, layered, rounded binary 24 bars in A minor 3/4) and some
  4-bar minor themes. Grow cannot grow the same theme alone: `grow ... --cadence HC` fails for seeds 1 to 16.
- Low shares come from what grow does not hear: 4-bar themes are never heard, and periods are never heard
  (see [grow.md](grow.md)).
