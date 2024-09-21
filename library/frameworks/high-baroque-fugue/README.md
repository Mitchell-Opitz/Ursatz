# high-baroque-fugue

Theory Framework instance `high-baroque-fugue-v1`: the fugal exposition of the
high Baroque (Bach, Handel, and their contemporaries). Path
`frameworks/high-baroque-fugue/`, tests in
`frameworks/high-baroque-fugue/tests/`. A Framework extension module
physically separate from `src/theory/`, the same shape as
`frameworks/standard-counterpoint/`.

The dependency on `standard-counterpoint` is one way: this module reads its
pitch arithmetic, intervals, and slot types
(`standard-counterpoint/counterpoint_pitch.h`, `counterpoint_interval.h`,
`counterpoint_slots.h`), and that framework knows nothing of this one. File
names carry a `fugue_` prefix so they never collide with the
`standard-counterpoint` headers on the include path.

## Scope

The subject of every rule is `FugueExposition` (`fugue_exposition.h`): the
voices of a fugue, listed from the highest to the lowest (soprano, alto,
tenor, bass in four voices), each a slotted line (attacks, ties, and rests) on
one common grid; the statements the caller identifies in them, each a voice,
a start slot, a `FugueRole` (`fugue_role.h`), and a number of
notes; the number of slots in a bar; the tonic; whether the countersubject is
declared invertible; and the limits behind the soft rules.

Every rule has a Rule/Constraint pair referencing this Framework plus a pure
check function, and each group has a check that reports the rules broken as
hard and soft bit masks. A rule that states what the style requires is hard;
"limit", "avoid", and "prefer" rules are soft. `constraint_evaluate` is never
called here.

The entries are the statements with the role subject or answer, in the order
they begin; entries beyond the number of voices are redundant entries
(`fugue_exposition_redundant_entries`). In a four-voice exposition
`fugue_voice_part` gives a voice its `VoicePart`.

- **Entries** (`fugue_entry_rules.h`). Hard: the first entry is the subject,
  the first round of entries (one for each voice) alternates subject and
  answer, every voice enters before any enters again, and every entry begins at
  the same position in the bar. Soft: a new voice enters as the highest or
  lowest voice so far (an inner part is uncommon as the last to enter), and the
  voice of each entry continues with a countersubject that begins where the
  entry ends and no later than the next entry.
- **Answer** (`fugue_answer_rules.h`), each answer against the first subject.
  Hard: the answer begins a perfect fifth or fourth from the subject; a subject
  on the tonic is answered on the dominant, and a subject on sol is answered
  on do, not re; the answer is real or tonal (`ImitationAnswer`,
  `standard-counterpoint/imitation_answer.h`; `fugue_answer_kind`), never free; and it
  keeps the subject's rhythm. Soft: it is altered as little as possible, with
  at most `max_altered_intervals` (default 2, in `FugueLimits`) intervals of a
  different generic size.
- **Countersubject** (`fugue_countersubject_rules.h`). The statements of one
  countersubject share a number. A countersubject accompanies the first entry in
  another voice that begins at or after its start. Hard: every statement of a
  countersubject keeps the rhythm of its first. Soft: when declared invertible
  at the octave, only unisons or octaves, thirds or sixths, and seconds or
  sevenths where either voice begins a note (no fifths, which invert to
  fourths, and so no 4-3 suspensions; 7-6 and 2-3 remain); one voice moves
  while the other is stationary (no more than half the attacks coincide); and a
  later statement differs from the first in no more intervals than the answer
  differs from the subject (a count, not a check of where the changes fall).
- **Aggregate check** (`fugue_check`, `fugue_valid`): the entries, answer, and
  countersubject groups over an exposition, reported as hard and soft bit masks
  indexed by `FugueGroup`. An exposition is valid when no hard rule is broken.

## Not covered

- **Tonal answers for other reasons.** A V-I gesture at the start of the
  subject other than starting on sol, and a modulating subject (split in two
  phrases, one in each key), need harmony or key regions; only the sol start is
  enforced.
- **Links and episodes.** A link or episode is a statement role
  (`FugueRole`), but nothing checks that it is built on a motive of the
  subject, or what it is for (changing harmony, register, phrase length, or
  dovetailing the entries).
- **Free counterpoint.** The part that does not recur is not constrained
  beyond the general rules of counterpoint.
- **General matters.** The control of rhythmic flow, the grading of cadences,
  and idiomatic writing for an instrument.
- **Where the changes fall.** That a countersubject changes exactly where the
  answer does; only the number of changes is compared.
- **Imitation over a repeating bass.** The dux and comes imitation of a ground
  bass is `frameworks/baroque-ground-bass`.
- **Finding the statements.** The caller says where each subject, answer, and
  countersubject is.

## Vocabulary enums

Owned here (moved from `src/semantics/`; type names and values unchanged):

- `FugueRole` is a plain enum value naming the part a passage of one voice plays in a fugue: subject (the main theme), answer (a transposition of it, sometimes altered), countersubject (a line sounded with the subject or answer), free counterpoint (an accompanying part that need not recur), link (a short interpolation between entries), or episode (a longer passage between statements). `fugue_role_is_entry` is true for the subject and the answer. It carries no `EntityID`, pitches, or framework claim. Which roles appear where is a convention of a framework (`frameworks/high-baroque-fugue`).
