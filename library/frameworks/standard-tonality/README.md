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

How accidentals are written and ordered in a key signature is notation
and lives in `frameworks/standard-notation/`. Built on `KeySignature`
(`src/pitch/`), `Scale`, `ScaleDegree`, and `Key` stay the entity types;
scale-degree vocabulary lives here, not in their opaque labels.

Each rule has a backing `Rule`/`Constraint` pair referencing this
Framework, plus a pure check function. `constraint_evaluate` is never
called here.

## Not covered

Fixed-do solfege, other collections (whole-tone, octatonic, and so
on), jazz chord-scale theory, and choosing between a signature's major and
minor key from the first and last notes: that is a judgement, not a
rule, and belongs with key identification (`common-practice-minimal`
keeps its own major and natural-minor vocabulary; the two are not yet
unified).

## 12-TET assumption

`tonal_pitch.h` assumes twelve-tone equal temperament, as
`frameworks/standard-notation/semitone.h` does. Frameworks may not depend
on each other, so each carries its own copy; extraction waits for a
shared module.
