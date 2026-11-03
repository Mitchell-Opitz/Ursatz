# Growing a theme

`grow_build_plan` and `grow_run` (`<ursatz_composer/grow.h>`) grow a theme from one motive: its structure (a
sentence, a period or one of the seven hybrids), then the motives made from the motive, the harmony, the bass and the
melody, and, when asked, an accompaniment or a four-part voicing, each through the fill loop. The [grow command](cli-grow.md) runs it.

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
| `motive` | The motive. It must fit in one bar of the meter (`GROW_ERR_MOTIVE_TOO_LONG` otherwise) and have two notes or more. |
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
`s1` (contrasting), `s2` (continuation) or `s3` (cadential), a Given melody span, after `m0`; the material is a bar longer for
each. The first idea of the slot is the motive itself (exact, or a saved form of it, see below); the later ideas of the slot
(the consequent's contrasting idea, fragment 2, cadential 2) are made from it as the basic motive's are, and the remoteness of
their forms is measured from it. The basic idea's ideas are unchanged.

`GROW_ERR_BAD_REQUEST` for a slot motive that is not a full motive of two notes or more in the theme's key and meter, that is
longer than the first idea of its slot (two bars of eight for the contrasting idea and the cadential idea, one for fragment 1),
or for a slot the pairing has no idea for (a contrasting slot in a sentence, a continuation slot without a continuation, a
cadential slot in a period). A slot motive keeps no harmony, features or role (a plan keeps only the notes).

### Idea motives

`GrowIdea` names the ideas of a theme as the plan does (`grow_idea_name`: `basic idea`, `repetition`, `contrasting idea`,
`consequent basic idea`, `consequent contrasting idea`, `fragment 1`, `fragment 2`, `cadential`, `cadential 1`,
`cadential 2`); `grow_idea_in_pairing` says whether a pairing has one (R-206). `GrowRequest.idea_motives` gives a single idea a
motive of its own, in place of its slot's for that idea. It is added to the material as the node `i<k>` (k the `GrowIdea`
value), a Given melody span after the slot motives (the material is a bar longer for each), and the idea's melody asks for it
directly, as an exact restatement: the motives stage makes no motive for that idea. The other ideas of the slot are made as
before, and the same `Motive` may back several nodes. Melody-fill places an exact restatement of a motive of its own as that
motive, not as a copy of the idea before it. `GROW_ERR_BAD_REQUEST` for an idea the pairing does not have, an entry for the
basic idea, or a motive that is not a full motive of two notes or more in the theme's key and meter or is longer than its idea
(a bar, or the idea's eighths of the theme when less).

### Given chords

`GrowRequest.chords` (`chord_count`) gives chords of the theme (R-208), `GivenChord` of [given.h](../include/ursatz_composer/given.h):
a Roman numeral, an onset from the theme's start and a length. `grow_build_plan` gives them to the theme section with
`plan_give_chords` once the section is made, one Given harmony span for each, exactly its onset and length;
`GROW_ERR_BAD_REQUEST` when it refuses them. The harmony stage opens targets only over the gaps (`given_gaps`), so the chords
stay as they are, the bass is written over them, and a gap between Given chords is judged with the Given chords before it: a
gap of one chord is allowed when it follows Given chords, and is heard as the end of the chain they make. The harmony and bass
operators write a chord on the cells of the meter only, so the `theme` command refuses any other onset
([theme-request.md](theme-request.md#chords)); `plan_give_chords` itself takes any onset (`given_cell_length` is the grid).
A run stuck at the harmony, bass or melody stage with a chord in or next to the span it left open says so, naming the bar
(`the given chords in bar <n> do not allow the <cadence>`, exit 2).

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
| harmony | `harmony-prolongation`, `harmony-phrase-model` | `harmony_prolong_checker` |
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

These are the P13 motive refs the structure plans; the motives stage writes the [motive-forms](#motive-forms) instead when it has one for the idea, and these when it has none.
- **Cadential ending.** Two ideas `liquidated` from `m0`, the second dropping at least as many notes as the first.

A candidate is dropped when a motive it asks for cannot be made or does not fit its idea.

### The checks

`grow_checker` asks standard-form's own rule functions of the planned structure.

| Form | Hard | Soft (the score is the share that hold) |
| --- | --- | --- |
| Sentence | `standard_form_subphrase_roles_in_order`, `standard_form_phrase_is_sentential`, `standard_form_pairing_form` a sentence, `standard_form_basic_idea_length_valid`, and each fragment shorter than the basic idea, as an idea and as a motive (`standard_form_is_fragmentation`) | `standard_form_sentence_is_archetypal`, `standard_form_presentation_length_usual` (which 4 bars do not meet), `standard_form_continuation_balanced`, and `standard_form_idea_reads_as_repetition` on the repetition's contour against `m0` |
| Period | `standard_form_phrase_roles_in_order`, `standard_form_period_phrase_ideas_valid`, `standard_form_period_cadences_valid`, the basic idea's length, a contrasting idea that does not read as a repetition of `m0` (contour), the antecedent's cadence weaker than the consequent's by `harmony_cadence_strength` | `standard_form_antecedent_cadence_usual`, `standard_form_consequent_cadence_usual`, `standard_form_antecedent_length_usual`, `standard_form_consequent_length_usual`, `standard_form_consequent_contrast_usual` (the two contrasting ideas differ) |
| Hybrid | `standard_form_pairing_is_hybrid`, the basic idea's length, a contrasting idea that does not read as a repetition, the antecedent's cadence weaker than the ending's, and for a continuation each fragment shorter than the basic idea | `standard_form_pairing_commonality` as three rules, of which a very common pairing meets all three, a slightly less common one two, an occasional one one and a rare one none; for a presentation the usual length and the repetition reading as one; for a continuation the balance |

## Motive-forms

The motives stage writes the motive node of each idea. `motive-form` (`<ursatz_composer/grow_form.h>`, `src/grow/grow_form.c`)
asks the [variant registry](variant-registry.md) for forms of the basic idea that suit the idea's place and ranks them by
how well they connect (`motive_connection`, `<ursatz_composer/motive_connect.h>`). The slot is the name of the idea whose
melody asks for the motive node. A candidate is a chain of registry steps applied to the basic idea (`m0`, or the
section's own motive), so the same plan always gives the same candidates; only a tie in the score is settled by the run's seed.

| Idea | Forms |
| --- | --- |
| repetition, restated | `exact` when the repetition is a plain restatement (its melody restates the basic idea) |
| repetition, answering | the `dominant-form` of the basic idea when the repetition's chords start on the dominant and the basic idea opens on the tonic; else a tonal transposition of 1 to 3 steps either way |
| consequent's basic idea | `exact` |
| contrasting ideas (antecedent, compound basic idea, consequent) | a one-step chain of any row whose result has the function `contrast` (R-143) and a contour other than the basic idea's, and is not another idea's motive |
| fragment 1, fragment 2 (continuation) | `liquidate-features` with `steps 1` and `shorten 1` on the basic idea, and twice: the units halve (ch. VIII), so a basic idea of 2 bars gives 1 bar, then 1/2 bar |
| cadential idea (continuation) | a one-step chain whose result has the function `cadential` and fewer notes than the basic idea |

Every form is a full motive in the key's scale between MIDI 58 and 84 (a form below is taken up an octave), no leap wider
than a fifth unless the basic idea has one, as many notes as the basic idea (a fragment or cadential idea has fewer), and
fits its idea. An idea that ends in a cadence ends on its tone (the tonic for a PAC, the third for an IAC). The cadential ending's
two ideas and the consequent's restated idea are not offered anything else, and a slot with no form is left to the P13
operators below.

The checker scores a form at 1 plus its connection score (R-145, `(2 x common + rhythm + harmony) / 4`, halved when the
form is over 3/4 from the basic idea, or over 1/2 at a tempo of 140 quarter notes per minute or more, R-163), so a form always outranks the P13 operators, whose recognizability is at most 1, and
the part after the 1 is the connection. The connection is measured against the idea before (its motive when written, else
the basic idea); the form's distance from the basic idea is its chain's cost over the cost plus 4.

`GrowConfig.tempo` sets the limit: at 140 or faster the connection is halved from a remoteness of 1/2 instead of 3/4, so the
fast themes keep nearer to the basic idea. `--explain` names the limit in a choice's label (`limit 1/2;`) when it is not 3/4.

`GrowConfig.basic` gives `m0` its own harmony, features and role (a plan keeps only the notes): `grow` passes the motive
it was given, so `--feature`, `--harmony` and `--role` reach the liquidation and the harmony rows. Its notes must be `m0`'s.

### The pool

`GrowConfig.pool` is a list of saved forms (`GrowPoolForm`, `grow_form.h`): the motive, the slot whose motive it derives from
(`slot_motive_of`, basic for `m0`), its functions as bits (`GROW_FUNCTION_*`, R-143), whether it is approved, and the item and
chain it was saved with. The motives stage offers a pool form for an idea when it derives from the motive the idea is made
from, its functions include the function of the idea's slot, and it fits as a registry form does (a full motive in range, no
wider a leap, no longer than the idea's room, the cadence tone of an idea that ends in a cadence). It scores 1 plus its
connection plus 1/2 when approved and 1/4 when unflagged, so it outranks a registry form of the same connection and an
approved form an unflagged one; when no pool form fits, the registry's forms are invented as before. A connection that is
much higher can still outweigh the flag. A pool form equal to another candidate replaces it, the approved ones first.

The record and `grow --explain` name each form: the label is the chain, the connection in percent and the derivation in
plain words ("transpose 90%: Transposed +2 steps of the key's scale. Kept the"), cut at 63 characters; the score shown is 1
plus the connection. A pool form is labelled `pool <item> <connection>%: <chain>`.

## The harmony

**Targets on beats.** A harmony target (phrase model, prolongation, schema, sequence, embellishment, and the bass under
it) lies in one meter lane span and starts and ends on a beat of it, a beat being 1/`beat_unit` of a whole note: a span
of whole bars from a barline, or any span between beats, such as 7 beats of 4/4 or beats 2 to 11. The cells are the
places in a bar where a chord may start (one for a bar of 2/4 or 3/4, two for 4/4 or 6/8), anchored to the barlines of
the meter lane span. A span that starts or ends between two of them has a *part cell* at that end: the `lead`, from its
start to the first cell boundary, and the `tail`, from the last cell boundary to its end (zero when it is on one). Each
part cell is a cell of its own, so the span's cells are the lead, the whole cells and the tail, and a chord is a number
of them. A part cell counts as a weak cell for the rules about strong beats unless it starts on a downbeat, and a
recognizer arrival may be as short as the part cell's one beat. A span that does not start and end on a beat is not a
target.

The phrase model lays the tonic, the predominant, the dominant and the tonic of the cadence over the cells, one cell to
a chord at the least. The cadence's last chord starts on the last downbeat the span holds and takes the cells from there
to the end (the whole last bar of a span of bars); a span that holds no downbeat after its lead starts it on the first
cell boundary after the lead, where the recognizer may not hear the cadence. `harmony_phrase_min_cells(cadence, meter)`
is the fewest cells a span can have (the cells the cadence needs before its last chord, and one for it), and a span with
fewer cells gets no candidates; `harmony_phrase_min_bars` is the same for a span of whole bars, which ends in a whole
bar. The schema and sequence operators need whole cells from a cell boundary, so they skip a span with a lead or a tail
(no candidates, not an error). A chord of one beat gets one bass note. Given material around a target with part cells is
not read: the Given operators decline such a target.

`harmony-prolongation` writes a Constrained harmony span with a `harmony_shape` of kind `prolongation`: one lane span
per chord, on the cells of the meter, from standard-harmony's prolongation figures (I-V-I, I-V6-I, I-viio6-I6, I-IV-I,
I-V4/3-I6, and the same turned round for a span that opens on the dominant). A span that ends on V ends on V6, V6/5 or
V4/3, or on a V off a downbeat. Its checker rejects any candidate in which standard-harmony's recognizer
hears a cadence, inside or at the end, whatever the soprano; the phrase model's tonic-then-dominant rule is not asked
of a prolongation. A prolongation that follows one of the same shape repeats its chords, which is how an exact
repetition gets the harmony of the basic idea.

`harmony-phrase-model` reads an optional `phrase_model` or `cadential` shape. `max_cells_per_chord` caps every chord
but the last, which always takes the cells from the last downbeat (the whole last bar of a span of bars) so the cadence
arrives on a downbeat; `split_at_bars` keeps every
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

The checker also scores root progressions (`root-progression`, Schoenberg ch. XII, R-164) as one soft rule: of the root
changes among the chords the phrase rules read (applied chords and the inside of a prolongation are left out), at least
2/3 are strong, the root rising a fourth or a step or falling a third or a step. Falling a fourth (rising a fifth) and
rising a third are weak. Fewer than two root changes hold. The melody's four shaping rules are in
[melody-fill.md](melody-fill.md#the-shaping-rules). All five are on by default and never reject.

## The melody

The melody stage writes the ideas in time order with the idea-target rules of [melody-fill.md](melody-fill.md):
joins, the phrase's one climax in each phrase, the end tone only on an idea that asks for a cadence (the antecedent's
contrasting idea asks for its weak cadence, whose tone is any), and an exact restatement said again as the basic idea
it restates was. A consequent's restated basic idea is said again as the first half's basic idea, whatever idea lies
between them.

## Free phrases

A section with a `free_phrase` constraint and no `archetype` or `theme_pairing` is a free phrase (R-170): a phrase of any
length in beats, the sections of a [piece structure](piece-structure.md) that are not themes of 4 or 8 whole bars.
`grow-theme` builds it in the same structure stage, with the cadence, the key and the accompaniment the section asks for
(a PAC, IAC or HC, in one key).

- **Ideas.** One idea for each bar, cut at the barlines inside the section. A part bar at the start of the section (it
  starts between barlines) joins the idea after it, and one at the end the idea before it; a section shorter than a bar
  is one idea. A section of more than 32 bars has several bars to an idea.
- **Motives.** The first idea (`basic idea`) is the section's motive exact, `m0`. The last (`cadential`) is a cadential
  form of it: `m0` liquidated. The ones between (`fragment 1`, `fragment 2` in turn) are continuation forms: a leading
  fragment, a transposition in the key's steps, an inversion or an interval changed, each fitting its idea and the range
  58 to 84. A section of one idea is the basic idea alone; the melody ends in the cadence.
- **Chords.** One phrase holds the ideas, with the harmony and the bass over all of it: a phrase model if the section has
  room for the cadence's chords, else the cadential progression. A section of whole bars from a barline has to have room
  for the last chord to be a whole bar (`harmony_phrase_room_cells`); one that starts or ends between barlines needs
  `harmony_phrase_min_cells`. A long phrase asks for chords of at most about a bar and a half, so the harmony changes.
- **Checks.** The cadential idea is shorter than `m0`. The score is the share of the soft rules that hold: no middle idea
  repeats the one before it, and the cadential idea is shorter than the basic idea.

The motives stage names the ideas as a theme's (`fragment 1`, `fragment 2`, `cadential`), so it scores a free phrase's
forms by function and connection as it does a theme's, and prefers a saved motive-form of the pool for the function
([Motive-forms](#motive-forms)).

## Hearing the result

After a complete run `grow_run` calls `form_hear` on the theme section (see [form-hear.md](form-hear.md)) and keeps
the themes in `GrowOutcome.heard`. A theme heard as the form the section asks for adds to `GrowOutcome.score`
(`structure share + heard`, out of `structure total + 1`); a mismatch is reported in `heard_form` and
`heard_as_requested`, never rejected. A hybrid is heard as requested only with the beginning and the ending asked
for (`requested_beginning`, `requested_ending`).

Measured by `tests/test_grow_sentence_heard.c`, over four keys and meters and seeds 1 to 10 (`ctest` runs it as
WILL_FAIL, since it asserts that every grown sentence is heard as a sentence). The theme is heard through the Analyzer
(P25 B7b); the figures measured with the adapter reading the planned chords (the motive-forms) were 28 of 40 (70%) at 8
bars, 12 as a hybrid, none unheard:

| Bars | Heard as a sentence | Heard as another form | No theme |
| --- | --- | --- | --- |
| 8 | 30 of 40 (75%) | 9 | 1 |
| 4 | 0 of 40 (0%) | 0 | 40 |
| all | 30 of 80 (37%) | 9 | 41 |

The causes below were found with the adapter reading the planned chords; the Analyzer identifies the chords from the notes
and hears every written lane, which moved the shares as above. They are not analysed again here. They are limits of the
phrase and theme stages, not tuned away:

- **Statement-response is heard as a hybrid.** The adapter compares ideas by the semitone interval and the onset
  rhythm of each pair of notes (75% must match). A tonal step up changes some intervals by a semitone, so the
  repetition reads as a contrasting idea. The exact repetitions are heard as a sentence, the statement-responses (a
  transposition or a dominant form) mostly as a hybrid.
- **A sentence of 4 bars is not heard at all.** The adapter cuts a phrase into ideas of two measures, so 4 bars are
  two ideas, which it reads as one part, not as a presentation and a continuation: no theme.
- **Fragmentation is not seen.** The nominal cut of 2 measures finds no 1 + 1 + 2 continuation, so `has_fragmentation`
  is false for every grown sentence.

### Periods and hybrids

Measured by `tests/test_grow_period_heard.c`, one `ctest` test for each pairing but the sentence, over four keys and
meters and seeds 1 to 10 (`ctest` runs each as WILL_FAIL, since each asserts that every theme of 8 bars is heard as the
form it was grown as: a period for an antecedent and a consequent, a hybrid with the requested beginning and ending for
the rest). The compound basic idea and a continuation was heard in every run with the motive-forms and is heard in 38 of
40 through the Analyzer. The "before" column is the figure measured with the adapter reading the planned chords:

| Pairing | Before | 8 bars: heard as requested | Heard as another form or with other parts | No theme |
| --- | --- | --- | --- | --- |
| Presentation + consequent | 9 of 40 (22%) | 5 of 40 (12%) | 33 (19 as another form, 14 as a hybrid with other parts) | 2 |
| Presentation + cadential | 0 of 40 (0%) | 0 of 40 (0%) | 39 (32 as another form, 7 as a hybrid with other parts) | 1 |
| Antecedent + continuation | 30 of 40 (75%) | 14 of 40 (35%) | 2 (hybrids with other parts) | 24 |
| Antecedent + consequent (period) | 0 of 40 (0%) | 3 of 40 (7%) | 16 (as another form) | 21 |
| Antecedent + cadential | 0 of 40 (0%) | 0 of 40 (0%) | 37 (hybrids with other parts) | 3 |
| Compound basic idea + continuation | 40 of 40 (100%) | 38 of 40 (95%) | 0 | 2 |
| Compound basic idea + consequent | 19 of 40 (47%) | 3 of 40 (7%) | 34 (hybrids with other parts) | 3 |
| Compound basic idea + cadential | 0 of 40 (0%) | 0 of 40 (0%) | 34 (hybrids with other parts) | 6 |

Every theme of 4 bars is the same as a sentence of 4 bars: 0 of 40 for every pairing, all no theme. Only the themes of 8
bars are held to the assertion for that reason.

The causes below were found with the adapter reading the planned chords; the Analyzer identifies the chords from the notes
and hears every written lane, which moved the shares as above. They are not analysed again here:

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
- **A compound basic idea and a continuation is heard 40 times in 40** since the motive-forms; before them 39, the other theme's contrasting idea read as a
  repetition of its basic idea, which makes it a presentation, so a sentence.
