# sixteenth-century-style

Theory Framework instance `sixteenth-century-style-v1`: the contrapuntal
style of 16th-century imitative choral polyphony (Palestrina and his
contemporaries). Path `frameworks/sixteenth-century-style/`, tests in
`frameworks/sixteenth-century-style/tests/`. A Framework extension module
physically separate from `src/theory/`, the same shape as
`frameworks/standard-counterpoint/`.

Unlike the species rules of `standard-counterpoint`, which are
style-independent training rules for two parts, these rules describe one
repertoire and work on any number of voices. The dependency on
`standard-counterpoint` is one way: this module reads its pitch arithmetic,
intervals, and slot types (`standard-counterpoint/counterpoint_pitch.h`,
`counterpoint_interval.h`, `counterpoint_slots.h`), and that framework knows
nothing of this one. File names carry a `century_` prefix so they never
collide with the `standard-counterpoint` headers on the include path.

## Scope

The subject of every rule is `SixteenthCenturyScore` (`century_score.h`): a
set of voices, each a slotted line (attacks, ties, and rests) on one common
grid, the length of the tactus in slots, the tonic of the mode (or NULL when
it is not known), and whether the mode is Phrygian. Positions are measured
from the first slot: two tactus make a bar, and a slot is on a strong tactus,
a weak tactus, or between tactus starts (sub-tactus).

Every rule has a Rule/Constraint pair referencing this Framework plus a pure
check function, and each group has a check that reports the rules broken as
hard and soft bit masks. A rule that states what the style requires is hard;
"limit", "avoid", and "prefer" rules are soft. `constraint_evaluate` is never
called here.

- **Melodic** (`century_melodic_rules.h`), on every voice's notes. Hard: no
  seventh, augmented, or diminished leap (the tritone included), none of those
  outlined by the first and last notes of a run in one direction, and a range of
  at most a twelfth. Soft: the only sixth is the rising minor sixth (rising
  major sixths are rare, descending sixths extremely rare), the larger of two
  successive same-direction leaps is the lower one, a leap larger than a fourth
  is recovered, no two successive thirds in one direction (an arpeggiated
  triad), a voice of five notes or more does not begin or end on its highest
  note (the arch), a range of at most 14 semitones, the last note is approached
  by step, and a final approached by step from below has the raised leading
  note (not in Phrygian modes).
- **Harmonic** (`century_harmonic_rules.h`), on pairs of voices at the slots
  where one of them begins a note. Hard: no parallel fifths or octaves (a
  diminished fifth counts as a fifth, a unison as an octave), and a diminished
  triad of three or more voices resolves (ti up a half step, fa down a half
  step, re down a step). Soft: no direct fifths or octaves, except at the end
  of the pair or when another voice moves in contrary motion, and a final
  chord that is a bare fifth or a major triad (the Picardy third).
- **Dissonance** (`century_dissonance_rules.h`). A dissonance is a second, a
  seventh, an augmented or diminished interval, or a fourth against the lowest
  voice. Hard: every new dissonance is a suspension (a tie into a tactus start)
  or, between tactus starts, a passing tone, a neighbor tone, or a nota cambiata
  (a step down into the dissonance, a leap down of a third out of it, and a step
  up: do, ti, sol, la, the only dissonance left by leap); and every suspension
  is prepared on a weaker position, consonant where it began, and resolved down
  by step to a consonance (7-6, 4-3, and 2-3). Soft: a figure dissonance is
  followed by a consonance. `sixteenth_century_consonant_at` is the
  consonance test.
- **Rhythm** (`century_rhythm_rules.h`), all soft: a note shorter than a
  tactus begins between tactus starts, and a voice begins and ends with a note
  of at least a tactus.
- **Imitation** (`century_imitation_rules.h`). The caller names a point of
  imitation (`SixteenthCenturyPoint`): the entries in order, each a voice and
  the index of its first note, and the number of notes. Hard: every answer
  keeps the leader's rhythm and its direction of motion through the first three
  notes. Soft: every answer is a real or tonal answer rather than a free one
  (`ImitationAnswer`, `standard-counterpoint/imitation_answer.h`;
  `sixteenth_century_point_answer`), the second entry sounds a perfect
  consonance from the first, answers begin at a comparable metric position, and
  every entry begins before the point has finished in the one before it.
- **Aggregate check** (`sixteenth_century_check`, `sixteenth_century_valid`):
  the melodic, harmonic, dissonance, rhythm, and imitation groups over a score
  and its points, reported as hard and soft bit masks indexed by
  `SixteenthCenturyGroup`. A score is valid when no hard rule is broken. The
  imitation group is skipped when no point is named.

## Not covered

- **Text setting.** Syllable placement, phrases that follow the text, and
  the one-beat-per-syllable guideline: there is no text entity yet.
- **Mass textures.** The imitative or homophonic conventions of each section of
  the Mass are liturgical metadata, not theory. Texture is
  `frameworks/standard-texture`.
- **Modal ranges.** The authentic and plagal ranges of a mode: the framework
  takes the tonic and a Phrygian flag, and applies the mode-independent limits
  on range.
- **Metrical details.** Ties that connect long notes to shorter ones, and the
  2+1 division of triple meter, need a notated rhythm; a slot grid keeps no
  notation, and two tactus are always one bar.
- **Fauxbourdon.** The extended passage of parallel triads, an exception to
  the parallels rule.
- **Enforcing the tonal answer.** `ImitationAnswer` names it and the point
  check classifies it, but no rule requires or forbids it; the high Baroque
  fugal exposition is `frameworks/high-baroque-fugue`.
- **Imitation over a repeating bass.** The dux and comes imitation of a
  Baroque ground bass is `frameworks/baroque-ground-bass`.
- **Finding points of imitation.** The caller says where a point is.
- **Repertoire statistics.** The Palestrina counts behind the rules (814 rising
  minor sixths and 54 rising major sixths in the masses, major final triads in
  about 85% of final chords) are the reason for the soft rules, not data the
  framework holds.

## Shared slot types

`SixteenthCenturyNote` and `SixteenthCenturyResult` are typedefs of `StandardCounterpointSlotNote` and `StandardCounterpointRuleResult` (`standard-counterpoint/slot_note.h`).
