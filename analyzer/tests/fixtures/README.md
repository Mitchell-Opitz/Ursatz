# Test fixtures

Copies of the Ursatz files the tests read, so no test reads `external/ursatz/tests`. Source commit: Ursatz `2c53118` (PR #181).
`URSATZ_ANALYZER_FIXTURE_DIR` is this folder.

| Here | Ursatz path |
| --- | --- |
| `framework-v1-corpus/*.mid` (Bach 114, Clementi 36, Field 1, Field 5, Mozart 545) | `tests/fixtures/framework-v1-corpus/` |
| `arpeggiation-investigation/Bach_Prelude_C_Major.mid` | `tests/fixtures/arpeggiation-investigation/Bach_Prelude_C_Major.mid` |
| `annotations/bach114_melody.txt`, `field1_texture.txt`, `field5_rhythm.txt`, `mozart545_motives.txt` | `tests/fixtures/annotations/` |
| `galant/galant_fixture.txt`, `galant_annotation.txt` | `tests/fixtures/galant/` |
| `export/theme.musicxml`, `export/piece-loop.musicxml`, `export/piece-ending.musicxml` | Ursatz-GUI `backend/tests/fixtures/composer/b6_compose/theme-v2/theme.musicxml`, `piece-v4/loop.musicxml`, `piece-v4/ending.musicxml` (Composer output, not Ursatz) |
| `export/motif*.musicxml` | Written by hand in the Composer's rendered form: a 2-bar motif and two variants (material export tests) |
| `chorales/<name>.musicxml` | `tests/accuracy/corpus/<name>/score.musicxml` |

Update a copy by recopying it from the Ursatz commit the submodule is pinned to and naming that commit here.
