# ezxml (vendored)

ezxml is a small C library for parsing XML documents into an in-memory tree. It is vendored here, unmodified,
for `src/io/musicxml_importer.c`, the only file in this repository that may use it.

- Project: ezxml by Aaron Voisine, <http://ezxml.sourceforge.net>
- Copy: the `lxfontes/ezxml` mirror, https://github.com/lxfontes/ezxml (branch `master`), fetched 2026-10-06
- License: MIT, in `license.txt` (it must stay next to the code)

| file | SHA-256 |
|---|---|
| `ezxml.c` | `8f60356fd37b5128e6a9bdfb15cbd83fe33cc8f32b12b4f6815fa18168e4b221` |
| `ezxml.h` | `31a25f58829052448545304e3796096bfebd38ad802419641baa90d95b5d5b9e` |
| `license.txt` | `e82e49402f6b21176c65eead46dbd39d8f7a3c28b052589d8b307dfc8f78a03f` |

## How it is built

`third_party/CMakeLists.txt` builds it as the private static target `ursatz_ezxml`, with `EZXML_NOMMAP` defined (the
importer parses from a buffer, never a file descriptor) and warnings off for this target only. `ursatz` links it
PRIVATE, so no consumer of `ursatz` sees it or its include path. `tests/test_architecture_third_party.c` fails if
any file other than `src/io/musicxml_importer.c` includes `ezxml.h`. Nothing in a public header exposes it, so it can
be replaced (for example by yxml) by changing that one file.

## Notes for maintainers

- ezxml parses IN PLACE: it writes into the buffer it is given. The importer passes its own copy.
- Treat input as untrusted. The importer caps the input size (`MUSICXML_IMPORT_MAX_BYTES`) before parsing.
- Do not edit these files. If they must change, mark the change plainly in the file and in this README.
