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

The rule groups are added in the sections below.
