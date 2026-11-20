# common

The helpers every Ursatz part shares: one status set, file and folder helpers, strings, JSON, a hash, the job handle and the event bus. [Repository README](../../README.md).

`common/` builds the static library `ursatz_common` (headers `<common/*.h>`, names `common_*`), on libc, `m` and cJSON only. The library, the Analyzer, the Composer, the workshop and the GUI call it in place of their own copies (R-638); it is POSIX only (Linux and macOS). The layer table (`cmake/layer-table.txt`) puts it under everything: no part below may be reached from it.

| Header | What it holds |
|---|---|
| `status.h` | `UrsatzStatus` (`URSATZ_OK`, `REQUEST`, `UNSATISFIABLE`, `NOT_FOUND`, `CONFLICT`, `FORMAT`, `IO`, `CANCELLED`, `ALLOC`, `INTERNAL`), `UrsatzError {message, path}` (path a JSON Pointer or empty), `ursatz_status_name`, `ursatz_status_parse` (its inverse), `ursatz_error_set` (printf style, a NULL error is allowed, returns the status) |
| `files.h` | read a whole file (NUL-terminated), `common_file_write_atomic` (temporary file, fsync, rename, fsync of the folder), `common_file_append_sync`, make a folder (`common_dir_make` with its parents, `common_dir_create` one that must be new), remove a tree, list a folder (sorted, no dot entries), sync a folder, join paths, exists, is a folder |
| `strings.h` | `CommonText` (a growing buffer with a sticky failure flag; printf and JSON-string appends), `CommonNames` (a sorted string list), `common_trim`, `common_compare_ci`, `common_utc_now` (`YYYY-MM-DDTHH:MM:SSZ`) |
| `json.h` | read and write a JSON file (the write is atomic), typed member readers (`common_json_string`, `_long` for whole numbers only, `_bool`), and the one rational codec: `{"num", "den"}` objects and `"n/d"` text into two `long long`, each caller naming the forms it accepts |
| `hash.h` | `common_fnv1a64` and `common_fnv1a64_hex`: the one FNV-1a 64 (moved from `library/model/core/content_hash`) |
| `job.h` | `UrsatzJob`: the progress and cancel callbacks of a long call (replaces `ComposerJob`) |
| `events.h` | the event bus: publishers, subscribers with bounded queues, `reset` on overflow ([jobs and events](../../workshop/docs/jobs-and-events.md#events)) |

Tests are in `common/tests/` (`common_files`, `common_text`, `common_json`, `common_hash`, `common_events`, each with a valgrind twin where it has one). `tools/ci/check-primitives.sh` names `common/hash.c` as the owner of the FNV hash.
