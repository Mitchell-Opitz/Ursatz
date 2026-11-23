# standard-tonality triads and sevenths

The triad and seventh chord quality rule groups of `theory/tonality/`. [Docs index](../README.md)

Framework overview: [theory/tonality/README.md](../../theory/tonality/README.md).

- **Triad structure** (`triad_rules.h`): three notes stacked in thirds, all
  on lines or all on spaces; root, third, fifth; four qualities set by
  the intervals from the root (major ma3 P5, minor mi3 P5, diminished
  mi3 d5, augmented ma3 A5); major and minor named for the third,
  diminished and augmented for the fifth.
- **Triad qualities in a key** (`triad_in_key_rules.h`): major on do, fa,
  sol and minor on re, mi, la with a diminished ti in major; in natural
  minor, minor on do, fa, sol, major on me, le, te, diminished on re; the
  raised leading tone makes the sol triad major and the ti triad
  diminished. The me triad of harmonic minor (augmented) is not in the
  source and is included because it follows from the spelled scale.
- **Quality semitones** (`triad_rules.h`, `seventh_rules.h`):
  `standard_tonality_triad_quality_semitones` and
  `standard_tonality_seventh_quality_semitones` give the root-relative
  semitones of each triad and seventh quality.
- **Chord types from the snapshot** (R-728, `common_practice_materials.h`):
  `common_practice_materials_resolve` also resolves the nine quality rows'
  chord types, `omt.chord-type.major-triad`, `minor-triad`,
  `diminished-triad`, `augmented-triad`, `major-seventh`,
  `dominant-seventh`, `minor-seventh`, `half-diminished-seventh` and
  `diminished-seventh`, into `triad_types` and `seventh_types` (a missing
  one refuses the resolve, naming it). `common_practice_triad_semitones`
  and `common_practice_seventh_semitones` give a quality's members from
  them; with NULL materials (a call that holds no snapshot) they fall back
  to the quality rows above, which the test `standard_tonality_quality_semitones`
  pins to the entries. The chord identification
  (`CommonPracticeRankedChord.intervals`), the harmony segment inputs
  (`HarmonySegmentInput.materials`), the applied-chord and numeral readers
  and the chain's cadence claims read the types this way;
  `chord_vocabulary.c` and `chord_vocabulary_seventh.c` keep the fallback
  and add `_intervals_with` variants.
- **Seventh chord structure** (`seventh_rules.h`): four notes stacked in
  thirds, all on lines or all on spaces; root, third, fifth, seventh;
  five qualities set by the triad and the seventh above the root
  (major-major, major-minor, minor-minor, half-diminished, fully
  diminished), with the alternate names major, dominant, minor, and
  diminished seventh chord (the half-diminished has none). Chords
  outside the five (a minor triad with a major seventh, an augmented
  triad) are a later chapter's and get no quality here.
- **Seventh chord qualities in a key** (`seventh_in_key_rules.h`): in
  major, major-major on do and fa, major-minor on sol, minor-minor on re,
  mi, la, half-diminished on ti; in natural minor, minor-minor on do, fa,
  sol, major-major on me and le, major-minor on te, half-diminished on
  re; the raised leading tone makes the sol chord major-minor and the ti
  chord fully diminished. The raised leading tone in the chords on do
  and me would give qualities outside the five, so they stay as in
  natural minor.
