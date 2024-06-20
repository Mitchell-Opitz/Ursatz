# Voicing

`voicing-satb` (`<ursatz_composer/voicing.h>`) is a Fill operator for the inner voice lanes. It reads a harmony lane and
a bass lane that are already written and voices the progression in four parts, soprano, alto, tenor and bass, by the
rules of `standard-voicing`, with the voices joined by `standard-counterpoint` and `standard-harmony` (R-066). It is the
texture stage of [grow](grow.md) when the request asks for a voicing, and it applies to any plan that has the targets
below. The hearing that checks it, `voice_hear`, is described at the end.

## The target

A Constrained inner voice span, voice 1 (the alto), with a `voicing` constraint ([plan-format.md](plan-format.md)) on
the span or on its owner, over the time of a harmony target: the chord spans under it tile it exactly (so it is whole
bars), its bass is written (Given or Realized, none open), and it lies in one key and one meter.

- The tenor is the inner voice 2: a Constrained span over the same time, or none at all (the candidate then adds a
  Realized span of the same owner).
- The soprano is the written melody, or, with `write_soprano`, a Constrained melody span over the same time that the
  voicing writes too, one note a chord (a chorale texture). Anything else over the time makes the target one the operator
  does not apply to.

| Member | Meaning |
| --- | --- |
| `write_soprano` | The voicing writes the melody. Without it, the soprano of a chord is the written melody note sounding at the start of the chord, or, if that note is not a tone of the chord, the first chord tone that sounds in it. |

## The parts

One note a voice for each chord, held for the chord. The chord tones come from the harmony lane's numeral in the key (as
for the [accompaniment](accompaniment.md)), spelled as the degrees of the key they are. The ranges are
standard-voicing's: soprano C4 to G5, alto G3 to D5, tenor C3 to G4, bass F2 to D4 (the library counts semitones from C0,
the Composer from C-1, so the operator adds twelve).

A voicing of a chord is kept only when `standard_voicing_check` finds no rule broken: the bass is the chord member the
inversion calls for, no chord tone is missing and no note is foreign (chord construction); each voice is in its range;
no more than an octave between soprano and alto and between alto and tenor, no more than a twelfth between tenor and
bass; no voice crosses another; the leading tone and a chordal seventh are never doubled, and a seventh chord doubles
nothing. The bass, and a written melody, are not the voicing's to write, so they are not held to a range, and a note
doubled between the two of them that the rules forbid doubled (the leading tone in a first-inversion V under a melody on
it) is not asked of the chord when it is the only note doubled.

A soprano the voicing writes ends the cadence asked of the melody span as the recognizer hears it: a PAC on the tonic,
an IAC on the third of the tonic chord.

## The search

`voicing_search.c`. For each chord, every voicing that holds the rules above. The chords are joined by a beam search of
width `VoicingParams.beam` (64 by default): the cost of a path is the total number of semitones the upper three voices
move, and 12 more for each leap of those voices wider than a fifth. A connection is refused when

- `standard-counterpoint` (`PARALLEL_FIFTHS`, `PARALLEL_OCTAVES`, and for the outer voices `DIRECT_PERFECTS`, from
  `harmonic_rules.h`) finds parallel fifths or octaves between any two of the four voices, or, with `write_soprano`,
  direct fifths or octaves between the soprano and the bass (with a written melody they cannot be avoided, so they are
  not asked);
- `standard-harmony` (`harmony_resolution_rules.h`) finds a chordal seventh, or the leading tone of a V, that does not
  resolve in the same voice. The rules read a chord in the key; an applied chord is read as the V or vii of the key of
  its target, when the target follows. A written melody's own seventh and leading tone are left to it, and so is the
  motion between a written melody and the bass.

The `VoicingParams.candidates` (4 by default) cheapest complete paths are the candidates; paths of the same cost are
ordered by the run's rng (every draw is from the run's seed). When no path is allowed, there is no candidate and the
target is stuck.

**The chord before.** An alto span follows others in a grown theme, one for each harmony target. When the harmony span
before the target has been voiced (written in all four voices at its start), it is the first chord of the search with
only the voicing it was written with, so the connection to it is held to the same rules and the voicing carries on from
what is written. Targets filled in another order do not have this.

## The checker

`voicing_checker`, for the candidates of this operator (others go to `harmony_phrase_checker`; in the texture stage
`texture_checker` hands each candidate to the checker of its operator):

- Hard: a chord that `standard-voicing` does not hold for, parallel fifths or octaves, an unresolved chordal seventh or
  leading tone (all by the library's rules, as above), a soprano that is not the written melody (without
  `write_soprano`), a chord before the target that is not the voicing it was written with.
- Soft, the score being the share that hold: of the connections between chords, those where every upper voice moves by
  a third or less.

## Growing with a voicing

`GrowRequest.has_voicing` and `GrowRequest.voicing` ask for one ([grow.md](grow.md)); `grow --voicing` and
`--texture chorale` ([cli-grow.md](cli-grow.md)) set them, and it is refused together with an accompaniment.
`grow-theme` opens, for every harmony target, a Constrained alto (voice 1, with the constraint) and tenor (voice 2) over
the same time. With `write_soprano` it opens a Constrained melody span there too, with the cadence the harmony target
ends in, and makes no melody for the ideas: the melody stage has nothing to do, the texture stage writes the melody with
the other voices, and the form is heard from that soprano.

## Hearing the voice leading

`voice_hear` (`<ursatz_composer/voice_hear.h>`) hears a written plan as Ursatz-Analyzer's voice-leading stage hears
real music. The grid is built here from the lanes (the Analyzer pin has no stage that does), from the library's own
adapter tests:

- four voices: the melody lane the soprano, inner voices 1 and 2 the alto and the tenor, the bass lane the bass;
- one row for each harmony lane span: the note each voice sounds at the start of the chord (a rest where none sounds),
  spelled as the tone of the chord it is, else as a degree of the key;
- `analysis_perfects.h` finds the parallel fifths and octaves between every pair of voices;
- `analysis_resolution.h`, given the chords as `harmony_hear` shows them, finds the sevenths and leading tones that do
  not resolve. It does not judge applied chords.

`VoiceHeard` has the four counts and the number of rows. It is a hearing of the chord starts: what happens between
them, inside a chord, is not heard.

## Measured

`tests/test_voicing.c` voices I IV V I, I ii6 V7 I (C major) and i iv6 V i (A minor), a chord a bar, each with a given
and a written soprano, seeds 1 to 10: every result holds the rules, worked out in the test from the lanes in plain
semitones, and the same seed gives the same voicing. A soprano below the bass leaves the alto stuck.

`tests/test_voicing_heard.c` grows 30 themes for each mode (C major 4/4, G major 3/4 and D minor 4/4, seeds 1 to 10, a
sentence with a PAC) and runs `voice_hear` on each. Both are WILL_FAIL tests in `ctest`: they assert that all 30 are
clean, and they are not.

| | `--texture chorale` | `--voicing` |
| --- | --- | --- |
| Themes complete | 30 of 30 | 19 of 30 |
| Themes heard clean | 29 of 30 | 2 of 30 |
| Chords heard | 254 | 136 |
| Parallel fifths | 0 | 0 |
| Parallel octaves | 0 | 0 |
| Unresolved sevenths | 0 | 17 |
| Unresolved leading tones | 2 | 1 |

- **Chorale.** The one theme that is not clean (C major, seed 5) has two V6 chords whose bass, the leading tone, leaps
  up 13 semitones to the tonic: the bass operator chooses its octaves freely, and the voicing does not write the bass.
  Every voice the operator writes resolves.
- **Voicing.** The unresolved sevenths are in the grown melody: the melody operator knows nothing of the seventh of a
  V7, and a melody that falls from it by a leap, or holds it, leaves it unresolved in the soprano, which the voicing
  does not change. The 11 themes not complete are those in which no round of `grow_run` reached a voicing the rules
  allow. The bass operator may place the bass up to MIDI 60 and the melody low, and the three voices between are then
  squeezed to one or two voicings of a chord, with no path between them that avoids a parallel. The way out is in the
  melody and bass stages, not in the voicing.

Nothing is tuned to the analysis's answer, and nothing is skipped.
