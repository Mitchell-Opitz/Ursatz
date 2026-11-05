# The loop seam

`<ursatz_composer/seam_hear.h>`, `src/piece/seam.c`, `src/piece/seam_hear.c` and the seam stage of `compose_run`. A game
track loops: the last bar of the loop leads back into the first bar of the body (R-171). The loop's end is filled last,
against a Given copy of that first bar, and four soft rules say how well it leads in. Nothing is rejected for the seam. Back to the [docs index](README.md).

## The hold

`piece_structure_build_plan` marks the loop's end `hold` (`PLAN_CONSTRAINT_HOLD`, no fields): the last section of the
turnaround, or the last body section when there is no turnaround. The stages from structure to texture leave a held
section alone, as they leave a section that returns an earlier one alone, so it has no nodes and no lane spans until the
seam stage.

A section is not held when

- it is Given: a theme fill, or a progression fill that covers the whole section (a progression that stops short of the
  section's end is held; the seam stage writes the rest);
- it is, or holds, the body's first bar: the first body section. The copy is made from the written bar, and a section
  cannot be filled against itself. A body of one section has no held end; its seam is copied after the stages and heard,
  but nothing steers toward it.

After the loop's end the plan has the seam section (`seam`, `PLAN_CONSTRAINT_SEAM`, no fields): a node named `seam`, one
bar long, that starts where the loop ends and is empty and open. The material section starts on the first barline at or
after its end. The seam is not a theme section, so no stage targets it; it has the `key` constraint and nothing else.

## The seam stage

`COMPOSE_STAGE_SEAM` (name `seam`) follows the returns stage. It does not run when the plan has no seam section.

1. The body's first bar is copied into the seam section as Given: every written lane span of harmony, bass, melody and
   the inner voices that sounds in the bar from the first body section's start, cut to the bar, moved to the seam and
   transposed by the key difference (none: the piece has one key). It goes through `plan_give_section` from a plan of one
   section that holds the cut spans, so the seam ends Realized with Given spans. A seam that already has lane spans (a run
   on a plan that was composed before) is not copied again. A first bar that is not written leaves the seam empty
   (`seam_planned` false); the held sections are then filled without it.
2. The HOLD constraint is taken off the held sections, and structure, motives, harmony, bass, melody and texture run for
   them; the HOLD constraint goes back on. The stages are seeded seed + 12 to + 15 for harmony, bass, melody and texture
   and seed + 16 and + 17 for structure and motives. Their records are kept together under `records[COMPOSE_STAGE_SEAM]`,
   in the order the stages ran.

The harmony operators already read the Given chords that follow a target, so the held section's last chord joins the
copy's first. The melody operator already reads the first Given note that follows a target, so the last note leads to it by
a leap standard-counterpoint allows. Nothing in the stage is new for this: the seam is the Given material after the
target.

A stuck seam stage is retried as the melody stage is (up to six times, each from the next seed, the climax moved), and then
follows the round restart rule: the run is thrown away and starts again from the plan as given, with other seeds, up to
twelve rounds. The rejection says `seam`.

## The rules

`seam_hear` reads the plan; `seam_hear_notes` judges the notes the checkers and the plan reader both give it. `SeamHeard`
has one flag for each rule and `held`, how many hold.

| Rule | Holds when | Owner |
| --- | --- | --- |
| `progression` | the two chords keep the phrase model's order: no predominant after a dominant, no tonic after a predominant (staying on one function is not backward). IV to I is not held | standard-harmony `harmony_phrase_rules.h` (`NO_BACKWARD_MOTION`) |
| `no_parallels` | bass and melody make no parallel fifths or octaves across the seam | standard-counterpoint `harmonic_rules.h` |
| `step` | the melody move is one standard-counterpoint allows (no tritone, no seventh, a sixth only as the ascending minor one, at most an octave), and a leap over 7 semitones, into the seam or across it, is followed by a step the other way | standard-counterpoint `melodic_rules.h`; the answer to a leap has no framework function and stays in `seam_hear.c` |
| `leading_tone` | the melody on the leading tone of the chord before the seam goes up to the tonic when the chord after is a tonic (true when the rule does not apply) | standard-harmony `harmony_resolution_rules.h` |

A rule whose notes are missing holds, except the progression, which needs both chords.

The held sections' checkers add the rules a candidate keeps to its soft score when it ends at the seam: a harmony
candidate adds `progression`, a melody candidate adds the other three. Each rule adds 1 to the denominator and 1 to the
numerator when it holds. They are soft: no candidate is rejected for one.

After a complete run `ComposeOutcome` has `seam_planned` and `seam` (`SeamHeard`, read from the finished plan), and the
compose score adds 4 to its denominator and `seam.held` to its numerator.

## Rendering and hearing

The seam is not part of the piece. `plan_render` skips its spans with `RENDER_SKIP_SEAM`, gives it no section marker and
ends the piece where the loop does; an export has the loop and nothing after it. `return_hear`, the key and cadence checks
leave the seam section out as they leave the material out. `form_hear` is asked about a span, and `compose_run` does not
ask it about the seam.

## Measured

`tests/test_loop_seam.c` composes the structure of `piece-structure` (intro 2 bars, body 8 bars and 6 beats, turnaround
1 bar, 4/4, C major), seeds 1 to 20. All 20 completed. The seam section was the body's first bar, nothing sounded past
the loop's end, and the held end was written. The seam rules held:

| Rule | Seeds of 20 |
| --- | --- |
| progression | 19 |
| no parallels | 17 |
| step | 19 |
| leading tone | 12 |
| all four | 10 |

The test records 10 as a baseline and fails below it.

## Limits

- The copy is made once. Regenerating the body's first bar does not update the seam, and the loop's end stays filled
  against the old copy.
- A body pickup is not copied: a pickup is a constraint on the section (see [piece-structure.md](piece-structure.md#the-plan)),
  and no melody span of a piece structure starts before its section.
- The melody rules read the seam through the idea that ends at it. A melody target that is a whole phrase ending at the
  seam is not scored.
