# Readings

`mozart-545.michelle.json` is Michelle's reading of Mozart, Piano Sonata K. 545, first movement, as a reading of the layers (R-630): 10
regions and 5 cadences, no evidence and no detail. It names the realization the Analyzer makes of `tests/fixtures/framework-v1-corpus/Mozart 545.mid`
(work `w1`, movement `m1`, and that realization's digest).

It was made with

```bash
build/analyzer/src/ursatz-analyzer --layers <dir> "tests/fixtures/framework-v1-corpus/Mozart 545.mid"
python3 tools/annotations/annotation_to_reading.py tests/fixtures/annotations/mozart545_v1_expected.txt <dir>/readings/analyzer.json \
    tests/fixtures/readings/mozart-545.michelle.json --person michelle
```

from the verified annotation `tests/fixtures/annotations/mozart545_v1_expected.txt` (the `meter`, `region`, `cadence` and `readings` lines).
The target's digest changes when the realization of an import changes (an import or realization change re-records this file with the
two commands above).
