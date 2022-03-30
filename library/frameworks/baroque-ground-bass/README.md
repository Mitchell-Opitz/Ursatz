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
(`OstinatoType`, `src/semantics/ostinato_type.h`); the imitations the caller
identifies, each a voice, a start slot, an `ImitativeRole`
(`src/semantics/imitative_role.h`), and a number of notes; and the limits
behind the soft rules. A piece has one meter.

The rule groups are added in the sections below.
