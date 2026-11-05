# Growing a theme: motive-forms, harmony and free phrases

The motive-forms, the pool, the harmony, the melody and the free phrases of the grow stages, continuing [growing a theme](grow.md). Back to the [docs index](README.md).

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
two ideas and the consequent's restated idea are not offered anything else, and a slot with no form is left to the motive
operators below.

The checker scores a form at 1 plus its connection score (R-145, `(2 x common + rhythm + harmony) / 4`, halved when the
form is over 3/4 from the basic idea, or over 1/2 at a tempo of 140 quarter notes per minute or more, R-163), so a form always outranks the motive operators, whose recognizability is at most 1, and
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
