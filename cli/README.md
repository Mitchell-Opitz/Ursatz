# ursatz

The one program for Claude sessions and scripts: `ursatz <command>` composes, analyzes and renders music, and reads and changes a data folder. It is the part `cli/`, a thin program over the [workshop](../workshop/docs/README.md): the Composer and the Analyzer run through the workshop's tools, the data folder through a session that opens the folder or connects to the process that holds it. Back to the [README](../README.md).

## Build and run

```sh
cmake -S . -B build
cmake --build build -j
build/cli/ursatz --help
```

The program is `build/cli/ursatz`. `ursatz help <command>` prints the help of one command. The data folder commands take `--data <dir>` (or `URSATZ_DATA`) before the subcommand:

```sh
build/cli/ursatz --data ~/ursatz-data status
```

Its pages are in [docs/README.md](docs/README.md): every command, the global flags, connect or run, exit codes, addresses and the [flag table](docs/flags.md).
