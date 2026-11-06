# The melody operator

A Fill operator for the melody lane. Given a written harmony lane, a written bass lane and a motive, it
writes the melody over them in two jobs (R-326): it **realizes** the motive, whose notes it keeps and bends only as the chords ask,
and it **generates** the notes after the motive, chord tones on the strong beats with passing and neighbor tones between. It is a library operator (`melody_fill_register`); the CLI has no
command for it. Back to the [docs index](README.md).

Header: `<ursatz_composer/melody_fill.h>`.

Times inside the operator are `Rational` positions in whole notes from the start of the target, not ticks, so the
operator reads a span whose times are not a multiple of any grid. The shortest note it writes is a 128th: a `density`
that would divide a cell into shorter notes keeps the cell.

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
  the other way, where the chord allows. The moves the Composer draws are judged by
  `standard_counterpoint_melodic_semitones_allowed` and `standard_counterpoint_is_step`: the tritone is refused, as is a
  seventh, and a sixth is allowed only as the ascending minor one.
- **Climax.** The climax position (`climax_position`, thousandths of the phrase) falls in one target. That
  target writes the phrase's single highest note, higher than every melody note before it in the phrase
  (a hard rule where the target generates notes, strict only); a target before it keeps its generated notes at or below `range_high - 3`, and stays in the lower half
  of the range where its motive allows; a target after it keeps its generated notes below the climax note.
- **End tone.** The end tone rule applies only to a target with a cadence constraint, on the span or its
  owner. A target without one has no end-tone rule, and its last note needs only to be a chord tone.
- **Exact restatement.** A target whose `motive_ref` is `restated` takes the whole melody of the written span it
  restates for its motive and says it again, choosing the nearest chord tone each time. The span restated is the
  latest written one, before the target and as long, that asks for the motive the target's own motive is a
  restatement of (a basic idea, however far back, as a consequent restates its antecedent's); failing that, the one
  right before the target if it is as long. Two ideas of a presentation made so are heard by standard-form as a
  repetition.

## What it writes

- **Realize** (`melody_realize`, no random numbers). The motive starts at `start_cell`, keeping its rhythm. Each note is aimed at
  its own pitch moved by one octave for the whole idea (the shift: of 0, -12, 12, -24, 24 those that put every note in the range,
  ranked: for the phrase's climax target the top at or up to 2 semitones above the phrase's top first, then the smaller shift,
  then the lower; R-334). In order: a note that is a tone of every chord it sounds through stays; every other note is labeled by
  the NonChordTone framework (`melody_nct_types`) and a clash it classifies as a figure of any type, escape tones and retardations
  included, is kept and its type allowed in the melody (R-336); an unclassified one moves, first to a pitch within 2 semitones that
  re-forms its own tone role, else to the nearest tone of its chords (ties: the pitch whose intervals to its neighbours are nearest
  the motive's, then the one farther from the note before); labeling and moving repeat until nothing moves; the last note of an idea
  with a cadence moves to the nearest cadence tone. A note moves only for its chords (R-338): never for a melodic, bass or climax
  rule. The octave and the figures are chosen per attempt: of 48 attempts the shifts cycle, the first 24 keep labeled figures and
  the last 24 move every clash.
- **Generate.** The notes after the motive are placed as before: the anchors, then the notes between, under the arch below.
- **Strong beats** (the start of a cell, where a chord may start) carry chord tones, as do notes as long
  as a cell and notes held into the next chord.
- **Between chord tones**, notes are chord tones or non-chord tones of the figures in `nct_types`,
  approached and left as the NonChordTone framework defines them. After the motive the rhythm is the
  motive's again (`density` 0) or an even subdivision of the cell.
- **Contour.** The generated notes make one climax, `climax_position` thousandths of the way through, approached from below and
  left downward, with no other generated note as high; the realized notes keep their pitches anywhere in the range. Leaps of a
  fourth or more are followed by a step the other way where the chord allows. Notes stay inside the range.
- **Cadence.** The last note is the cadence's stable tone: the tonic for a PAC or PC, the third of the
  tonic for an IAC (the soprano the harmony operators' recognizer is told).

## Tone roles

A motive made from a request carries its **tones** in the plan (`motive_tones` on its node, [plan-constraints.md](plan-constraints.md#constraints)):
the role of every note against the motive's base harmony, its own harmony or the one proposed from the notes
(`ursatz-composer motive` shows them; the request's `tone_overrides` change them, [theme-request.md](theme-request.md#motives)).
A developed motive (a `motive_ref` that keeps the notes: transposed, inverted, augmented, diminished, restated, interval changed;
a retrograde reads the labels in reverse) takes the tones of the motive it was made from. A motive whose tones are not there, or
whose label count is not its note count, is placed as it always was. An exact restatement keeps the tones of the notes of the
motive; the notes after them have none.

The tones name the figure a clash is moved to: a note whose role is a non-chord tone type moves first to a pitch within 2 semitones
that the labeler gives that same type, else to a chord tone. A figure the labeler classifies is kept whatever its role. A motive
whose tones are not there is bent by the labeler alone.

## Strict and relaxed (R-338)

A motive's notes are fixed, so the generated notes may have no pitch the strict rules allow. `melody_generate` runs its 48 attempts
strict; when none passes and the target has motive notes it runs 48 more **relaxed** (`MelodyCandidate.relaxed`), never otherwise.
A relaxed melody is held to the melodic rules only over its generated runs (each run with the realized note either side, by the
framework's check); the other melodic rules, parallel fifths and octaves against the bass, a chord tone dissonant over the bass, an
unresolved leading tone or seventh and a climax not above the phrase before it score with the soft rules; voice crossing, figures,
strong-beat chord tones and the cadence tone stay hard. Its generated notes fall back past the arch and the bass checks when no
option passes, and the fill search starts at the nearest motive note and tries again without the bass checks.
`melody_check_lane` judges a written lane with motive notes strict, then relaxed. Realized notes are not held to the rules the motive
itself breaks (`motive_waived`), nor is the join to the idea before or after it.

## Spans on beats

The melody reads the same cell grid as the harmony (`harmony_cells_of_span`): the places a chord may start are
anchored to the barlines of the meter span, and a span that starts or ends between two of them has part cells at
its ends. Strong beats are the barline grid's, so in 4/4 a span starting on beat 3 starts on a strong beat and one
starting on beat 2 does not. Chord starts are read from the cell list, part cells included. On a span that does not
run from a barline over whole bars the climax stays in the motive when no chord tone is within a fifth above the
last anchor of the motive, since few anchors follow it. Whole-bar spans are written as before.

## Pickups

A `pickup` constraint (see [plan-format.md](plan-format.md)) on a melody span makes its first `length` an
anacrusis. The phrase, the harmony and the key are read from the end of the pickup. The pickup is written as 1 to 4
notes of equal beats (the first takes what is left over when there are more than four): the last is a step from the
first downbeat note or a chord tone of the chord before (the harmony span sounding under the pickup; none at the
start of a plan, where it is a step), and the others come by step toward it. The pickup is held to the range and
the melodic rules only, never to the chord-tone rule. A pickup of a bar or more is not one the operator applies to.
A pickup takes no accompaniment.

A motive that cannot be placed (too long for the phrase, wider than the range, no chord tone near enough
to its contour) leaves the span open: the loop reports `FILL_STUCK_ALL_REJECTED`.

## Parameters

`MelodyFillParams`, passed to `melody_fill_register` (the registry keeps the pointer). The defaults are
`melody_fill_params_default()`.

| Field | Default | Meaning |
| --- | --- | --- |
| `range_low`, `range_high` | 60, 84 | MIDI range of the melody voice (C4 to C6), both included (R-327). |
| `start_cell` | 0 | Cell of the phrase the motive starts on; the time before it is a rest. |
| `nct_types` | passing, neighbor | `MELODY_NCT_` bits: `PASSING`, `NEIGHBOR`, `APPOGGIATURA`, `SUSPENSION`, `ANTICIPATION`; a realized motive adds `RETARDATION`, `ESCAPE`, `DOUBLE_NEIGHBOR`, `CHANGING_TONE` for its own figures. 0 writes chord tones only. |
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
