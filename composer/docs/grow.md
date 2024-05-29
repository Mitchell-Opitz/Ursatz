# Growing a theme

`grow_build_plan` and `grow_run` (`<ursatz_composer/grow.h>`) grow a theme from one motive: its structure (a
sentence, a period or one of the seven hybrids), then the motives made from the motive, the harmony, the bass and the
melody, and, when asked, an accompaniment, each through the fill loop. The [grow command](cli-grow.md) runs it.

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
| `pairing` | Any of the nine pairings of a beginning (presentation, antecedent, compound basic idea) and an ending (continuation, consequent, cadential). Presentation and continuation make a sentence, antecedent and consequent a period, the other seven a hybrid (`standard_form_pairing_form`). |
| `cadence` | PAC, IAC or HC: the cadence the theme ends in. |
| `first_cadence` | The weak cadence an antecedent ends in, HC or IAC and weaker than `cadence` by `harmony_cadence_strength`. Zero (PAC) means the default, HC. Read only for an antecedent. |
| `has_accompaniment`, `accompaniment` | An accompaniment under the melody and bass ([accompaniment.md](accompaniment.md)): a `PlanAccompaniment` with a pattern (`ANY` for any), a texture (`-1` for none) and a range (0 for the defaults 48 and 67). Without `has_accompaniment` the theme has no inner voice. The theme section carries it as an `accompaniment` constraint. |

`GROW_ERR_BAD_REQUEST` is also the result for a meter the harmony operators do not support, bars other than 4 or 8, a
cadence other than those three, a motive of one note, a half of the theme too short for its cadence, and an antecedent
whose cadences cannot be ordered: a theme that ends in HC has nothing weaker for the antecedent, `first_cadence` DC or
PC is not a weak cadence, and equal cadences are not a period. An accompaniment is refused when its pattern is not one,
its texture is not homophonic, its range has no room (`low` not below `high`), or it is Alberti in a compound meter.

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
| motives | `motive-restate`, `motive-fragment`, `motive-rhythm` | `motive_develop_checker` |
| harmony | `harmony-prolongation`, `harmony-phrase-model` | `harmony_prolong_checker` |
| bass | `harmony-bass` | `harmony_phrase_checker` |
| melody | `melody-fill` | `melody_fill_checker` |
| texture | `accompaniment` | `accompaniment_checker` |

The texture stage (`GROW_STAGE_TEXTURE`, seed `config.seed + 5`) writes the inner voice spans `grow-theme` opened, and has
nothing to do, and an empty record, when the request asked for no accompaniment.

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

And in the material section, one motive node for each idea after the basic idea, with a `motive_ref` to `m0`: `m1` for
the repetition or the contrasting idea of the first half, then `m2` to `m4` for the second half (the fragments and the
cadential idea of a continuation, the restated basic idea and the contrasting idea of a consequent, the two ideas
of a cadential ending). The theme section becomes Realized.

### Choices

The operator proposes `GrowContext.candidates` (default 4) candidates, drawn from the run's seed:

- **Repetition** (presentation). *Exact*: `restated`, the harmony tonic to tonic in both ideas. *Statement-response*:
  `transposed`, tonal, one step up; the basic idea's harmony goes tonic to dominant and the repetition's dominant to
  tonic. Not offered when the basic idea has one cell (a bar of 2/4 or 3/4 in 4 bars).
- **Contrasting idea** (antecedent, compound basic idea, and the consequent's). A development of `m0` whose contour
  differs from `m0`'s: a tonal inversion about a note within two semitones of the first, every interval widened or
  narrowed by one or two semitones, or one interval changed by one to four semitones. It must fit its idea and stay
  between MIDI 58 and 84. One is chosen at random among those that do; when the consequent's cadence is a PAC or an
  IAC, among those whose last note is the cadence's tone (the tonic, or the third) when there are any, so the melody
  can end where the cadence needs it. The consequent's is never the antecedent's. The melody is not asked to follow it
  note for note: it takes the motive as the intervals to keep within the tolerance.
- **Restated basic idea** (consequent). `restated`, as the repetition: the motive stage may keep it or transpose it.
  The melody says it again as the basic idea was said, from the first half, however far back it is (see below).
- **Compound basic idea.** Its prolongation goes tonic to tonic or tonic to dominant.
- **Fragments** (continuation). `fragmented`, leading, 2 or 3 notes, shortened until the fragment fits its idea.
- **Cadential idea** (continuation). `liquidated`, dropping 1 to half of the motive's notes.
- **Cadential ending.** Two ideas `liquidated` from `m0`, the second dropping at least as many notes as the first.

A candidate is dropped when a motive it asks for cannot be made or does not fit its idea.

### The checks

`grow_checker` asks standard-form's own rule functions of the planned structure.

| Form | Hard | Soft (the score is the share that hold) |
| --- | --- | --- |
| Sentence | `standard_form_subphrase_roles_in_order`, `standard_form_phrase_is_sentential`, `standard_form_pairing_form` a sentence, `standard_form_basic_idea_length_valid`, and each fragment shorter than the basic idea, as an idea and as a motive (`standard_form_is_fragmentation`) | `standard_form_sentence_is_archetypal`, `standard_form_presentation_length_usual` (which 4 bars do not meet), `standard_form_continuation_balanced`, and `standard_form_idea_reads_as_repetition` on the repetition's contour against `m0` |
| Period | `standard_form_phrase_roles_in_order`, `standard_form_period_phrase_ideas_valid`, `standard_form_period_cadences_valid`, the basic idea's length, a contrasting idea that does not read as a repetition of `m0` (contour), the antecedent's cadence weaker than the consequent's by `harmony_cadence_strength` | `standard_form_antecedent_cadence_usual`, `standard_form_consequent_cadence_usual`, `standard_form_antecedent_length_usual`, `standard_form_consequent_length_usual`, `standard_form_consequent_contrast_usual` (the two contrasting ideas differ) |
| Hybrid | `standard_form_pairing_is_hybrid`, the basic idea's length, a contrasting idea that does not read as a repetition, the antecedent's cadence weaker than the ending's, and for a continuation each fragment shorter than the basic idea | `standard_form_pairing_commonality` as three rules, of which a very common pairing meets all three, a slightly less common one two, an occasional one one and a rare one none; for a presentation the usual length and the repetition reading as one; for a continuation the balance |

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
shape it behaves as it always did. The continuation of a grown theme has the `phrase_model` shape with
`split_at_bars`, and the cadence; its cap is half the cells of the basic idea (at least 1, and at least what lets
the chords add up to the cells), since the presentation is not yet written when the structure is. A half with too
few cells for a phrase model (2 bars of 2/4 or 3/4) gets the `cadential` shape.

The antecedent and the consequent have the same: one `phrase_model` span over the phrase, no cap, and the cadence
(HC or IAC for the antecedent). `split_at_bars` is asked for only when the chords can still cover the cells without
crossing a barline: a phrase model has a tonic, one predominant in a minor key and two in a major, and a dominant
unless the cadence is a half cadence, and no chord may then be longer than a bar. Four bars of 3/4 in a minor key
ending in a half cadence have three chords for four cells, so they are not split; the chords may then cross a barline.
A compound basic idea has one `prolongation` span over its four bars, a cadential ending one `cadential` span.

## The melody

The melody stage writes the ideas in time order with the idea-target rules of [melody-fill.md](melody-fill.md):
joins, the phrase's one climax in each phrase, the end tone only on an idea that asks for a cadence (the antecedent's
contrasting idea asks for its weak cadence, whose tone is any), and an exact restatement said again as the basic idea
it restates was. A consequent's restated basic idea is said again as the first half's basic idea, whatever idea lies
between them.

## Hearing the result

After a complete run `grow_run` calls `form_hear` on the theme section (see [form-hear.md](form-hear.md)) and keeps
the themes in `GrowOutcome.heard`. A theme heard as the form the section asks for adds to `GrowOutcome.score`
(`structure share + heard`, out of `structure total + 1`); a mismatch is reported in `heard_form` and
`heard_as_requested`, never rejected. A hybrid is heard as requested only with the beginning and the ending asked
for (`requested_beginning`, `requested_ending`).

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

### Periods and hybrids

Measured by `tests/test_grow_period_heard.c`, one `ctest` test for each pairing but the sentence, over four keys and
meters and seeds 1 to 10 (`ctest` runs each as WILL_FAIL, since each asserts that every theme of 8 bars is heard as the
form it was grown as: a period for an antecedent and a consequent, a hybrid with the requested beginning and ending for
the rest):

| Pairing | 8 bars: heard as requested | Heard as another form or with other parts | No theme |
| --- | --- | --- | --- |
| Presentation + consequent | 2 of 40 (5%) | 38 (13 as a sentence, 25 as a hybrid with another beginning or ending) | 0 |
| Presentation + cadential | 0 of 40 (0%) | 40 (33 as a sentence, 7 as a hybrid) | 0 |
| Antecedent + continuation | 30 of 40 (75%) | 0 | 10 |
| Antecedent + consequent (period) | 0 of 40 (0%) | 10 (hybrids: antecedent and continuation) | 30 |
| Antecedent + cadential | 0 of 40 (0%) | 40 (hybrids: compound basic idea and continuation) | 0 |
| Compound basic idea + continuation | 39 of 40 (97%) | 1 (a sentence) | 0 |
| Compound basic idea + consequent | 19 of 40 (47%) | 21 (hybrids: compound basic idea and continuation) | 0 |
| Compound basic idea + cadential | 0 of 40 (0%) | 40 (hybrids: compound basic idea and continuation) | 0 |

Every theme of 4 bars is the same as a sentence of 4 bars: 0 of 40 for every pairing, all no theme. Only the themes of 8
bars are held to the assertion for that reason.

The causes are limits of the adapter, not tuned away:

- **The antecedent's half cadence is never heard.** The V that ends the antecedent is followed by the root-position
  tonic that begins the consequent, which the adapter hears as an authentic cadence, at the consequent's first bar. It
  cuts the first phrase one bar late, bars 1 to 5 and 6 to 8 of 8, in all 40 periods and all 40 antecedents with a
  continuation. The second phrase is then three bars, one idea, and a consequent needs two, so a period cannot be
  heard as one; at best the theme is heard as an antecedent and a continuation, which needs the second cadence
  stronger than the first. An antecedent and a continuation: 30 of 40 have an imperfect authentic cadence at the
  cut and a perfect one at the end, and are heard; the other 10, all in D minor, have a perfect authentic cadence at
  the cut too, as strong as the last, and have no theme. A period: 27 of 40 have an imperfect authentic cadence at
  the end, since the cadence's tone comes late in the last bar and not with the chord (the consequent's contrasting idea
  is a one-bar motive that fills the bar before the last chord, the melody fills the last bar with the motive's rhythm,
  and the adapter reads the soprano at the chord's start): equal to the first, no theme; 3 have two perfect ones: no
  theme; and 10 are heard as an antecedent and a continuation. This is the limit `docs/form-hear.md` describes: a V
  reached from the tonic and a consequent that starts on I6 are heard as written. The harmony operators write a
  root-position tonic first and a predominant before the V, and a harmony shape cannot ask for anything else, so a
  period cannot be heard here until it can (written in `notes.md`).
- **The cadential ending is never identified.** The adapter's theme identification passes the bass pattern of the
  cadence as false (`analysis_theme.c`), so a second part that is neither a consequent nor fragmented is a continuation
  whatever its bass: 0 of 120 cadential endings.
- **A consequent is heard when its basic idea is heard as the basic idea again.** The adapter labels the ideas of a
  phrase by their interval and rhythm similarity to its first idea, and it needs the first idea of the second part to
  be a match and the second to differ. The consequent's restated basic idea is written over a phrase model, not over the
  prolongation the first half has, so the melody cannot always repeat its intervals (similarity 0.11 to 0.33 in the
  samples, where it needs a match): 2 of 40 after a presentation, 19 of 40 after a compound basic idea.
- **A compound basic idea and a continuation is heard 39 times in 40**; the other theme's contrasting idea is read as a
  repetition of its basic idea, which makes it a presentation, so a sentence.
