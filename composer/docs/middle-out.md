# Given material

A piece can be planned from the middle: a phrase's melody, a progression or a bass line is placed in a plan, in any
section and from any bar of it, and the stages grow the form around it and fill the rest. What was given is never
changed (R-071). `<ursatz_composer/given.h>`.

```c
GivenResult plan_give_melody(PiecePlan *, NodeId section, size_t bar, const PlanNote *notes, size_t count);
GivenResult plan_give_progression(PiecePlan *, NodeId section, size_t bar, const char *const *numerals, size_t count,
                                  size_t cells_per_chord);
GivenResult plan_give_bass(PiecePlan *, NodeId section, size_t bar, const PlanNote *notes, size_t count);
GivenResult plan_give_section(PiecePlan *, NodeId section, const PiecePlan *source, NodeId source_section);
```

From the command line: `plan --given-melody <file> --at B:1`, `--given-progression "I IV V I" --at A:1 --cells 2` and
`--given-bass <file> --at B` ([cli-plan.md](cli-plan.md)); `fill` says which spans were given and that they are
unchanged ([cli-fill.md](cli-fill.md)).

## Placing

The material is Given lane spans owned by the section, from `bar` (counted from 0), as many bars long as it needs:

- a melody or a bass line is one span, its notes' offsets from the start of its first bar. A melody may open and end
  with a rest. A bass line has no rest, because the bass lane is read as one unbroken line: its notes tile its bars;
- a progression is one harmony span for each chord, `cells_per_chord` cells long (a cell is a beat or half a bar,
  see [harmony_phrase.h](../include/ursatz_composer/harmony_phrase.h): a bar of 2/4 or 3/4 has one, a bar of 4/4 or
  6/8 has two). Each numeral is read in the mode of the key under it (a numeral the harmony operators write, or an
  applied chord such as `V7/V`) and given its function.

Every call checks all of the material before it adds any, so a call that is refused changes nothing. The results are
`GIVEN_ERR_NOT_SECTION` (the form, the material, an unknown node), `GIVEN_ERR_BAD_NOTES`, `GIVEN_ERR_BAD_NUMERAL`,
`GIVEN_ERR_BAD_BAR` (the bar is outside the section), `GIVEN_ERR_PAST_END` (the material would cross the section's
end), `GIVEN_ERR_KEY` (it is not inside one key span, so a melody cannot cross the change of key a section may have in
its second half), `GIVEN_ERR_METER` and `GIVEN_ERR_OVERLAP` (Given material of the same lane is already there).

Two more results belong to `plan_give_section` below: `GIVEN_ERR_NOT_REALIZED` and `GIVEN_ERR_LENGTH`.

A piece that is to take given melodies is requested with `PieceRequest.given_melodies`: the material section holds a
bar more for each, for the motive described below.

`given_gaps(plan, lane, voice, span, ...)` is what a span has left when the Given spans of a lane are taken out, and
`given_touches` asks whether there is any. `given_snapshot` copies the material the user placed (the spans owned by a
section) and `given_snapshot_report` says, run by run (a progression is a run), whether every span is still there, Given,
with the same payload byte for byte. `fill` takes a snapshot before the run and reports after it.

## A section from another plan

`plan_give_section` gives a whole realized section of another plan (a theme the `theme` command composed) to a section of
this one, the way a piece structure fills a section with a theme ([piece-structure.md](piece-structure.md#fills)):

- its harmony, bass, melody and inner voice spans become Given spans owned by the section, moved to its start and
  transposed by the key difference between the two sections (the key lanes must have the same shape and mode, else
  `GIVEN_ERR_KEY`);
- its phrase and idea nodes are copied under the section, in the states they had, so `form_hear` reads the section as the
  theme it was; the section is then Realized and no stage targets it. Constraints that name a node of the source plan
  (a motive reference) are dropped, since the plan has no such node;
- the source must be realized (not Constrained, with harmony, bass and melody all written: `GIVEN_ERR_NOT_REALIZED`),
  as long as the section, a pickup before it included (`GIVEN_ERR_LENGTH`; a span that would start before the section's
  `pickup` is refused the same way), and in the same meter (`GIVEN_ERR_METER`). A section that has nodes already, or lane
  spans where the copy would go, is `GIVEN_ERR_OVERLAP`. A refused call leaves the plan as it was.

The Given spans are in the snapshot like any others, so `given_snapshot_report` shows that a run left them as they were.

## How the stages go around it

**grow-theme** gives a section its nodes as it always does, and opens its lane targets only in the gaps:

- *harmony*: one target for each gap of the harmony lane in a phrase, subphrase or idea. The shape of a gap that
  stops short of its owner's end is a prolongation with no anchor (a cadence arrives only at an end); a gap that starts
  inside its owner opens with no tonic (a cadential progression); the cadence constraint goes to the gap that ends at the
  owner's end;
- *bass*: one target for each gap of the bass lane;
- *melody*: one target for each gap of the melody lane in an idea, with the cadence on the one that ends the idea. An
  idea with a Given melody inside it is made from that melody: a motive node in the last free bar of the material,
  Given, holds the first bar of the Given span (`given N`, N the span's id), and the idea's melody targets ask for it.
  An idea that ends the theme does not take it (its last note must be the cadence's tone, which the given first bar
  need not be).

**Harmony.** The compose run registers `harmony_prolong_register_given` and `harmony_phrase_register_given`
(`harmony_prolong.h`, `harmony_phrase.h`). They write the same chords as the operators without `_given`, and they read
what is Given around a target:

- *a Given bass* under it no longer leaves the target to `harmony_octave`: each chord's bass member (root, or third,
  fifth, seventh by inversion, `harmony_chord_bass_pc`) is the pitch class of the Given bass note sounding where the
  chord starts;
- *a Given melody* over it is harmonized: a note that starts on a strong beat (the first cell of a bar) is a tone of the
  chord there (`melody_chord_mask`);
- *Given chords* before and after it: the chords are heard with them (up to 8 each side, the ones before trimmed to
  start on a barline) and a cadence the recognizer hears that arrives inside the target and is not the target's own at
  its end, or that the target's last chord leads into the Given chords, rejects the candidate. A cadence is not heard at a
  boundary: where a phrase or a section starts or ends at the join, V then the tonic is no cadence. A cadence the Given
  chords make among themselves is theirs.

The generators deal chords until they fit (up to 480 deals for a phrase model, 160 for a prolongation figure), so the
requests are narrowed before the checker is asked.

**Melody.** A target before Given or written melody reads its first note (`lead_out`), and the target's last note
leads to it by a leap standard-counterpoint allows; the join is judged like the join before a target. Of a Given line
only the one note joined is read, not its own faults. The motive m0 of the material, which starts where the piece
ends, is not melody of the piece.

## What does not work

These were met, and are the limits of what is built:

- the harmony operators write a small vocabulary (a phrase model: the tonic, a predominant, the dominant, the tonic; the
  prolongation figures), so a given melody is harmonized only when its strong-beat notes are tones of chords from it. A
  melody whose downbeats are not (a suspension on the downbeat, a downbeat on `D` in the last bar) leaves the harmony
  target with no candidate, and the run is stuck at the harmony stage;
- a phrase model never lets a chord cross a barline in a continuation (`split_at_bars`), so the downbeat of every bar of
  a continuation is a tone of the chord of that bar;
- a harmony target must begin and end on a beat ([grow.md](grow.md)), so a Given progression that ends between beats
  leaves a gap the operators cannot write, and Given material around a target with part cells is not read;
- the bass and the melody operators read the chords over their target as a row that tiles it: a Given chord that
  crosses the edge of a target (an idea, or a Given bass span) is not read;
- a cadence needs room: a gap of one bar cannot hold a perfect authentic cadence, only a half cadence.

## Tests and measured results

`tests/test_middle_out.c` (`ctest -R test_middle_out`):

| Test | |
| --- | --- |
| `give` | The refusals (not a section, a bar outside the section, past the end, notes, numerals, a bass with a rest, overlap, a key change inside a section) leave the plan as it was; the Given spans a call adds; the gaps; the snapshot. |
| `smoke` | The first of seeds 1 to 5 that composes with the melody below. |
| `partial` | A melody given in bars 4 to 7 of the B of a ternary form: composes, the idea that begins in it is made from its first bar (a Given motive node in the material), every melody span is written or Given. |
| `melody`, `progression`, `bass` | Seeds 1 to 10, C major 4/4: `melody` an 8-bar melody given as the B of a ternary form (24 bars); `progression` `I IV V I`, a bar each, given at bars 1 to 4 of the A of a rounded binary form (16 bars); `bass` an 8-bar bass line given in the B of a binary form (16 bars). |

Every run that completes leaves the Given payloads byte for byte as they were (every run, completed or stuck), the chords
stand on the given bass at each chord's start and carry the given melody's downbeats. The shares are baselines in
`tests/test_middle_out.c`: a share below its baseline fails the test. Measured, seeds 1 to 10:

| Scenario | Completed | Sections heard | Cadence heard as planned |
| --- | --- | --- | --- |
| melody (ternary) | 10 of 10 | 10 of 30 | 12 of 30 |
| progression (rounded binary) | 10 of 10 | 6 of 30 | 24 of 30 |
| bass (binary) | 9 of 10 | 18 of 18 | 18 of 18 |

A section is heard when standard-form hears a theme in it; it need not be the one planned. A cadence is heard as planned
when the last cadence heard in a section is the type planned for it, arriving in its last bar. For comparison, the same
three forms without given material, fill of seeds 1 to 10 (the sections heard): ternary 16 of 30, rounded binary 10 of
30, binary 20 of 20. The share of sections heard in the progression scenario is under its control (6 against 10); why is not
investigated.

The melody the test gives is `C5:h E5:h G5:q E5:q C5:h E5:h G5:h G5:q F5:q E5:h E5:h C5:h A4:q B4:q C5:h D5:h B4:h
C5:w`: its downbeats are `C G E G E A D C`, tones of the chords the operators can write there (the tonic, then a
predominant, the dominant and the tonic in the last four bars). The bass is `C3:w` five times, `F3:w`, `G3:w`, `C3:w`.

`tests/test_plan_cli.c` has the command cases (`test_given`): no form gives a ternary form of 24 bars with the melody in
B and says so, the given spans are printed as unchanged by `fill` in text and JSON, the same seed gives the same file,
and each usage error and each refusal is named.
