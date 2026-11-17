# standard-tonality

Theory Framework instance `standard-tonality-v1`: the tonal conventions
of Western music. Where it lives: the rules are in `library/theory/tonality/` (tests `library/theory/tests/tonality/`) and the analysis adapter is in `library/analysis/harmony/` (tests `library/analysis/tests/harmony/`). A Framework extension module
physically separate from `theory/style/`, same shape as
`theory/tonality/`. It does not depend on any other
framework module: pitch arithmetic it needs is its own.

## Scope

Rules about how pitches relate within a key:

- **Keys** (`tonal_key.h`): degrees, spellings and signatures of a key in major and the
  three minor forms, and the key of a signature on the line of fifths.
- **Degree of a semitone** (`tonal_key.h`): `standard_tonality_degree_of_semitone` is the one owner of the no-context
  degree and alteration of a chromatic note (semitone 1 is `b2`, R-285); common-practice-minimal and the chain call it.
- **Triad and seventh chord qualities**: see [standard-tonality-chords.md](../../docs/modules/standard-tonality-chords.md).
- **Roman numerals, the grammar, analysis and key functions**: see [standard-tonality-numerals-and-keys.md](../../docs/modules/standard-tonality-numerals-and-keys.md).
## Segment bass input

`StandardTonalitySegmentInput` carries `has_bass` and `bass_pitch` (semitone number, C4 = 60). Zero-initialized, the inversion is read from the lowest chord tone of `pitches`, as before. With `has_bass`, the chord member on `bass_pitch`'s pitch class is the bass, whatever lower pitches sound, so a held bass reads as root position under a passing lower note. A bass that is not a chord member falls back to the previous segment's bass, as without one. `pitches` may be empty when a bass is given.

## Modes on pitch classes and semitones

`standard_tonality_mode_pitch_classes` (`tonal_key.h`) gives a mode's scale on a tonic as a 12-bit pitch class mask; a minor set built from several forms is a union of masks. `standard_tonality_degree_of_semitone` reads a semitone above the tonic as a degree and an alteration, a note outside the scale being the lower degree raised. Owners for the Composer's scale mask and degree tables.

## Not covered

Interval spelling beyond triply augmented or diminished, intervals
between notes spelled with more than a double accidental, fixed-do
solfege, other collections (whole-tone, octatonic, and so
on), jazz chord-scale theory, and choosing between a signature's major and
minor key from the first and last notes: that is a judgement, not a
rule, and belongs with key identification (`common-practice-minimal`
keeps its own major and natural-minor vocabulary; the two are not yet
unified).

## 12-TET assumption

`tonal_key.h` and `roman_numeral_analysis_rules.h` assume twelve-tone equal
temperament. They call `model/pitch/pitch_arithmetic.h`, which holds the one letter table.
