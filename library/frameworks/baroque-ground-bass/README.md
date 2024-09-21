# baroque-ground-bass

Theory Framework instance `baroque-ground-bass-v1`: the Baroque ground bass,
in the manner of the trio sonatas of Purcell. Path
`frameworks/baroque-ground-bass/`, tests in
`frameworks/baroque-ground-bass/tests/`. A Framework extension module
physically separate from `src/theory/`, the same shape as
`frameworks/standard-counterpoint/`.

The dependency on `standard-counterpoint` is one way: this module reads its
pitch arithmetic, intervals, and slot types
(`standard-counterpoint/counterpoint_pitch.h`, `counterpoint_interval.h`,
`counterpoint_slots.h`), and that framework knows nothing of this one. File
names carry a `ground_` prefix so they never collide with the
`standard-counterpoint` headers on the include path.

## Scope

The subject of every rule is `GroundBassPiece` (`ground_piece.h`): the voices
of the piece, each a slotted line (attacks, ties, and rests) on one common
grid; which voice is the bass; the number of slots in an iteration of the
ground and in a bar; the number of beats in a bar; the kind of ostinato
(`OstinatoType`, `ostinato_type.h`); the imitations the caller
identifies, each a voice, a start slot, an `ImitativeRole`
(`imitative_role.h`), and a number of notes; and the limits
behind the soft rules. A piece has one meter.

Every rule has a Rule/Constraint pair referencing this Framework plus a pure
check function, and each group has a check that reports the rules broken as
hard and soft bit masks. A rule that states what the style requires is hard;
"limit", "avoid", and "prefer" rules are soft. `constraint_evaluate` is never
called here.

The ground is the first iteration of the bass voice. A note belongs to the
iteration it begins in, and only whole iterations are judged.

- **Ground** (`ground_bass_rules.h`). Hard: the piece is a whole number of
  iterations long; every iteration repeats the ground, with at most
  `max_altered_notes` (default 1, in `GroundLimits`) notes that differ (a note
  with another pitch, place, or length counts as one;
  `ground_bass_altered_notes`); the bass is never above another sounding voice;
  and a chaconne or a passacaglia (`ostinato_type_is_triple_meter_form`) has
  three beats to the bar. Soft: the repetition is exact.
- **Variety** (`ground_variety_rules.h`), all soft, on the upper voices (every
  voice but the bass): each iteration's upper parts differ from the one
  before; in a piece of three or more iterations the shortest upper note is not
  the same length in every iteration (the metrical level changes); and every
  iteration contains the beginning of a dux entry.
- **Imitation** (`ground_imitation_rules.h`). A comes is paired with the nearest
  dux listed before it. Hard: the comes begins after its dux, keeps its rhythm,
  and moves in the same direction through the first three notes. Soft: it
  begins at the same position in the bar (the gap is a whole number of bars,
  three or six beats in 3/4; closer imitation is occasional), and it is in
  another voice. The bass may be the dux, as when an upper voice imitates the
  ground.
- **Aggregate check** (`ground_check`, `ground_valid`): the ground, variety,
  and imitation groups over a piece, reported as hard and soft bit masks
  indexed by `GroundGroup`. A piece is valid when no hard rule is broken.

## Not covered

- **Re-harmonization.** The options for re-harmonizing a bass interval (the
  bass harmonization cheat sheet) are not encoded: the source table is not part
  of the framework yet.
- **Tonicization and seams.** Tonicized keys, and the avoided perfect cadence
  where one iteration joins the next (such as V/v-v6 in place of V-i, or vii
  diminished 6-I in a neighboring key), need chord analysis, and there is no
  harmonic-analysis input for a framework to take.
- **5-6 steps and 7-6 suspension sequences.** The harmonic ambiguity of the 5-6
  step in the melodic parts, and the suspensions it sets up.
- **Changes of meter.** A piece has one meter, so the change to compound time
  that often marks a new iteration is not modeled.
- **Imitation by finding it.** The caller says where each dux and comes is.

## Vocabulary enums

Owned here (moved from `src/semantics/`; type names and values unchanged):

- `OstinatoType` is a plain enum value naming the kind of repeating pattern a work or section is built on, from broad to specific: ostinato (any pattern that repeats throughout in one voice), ground bass (an ostinato in the bass), chaconne, and passacaglia (the Baroque sub-genres of the ground bass, in triple meter). `ostinato_type_is_ground_bass` is true for the ground bass, chaconne, and passacaglia, and `ostinato_type_is_triple_meter_form` for the last two. It carries no `EntityID`, pitches, or framework claim. What a framework asks of each kind is its own convention (`frameworks/baroque-ground-bass`).
- `ImitativeRole` is a plain enum value naming the part a voice plays in an imitation: dux (the leader, which states the melody first) or comes (the follower, which imitates it). It carries no `EntityID`, pitches, or framework claim, and is neither `ImitationAnswer` (how closely an entry follows) nor `FugueRole` (the part a passage plays in a fugue).
