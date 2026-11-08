# framework-v1-corpus fixtures

The Framework v1 reference test corpus: five MIDI files.

- `Bach 114.mid`: Bach, Minuet in G, BWV Anh. 114
- `Clementi 36.mid`: Clementi, a Sonatina from Op. 36
- `Field 1.mid`: Field, Nocturne No. 1 (12/8)
- `Field 5.mid`: Field, Nocturne No. 5
- `Mozart 545.mid`: Mozart, Sonata K. 545, first movement only (cut at the start of measure 147 by `tests/fixtures/tools/cut_midi.py`: `python3 tests/fixtures/tools/cut_midi.py <uncut.mid> "Mozart 545.mid" 140160`)

`tests/corpus/test_corpus_regression_framework_v1.c` scans this directory at
test-run time, registers one Corpus piece_ref per `.mid` file found, and
asserts the Corpus reflects exactly that set. It checks registration only, not
analysis output.

The Framework v1 verification suite (tests/integration/framework_v1_verification.c and framework_v1_thematic_returns.c) checks the full chain
on these five files against `tests/fixtures/annotations/<piece>_v1_expected.txt`. Where the chain does not give an expected value the
location is a deviation listed in `expected_deviations.md` here; see docs/testing/framework_v1_verification.md.
