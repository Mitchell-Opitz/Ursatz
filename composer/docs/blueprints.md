# Blueprints

A theme written as a blueprint (R-736): harmony, rhythm per bar, pitch anchors, chosen non-chord tones, a climax, each fixed or left open, that the Composer realizes. This page holds what the melody stage does with a blueprint's melody (R-746): the plan constraints `anchor`, `event` and `climax`, rhythm-only idea motives, and the blueprint motive. Back to the [docs index](README.md).

## The melody constraints

Each is a constraint on a melody lane span ([plan-constraints.md](plan-constraints.md)), its `offset` from the span's start. The melody operator ([melody-fill.md](melody-fill.md)) reads the span's anchors and events (up to 64 and 32; more leave the span open) and the plan's climax, wherever it is. One in a pickup is not read.

| Constraint | What the melody stage does |
| --- | --- |
| `anchor` | The note that starts at its offset takes its pitch, a generated note or a motive's (a realized motive note at the offset is moved to it). |
| `event` | The note that starts at its offset is that non-chord tone: it is placed after the notes around it, by the rule below. It is exempt from the strong-beat chord-tone rule and from the checker's figure rules (its label and its type are not asked of it). The notes right before and after it are anchors, so it sits between two placed notes. |
| `climax` | The note that starts at its offset takes its pitch and is the contour's climax (the contour takes that note and pitch in place of its own choice). No other note of any melody span reaches it: the targets before and after it keep their generated notes below it, and the checker refuses a candidate with another note at or above it. Where the plan has a climax, a target's place in its phrase's arch (before, at or after the climax) is read from it, not from `climax_position`. |

An anchor and an event at one offset: the note takes the anchor's pitch and is the event (exempt as above; its pitch is not placed by the rule). A climax and an anchor at one offset: the climax's pitch wins.

The checker refuses a candidate whose anchor or climax has no note starting there ("anchor without a note", "climax without a note"), whose note there is off its pitch ("anchor off its pitch", "climax off its pitch"), or with another note at or above the climax ("a note at or above the climax"). These rules cannot be waived. Without the three constraints every melody is as before.

### The order of placement

1. The motive's notes are realized (a rhythm-only motive has none: see below); those at an anchor or the climax take its pitch.
2. The anchors after them in order: a note with an anchor's or the climax's pitch takes it; the others take the chord tone the arch asks for, as before. The first note, when nothing is placed before it, starts from the last note of the melody before the target, else from the middle of the range.
3. The notes between two anchors are filled as before; a run that holds events is left to the last step.
4. Each event without an anchor's pitch takes its pitch from the note before it (or the melody before the target) and the next note that is not such an event (or the melody after it). An event on the first or the last note is placed as an anchor first, then moved to its pitch.

### Where an event sounds

`composer_event_pitch` (`motive_blueprint.h`) places an event between `previous` and `next`, in the key's natural scale with the two neighbours' pitch classes added (so a raised leading tone steps as it should):

| Type | Pitch |
| --- | --- |
| `appoggiatura` | A scale step above `next`; below it when `previous` is a step (1 or 2 semitones) below `next`. |
| `suspension`, `retardation` | `previous`. |
| `passing` | The scale step between, when the neighbours are a third apart (3 or 4 semitones); else the scale tone nearest their middle, the lower on a tie. |
| `neighbor` | A scale step above `previous`; below when `next` is above `previous`. |
| `escape` | A scale step from `previous` away from `next` (up when they are the same). |
| `anticipation` | `next`. |

A missing neighbour takes the other's pitch. In C major: an appoggiatura before E4 is F4, a suspension after D5 is D5, a passing tone between C4 and E4 is D4, the neighbor of G4 returning to it is A4, the anticipation of C5 is C5.

## Rhythm-only idea motives

A motive with a rhythm and no pitches (R-120's rhythm form) is attached with `composer_rhythm_motive_attach`: a Given motive node whose `motive_tones` label every note `rhythm` (its harmony is the motive's own chords, else one `I`, and is not read as the idea's chords). An idea whose motive is rhythm-only is realized by the melody stage: its notes are laid out with the motive's onsets and durations as notes of the stage's own (no motive link), placed as generated notes are, and nothing is filled after them when the motive is as long as the span. Its rhythm is kept: the closure settle, the held cadence and the rhythm repetition cap leave it alone. It waives no melodic rule, and it is tried at one shift.

## The blueprint motive

`composer_blueprint_motive` (`motive_blueprint.c`) draws a full motive from a `BlueprintMotiveRequest`: the key and meter, the notes' onsets and durations (a rhythm, rests as gaps), the chords by onset (as a motive's own harmony), up to 32 anchors and 16 events (each at a note's onset), and the melody range.

1. The anchors take their pitch.
2. Each other note in order, not an event: the chord tone of the chord at its onset nearest the note placed before it (the first: nearest the middle of the range), a step where a chord tone lies a step away, never the previous pitch, inside the range; a leap of a fourth or more at most once (a second only when no nearer chord tone is left), followed by a step the other way where a chord tone is there; ties drawn from the run's random numbers. A note right after an event with an anchor's pitch takes the event's resolution when that is a chord tone in range: a scale step below an appoggiatura or a suspension, above a retardation, the anticipated pitch, the note before a neighbor, a step on through a passing tone.
3. Each event without an anchor's pitch, by the rule above.

Every note carries a tone label (`motive_tones`): the event's type, else `chord` for a chord tone. Refusals (`MOTIVE_ERR_VALUE`, the reason in the error): no notes or more than 64, overlapping notes, no chords ("a blueprint motive drawn from its rhythm needs its chords"), an anchor or event where no note starts ("anchor at 1/4: no note starts there"), a note with no chord tone in range ("note 0 at 0: no chord tone of I in 61 to 63"), an event that falls outside the range.

The theme request builds these from its `blueprint` member (P69b B6).
