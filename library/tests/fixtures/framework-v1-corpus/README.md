# framework-v1-corpus fixtures

Drop-in location for the framework v1 reference test corpus's MIDI files.

This directory is intentionally empty until the real `.mid` files are added
separately. `tests/corpus/test_corpus_regression_framework_v1.c` scans this
directory at test-run time, registers one Corpus piece_ref per `.mid` file
found, and asserts the Corpus reflects exactly that set -- an empty
directory is a valid pre-fixture state, not a test failure.
