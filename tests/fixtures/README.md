# Test fixtures

The reference pieces and annotations every part's tests read, once. Tests reach this folder through the compile definition
`URSATZ_FIXTURES_DIR` that `ursatz_test()` gives every test executable (`cmake/UrsatzTest.cmake`), and CMake scripts through
`${URSATZ_FIXTURES_DIR}`. A part keeps in its own `tests/` folder only what differs by design: the Analyzer's `musicxml/` and
`export/`, the Composer's `fixtures/` (requests, forms, styles) and the golden snapshots.

| Folder | What it holds | Read by |
| --- | --- | --- |
| `framework-v1-corpus/` | The five reference MIDI files (Bach 114, Clementi 36, Field 1, Field 5, Mozart 545), `expected_deviations.md` | library, Analyzer, Composer (seeded corpora), GUI |
| `arpeggiation-investigation/` | `Bach_Prelude_C_Major.mid` | library export, Analyzer CLI |
| `annotations/` | Hand annotations of the reference pieces (`*_expected.txt`, melody, texture, rhythm, motives) | library integration and accuracy, Analyzer |
| `galant/` | The galant schema fixture and its annotation | library integration, Analyzer schemas |
| `musicxml/` | Hand-written MusicXML fixtures | library io and export, GUI |
| `two-voice/`, `voice_leading/` | Two-voice MIDI and the voice leading expectations | library analysis and integration |
| `accuracy-corpus/` | Ten Bach chorales (`chorale-*/score.musicxml`, `analysis.rntxt`) and `LICENSES.md` | library accuracy harness, Analyzer (the `chorales` test corpus), GUI |
| `links/` | Four pieces of one motive family each | library chain, Analyzer, GUI |
| `knowledge/` | `valid/` entries and `invalid/<problem>/` folders (each with its `expect.txt`) for `tools/ci/check-knowledge.py` | the `knowledge_validate_fixtures` test |
| `tools/` | `cut_midi.py`, the script that cut the reference MIDI files | by hand |

Licensing: the chorales are third-party material, private use only (see `accuracy-corpus/LICENSES.md`); the READMEs in
`framework-v1-corpus/`, `two-voice/` and `links/` say where each file comes from. A copy of a file in this folder in another
part's `tests/` is a defect: read this one.
