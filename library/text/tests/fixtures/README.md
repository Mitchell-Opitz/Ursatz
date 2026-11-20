# Fixtures of the text group

Layer files the codecs read and write (test `text_layers`); each is made by the command named here, from the repository root after a
build, and never edited by hand.

| File | Made by |
|---|---|
| `loop.plan.json` | `build/cli/ursatz piece composer/tests/fixtures/piece/structure-only.json --seed 1 --layers /tmp/p65-loop`, its `plan.json` |
| `loop.realization.json` | the same command, its `realization.json` |
| `loop.reading.json` | the same command, its `readings/composer.json` |
| `bach-114.realization.json` | `mkdir -p /tmp/p65-bach && cp "tests/fixtures/framework-v1-corpus/Bach 114.mid" /tmp/p65-bach/ && build/cli/ursatz analyze "/tmp/p65-bach/Bach 114.mid" --layers /tmp/p65-bach/layers`, its `realization.json` (the copy keeps `Bach 114.analysis.json` out of the source tree) |
| `bach-114.reading.json` | the same command, its `readings/analyzer.json` |
