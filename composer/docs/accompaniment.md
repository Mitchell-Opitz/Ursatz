# Accompaniment

`accompaniment` (`<ursatz_composer/accompaniment.h>`) is a Fill operator for the inner voice lanes. It reads a
harmony lane, a bass lane and a melody lane that are already written and writes an accompaniment under the melody and
over the bass, in the inner voices 1 to 3 (R-065). Patterns are Composer vocabulary: the library names none (R-063).
It is the texture stage of [grow](grow.md), and it applies to any plan that has the targets below.

## The target

A Constrained inner voice span, voice 1, with an `accompaniment` constraint ([plan-format.md](plan-format.md)) on the
span or on its owner, over the time of a harmony target: the chord spans under it tile it exactly (so it is whole
bars), its bass and melody are written (Given or Realized, none open), and it lies in one key and one meter.

The other voices the pattern needs, 2 and 3, are Constrained spans over the same time, or are not there at all. One
candidate writes every voice: a voice the pattern needs and has no span is added as a Realized span of the same owner;
a voice the pattern does not use and has an open span is written empty (a span with no notes, which the plan
allows only for an inner voice). Any other span in voice 2 or 3 over the time makes the target one the operator does
not apply to.

## The constraint

| Member | Meaning |
| --- | --- |
| `pattern` | `block`, `arpeggiated`, `alberti`, `oom-pah`, `motive` or `any`. |
| `motive_node` | With `motive` only, and required for it: the name of the material node holding the accompaniment motive. |
| `texture` | A `standard-texture` name the written parts are to be heard as (`homophony`, `homorhythm`, `melody and accompaniment`), or none. |
| `low`, `high` | The MIDI pitches the accompaniment keeps to; 48 and 67 when absent. |

## The patterns

All sit on the meter's beat: a quarter in 2/4, 3/4 and 4/4, the dotted quarter in 6/8 (a compound meter), and the
eighth for the figures. A chord is written on every beat from the harmony lane's numeral in the key.

| Pattern | Voices | What is written |
| --- | --- | --- |
| `block` | 1 to 3 | The chord on each beat, held to the next: one chord tone for each voice, voice 1 the top. |
| `arpeggiated` | 1 | The chord's tones, low, middle, high, middle, one for each eighth, the figure restarting on every bar. |
| `alberti` | 1 | Low, high, middle, high, one for each eighth, restarting on every bar. A simple meter only: it is refused in 6/8, and `any` leaves it out. |
| `oom-pah` | 1 to 3 | A rest on the strong beats (the first of a bar, and the third of four) where the bass sounds, and the chord, as the block writes it, on the others. |
| `motive` | 1 | A saved accompaniment motive, restated on each chord (below). |

The figures run on the eighths of the bar, not of the chord, so a bar of 3/4 ends in the middle of a figure
(`low high middle high low high`).

### The motive pattern

The pattern `motive` reads the Given melody span of the node named by `motive_node` (a motive of the accompaniment
role) and restates it on each chord span, in voice 1; voices 2 and 3 are written empty.

- The motive's rhythm starts at the chord's start and repeats every motive length, cut at the chord's end.
- Each statement starts on the chord tone nearest the motive's first pitch (a tie takes the lower). Each next note moves
  the way the motive does, by the number of chord-tone steps whose distance is nearest the motive's interval (a tie takes
  the fewer steps; an interval of 0 repeats the tone, and so does a move with no tone left in its direction).
- The tones are the chord's tones inside `low` to `high`, above the bass and below the melody sounding with the
  statement. A statement with no such tone is silent, as a beat of the other patterns is.
- In the last bar of the section the target is in (of the target, when it is in no section) the motive is liquidated:
  only the notes that start in its first beat sound, and the last of them is held to the chord's end.

`any` never chooses `motive`; it is written only when named, with a motive in the plan. The motive is not varied by the
registry operations yet.

`any` chooses among the patterns the meter allows and the texture asked for allows: a homophonic texture (`homophony`,
`melody and accompaniment`) every pattern, `homorhythm` the block only. A texture that is not homophonic has no
accompaniment, and the operator does not apply. With no texture, `any` tries every pattern. A pattern named is
written whatever the texture. The operator proposes four candidates (`AccompanimentParams.candidates`), taking the
patterns in turn from one drawn from the run's seed, so each is proposed before one is proposed twice, and every other
choice (which of the voicings that move equally little, the first voicing) also comes from the seed.

## Voicing

Each beat is voiced on its own, between the bass and the melody: a note has to be above every bass note sounding while
it sounds and below every melody note sounding while it sounds, and inside `low` to `high`. That gives the beat a
window. In it the chord is voiced in the position closest to the one before (the fewest semitones moved, voice by
voice from the top); the first is the one nearest the middle of the window. A voicing:

- is three chord tones when the window has room for three, two notes apart at least (a third is always in it; the
  root may be left out, the bass sounding it; a tone may be doubled; each of the root, third and fifth left out costs
  12 semitones, so a voicing of all three is chosen when there is one);
- is two notes, or one, when the bass and the melody close the window in. The chord is then thinner for that beat;
- is a rest when the window holds no chord tone: the accompaniment is silent for the beat.

The last case is not rare. The bass operator reaches MIDI 60, and a melody can be there too, so a bar of a unison
leaves nothing between them; in a grown theme in C major the last bar can be that.

## The checker

`accompaniment_checker`, for the candidates of this operator (others go to `harmony_phrase_checker`):

- Hard: every note a chord tone of the harmony sounding at its start; inside `low` to `high`; above the bass and below
  the melody at every moment it sounds. A candidate that breaks one is rejected, and the operator proposes none
  that does.
- Soft, the score being the share that hold: the candidate moves fewer than 3 semitones per voice per chord change on
  average; and, when the constraint names a texture, `standard-texture` hears that texture, or the texture whose
  category it is, in every measure of the span.

### How the texture is heard

The melody, the bass and the candidate are given to `standard_texture_analyze` (the library's analysis adapter, with its
default configuration) as three tracks: the melody the first, the bass the second, the accompaniment the third, its
voices the voices of the track, so a block chord is three voices that attack together and a figure is one. The analysis
cuts the span into measures and classifies each: the span is heard as asked when every measure is heard as the
texture asked for or as a texture of the category asked for. (`homophony` is a category: `homorhythm` and
`melody and accompaniment` are heard as it.) The analysis gives no measure the single answer `homophony`, so asking for
it means one of the two.

The library at this pin has no Ursatz-Analyzer texture stage, so the input is built here from the lanes.

## Growing with an accompaniment

`GrowRequest.has_accompaniment` and `GrowRequest.accompaniment` ask for one ([grow.md](grow.md)); `grow --accompaniment`
and `--texture` ([cli-grow.md](cli-grow.md)) set them. `grow-theme` opens, for every harmony target, a Constrained inner
voice span over the same time: voice 1 with the constraint, and voices 2 and 3 for a block, an oom-pah and `any`. With
`GrowRequest.accompaniment_motive` (an accompaniment-role motive of at most a bar, not with a voicing) the request makes
the accompaniment: the motive is the material node `acc`, in the last bar of the material, the constraint is pattern
`motive` naming it, and only voice 1 is opened; `texture`, `low` and `high` come from `accompaniment` when
`has_accompaniment` is set. The
texture stage runs after the melody and writes them. An accompaniment is written for each harmony target and not for
the whole theme, so the first chord of a target is voiced from the middle of the window and not from the last chord of
the one before.

## Measured

`tests/test_grow_accompaniment.c` grows 100 themes (C major 4/4 and D minor 3/4, the five patterns, seeds 1 to 10):
every one is complete and every note holds the hard rules, worked out in the test from the lanes.

Run as `test_grow_accompaniment heard`, it grows 20 themes with `any` and the texture `melody and accompaniment`
(seeds 1 to 10 in both) and asks `standard-texture` to hear the whole theme. It is a WILL_FAIL test in `ctest`:

| | Heard as melody and accompaniment |
| --- | --- |
| Themes, in every measure | 9 of 20 (45.0%) |
| Measures | 132 of 160 (82.5%) |

The 28 measures that are not: 25 are heard as homorhythm and 3 as monophony. The monophony is a bar with no room for the
accompaniment, bass and melody both on MIDI 60 (above). The homorhythm is the classification: the adapter reads voices
that attack together at 75% of the attack times or more as rhythmically unified, and a melody that moves with the
accompaniment's beat is read so. The operator asks the analysis of each target, and where none of its four candidates is
heard as asked (the first two bars of seed 9 in C major are such a target) it keeps the smoothest of them. It is not
tuned to the analysis's answer.
