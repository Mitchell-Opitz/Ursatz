# standard-voicing

Theory Framework instance `standard-voicing-v1`: writing a chord in strict
SATB style. Path `frameworks/standard-voicing/`, tests in
`frameworks/standard-voicing/tests/`. A Framework extension module
physically separate from `src/theory/`, same shape as
`frameworks/standard-tonality/` and `frameworks/standard-texture/`. It does
not depend on any other framework module.

## Scope

Rules for a chord written in four parts: soprano, alto, tenor, bass
(`VoicePart`, `src/semantics/voice_part.h`). A voiced chord is
`StandardVoicingChord` (`voicing_chord.h`): one sounding `PitchSpelling`
per part. Six rules (`voicing_rules.h`), each with a backing hard
`Rule`/`Constraint` pair referencing this Framework plus a pure check
function. `constraint_evaluate` is never called here.

- **Stem direction** (`stem_rules.h`): soprano and alto on the treble
  staff, tenor and bass on the bass staff; soprano and tenor take up-stems,
  alto and bass down-stems. This is a rule about voices and coexists with
  standard-notation's position-based stem rules, which are unchanged.
- **Chord construction** (`construction_rules.h`): the bass is the chord
  tone the inversion calls for, every chord tone sounds in some voice, and
  no voice sounds a note outside the chord. Notes are compared by letter
  and accidental, so an erroneous accidental is caught. The first problem
  found is reported. `standard_voicing_inversion_from_figure` maps a Roman
  numeral figure to an inversion (triads: none/5/53, 6/63, 64; sevenths:
  7, 65, 43, 42/2). The upper voices may be arranged in any order.
- **Range** (`range_rules.h`): soprano C4-G5, alto G3-D5, tenor C3-G4,
  bass F2-D4, both ends included. Pitches are compared by sounding
  semitone number (12-TET, this module's own copy in `voicing_pitch.h`),
  so B-sharp 3 is in the soprano range as C4.
- **Spacing** (`spacing_rules.h`): no more than an octave (generic size 8)
  between soprano and alto or alto and tenor, no more than a twelfth
  between tenor and bass. The first wide pair is reported.
- **Voice crossing** (`crossing_rules.h`): soprano above alto above tenor
  above bass, strictly: two adjacent voices on the same pitch are not in
  order. The first crossed pair is reported.
- **Doubling** (`doubling_rules.h`): the leading tone and a chordal seventh
  are never doubled, and a seventh chord has one note per voice, so none is
  doubled (hard). A triad usually doubles its bass note: a preference, so
  its constraint is soft and failing it never makes a voicing invalid.
- **Aggregate check** (`standard_voicing_check`, `standard_voicing_valid`):
  runs the six rules over a voicing and reports the broken ones as a bit
  mask indexed by `StandardVoicingRule`. Stem directions are optional in
  the input: an unengraved chord skips that rule.

The chord a voicing realizes is the caller's input: its tones in root
position order (3 or 4 spellings, octaves ignored), the inversion, and the
key's leading tone. They come from standard-tonality's chord and Roman
numeral rules or any other source; this module does not depend on them.

## Not covered

Voice leading between chords (parallel fifths and octaves, leaps, the
resolution of tendency tones) is a later chapter, as is deciding which
voicing to choose. A seventh chord must have all four tones, one per voice;
omitted-fifth and other incomplete seventh voicings are a violation here.
Tendency tones other than the leading tone and the chordal seventh have no
vocabulary yet. Pitches are read as sounding pitch: transposing clefs and
instruments are not modeled.

`standard_voicing_read_doubling` (`doubling_reading.h`) reads a voiced chord: for 3 to 6 sounding pitches (highest voice first) and the chord's intervals above its root it gives the voices on each chord member, the voices on none, the doubled member and the gap between neighbouring voices. It judges nothing; the doubling and spacing rules are above.
