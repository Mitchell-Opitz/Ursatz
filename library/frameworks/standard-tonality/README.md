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

How accidentals are written and ordered in a key signature is notation
and lives in `frameworks/standard-notation/`. Built on `KeySignature`
(`src/pitch/`), `Scale`, `ScaleDegree`, and `Key` stay the entity types;
scale-degree vocabulary lives here, not in their opaque labels.

Each rule has a backing `Rule`/`Constraint` pair referencing this
Framework, plus a pure check function. `constraint_evaluate` is never
called here.

## Not covered

Minor scales (natural, harmonic, melodic), modes, the subtonic, fixed-do
solfege, and key identification from music (`common-practice-minimal`
keeps its own major and natural-minor vocabulary; the two are not yet
unified).

## 12-TET assumption

`tonal_pitch.h` assumes twelve-tone equal temperament, as
`frameworks/standard-notation/semitone.h` does. Frameworks may not depend
on each other, so each carries its own copy; extraction waits for a
shared module.
