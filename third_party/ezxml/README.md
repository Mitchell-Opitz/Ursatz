# ezxml (vendored)

ezxml is a small C library for parsing XML documents into an in-memory tree. It is vendored here, with the changes
under Notes for maintainers, for `library/io/musicxml_importer.c`, the only file in this repository that may use it.

- Project: ezxml by Aaron Voisine, <http://ezxml.sourceforge.net>
- Copy: the `lxfontes/ezxml` mirror, https://github.com/lxfontes/ezxml (branch `master`), fetched 2026-10-06
- License: MIT, in `license.txt` (it must stay next to the code)

| file | SHA-256 |
|---|---|
| `ezxml.c` | `d4398b01d36519f967a6e1fa974af43a86d42b8ea4980528a17662900a1e2dc1` |
| `ezxml.h` | `968fffdd0f2ad7c627a63fc21b8ec4888db7e14827c16cad6a08fd024c118b19` |
| `license.txt` | `e82e49402f6b21176c65eead46dbd39d8f7a3c28b052589d8b307dfc8f78a03f` |

## How it is built

`third_party/CMakeLists.txt` builds it as the private static target `ursatz_ezxml`, with warnings off for this
target only (the file readers and the mmap code are deleted, so there is no `EZXML_NOMMAP`). `ursatz` links it
PRIVATE, so no consumer of `ursatz` sees it or its include path. the layer table (`cmake/layer-table.txt`) fails if
any file other than `library/io/musicxml_importer.c` includes `ezxml.h`. Nothing in a public header exposes it, so it can
be replaced (for example by yxml) by changing that one file.

## Notes for maintainers

- ezxml parses IN PLACE: it writes into the buffer it is given. The importer passes its own copy.
- Treat input as untrusted. The importer caps the input size (`MUSICXML_IMPORT_MAX_BYTES`, 16 MiB) before parsing.
- Changes to the upstream copy (P66b B1, R-684), each marked `/* Ursatz: ... */` in `ezxml.c`:
  - A UTF-16 document with fewer than two bytes of text after the byte order mark is refused (it crashed on a NULL buffer, and the shrink to zero bytes is no longer a `realloc` to 0).
  - An internal DTD subset (`<!DOCTYPE x [ ... ]>`) is refused with "internal DTD subset not supported". `ezxml_internal_dtd` and `ezxml_ent_ok` are deleted, which ends the entity expansion (CVE-2019-20199, -20201, CVE-2021-31598, CVE-2022-30045 reached through entities). MusicXML files declare only an external DTD.
  - A character reference outside 0 to 0x10FFFF, or with a sign, stays as written text (the UTF-8 encoder shifted by an amount UBSan flags).
  - `ezxml_new` checks its allocations and returns NULL (CVE-2021-26221, -26222); `ezxml_parse_str` passes the NULL on, which the importer reports as an allocation error.
  - Deleted as unused: `ezxml_toxml` and its helpers, `ezxml_set_txt`, `ezxml_set_attr`, `ezxml_cut`, `ezxml_get`, `ezxml_vget`, `ezxml_idx`, `ezxml_pi`, `ezxml_parse_fp`, `ezxml_parse_fd`, `ezxml_parse_file`, the mmap code and the test harness. The importer calls only `ezxml_parse_str`, `ezxml_error`, `ezxml_free`, `ezxml_child`, `ezxml_attr`, `ezxml_txt` and `ezxml_name`.
- Do not edit these files further without marking the change plainly in the file and in this README.
