# framework-v1-corpus fixtures

The Framework v1 reference test corpus: five MIDI files.

- `Bach 114.mid`: Bach, Minuet in G, BWV Anh. 114
- `Clementi 36.mid`: Clementi, a Sonatina from Op. 36
- `Field 1.mid`: Field, Nocturne No. 1 (12/8)
- `Field 5.mid`: Field, Nocturne No. 5
- `Mozart 545.mid`: Mozart, Sonata K. 545

`tests/corpus/test_corpus_regression_framework_v1.c` scans this directory at
test-run time, registers one Corpus piece_ref per `.mid` file found, and
asserts the Corpus reflects exactly that set. It checks registration only, not
analysis output.
