# The melody operator

A Fill operator for the melody lane. Given a written harmony lane, a written bass lane and a motive, it
writes the melody over them: the motive's rhythm and contour, chord tones on the strong beats, and
passing and neighbor tones between. It is a library operator (`melody_fill_register`); the CLI has no
command for it.

Header: `<ursatz_composer/melody_fill.h>`.

## The target

A Constrained melody lane span (voice 0) that stands for the phrase, holding one placeholder note. It
must:

- cover exactly the chord spans of the harmony lane, none of them open, with bass spans that tile it;
- lie inside one meter lane span and have a key lane span or key constraint over it;
- carry a `motive_ref` constraint, on the span or its owner, naming a motive node whose melody lane span
  is Given or Realized.

The motive is the **original** when that melody span is Given and a **development** (`motive_develop.h`)
when it is Realized. The candidate's label begins `original` or `developed`, so the fill record says
which. The loop never offers a Given span, so Given melody is never overwritten; the harmony and bass
lanes are never touched. The result is one Realized span over the phrase.

## Idea targets

A melody span whose owner is an **idea** or a **subphrase** stands for that part of a phrase, not for the
whole of it. The enclosing phrase node's span is the phrase. A phrase is then written idea by idea:

- **Time order.** The operator applies to an idea target only when every melody span of the same phrase
  before it is written. A later idea is refused (`FILL_STUCK_NO_OPERATOR` for that step) until the ones before
  it are there.
- **Chords and bass.** The harmony and bass lanes are read over the target only: the chord and bass spans that
  overlap it, the first and the last clipped to it. A chord may begin before the idea or end after it, and an
  idea may begin inside a cell (a half bar of 3/4); the strong beats are the cell lines of the meter, not the
  idea's own start.
- **Joins.** The last notes of the written melody span right before the target are read with the target's
  notes as one line by standard-counterpoint's melodic rules: no forbidden leap or outline across the join,
  and the first note is a leap the rules allow from the last. A leap into the target is answered by a step
  the other way, where the chord allows.
- **Climax.** The climax position (`climax_position`, thousandths of the phrase) falls in one target. That
  target writes the phrase's single highest note, higher than every melody note before it in the phrase
  (a hard rule); a target before it keeps every note at or below `range_high - 3`, and stays in the lower half
  of the range where its motive allows; a target after it stays below the climax note.
- **End tone.** The end tone rule applies only to a target with a cadence constraint, on the span or its
  owner. A target without one has no end-tone rule, and its last note needs only to be a chord tone.
- **Exact restatement.** A target whose `motive_ref` is `restated` takes the whole melody of the written span it
  restates for its motive and says it again, choosing the nearest chord tone each time. The span restated is the
  latest written one, before the target and as long, that asks for the motive the target's own motive is a
  restatement of (a basic idea, however far back, as a consequent restates its antecedent's); failing that, the one
  right before the target if it is as long. Two ideas of a presentation made so are heard by standard-form as a
  repetition.

## What it writes

- **Placement.** The motive starts at `start_cell`. Each note keeps its interval to the note before
  within `tolerance` semitones. The first note is the chord tone nearest the motive's first pitch: that
  pitch itself when it is a chord tone of the first chord, a transposition otherwise.
- **Strong beats** (the start of a cell, where a chord may start) carry chord tones, as do notes as long
  as a cell and notes held into the next chord.
- **Between chord tones**, notes are chord tones or non-chord tones of the figures in `nct_types`,
  approached and left as the NonChordTone framework defines them. After the motive the rhythm is the
  motive's again (`density` 0) or an even subdivision of the cell.
- **Contour.** One climax, `climax_position` thousandths of the way through, approached from below and
  left downward; no other note is as high. Leaps of a fourth or more are followed by a step the other way
  where the chord allows. Notes stay inside the range.
- **Cadence.** The last note is the cadence's stable tone: the tonic for a PAC or PC, the third of the
  tonic for an IAC (the soprano the harmony operators' recognizer is told).

A motive that cannot be placed (too long for the phrase, wider than the range, no chord tone near enough
to its contour) leaves the span open: the loop reports `FILL_STUCK_ALL_REJECTED`.

## Parameters

`MelodyFillParams`, passed to `melody_fill_register` (the registry keeps the pointer). The defaults are
`melody_fill_params_default()`.

| Field | Default | Meaning |
| --- | --- | --- |
| `tolerance` | 2 | Semitones an interval of the placed motive may differ from the motive's own. |
| `range_low`, `range_high` | 60, 79 | MIDI range of the melody (C4 to G5), both included. |
| `start_cell` | 0 | Cell of the phrase the motive starts on; the time before it is a rest. |
| `nct_types` | passing, neighbor | `MELODY_NCT_` bits: `PASSING`, `NEIGHBOR`, `APPOGGIATURA`, `SUSPENSION`, `ANTICIPATION`. 0 writes chord tones only. |
| `density` | 0 | Notes to a cell after the motive; 0 repeats the motive's rhythm. |
| `climax_position` | 667 | Where the climax falls, in thousandths of the phrase. |
| `plain_shaping` | false | True leaves out the four shaping rules below, for tests and comparisons; there is no CLI option. |

Appoggiaturas, suspensions and anticipations are off unless asked for. Appoggiaturas and anticipations
are written on weak beats; a suspension is a chord tone held into a chord it is not a tone of.

## The checker

`melody_fill_checker` goes in `FillConfig.checker`. It calls the frameworks and does not write their
rules again. Hard rules reject a candidate; soft rules give the score, the share that hold.

| Framework | Hard | Soft |
| --- | --- | --- |
| NonChordTone (common-practice-minimal's labeler) | A melody note outside its chord that is no allowed figure | |
| standard-counterpoint, melodic | Forbidden leaps and outlines | Repeated notes, leap density, pitch occurrences, climax rules |
| standard-counterpoint, harmonic (melody over bass) | Voice crossing, parallel fifths and octaves, a dissonance against the bass outside a non-chord tone or the chord's seventh | Direct perfects, interior unisons, imperfect consonances on strong beats |
| standard-harmony | The leading tone and a chord's seventh resolve from chord to chord | |

The strict-species rules for the first interval, the last interval, the largest gap and the last note
approached by step are not asked of a tonal melody. The one climax, its place, recovered leaps and few
repeated notes also score.

### The shaping rules

Four more soft rules, from Schoenberg's *Fundamentals of Musical Composition* (R-164). Each is one rule that holds or
not, never a rejection, and is on for every caller of the checker (grow, fill, regenerate, variations) unless
`plain_shaping` is set. They read the candidate's own notes, so an idea reads its own and not its phrase.

| Rule (chapter) | Holds when |
| --- | --- |
| `ending-reduction` (II) | An idea that ends the phrase in a cadence has no more attacks per beat than the mean of the phrase before it, and ends on its longest note. Any other target holds. |
| `waves` (XI) | A candidate of 8 or more notes has at least 2 local high points (a note higher than the notes either side, repeated notes read as one) at or before its climax. |
| `central-range` (XI) | At least 3/4 of the notes lie within 6 semitones of the median pitch. |
| `monotony` (XII) | No three consecutive bars of two or more notes have the same rhythm and the same intervals. |

`melody_check_lane(plan, span, key, params, &violation)` runs the same rules on a melody lane read from
the plan, for tests and tools.
