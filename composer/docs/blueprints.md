# Blueprints

A theme written as a blueprint (R-736): harmony, rhythm per bar, pitch anchors, chosen non-chord tones, a climax, each fixed or left open, that the Composer realizes. This page holds the theme request's `blueprint` member (R-743): what it gives, what it refuses and how it becomes the theme's plan; then what the melody stage does with a blueprint's melody (R-746): the plan constraints `anchor`, `event` and `climax`, rhythm-only idea motives, and the blueprint motive. Back to the [docs index](README.md).

## The blueprint member

A theme request ([theme-request.md](theme-request.md)) with `blueprint` (API 4.8) gives the theme bar by bar. Its `motives` and `slots` are optional; only `slots.basic` is read beside it. The members:

| Member | What it is |
| --- | --- |
| `bars` | One object a bar, exactly the theme's `bars` (1 to 16), bar 0 first. |
| `climax` | `{"bar", "at", "pitch"}`: the theme's one highest note, its bar from 0, its onset and its pitch (`"Ab5"`). |
| `accompaniment` | `{"pattern", "voicing"}` as the request's own `accompaniment` reads it (one of the four names, `any`, or a K11 accompaniment id; a voicing with `block`); not `motive`. |

A bar's members, each left out when it is open:

| Member | What it is |
| --- | --- |
| `chords` | The bar's chord text (R-744), as an entry of the request's `chords`: `"I"`, `"II6 V7@2"`. |
| `melody` | A note list of the bar (`"D5:q. C5:e r:h"`), rests allowed, a rest at the end too: Given melody. |
| `rhythm` | A rhythm of the bar (`"x:q. x:e x:h"`, `r` rests, `~` ties). |
| `job` | A rhythm job (below), or the id of a K11 rhythm entry of the theme's meter (its rhythm). |
| `anchors` | Up to 8 `{"at", "pitch"}`: the note starting there takes the pitch. |
| `events` | Up to 4 `{"at", "type"}`, type one of `appoggiatura`, `suspension`, `passing`, `neighbor`, `escape`, `anticipation`, `retardation`. |
| `bass` | A note list of the bar: Given bass. |
| `inner` | Up to 8 note lists, inner voice 1 first: Given inner voices. A voice of rests in a bar gives nothing there. |

`at` is an onset from the bar's start in beats of the beat unit, as a chord onset writes it: `"0"`, `"2"`, `"3/2"` (a whole number is read too); in 6/8 a beat is an eighth. Every note list and rhythm fills its bar exactly, read with the note-list reader. Messages count bars from 0, as the paths do.

### The jobs

Drawn when the plan is built (R-745), with one random stream seeded with the run's seed + 9, bar by bar, before the basic motive:

| Job | The bar's rhythm |
| --- | --- |
| `running`, `long`, `syncopated`, `cadence` | A K11 rhythm cell of the meter tagged with the job; refused when there is none ("no syncopated rhythm in 3/4"). |
| `motif` | The basic idea's bar at the same place in its idea. |
| `variant` | That rhythm with one note split in two equal halves or two neighbours joined, one draw among all of them. |
| `contrast` | A K11 cell of the meter whose rhythm similarity (`similarity_rhythm_score`) to that bar is at most 1/2, drawn; the least similar when none is. |
| `liquidation` | The rhythm of the bar before. |

A bar whose idea restates the basic motive (the repetition, the consequent's basic idea) and gives nothing has the basic idea's bar at its place as its rhythm, for a later job and for its anchors.

### Refusals

When the request is read (`URSATZ_ERR_REQUEST`, the path of the member):

| Path | Refused |
| --- | --- |
| `/chords`, `/accompaniment`, `/ideas` | Beside a blueprint: "a blueprint gives its chords (accompaniment, idea motives) per bar". |
| `/slots/<slot>` | A slot other than `basic` ("only slots.basic is read beside it"). |
| `/blueprint/bars` | A bar count other than the theme's ("the blueprint has 7 bars; the theme has 8"). |
| `/blueprint/bars/<n>/rhythm`, `.../job` | Two of melody, rhythm and job in a bar ("bar 0 gives a melody and a rhythm"). |
| `.../anchors`, `.../events` | Anchors or events in a bar that gives its melody. |
| `.../job` | A word that is no job and no K11 rhythm id; a rhythm of another meter; `motif`, `variant` or `contrast` in the basic idea; `liquidation` in bar 0. |
| `.../melody`, `.../rhythm`, `.../bass`, `.../inner/<k>` | A text that does not read, ends on a tie or does not fill the bar ("bar 7 rhythm: the notes fill 2 beats; a bar has 4"); a melody, rhythm or bass of rests only. |
| `/blueprint/bars/<n>` | An idea (`grow_layout_of`) whose bars mix a melody and none, or a rhythm or job and none ("bars 2 and 3 are one idea: give a melody in both or neither"); a basic idea with neither and no `slots.basic` (with it, its bars give none of them); a rhythm or job in a construction's extension; a bass in some bars only. |
| `.../bass` | A bass that rests in a bar whose melody is composed. |
| `.../inner` | Inner voice counts that differ; inner voices with `blueprint.accompaniment`. |
| `/blueprint/climax/...` | A bar, onset or pitch that does not read. |

When the plan is built (`URSATZ_ERR_UNSATISFIABLE`): a job with no cell; `motif`, `variant` or `contrast` with no basic idea bar at its place, `liquidation` after a bar with no rhythm of its own; an anchor or event on no note onset of its bar's melody, rhythm, job or restated basic motive ("bar 1 anchor at 1: no note starts there"); a basic idea drawn from a rhythm with no chords ("a basic idea drawn from its rhythm needs its chords") or that the blueprint motive refuses; the climax on no note onset, a Given note at its onset that is not its pitch, or another Given note or an anchor at or above it ("the climax G5 is not the theme's highest note: bar 0 has G5"); a chord text, a melody or a bass the plan does not take.

### How it becomes the plan

1. The chords of each bar are the theme's Given chords, read as the request's `chords` are; a cadence bar is checked as theirs is.
2. The basic motive (m0) is `slots.basic`'s; else the basic idea's melody as a full motive; else drawn with the blueprint motive from its bars' rhythms, chords, anchors and events (and the climax, as an anchor, when it lies there), the range the melody's default and below the climax.
3. Each melody bar is Given (`plan_give_melody`, a span a bar); an idea whose bars are all given opens no melody target. An idea after the basic idea whose bars give rhythms or jobs restates them as a rhythm-only motive (its material node `i<k>`, `composer_rhythm_motive_attach`), rests kept. A repetition or a consequent basic idea with nothing of its own restates the basic motive as today.
4. Each bass bar is Given (`plan_give_bass`, or `plan_give_bass_with_rests` under a Given melody); each inner voice k of a bar with a note is Given as inner voice k + 1 (`plan_give_inner_voice`).
5. `blueprint.accompaniment` is the theme's accompaniment.
6. The structure stage draws its spec as always; once it has made the melody spans, each anchor, event and the climax becomes an `anchor`, `event` or `climax` constraint on the melody span that holds its onset, at its offset from the span's start.

Without `blueprint` every plan is as before. The response has the same shape.

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

A motive with a rhythm and no pitches (R-120's rhythm form) is attached with `composer_rhythm_motive_attach`: a Given motive node whose `motive_tones` label every note `rhythm` (its harmony is the motive's own chords, else one `I`, and is not read as the idea's chords). An idea whose motive is rhythm-only is realized by the melody stage: its notes are laid out with the motive's onsets and durations as notes of the stage's own (no motive link), placed as generated notes are, and nothing is filled after them: a rest that ends the motive stays a rest (a blueprint's rhythm is its idea's whole rhythm). Its rhythm is kept: the closure settle, the held cadence and the rhythm repetition cap leave it alone. It waives no melodic rule, and it is tried at one shift.

## The blueprint motive

`composer_blueprint_motive` (`motive_blueprint.c`) draws a full motive from a `BlueprintMotiveRequest`: the key and meter, the notes' onsets and durations (a rhythm, rests as gaps), the chords by onset (as a motive's own harmony), up to 32 anchors and 16 events (each at a note's onset), and the melody range.

1. The anchors take their pitch.
2. Each other note in order, not an event: the chord tone of the chord at its onset nearest the note placed before it (the first: nearest the middle of the range), a step where a chord tone lies a step away, never the previous pitch, inside the range; a leap of a fourth or more at most once (a second only when no nearer chord tone is left), followed by a step the other way where a chord tone is there; ties drawn from the run's random numbers. A note right after an event with an anchor's pitch takes the event's resolution when that is a chord tone in range: a scale step below an appoggiatura or a suspension, above a retardation, the anticipated pitch, the note before a neighbor, a step on through a passing tone.
3. Each event without an anchor's pitch, by the rule above.

Every note carries a tone label (`motive_tones`): the event's type, else `chord` for a chord tone. Refusals (`MOTIVE_ERR_VALUE`, the reason in the error): no notes or more than 64, overlapping notes, no chords ("a blueprint motive drawn from its rhythm needs its chords"), an anchor or event where no note starts ("anchor at 1/4: no note starts there"), a note with no chord tone in range ("note 0 at 0: no chord tone of I in 61 to 63"), an event that falls outside the range.

The theme request draws it for a basic idea whose bars give a rhythm or a job ([the plan](#how-it-becomes-the-plan)).
