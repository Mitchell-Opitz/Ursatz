# Two-part counterpoint

The counterpoint operator (`<ursatz_composer/counterpoint.h>`, `src/counterpoint/`) writes a counter-melody against a
given line in the first, second, third or fourth species of strict counterpoint. It is a Fill operator named
`counterpoint-species`, and `counterpoint_checker` holds its candidates to the rules of Ursatz's standard-counterpoint
framework. The composing logic is the Composer's; every rule it is held to is the framework's. Back to the [docs index](README.md).

Times inside the operator are `Rational` positions in whole notes from the start of the target. A bar is the meter's
beats over its beat unit, so a meter whose beat unit does not divide 128 (the old whole note grid) is accepted, and a bar is no
longer refused for not dividing into the slots of the species.

## The target

A target is a Constrained inner voice span, voice 1, with a `counterpoint` constraint on the span or its owner
(`{"kind": "counterpoint", "species": 1-4, "above": bool}`, see [plan-format.md](plan-format.md)) and a key.

- With `above` false the given line is the melody lane and the counter-melody sounds below it. With `above` true the given
  line is the bass lane, the cantus, and the counter-melody sounds above it.
- The given line is written, its notes follow one another from the start of the span to its end with no rest, and every
  note is in the key's scale. Anything else, the operator does not apply to; `counterpoint_why_not` says why in a phrase.
- Species 1 writes one note against each note of the line, whatever its rhythm.
- Species 2 to 4 need a cantus firmus: one whole-bar note for each bar, from a barline, three bars at the least. Species 2
  writes two notes a bar, species 3 four, species 4 syncopated half notes tied over the barline, after a half rest. The
  framework's third species is four notes to a bar, so species 3 needs a meter of four beats. The last bar is a whole note
  on the tonic.

## What is written

The pitches are the key's scale inside the range of a soprano (above) or a tenor (below), standard-voicing's ranges. The
scale of a minor key has the raised sixth and seventh (Dorian lines and the cadence need them). Take care with register:
the framework allows no more than a twelfth between the parts, so a cantus in the bass register leaves a counter-melody
little room to reach the leading tone. A cantus around octave 4 works in both positions.

## The search

The search is a beam over the slots of the counter-melody (`counterpoint_species.c`):

1. Children of a state are written note by note with rules that need no library: the pitches of the key, a consonance on a
   strong slot, a dissonance on a weak slot reached by step and left by a step or a third, no leap a strict counterpoint
   forbids (`standard_counterpoint_melodic_semitones_allowed`: the tritone is refused), no pitch more than a twelfth from the line, and for the downbeats the look-back rules of the species.
2. The children are ordered by cost, ties by the run's rng. The cost is the semitones the leaps span over 2, a repeated
   note, a broken bar in species 4, and a run of three parallel imperfect consonances (`standard_counterpoint_imperfect_run`).
3. The cheapest are asked of the library (`counterpoint_eval`) until the beam is full. A line that is not finished is asked
   the rules that hold for a prefix: the last bar stands for a whole note, and the rules of the ending and the final step
   are asked of the finished line. The first pass keeps at most a share of the beam for each last note, so that the notes
   the ending needs are not crowded out by cheaper ones.
4. A complete line is kept only when the whole check holds.

Species 2 and 3 add the weak slots of a bar with the downbeat after it, so a dissonance is judged with the note it resolves
to. Species 4 adds a bar: the downbeat is the weak note before it tied over (a dissonant one is a suspension that resolves
by a step down) or, with a cost, a new note, a bar in which species is broken.

## The check

`counterpoint_checker` rejects a candidate with a hard violation:

- Species 1: `standard_counterpoint_check` on the pair (the line the cantus firmus in the part `above` says, the tonic from
  the key, the default limits) and the first species check.
- Species 2 to 4: the species' own exercise check (`standard_counterpoint_second_species_check` and the third and fourth).
  The pair check compares note with note and does not fit more than one note to a bar; the species check runs the common
  spacing rules on every note, so it takes its place.

The group of the given line itself (the cantus firmus rules) is not judged: a line to write against need not be a model
cantus. The soft score is the share of soft groups with no violation. `counterpoint_report` gives the verdict group by
group for a written target.

## Out of scope

Fifth species, invertible counterpoint and imitation. A counter-melody inside a grown theme or a piece is not planned; the operator applies to any plan that has the target above.

## Measured

The tests (`test_counterpoint_species_1` to `_4`) write Fux's D Dorian cantus and a C major cantus, above and below, seeds
1 to 10, with a beam of 48, and every result passes the framework's checks. The default beam is 128.
