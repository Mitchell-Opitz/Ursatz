# Motive link fixtures

Four small MusicXML pieces for `test_motive_links` and `cli_link_motives.cmake`. Each states one motive three
times in the top line, with a bar of silence (in both voices) between the statements, so the library's motive
analysis finds exactly one family per piece. Written for this repository (same licence as the repository).

| File | Key and meter | Motive |
|---|---|---|
| `a.musicxml` | C major, 4/4 | Degrees 1 3 5 4 3 (`C5 E5 G5 F5 E5`) as quarter, eighth, eighth, quarter, quarter: one bar. |
| `b.musicxml` | G major, 4/4 | `a`'s motive tonally transposed to G and augmented 2:1 (`G4 B4 D5 C5 B4` as half, quarter, quarter, half, half): two bars. Links to `a` by `augment ratio=2:1` once both are moved to the key of C. |
| `c.musicxml` | A minor, 3/4 | Unrelated: degrees 5 1 b6 (`E4 A4 F5`) as dotted quarter, eighth, quarter. Not near enough to any other motive to be compared. |
| `d.musicxml` | C major, 4/4 | `a`'s motive inverted about its first note (`C5 A4 F4 G4 A4`, same rhythm). Links to `a` by `invert tonal=1`. |
