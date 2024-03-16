# Growing a sentence

`grow_build_plan` and `grow_run` (`<ursatz_composer/grow.h>`) grow a theme from one motive: the structure of a
sentence, then the motives made from the motive, the harmony, the bass and the melody, each through the fill
loop. This is a library API; the `grow` command comes later.

```c
GrowRequest request = {.motive = &motive, .key = key, .meter = meter, .bars = 8,
                       .pairing = {PLAN_THEME_BEGINNING_PRESENTATION, PLAN_THEME_ENDING_CONTINUATION},
                       .cadence = PLAN_CADENCE_PAC};
PiecePlan *plan;
GrowConfig config = {.seed = 7};
GrowOutcome outcome;
if (grow_build_plan(&request, &plan) == GROW_OK && grow_run(plan, &config, &outcome) == GROW_OK) {
    /* plan holds the theme; outcome.heard and outcome.score say how standard-form hears it */
}
```

## The request

| Field | Meaning |
| --- | --- |
| `motive` | The motive. It must fit in one bar of the meter (`GROW_ERR_MOTIVE_TOO_LONG` otherwise) and have two notes or more. |
| `key`, `meter` | The key and meter. The harmony operators write for 2/4, 3/4, 4/4 and 6/8 only. |
| `bars` | 8, with a basic idea of 2 bars, or 4, with one of 1 bar. |
| `pairing` | Presentation and continuation. The other pairings are not built yet: `GROW_ERR_BAD_REQUEST`. |
| `cadence` | PAC, IAC or HC. |

`GROW_ERR_BAD_REQUEST` is also the result for a meter the harmony operators do not support, bars other than 4 or 8, a
cadence other than those three, a motive of one note, and a continuation too short for the cadence.

## The plan

`grow_build_plan` makes a Form over the theme and the material, with the meter and the key as Given lane spans over
all of it, and two sections:

- **theme** (Constrained) over the bars, with `archetype` sentence, `theme_pairing`, `cadence` and `key` constraints;
- **material**, after it, with the `material` constraint, so it is not part of the piece. It holds the motive node
  `m0`, with the motive as a Given melody span, in a phrase and an idea that have room for the motives made from it.

## The stages

Each stage is one `fill_run` with only its operators registered and its own checker. Stage k runs with the seed
`config.seed + k`. A stage's own targets must all be written when it ends, else `grow_run` returns `GROW_ERR_STUCK`
with the stage in `GrowOutcome.stuck_stage`. `GrowOutcome.records` keeps the `FillRecord` of every stage.

| Stage | Operators | Checker |
| --- | --- | --- |
| structure | `grow-theme` | `grow_checker` |
| motives | `motive-restate`, `motive-fragment`, `motive-rhythm` | `motive_develop_checker` |
| harmony | `harmony-prolongation`, `harmony-phrase-model` | `harmony_prolong_checker` |
| bass | `harmony-bass` | `harmony_phrase_checker` |
| melody | `melody-fill` | `melody_fill_checker` |

Two things make a run more reliable than one pass of the stages. A melody that gets stuck is written again from the
seed `+ 1000` up to 3 more times (the spans are reopened; the chords stay). A run that is still stuck starts again
from the plan as it was given, with seeds `+ 100000` apart, up to 11 more times. The first run always uses the
seeds above, and the same seed always gives the same result.

## The structure

`grow-theme` is a Grow operator for a Constrained section with an `archetype` and a `theme_pairing`. For a sentence of
8 bars it adds, in bars (4 bars halves every length):

| Node | Bars | Open lanes |
| --- | --- | --- |
| Phrase | 0-8 (the cadence constraint) | |
| Subphrase presentation | 0-4 | |
| Idea basic idea | 0-2 | harmony (prolongation), bass, melody |
| Idea repetition | 2-4 | harmony (prolongation), bass, melody |
| Subphrase continuation | 4-8 | harmony (phrase model or cadential, with the cadence), bass |
| Idea fragment 1 | 4-5 | melody |
| Idea fragment 2 | 5-6 | melody |
| Idea cadential | 6-8 | melody (with the cadence) |

And in the material section, one motive node for each idea after the basic idea, with a `motive_ref` to `m0`: `m1` for
the repetition, `m2` and `m3` for the fragments, `m4` for the cadential idea. The theme section becomes Realized.

### Choices

The operator proposes `GrowContext.candidates` (default 4) candidates, drawn from the run's seed:

- **Repetition.** *Exact*: `restated`, the harmony tonic to tonic in both ideas. *Statement-response*: `transposed`,
  tonal, one step up; the basic idea's harmony goes tonic to dominant and the repetition's dominant to tonic. Not
  offered when the basic idea has one cell (a bar of 2/4 or 3/4 in 4 bars).
- **Fragments.** `fragmented`, leading, 2 or 3 notes, shortened until the fragment fits its idea.
- **Cadential idea.** `liquidated`, dropping 1 to half of the motive's notes.

A candidate is dropped when a motive it asks for cannot be made or does not fit its idea.

### The checks

`grow_checker` asks standard-form's own rule functions of the planned structure. Hard: the subphrase roles in order
(`standard_form_subphrase_roles_in_order`), the phrase sentential (`standard_form_phrase_is_sentential`), the
pairing a sentence (`standard_form_pairing_form`), the basic idea's length valid
(`standard_form_basic_idea_length_valid`), and each fragment shorter than the basic idea, as an idea and as a motive
(`standard_form_is_fragmentation`). Soft, the score being the share that hold: the sentence archetypal
(`standard_form_sentence_is_archetypal`), the presentation of the usual length
(`standard_form_presentation_length_usual`, which 4 bars do not meet), the continuation balancing it
(`standard_form_continuation_balanced`) and the repetition reading as one
(`standard_form_idea_reads_as_repetition`, on the repetition's contour against `m0`).

## The harmony

`harmony-prolongation` writes a Constrained harmony span with a `harmony_shape` of kind `prolongation`: one lane span
per chord, on the cells of the meter, from standard-harmony's prolongation figures (I-V-I, I-V6-I, I-viio6-I6, I-IV-I,
I-V4/3-I6, and the same turned round for a span that opens on the dominant). A span that ends on V ends on V6, V6/5 or
V4/3, or on a V off the bar's first cell. Its checker rejects any candidate in which standard-harmony's recognizer
hears a cadence, inside or at the end, whatever the soprano; the phrase model's tonic-then-dominant rule is not asked
of a prolongation. A prolongation that follows one of the same shape repeats its chords, which is how an exact
repetition gets the harmony of the basic idea.

`harmony-phrase-model` reads an optional `phrase_model` or `cadential` shape. `max_cells_per_chord` caps every chord
but the last, which always takes the whole last bar so the cadence arrives on a downbeat; `split_at_bars` keeps every
chord off the barlines; `cadential` drops the opening tonic (a predominant, a dominant and the arrival). Without a
shape it behaves as it always did. The continuation of a grown sentence has the `phrase_model` shape with
`split_at_bars`, and the cadence; its cap is half the cells of the basic idea (at least 1, and at least what lets
the chords add up to the cells), since the presentation is not yet written when the structure is. A continuation with
too few cells for a phrase model (2 bars of 2/4 or 3/4) gets the `cadential` shape.

## The melody

The melody stage writes the five ideas in time order with the idea-target rules of [melody-fill.md](melody-fill.md):
joins, the phrase's one climax, the end tone only on the cadential idea, and an exact repetition said again as the
basic idea was.

## Hearing the result

After a complete run `grow_run` calls `form_hear` on the theme section (see [form-hear.md](form-hear.md)) and keeps
the themes in `GrowOutcome.heard`. A theme heard as the form the section asks for adds to `GrowOutcome.score`
(`structure share + heard`, out of `structure total + 1`); a mismatch is reported in `heard_form` and
`heard_as_requested`, never rejected.

Measured by `tests/test_grow_sentence_heard.c`, over four keys and meters and seeds 1 to 10 (`ctest` runs it as
WILL_FAIL, since it asserts that every grown sentence is heard as a sentence):

| Bars | Heard as a sentence | Heard as another form | No theme |
| --- | --- | --- | --- |
| 8 | 26 of 40 (65%) | 14 (hybrids: compound basic idea and continuation) | 0 |
| 4 | 0 of 40 (0%) | 0 | 40 |
| all | 26 of 80 (32%) | 14 | 40 |

The causes are limits of the adapter, not tuned away:

- **Statement-response is heard as a hybrid.** The adapter compares ideas by the semitone interval and the onset
  rhythm of each pair of notes (75% must match). A tonal step up changes some intervals by a semitone, so the
  repetition reads as a contrasting idea. Every exact repetition (26 of 26) is heard as a sentence; every
  statement-response (0 of 14) as a hybrid.
- **A sentence of 4 bars is not heard at all.** The adapter cuts a phrase into ideas of two measures, so 4 bars are
  two ideas, which it reads as one part, not as a presentation and a continuation: no theme.
- **Fragmentation is not seen.** The nominal cut of 2 measures finds no 1 + 1 + 2 continuation, so `has_fragmentation`
  is false for every grown sentence.
