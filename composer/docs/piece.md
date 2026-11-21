# Pieces from forms

`piece_build_plan` (`<ursatz_composer/piece.h>`) plans a short piece from a named built-in form, and `compose_run`
(`<ursatz_composer/compose.h>`) fills every section of the plan with the grow stages. A piece is a row of theme sections
as [grow](grow.md) builds them (4 or 8 bars each), so every section can be heard with `form_hear_in`. Large forms (sonata,
rondo) are not offered: standard-form and the kernel do not analyze them. Back to the [docs index](README.md).

```c
PieceRequest request = {.form = "ternary", .key = key, .meter = meter, .bars = 24, .motive = NULL};
Plan *plan;
ComposeConfig config = {.seed = 7};
ComposeOutcome outcome;
if (piece_build_plan(&request, &plan) == PIECE_OK && compose_run(plan, &config, &outcome) == PIECE_OK) {
    /* plan holds the piece; outcome.sections say how each theme was heard */
}
```

## The request

| Field | Meaning |
| --- | --- |
| `form` | The name of a built-in form: `binary`, `rounded-binary`, `ternary`, `variations`, `loop` or `layered` ([forms.md](forms.md)). Any other name is `PIECE_ERR_BAD_REQUEST`. `piece_build_plan` reads the form's document and calls `form_build_plan`, the one builder of every form. |
| `key`, `meter` | The key and meter, as in grow. |
| `bars` | The total bars, one of the sizes below. 0 for the default. Any other size is `PIECE_ERR_BAD_SIZE`. |
| `count` | The variations or the layers, instead of `bars`. 0 for the default. |
| `motive` | The motive, or NULL to have one invented. |
| `has_accompaniment`, `accompaniment` | An accompaniment under every section, as in grow. A layered form has one in any case (pattern `ANY`). |
| `modulate`, `seed` | With `modulate`, the sections' keys and the cadences across them are planned from `seed` (see Keys and cadences). Without it every section is in the home key with the template's cadences, as before. |
| `given_melodies` | The given melodies ([middle-out.md](middle-out.md)) the plan is to take: the material holds a bar more for each. 0 for none. |

## The forms

The forms are documents, one for each file in `forms/` ([forms.md](forms.md)); this table is what they hold. Each section is a theme section: an `archetype`, a `theme_pairing`, a `cadence`, a `key` (home), the antecedent's second
`cadence`, a `section_letter`, and, for a returning section, a `return`. Section keys other than home and cadences
other than the table's are planned only with `modulate`.

| Template | Bars | Sections |
| --- | --- | --- |
| binary | 16 | A 8 (presentation + continuation, PAC), B 8 (presentation + continuation, PAC) |
| | 8 | A 4, B 4 |
| rounded binary | 16 | A 8 (PAC), B 4 (compound basic idea + continuation, HC), A' 4 (returns A, PAC) |
| | 24 | A 8, B 8, A' 8 |
| ternary | 24 | A 8 (antecedent + consequent, PAC), B 8 (presentation + continuation, HC), A 8 (returns A, varied, PAC); [small-ternary.md](small-ternary.md) |
| | 12 | 4 each |
| variations | 8 x (1 + n) | Theme 8 (antecedent + consequent, PAC), Var 1 to n, each returning the theme; n 1 to 8, default 3 |
| loop | 8 | one section (presentation + continuation, HC), so the end leads back to the first bar |
| | 16 | A 8 ending IAC, B 8 ending HC |
| layered | 8 x layers | Layer 1 8 (presentation + continuation, HC), then each later layer returns Layer 1; layers 2 to 4, default 4 |

The return defaults: A' of the rounded binary is `exact`, the second A of the ternary `varied`, Var k is `ornament` for odd k
and `reharmonize` for even k, a layer is `exact`.

Pieces of other shapes, laid out in beats and not from these templates, are [the game loop](piece-structure.md), a form whose
sections the piece request gives: sections of any length with a pickup and a cadence each, the loop of a game track. It
is built by the same builder, from its form document, and the forms here are unchanged.

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
| `reharmonize` across modes | A `reharmonize` return whose RETURN has `across_modes` (the form's `"mode": "home"`, R-524) and whose source is in the other mode from the target: the copy is moved by the tonic step (a minor piece's B in III returns up 9 semitones, written -3, into the tonic minor), its keys take the target's mode and its melody is mapped into it relative to the target's tonic (degrees 4 and 9 semitones above the tonic, lowered by a semitone), then the harmony is reharmonized as for `reharmonize`. The check compares the returned melody through that mapping. A source in the same mode is moved as a return is, with no mapping. |
| `varied` | A moderately varied repetition (Schoenberg's A1): the `ornament` melody, at most one added note a bar, over a `reharmonize` harmony; the returns are made again (up to 12 attempts) until the melody fits the chords and, in the first six, a chord of the last quarter of the section differs from the returned one (`form_return_cadence_changed`). |
| `embellish` | The `ornament` melody (R-516); when no note of a quarter or longer on a cell start can be split, as in 2/4 where the theme has none, each eighth note on a cell start followed at once by a note a third away or the same is split into two sixteenths, the second a passing or neighbor tone. Checked as `ornament` is (every note of the returned melody kept, an added note off the strong beat, one a beat). `ornament` and `varied` do not split eighths. |
| `setting` | The harmony, bass and melody are the returned section's, moved; the accompaniment is written anew in the pattern the return names (R-518). The copy's inner voice spans are dropped and, over each harmony target (found as `reharmonize` finds them), open ones are added as `grow-theme` opens them: voice 1 with an `accompaniment` constraint of the pattern, and voices 2 and 3 for `block` and `oom-pah`. The texture stage writes them after the returns. The form's `setting` (`block`, `arpeggiated`, `alberti` or `oom-pah`) is the pattern, except when it is the piece's own accompaniment pattern or `alberti` in a compound meter: then the next of `block`, `arpeggiated`, `alberti`, `oom-pah` after it, wrapping, skipping those two (`form_build.c`). Checked by `return_vary_check`: every node and every harmony, bass and melody span, notes included, are the returned section's, and the inner voice spans are open with the pattern on voice 1. |
| `reregister` | The melody and the inner voices an octave up or down, whichever keeps the melody inside the range of `MelodyFillParams` (`ComposeConfig.melody`, else the default 60 to 79). One candidate for each that fits, none when neither does: the section stays open and the run is stuck at the returns. |

`return_hear_in` (`src/check/hearing.h`) hears the returns of a written plan: each section of the piece is heard with `form_hear_in`,
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
| ternary (24 bars) | 9 of 10 | |
| variations (3) | 6 of 30 | The same: the theme and every variation are periods |

The cause is the kernel's: the detection compares the exact contour and rhythm of the themes standard-form finds, and
grow's period is heard as a period in 32 of 40 runs of 8 bars ([grow-hearing.md](grow-hearing.md), the test's baseline at `243bd316`). A section without a heard theme can neither return nor be
returned in what is heard, whatever the plan copied.

## Keys and cadences

With `modulate`, `form_build_plan` runs `piece_plan_keys` and `piece_plan_cadences` (`<ursatz_composer/piece_plan_keys.h>`)
with `seed`, from the form's regions and its sections' `keys` and `closed` ([forms.md](forms.md#keys-and-cadences)). A key is the home key or one of its closely related keys (`harmony_related_key_at`, kept only when
`harmony_key_is_closely_related`). A fixed share of seeds, one in three, stays home. Otherwise:

| Form | Keys |
| --- | --- |
| binary | A ends in V (major) or III (minor), B starts there and returns home. |
| rounded binary | The same for A and B; A' is home. |
| ternary | A ends in V or iii (major), III or v (minor); B starts there and returns home, ending on V; A home again. |
| variations | All home, except with three variations or more the middle one, wholly in the relative key (vi in major, III in minor). The parallel mode is not closely related and has no pivot, so it is not used. |
| loop, layered | Home. |

The key lane is Given and owned by the form. A section that changes key halfway has its first half in the key it starts
in and its second in the key it ends in (4 of 8 bars, 2 of 4); spans that follow each other in one key are one span, so
a plan in one key has one span, as it always had. The KEY constraint of a section that ends in another key is that key.

A key opens where a span starts after another. The harmony target that starts there has `starts_on` `pivot`
(`PLAN_HARMONY_ANCHOR_PIVOT`): `harmony-phrase-model` writes a pivot chord first, one `harmony_pivot_find` gives between
the two keys, as its numeral in the new key. The candidates are the pivots by `harmony_pivot_rank` (best first), without
a dominant, a diminished triad, or in minor the minor v; the first candidate opens on the best, the others on the
others. A predominant pivot is the phrase's predominant (only ii may follow IV). When no pivot deals a phrase that fits
(into V of a major key the pivots are its IV, ii and vi, all predominants, which leave a half cadence two chords at most,
too few for four bars of at most 2 cells a chord with no barline crossed), the predominant pivots are dealt again going on to
the new key's V7 and I and then the predominant run and the cadence, which confirms the new key before the half cadence
(R-512); a phrase that dealt before is dealt the same. `harmony_phrase_checker` rejects a
candidate whose first chord is not `harmony_pivot_valid` and does not ask for an opening tonic. Because the target is in
the new key, the cadence the checker hears is heard in it. Only a phrase model has a pivot, so grow-theme opens a key
only in the second half of a section (a continuation or a consequent) and at the start of an antecedent; a presentation
or a compound basic idea never opens a key, and the planner never asks them to.

`piece_plan_cadences` sets each section's cadence in a closing form (binary, rounded binary, ternary, variations): the
last section PAC; a section that ends in another key PAC, except a transition, which ends on the new key's V in a half cadence (R-503, R-512); a binary A that stays home HC, since it would otherwise close
in the home key before the piece does; every other section the form's. A form that does not close (`closing` false: loop
and layered) ends open by design and keeps its document's cadences. `piece_check_cadences` checks the plan against `harmony_cadence_strength`: the last
cadence is a PAC, so the strongest; a section ending in another key ends in a PAC or IAC (a transition: a half cadence); no section before the last
closes in the home key with a PAC unless it is `closed` (ternary A, rounded binary A, the theme and every variation).

After a complete run, `compose_run` of a plan with several key spans runs `piece_check_keys_in`: every key is closely
related to the home key, and each key change a section ends in is classified from its length and what follows the
cadence (`harmony_key_change_classify`) and confirmed by the authentic cadences `form_hear_in` heard (an authentic cadence
must arrive inside the new key's span, `harmony_key_change_confirm`); it must be a modulation or a weak modulation, not
a tonicization. A run that fails the key or cadence check is rejected like a stuck one and starts again from the plan as
given with other seeds, up to the same 12 rounds; the check of the last run is in `ComposeOutcome.key_check`
and `.cadence_check`, with `key_rejected` set. No run of the sweeps below was rejected by it.

## Motives

The plan has a material section (marked `material`, so it is not part of the piece) with the motive node `m0`: the
motive as a Given melody span, or, when none is given, a Constrained one-bar span with an `invent_motive` constraint.
A section's basic idea takes its motive from `m0`. A section may carry a `motive_ref` to `m0` with a transform, and
`grow-theme` then adds the section's own motive node ("<section> motive") to the material and builds the basic idea
from it. The forms give B sections an inverted `m0`, and Var k the diminished (k odd) or interval-changed (k even)
`m0`, so contrasting sections sound different; returning sections use their source's. The material holds `m0` and ten
bars for each section (its motive, the four its ideas are made from and up to five made motives) (`piece_material_bars_of`); each section's motives go after the ones already there.

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
snapshot of the plan as given, up to 12 rounds. After a complete run every theme section is heard with `form_hear_in` over
its own span: `ComposeOutcome.sections` has what each was heard as, and `heard_as_requested` how many were as requested.
A layered form with fewer than 4 layers keeps the build-up order and makes its last layer everything.

## Layers

A section with a `layers` constraint renders only the lanes its mask names; a span of another lane of that section is
skipped with `RENDER_SKIP_LAYER`, which the command line names in its skip report. Every layer is grown in full, so
every layer has every lane and the mask decides what sounds: layer 1 the bass, layer 2 the bass and the accompaniment,
layer 3 adds the melody, layer 4 everything.

The tests and the measured results are in [piece-tests.md](piece-tests.md).
