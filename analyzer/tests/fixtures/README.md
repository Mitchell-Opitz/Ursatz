# Test fixtures

Copies of the Ursatz files the tests read, so no test reads `external/ursatz/tests`. Source commit: Ursatz `2c53118` (PR #181).
`URSATZ_ANALYZER_FIXTURE_DIR` is this folder.

| Here | Ursatz path |
| --- | --- |
| `framework-v1-corpus/*.mid` (Bach 114, Clementi 36, Field 1, Field 5, Mozart 545) | `tests/fixtures/framework-v1-corpus/` |
| `arpeggiation-investigation/Bach_Prelude_C_Major.mid` | `tests/fixtures/arpeggiation-investigation/Bach_Prelude_C_Major.mid` |
| `annotations/bach114_melody.txt`, `field1_texture.txt`, `field5_rhythm.txt`, `mozart545_motives.txt` | `tests/fixtures/annotations/` |
| `galant/galant_fixture.txt`, `galant_annotation.txt` | `tests/fixtures/galant/` |
| `chorales/<name>.musicxml` | `tests/accuracy/corpus/<name>/score.musicxml` |

Update a copy by recopying it from the Ursatz commit the submodule is pinned to and naming that commit here.
