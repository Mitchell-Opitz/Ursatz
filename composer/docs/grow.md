# Growing a theme

`grow_build_plan` and `grow_run` (`<ursatz_composer/grow.h>`) grow a theme from one motive: its structure (a
sentence, a period or one of the seven hybrids), then the motives made from the motive, the harmony, the bass and the
melody, and, when asked, an accompaniment or a four-part voicing, each through the fill loop. The [grow command](cli-grow.md) runs it. Back to the [docs index](README.md).

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

`grow_run` is a [compose run](piece.md) over the plan's one theme section: the stages, seeds, retries and records below are
the compose run's, and `GrowOutcome` is filled from its `ComposeOutcome`. The stage seeds are unchanged (seed + 0 to
seed + 5; the compose run's invention stage takes seed + 6), so a grown theme is the same as before.

## The request

| Field | Meaning |
| --- | --- |
| `motive` | The motive. It must be no longer than the basic idea (`GROW_ERR_MOTIVE_TOO_LONG` otherwise) and have two notes or more. See [How a motif fills its idea](#how-a-motif-fills-its-idea). |
| `key`, `meter` | The key and meter. The harmony operators write for 2/4, 3/4, 4/4 and 6/8 only. |
| `bars` | 8, with a basic idea of 2 bars, or 4, with one of 1 bar. |
| `pairing` | Any of the nine pairings of a beginning (presentation, antecedent, compound basic idea) and an ending (continuation, consequent, cadential). Presentation and continuation make a sentence, antecedent and consequent a period, the other seven a hybrid (`standard_form_pairing_form`). |
| `cadence` | PAC, IAC or HC: the cadence the theme ends in. |
| `first_cadence` | The weak cadence an antecedent ends in, HC or IAC and weaker than `cadence` by `harmony_cadence_strength`. Zero (PAC) means the default, HC. Read only for an antecedent. |
| `slot_motives` | The motives of the contrasting, continuation and cadential slots ([Slots](#slots)); NULL for none. |
| `has_accompaniment`, `accompaniment` | An accompaniment under the melody and bass ([accompaniment.md](accompaniment.md)): a `PlanAccompaniment` with a pattern (`ANY` for any), a texture (`-1` for none) and a range (0 for the defaults 48 and 67). Without `has_accompaniment` the theme has no inner voice. The theme section carries it as an `accompaniment` constraint. |
| `has_voicing`, `voicing` | A four-part voicing of the harmony and bass ([voicing.md](voicing.md)): a `PlanVoicing`, whose `write_soprano` has the voicing write the melody too, a note a chord (a chorale), instead of the melody stage. The alto and the tenor are the inner voices 1 and 2. The theme section carries it as a `voicing` constraint. Refused together with an accompaniment. |

`GROW_ERR_BAD_REQUEST` is also the result for a meter the harmony operators do not support, bars other than 4 or 8, a
cadence other than those three, a motive of one note, a half of the theme too short for its cadence, and an antecedent
whose cadences cannot be ordered: a theme that ends in HC has nothing weaker for the antecedent, `first_cadence` DC or
PC is not a weak cadence, and equal cadences are not a period. An accompaniment is refused when its pattern is not one,
its texture is not homophonic, its range has no room (`low` not below `high`), or it is Alberti in a compound meter, and a request for both an accompaniment and a voicing.

## Slots

A theme can be built from more than the basic motive (R-166). `GrowSlot` names the four functions of the ideas (R-143): `basic`
(the basic idea, the repetition of a presentation, the consequent's basic idea), `contrasting` (the contrasting idea, the
consequent's contrasting idea), `continuation` (fragment 1 and 2) and `cadential` (the cadential idea, cadential 1 and 2).
`GrowRequest.slot_motives` gives a motive to each slot but the basic one. A slot motive is added to the material as the node
`s1` (contrasting), `s2` (continuation) or `s3` (cadential), a Given melody span, after `m0`; the material is as many bars
longer as the motive has bars. The first idea of the slot is the motive itself (exact, or a saved form of it, see below); the later ideas of the slot
(the consequent's contrasting idea, fragment 2, cadential 2) are made from it as the basic motive's are, and the remoteness of
their forms is measured from it. The basic idea's ideas are unchanged.

`GROW_ERR_BAD_REQUEST` for a slot motive that is not a full motive of two notes or more in the theme's key and meter, that is
longer than the first idea of its slot (two bars of eight for the contrasting idea and the cadential idea, one for fragment 1; a
motive as long as the idea fills it whole),
or for a slot the pairing has no idea for (a contrasting slot in a sentence, a continuation slot without a continuation, a
cadential slot in a period). A slot motive keeps no harmony, features or role (a plan keeps only the notes).

### Idea motives

A theme's shape is one table (R-416, `src/grow/theme_shape.c`, `theme_shape.h`). Each beginning and ending is a `ThemeShapeHalf` (its plan name and its standard-form name); each idea of each half is a `ThemeShapeIdea` row with its `GrowIdea`, name, `GrowPlace`, slot, whether it is the first of its slot, role and length in eighths of the theme; each slot is a `ThemeShapeSlot` (name, function, plan node name). `grow_idea_name`, `grow_idea_place`, `grow_idea_in_pairing`, `grow_place_units`, `grow_idea_slot`, `grow_slot_used`, the slot readers, `grow_idea_role`, the vocabulary's `theme_slots` and `theme_ideas` and the mapping to standard-form's `PhraseBeginning` and `PhraseEnding` all read it, so a new idea or a new length is a row, not a `switch`. `GrowSlot`, `GrowIdea` and `GrowIdeaRole` are declared in `theme_shape.h`, which `grow.h` includes. `tests/test_theme_shape.c` checks the table against the values the old code gave for the nine pairings.

`GrowIdea` names the ideas of a theme as the plan does (`grow_idea_name`: `basic idea`, `repetition`, `contrasting idea`,
`consequent basic idea`, `consequent contrasting idea`, `fragment 1`, `fragment 2`, `cadential`, `cadential 1`,
`cadential 2`); `grow_idea_in_pairing` says whether a pairing has one (R-206). `GrowRequest.idea_motives` gives a single idea a
motive of its own, in place of its slot's for that idea. It is added to the material as the node `i<k>` (k the `GrowIdea`
value), a Given melody span after the slot motives (the material is as many bars longer as the motive has bars), and the idea's melody asks for it
directly, as an exact restatement: the motives stage makes no motive for that idea. The other ideas of the slot are made as
before, and the same `Motive` may back several nodes. Melody-fill places an exact restatement of a motive of its own as that
motive, not as a copy of the idea before it. `GROW_ERR_BAD_REQUEST` for an idea the pairing does not have, an entry for the
basic idea, or a motive that is not a full motive of two notes or more in the theme's key and meter or is longer than its idea
(the idea's bars: two bars of eight for the contrasting idea and the cadential idea, one for fragment 1 and in a 4-bar theme).

### How a motif fills its idea

A motif's length in bars is `ceil((length less its pickup) / bar)`; the pickup stays before the idea's first barline, and
the motif is placed from its start, so a pickup counts toward the length the idea has room for. `grow_motive_fit` compares
the motif's bars with the bars of the idea it fills (2 for the basic idea of an 8-bar theme, 1 for a 4-bar one; for a slot
or idea motive, the bars of its place):

| Fit | Meaning |
| --- | --- |
| `GROW_FIT_SHORTER` | The motif is grown to the idea: it is placed at the start, and its rhythm is repeated with new pitches to the idea's end. |
| `GROW_FIT_WHOLE` | The motif is the idea: its notes are realized, bent only as the chords ask, nothing filled after, the last on the cadence tone ([melody-fill.md](melody-fill.md)). |
| `GROW_FIT_LONGER` | Refused: `GROW_ERR_MOTIVE_TOO_LONG` for the basic motive (unless it is as long as the first part, see below), `GROW_ERR_BAD_REQUEST` for a slot or idea motive. |

A basic motif exactly as long as the first part of the theme (4 bars of an 8-bar theme, 2 of a 4-bar one) fills that part
whole when the beginning is a presentation or an antecedent (`grow_motive_fills_part`). The part is one idea, `basic idea`: the
motif is its melody, placed as written, and an antecedent's weak cadence moves the last note as above. It has no repetition or
contrasting idea, so a contrasting slot motive or a repetition or contrasting idea motive is `GROW_ERR_BAD_REQUEST` (the theme
API: `/slots/contrasting`, `/ideas/<name>`). The basic idea the rest of the theme is made from is the motif's leading notes that
start inside the first idea-length bars (`grow_part_basic_idea`), made as the section's motive; fewer than two such notes, or a
note running past that idea, is refused. Any other longer motif, and any longer than the basic idea in a compound basic idea,
is `GROW_ERR_MOTIVE_TOO_LONG`.

The ideas made from a whole motif (the repetition, the dominant form, the consequent's restatement and a section's drawn
motive) develop all of it and must fit the idea they fill. Each motive in the material takes its own bars, laid end to end.

### Given chords

`GrowRequest.chords` (`chord_count`) gives chords of the theme (R-208), `GivenChord` of [given.h](../src/include/ursatz_composer/given.h):
a Roman numeral, an onset from the theme's start and a length. `grow_build_plan` gives them to the theme section with
`plan_give_chords` once the section is made, one Given harmony span for each, exactly its onset and length;
`GROW_ERR_BAD_REQUEST` when it refuses them. The harmony stage opens targets only over the gaps (`given_gaps`), so the chords
stay, the bass is written over them, and a gap is judged with the Given chords before it (one chord after them is
heard as the end of their chain). The harmony and bass operators write a chord on the cells of the meter only, so the `theme` command refuses any other onset
([theme-request.md](theme-request.md#chords)).
A run stuck at the harmony, bass or melody stage next to a given chord names the bar
(`the given chords in bar <n> do not allow the <cadence>`, exit 2).

The order: given, then motive ([grow-harmony.md](grow-harmony.md#chords-from-developed-motives)), then filled.

## The plan

`grow_build_plan` makes a Form over the theme and the material, with the meter and the key as Given lane spans over
all of it, and two sections:

- **theme** (Constrained) over the bars, with the `archetype` the pairing makes (sentence, period or hybrid),
  `theme_pairing`, `cadence` and `key` constraints. A theme with an antecedent has a second `cadence` constraint, after
  the first: the weak cadence the antecedent ends in (the first is the theme's);
- **material**, after it, with the `material` constraint, so it is not part of the piece. It holds the motive node
  `m0`, with the motive as a Given melody span, in a phrase and an idea that have room for the motives made from it.

## The stages

Each stage is one `fill_run` with only its operators registered and its own checker. Stage k runs with the seed
`config.seed + k`. A stage's own targets must all be written when it ends, else `grow_run` returns `GROW_ERR_STUCK`
with the stage in `GrowOutcome.stuck_stage`. `GrowOutcome.records` keeps the `FillRecord` of every stage.

| Stage | Operators | Checker |
| --- | --- | --- |
| structure | `grow-theme` | `grow_checker` |
| motives | `motive-form`, then `motive-restate`, `motive-fragment`, `motive-rhythm` | `grow_motives_checker` |
| harmony | `harmony-motive`, `harmony-prolongation`, `harmony-phrase-model` | `harmony_prolong_checker` |
| bass | `harmony-bass` | `harmony_phrase_checker` |
| melody | `melody-fill` | `melody_fill_checker` |
| texture | `accompaniment`, `voicing-satb` | `texture_checker` |

The texture stage (`GROW_STAGE_TEXTURE`, seed `config.seed + 5`) writes the inner voice spans `grow-theme` opened, and has
nothing to do, and an empty record, when the request asked for no accompaniment and no voicing. With a voicing that
writes the soprano the melody stage has nothing to do (its spans are never made) and the texture stage writes the melody
with the alto and the tenor.

Two things make a run more reliable than one pass of the stages. A melody that gets stuck is written again from the
seed `+ 1000` up to 5 more times (the spans are reopened; the chords stay). A theme other than a sentence asks for
the phrase's climax at 250, 833, 400, 950 and 560 thousandths of the phrase in those retries (the configured position
is kept for the first attempt): a climax that falls on a restated idea, or on an idea that ends in the cadence's tone
and leaves no note above the rest, cannot be written, and moving it is the way out. A sentence keeps its position.
A run that is still stuck starts again from the plan as it was given, with seeds `+ 100000` apart, up to 11 more
times. The first run always uses the seeds above, and the same seed always gives the same result.

## The structure

`grow-theme` is a Grow operator for a Constrained section with an `archetype` and a `theme_pairing`. It builds the
first half of the theme from the beginning and the second from the ending (`src/grow/grow_beginning.c`,
`src/grow/grow_ending.c`). For a theme of 8 bars it adds, in bars (4 bars halves every length):

| Beginning | Node | Bars | Open lanes |
| --- | --- | --- | --- |
| Presentation | Phrase `phrase` (the cadence constraint) | 0-8 | |
| | Subphrase `presentation` | 0-4 | |
| | Idea `basic idea` | 0-2 | harmony (prolongation), bass, melody |
| | Idea `repetition` | 2-4 | harmony (prolongation), bass, melody |
| Antecedent | Phrase `antecedent` (the weak cadence) | 0-4 | harmony (phrase model, the weak cadence), bass |
| | Idea `basic idea` | 0-2 | melody |
| | Idea `contrasting idea` | 2-4 | melody (with the weak cadence) |
| Compound basic idea | Phrase `phrase` (the cadence constraint) | 0-8 | |
| | Subphrase `compound basic idea` | 0-4 | harmony (prolongation, tonic to tonic or to dominant), bass |
| | Ideas `basic idea` and `contrasting idea` | 0-2, 2-4 | melody |

| Ending | Node | Bars | Open lanes |
| --- | --- | --- | --- |
| Continuation | Subphrase `continuation` | 4-8 | harmony (phrase model or cadential, with the cadence), bass |
| | Ideas `fragment 1`, `fragment 2` | 4-5, 5-6 | melody |
| | Idea `cadential` | 6-8 | melody (with the cadence) |
| Consequent | Subphrase `consequent` | 4-8 | harmony (phrase model or cadential, with the cadence), bass |
| | Ideas `consequent basic idea`, `consequent contrasting idea` | 4-6, 6-8 | melody; the second with the cadence |
| Cadential | Subphrase `cadential` | 4-8 | harmony (cadential, with the cadence), bass |
| | Ideas `cadential 1`, `cadential 2` | 4-6, 6-8 | melody; the second with the cadence |

A theme with an antecedent has two phrases, as the adapter's two-phrase theme: the second half then goes in a Phrase of
its own, `consequent` for a consequent (its ideas directly in it, as the antecedent's are) and `second phrase` for the
others (the continuation or cadential subphrase inside it), carrying the theme's cadence. Any other beginning makes one
Phrase of 8 bars with the two halves as subphrases. The ideas of a consequent are named for it because a theme has
a `basic idea` and a `contrasting idea` in its first half already.

With an accompaniment, every harmony span `grow-theme` opens (the "harmony" rows above) also opens, over the same
time and under the same owner, Constrained inner voice spans: voice 1, with the `accompaniment` constraint, and voices 2
and 3 for a block, an oom-pah or `any` (an arpeggiated or Alberti accompaniment is voice 1 alone).

With a voicing, every such harmony span opens a Constrained alto (voice 1, with the `voicing` constraint) and tenor
(voice 2) over the same time, and, when the voicing writes the soprano, a Constrained melody span there too with the
cadence the harmony span ends in, in place of the melody span of each idea (those ideas have no melody span, and the
form is heard from the voiced soprano). See [voicing.md](voicing.md).

And in the material section, one motive node for each idea after the basic idea, with a `motive_ref` to `m0`: `m1` for
the repetition or the contrasting idea of the first half, then `m2` to `m4` for the second half (the fragments and the
cadential idea of a continuation, the restated basic idea and the contrasting idea of a consequent, the two ideas
of a cadential ending). The theme section becomes Realized.

### Choices

The operator proposes `GrowContext.candidates` (default 4) candidates, drawn from the run's seed:

- **Repetition** (presentation). One of three (ch. V), drawn by weight, by the [development setting](grow-development.md) (`REPEAT_WEIGHTS`,
  `src/grow/grow_beginning.c`; the balanced weights here): *exact* (2), `restated`, the harmony tonic to tonic in
  both ideas; *dominant form* (1), the `dominant-form` variant operation applied to the basic idea, the basic idea's
  harmony going tonic to dominant and the repetition's dominant to tonic; *sequence* (1), `sequence, 1 step up (tonal)`,
  the basic idea moved one step up in the key with its chord moved with it: the basic idea is the tonic and the
  repetition the chord a step up (I to ii, i to iio), each written whole. The dominant form and the sequence are not
  offered when the basic idea has one cell (a bar of 2/4 or 3/4 in 4 bars), and a sequence is not drawn when a Given melody,
  harmony or bass lies in the first half (its chords are written whole); a motif as long as the presentation draws no
  repetition. The motive of a sequence is the transposition by
  exactly one step when that fits the idea, else the other steps (`offer_transpositions`).
- **Contrasting idea** (antecedent, compound basic idea, and the consequent's). A development of `m0` whose contour
  differs from `m0`'s: a tonal inversion about a note within two semitones of the first, every interval widened or
  narrowed by one or two semitones, or one interval changed by one to four semitones. It must fit its idea and stay
  between MIDI 58 and 84. One is chosen at random among those that do; when the consequent's cadence is a PAC or an
  IAC, among those whose last note is the cadence's tone (the tonic, or the third) when there are any, so the melody
  can end where the cadence needs it. The consequent's is never the antecedent's.
- **Restated basic idea** (consequent). `restated`, as the repetition: the motive stage may keep it or transpose it.
  The melody says it again as the basic idea was said, from the first half, however far back it is (see below).
- **Compound basic idea.** Its prolongation goes tonic to tonic or tonic to dominant.
- **Fragments** (continuation). [Segments](grow-segments.md), else `fragmented`, leading, 2 or 3 notes.
- **Cadential idea** (continuation). [Segments](grow-segments.md) `liquidated`, else dropping notes.

These are the motive refs the structure plans; the motives stage writes the [motive-forms](grow-harmony.md#motive-forms) instead when it has one for the idea, and these when it has none.
- **Cadential ending.** Two ideas `liquidated` by segments or notes, the second at least as far as the first.

A candidate is dropped when a motive it asks for cannot be made or does not fit its idea.

### The checks

`grow_checker` asks standard-form's own rule functions of the planned structure; the rules are in [grow-hearing.md](grow-hearing.md#the-checks).

The motive-forms, the pool, the harmony, the melody and the free phrases are in [grow-harmony.md](grow-harmony.md); how the result is heard is in [grow-hearing.md](grow-hearing.md).
