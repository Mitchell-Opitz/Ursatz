#!/usr/bin/env python3
"""Converts a verified annotation file (tests/fixtures/annotations/*_v1_expected.txt) into one person's reading (R-630).

    python3 tools/annotations/annotation_to_reading.py <annotation> <analyzer reading.json> <out.json> --person <name>

The target (work, movement and digest of the realization) is copied from the Analyzer's reading of the same piece, so both readings
name one realization. The annotation lines used are `meter <beats> <unit>`, `region <first> <last> <tonic> <major|minor>`,
`cadence <measure> <beat> <type> [optional]` and `readings <m> <b> <type 1> <m> <b> <type 2>`; every other line is ignored. Times are in
whole notes, measures from 1 and no pickup.
"""
import argparse
import json
import sys
from fractions import Fraction

PITCH_CLASS = {"C": 0, "D": 2, "E": 4, "F": 5, "G": 7, "A": 9, "B": 11}
MATERIAL = {"major": "omt.material.major", "minor": "omt.material.natural-minor"}
CADENCE = {
    "pac": "perfect-authentic",
    "iac": "imperfect-authentic",
    "hc": "half",
    "pc": "plagal",
    "dc": "deceptive",
    "subverted": "subverted",
}
HALF = Fraction(1, 2)


def rational(value):
    return {"num": value.numerator, "den": value.denominator}


def tonic_class(name):
    value = PITCH_CLASS[name[0].upper()]
    for mark in name[1:]:
        value += 1 if mark in "#s" else -1 if mark == "b" else 0
    return value % 12


def claim(identifier, start, end, confidence, extra):
    head = {"id": identifier, "start": rational(start), "end": rational(end),
            "confidence": rational(confidence) if confidence is not None else None, "evidence": []}
    head.update(extra)
    return head


def read_annotation(path):
    meter, regions, cadences = None, [], []
    for number, line in enumerate(open(path, encoding="utf-8"), 1):
        words = line.split()
        if not words or words[0].startswith("#"):
            continue
        try:
            if words[0] == "meter":
                meter = (int(words[1]), Fraction(words[2]))
            elif words[0] == "region":
                regions.append((int(words[1]), int(words[2]), words[3], words[4]))
            elif words[0] == "cadence":
                cadences.append((int(words[1]), int(words[2]), CADENCE[words[3]], None, len(words) > 4 and words[4] == "optional"))
            elif words[0] == "readings":
                second = CADENCE.get(words[6])
                cadences.append((int(words[1]), int(words[2]), CADENCE[words[3]], second, True))
        except (IndexError, KeyError, ValueError) as error:
            sys.exit("%s:%d: cannot read the line (%s)" % (path, number, error))
    if meter is None:
        sys.exit("%s: no meter line" % path)
    return meter, regions, cadences


def build(annotation, target, person):
    (beats, unit), regions, cadences = read_annotation(annotation)
    beat, bar = unit, beats * unit
    region_claims = []
    for first, last, tonic, mode in regions:
        region_claims.append(claim("region-%d" % (len(region_claims) + 1), (first - 1) * bar, last * bar, None,
                                   {"center": tonic_class(tonic), "material": MATERIAL[mode], "strength": "established",
                                    "label": "%s %s" % (tonic, mode)}))
    cadence_claims = []
    for measure, beat_number, kind, alternative, uncertain in cadences:
        start = (measure - 1) * bar + (beat_number - 1) * beat
        cadence_claims.append(claim("cadence-%d" % (len(cadence_claims) + 1), start, start + beat, HALF if uncertain else Fraction(1),
                                    {"type": kind, "arrival": rational(start),
                                     "alternatives": [alternative] if alternative else []}))
    return {
        "format": "ursatz-reading",
        "format_version": 1,
        "source": {"kind": "person", "name": person, "producer_key": "", "stages": 0, "knowledge_hash": ""},
        "target": target,
        "regions": region_claims,
        "chords": [],
        "cadences": cadence_claims,
        "groups": [],
        "motive_occurrences": [],
        "meters": [],
        "lines": [],
        "voice_faults": [],
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("annotation")
    parser.add_argument("analyzer_reading")
    parser.add_argument("out")
    parser.add_argument("--person", required=True)
    args = parser.parse_args()
    target = json.load(open(args.analyzer_reading, encoding="utf-8"))["target"]
    reading = build(args.annotation, target, args.person)
    with open(args.out, "w", encoding="utf-8") as out:
        json.dump(reading, out, indent=2)
        out.write("\n")


if __name__ == "__main__":
    main()
