# standard-tonality

Theory Framework instance `standard-tonality-v1`: the tonal conventions
of Western music. Path `frameworks/standard-tonality/`, tests in
`frameworks/standard-tonality/tests/`. A Framework extension module
physically separate from `src/theory/`, same shape as
`frameworks/standard-notation/`. It does not depend on any other
framework module: pitch arithmetic it needs is its own.

## Scope

Rules about how pitches relate within a key:

- **Major scale** (`major_scale_rules.h`): ascending W-W-H-W-W-W-H; the
  scale starts and ends on the same letter one octave apart and is named
  for that note, accidental included (B-flat major is not B major). A
  spelled scale uses each letter once in turn -- the module's assumption,
  which separates F-flat major from its enharmonic E major.
- **Scale degrees** (`scale_degree_rules.h`): degrees 1 to 7 (1 again at
  the top), movable-do solfege (do re mi fa sol la ti), and the names
  tonic, supertonic, mediant, subdominant, dominant, submediant, leading
  tone. The names come from generic intervals to the tonic (super- above,
  sub- below); the leading tone is a half step below the tonic.
  `standard_tonality_build_scale_degree` builds a populated `ScaleDegree`
  labeled with the degree name.
- **Key signature to key** (`key_signature_rules.h`): in a sharp
  signature the last sharp is a half step below the tonic; in a flat
  signature the second-to-last flat is the tonic; C major (none) and F
  major (one flat) are memorized; C-flat and C-sharp major are the
  all-flat and all-sharp signatures.
- **Circle of fifths** (`circle_of_fifths_rules.h`): keys in order of
  accidentals, neighbors a fifth apart, the enharmonic pairs at the
  bottom (B / C-flat, F-sharp / G-flat, C-sharp / D-flat), and real
  versus imaginary keys (imaginary needs a double sharp or flat: F-flat
  major needs B double-flat).

- **Minor scales** (`minor_scale_rules.h`): natural W-H-W-W-H-W-W,
  harmonic W-H-W-W-H-3H-H, melodic ascending W-H-W-W-W-W-H and
  descending W-W-H-W-W-H-W (natural minor). A minor scale's third is a
  half step below the major scale's on the same tonic. Descending scales
  are listed from the upper tonic. Spelling rules are shared with major
  (`scale_pattern.h`).
- **Degree of a semitone** (`tonal_key.h`): `standard_tonality_degree_of_semitone` is the one owner of the no-context
  degree and alteration of a chromatic note (semitone 1 is `b2`, R-285); common-practice-minimal and the chain call it.
- **Minor scale degrees** (`minor_scale_degree_rules.h`): same numbers as
  major; solfege me, le, te (natural), ti for harmonic, la ti for melodic
  ascending; the subtonic (te, a whole step below the tonic) beside the
  leading tone (ti); lowered degrees versus major (natural 3, harmonic 2,
  melodic ascending 1); a minor `ScaleDegree` builder.
- **Key relationships** (`key_relationship_rules.h`): parallel keys share
  a tonic; relative keys share a signature, the minor tonic three half
  steps below the major's and spelled so the signature matches (D-flat
  major's relative is B-flat minor, not A-sharp minor).
- **Minor keys** (`minor_key_rules.h`): minor keys and signatures rest on
  natural minor; the minor key of a signature; real versus imaginary
  minor keys; the major and minor candidates a signature allows.
- **Circle of fifths, minor ring**: minor keys beside the majors with the
  same signature, fifth neighbors, and the enharmonic minor pairs (G-sharp
  / A-flat, D-sharp / E-flat, A-sharp / B-flat).

- **Chromatic solfege** (`solfege_rules.h`): syllables by degree and
  alteration -- raised di ri fi si li (-i), lowered ra me se le te (-e,
  ra the exception); there is no lowered do or fa and no raised mi or ti.
- **Diatonic modes** (`mode_rules.h`): lydian, ionian, mixolydian,
  dorian, aeolian, phrygian, locrian, each with its step pattern, in
  brightness order from lydian to locrian. The three bright modes have
  mi, the four dark ones me. Alterations are computed against ionian or
  aeolian (lydian raises 4; mixolydian lowers 7; dorian raises 6 against
  minor; phrygian lowers 2; locrian lowers 2 and 5). Modes are described
  by the parallel relationship: any mode builds on any note.
  `modal_solfege_rules.h` gives each mode's syllables.
- **Pentatonic collection** (`pentatonic_rules.h`): major 2 2 3 2 3
  (degrees 1 2 3 5 6, do re mi sol la), spelled from the major scale on
  the same tonic, the black keys when it begins on F-sharp or G-flat. The
  minor pentatonic is the same collection begun on la (3 2 2 3 2, do me
  fa sol te); the source states only the major's pattern, so this is the
  standard definition.
- **Chromatic scale** (`chromatic_scale_rules.h`): twelve adjacent half
  steps (B-C and E-F are half steps already); usually written with
  naturals and sharps ascending and flats descending, a flat tonic
  included, but that is a convention, so the half-step check is separate
  from the spelling check; raised syllables ascending, lowered descending.

- **Interval size** (`interval_size_rules.h`): counted in lines and
  spaces from one (`GenericInterval`, `src/pitch/`), named by ordinals
  except unison and octave, generic (accidentals never change it), simple
  up to the octave and compound above (add 7).
- **Interval quality** (`interval_quality_rules.h`): perfect for unisons,
  fourths, fifths, octaves; major and minor for seconds, thirds, sixths,
  sevenths; augmented a half step above perfect or major; diminished a
  half step below perfect or minor; doubly and triply altered beyond;
  compound intervals keep their simple quality. Found by the
  major-scale method (the top note against the bottom note's major
  scale) with a semitone engine that agrees with it wherever the scale
  exists.
- **Interval inversion** (`interval_inversion_rules.h`): sizes add to 9;
  perfect to perfect, major to minor, augmented to diminished;
  identifying through the inversion.
- **Consonance** (`interval_consonance_rules.h`): melodic and harmonic
  tables as in the source; the perfect fourth is dissonant harmonically.
  The categorization varies with milieu, so it is this framework's.
- **White-key intervals** (`interval_white_key_rules.h`): seconds, thirds,
  fourths and their exceptions, the rest by inversion, and any interval as
  a white-key interval altered by accidentals.
- **Enharmonic intervals** (`interval_enharmonic_rules.h`): equivalence by
  half steps (the source's chart), and identifying an interval through
  enharmonic respelling of its notes.

- **Interval spelling** (`interval_spelling_rules.h`): the note a given
  size and quality above a lower note, the inverse of identifying an
  interval (a perfect fifth above F is C, a diminished fifth is C-flat).
- **Triads, seventh chords and chord symbols**: see [standard-tonality-chords.md](../../docs/modules/standard-tonality-chords.md).
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

`tonal_pitch.h` assumes twelve-tone equal temperament, as
`frameworks/standard-notation/semitone.h` does. Both now call
`src/pitch/pitch_arithmetic.h`, which holds the one letter table.
