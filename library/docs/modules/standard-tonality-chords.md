# standard-tonality triads and sevenths

The triad, seventh chord and chord symbol rule groups of `frameworks/standard-tonality/`. [Docs index](../README.md)

Framework overview: [frameworks/standard-tonality/README.md](../../frameworks/standard-tonality/README.md).

- **Triad structure** (`triad_rules.h`): three notes stacked in thirds, all
  on lines or all on spaces; root, third, fifth; four qualities set by
  the intervals from the root (major ma3 P5, minor mi3 P5, diminished
  mi3 d5, augmented ma3 A5); major and minor named for the third,
  diminished and augmented for the fifth.
- **Triad spelling** (`triad_spelling_rules.h`): from root and quality, by
  the interval rules and, as the source steps it, from the root's major
  key signature with the third and fifth altered (minor lowers the third,
  diminished both, augmented raises the fifth).
- **Triad identification** (`triad_identification_rules.h`): root and
  quality from any notes, in any order; doublings and open spacing do not
  matter (octave equivalence) and reduce to closed position. An imaginary
  root is still identified; the literal key-signature steps report no
  answer for it. Inversion beyond the bass note in a chord symbol is a
  later chapter's.
- **Chord symbols** (`chord_symbol_rules.h`): root, accidental, then
  nothing / "mi" / "°" / "+", and a slash bass when the lowest note is not
  the root (Cmi/E♭, Cmi/G); parse and format, and a `Chord` labeled with
  the symbol. These are absolute symbols; common-practice-minimal's
  Roman-numeral triads are relative to a key and are not touched.
- **Triad qualities in a key** (`triad_in_key_rules.h`): major on do, fa,
  sol and minor on re, mi, la with a diminished ti in major; in natural
  minor, minor on do, fa, sol, major on me, le, te, diminished on re; the
  raised leading tone makes the sol triad major and the ti triad
  diminished. The me triad of harmonic minor (augmented) is not in the
  source and is included because it follows from the spelled scale.
- **Quality semitones** (`triad_rules.h`, `seventh_rules.h`):
  `standard_tonality_triad_quality_semitones` and
  `standard_tonality_seventh_quality_semitones` give the root-relative
  semitones of each triad and seventh quality, checked against the interval
  rules.
- **Seventh chord structure** (`seventh_rules.h`): four notes stacked in
  thirds, all on lines or all on spaces; root, third, fifth, seventh;
  five qualities set by the triad and the seventh above the root
  (major-major, major-minor, minor-minor, half-diminished, fully
  diminished), with the alternate names major, dominant, minor, and
  diminished seventh chord (the half-diminished has none). Chords
  outside the five (a minor triad with a major seventh, an augmented
  triad) are a later chapter's and get no quality here.
- **Seventh chord spelling** (`seventh_spelling_rules.h`): from root and
  quality, by the interval rules and, as the source steps it, from the
  root's major key signature with the third, fifth, and seventh altered.
- **Seventh chord identification** (`seventh_identification_rules.h`):
  root and quality from any notes, in any order; doublings and open
  spacing do not matter and reduce to closed position. Root position
  only; the bass note a chord symbol names is the one nod to inversion.
- **Seventh chord symbols** (`chord_symbol_rules.h`): the root, then "ma7",
  "7", "mi7", "∅7", or "°7", and a slash bass when the lowest note is not
  the root (G∅7/F); parse and format, and a `Chord` labeled with the
  symbol.
- **Seventh chord qualities in a key** (`seventh_in_key_rules.h`): in
  major, major-major on do and fa, major-minor on sol, minor-minor on re,
  mi, la, half-diminished on ti; in natural minor, minor-minor on do, fa,
  sol, major-major on me and le, major-minor on te, half-diminished on
  re; the raised leading tone makes the sol chord major-minor and the ti
  chord fully diminished. The raised leading tone in the chords on do
  and me would give qualities outside the five, so they stay as in
  natural minor.
