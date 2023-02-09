# Frameworks

Theory Framework extension modules. Each one lives under `frameworks/<name>/`, outside
`src/`, and builds on the kernel's `Framework`, `Rule` and `Constraint` types
(`src/theory/`, `src/rules/`) without changing them. The kernel never includes a framework
header. The one exception is `src/analysis/chord_identification.c` and `key_estimation.c`,
which are compiled into `common_practice_minimal`, not into `ursatz` (see
`docs/modules/analysis.md`).

Each framework's own `README.md` holds its full scope and exclusions. This page is the
catalog and the shared conventions.

## Catalog

| Framework | Covers | Depends on |
|---|---|---|
| `common-practice-minimal` | 7 diatonic triads, 4 diatonic seventh qualities, major and natural-minor keys. **The only framework the analyzers use today** | ursatz |
| `standard-notation` | Staff, clefs, ledger lines, stems, accidentals, half and whole steps | ursatz |
| `standard-markings` | Dynamics order, articulation classes, tempo words, repeat and D.C./D.S. playback order | ursatz |
| `western-periods` | The six conventional stylistic periods | ursatz |
| `standard-rhythm` | Note values, dots, ties, rests, tuplets | ursatz |
| `standard-meter` | Simple and compound meter, beaming, anacrusis, counting, hypermeter, syncopation | ursatz |
| `standard-tonality` | Scales, modes, solfege, key signatures, circle of fifths, intervals, triads, sevenths, chord symbols, Roman numerals | ursatz |
| `standard-texture` | Monophony, heterophony, homophony, polyphony | ursatz |
| `standard-voicing` | Strict SATB voicing of one chord | ursatz |
| `standard-counterpoint` | Species counterpoint, first through fifth | ursatz |
| `sixteenth-century-style` | Palestrina-style imitative polyphony | standard-counterpoint, ursatz |
| `high-baroque-fugue` | Fugal exposition | standard-counterpoint, ursatz |
| `baroque-ground-bass` | Ground bass, chaconne, passacaglia | standard-counterpoint, ursatz |
| `galant-schemas` | Gjerdingen's Galant schemas | standard-counterpoint, ursatz |
| `scale-harmonization` | Rule of the octave, sequences over a bass scale | galant-schemas, standard-counterpoint, ursatz |
| `standard-harmony` | Harmonic function, phrase model, cadences, six-four chords, prolongation, tonicization, modulation | ursatz |
| `standard-form` | Form levels, motives, sentence, period, hybrid forms | standard-harmony, ursatz |

Most of the frameworks after `common-practice-minimal` follow Open Music Theory, a free
textbook; each README names the chapters it covers.

## Shared conventions

- **Rule libraries, not analyzers.** Every framework except `common-practice-minimal` takes
  caller-supplied plain data (scale degrees, spellings, inversions, flags) and returns `bool`
  results and hard/soft violation bitmasks. Only parts of `standard-tonality` and
  `standard-texture` build an `Interpretation`. Nothing yet reads MIDI or `NoteEvent`s into
  these inputs. That bridge is the next piece of work (see the project roadmap).
- **Framework singleton.** Each exposes one lazily built `Framework` instance plus one
  `Rule`/`Constraint` pair per rule. Rules are checked with the framework's own
  `*_rule_holds()` functions, never with `constraint_evaluate()`, which is still a stub.
- **Hard vs. soft.** Rules phrased as "must" are hard. Rules phrased as "limit", "avoid" or
  "prefer" are soft.
- **Dependency ceiling.** L0 to L6 plus `theory/framework.h`, `rules/rule.h` and
  `rules/constraint.h`, plus any framework listed in its row above. Each framework's
  `tests/test_architecture_<name>.c` enforces this.
- **Vocabulary enums.** Names a framework needs (for example `CadenceType`, `VoicePart`) live
  as plain enums in `src/semantics/` and carry no framework claim. What each name means is the
  framework's convention. See `docs/modules/semantics.md`.

## Building and testing

The root build compiles every framework library. **Framework tests do not run from the root
build yet**: each `CMakeLists.txt` adds its `tests/` directory only when that framework is
the top-level project, so root `ctest` and CI skip them. Until that is fixed, run them by
configuring a wrapper project that adds the root and each `frameworks/<name>/tests` directory.
Tracked as a known gap in the project docs.
