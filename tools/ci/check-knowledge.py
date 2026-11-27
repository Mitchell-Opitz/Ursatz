#!/usr/bin/env python3
"""Knowledge check: tools/ci/check-knowledge.py <knowledge dir> [<json-schema dir>]

Validates every entry under the kind folders of the knowledge dir (target architecture 5): the files parse, the envelope and the
entry anatomy follow entry.schema.json, the content and examples follow the kind's schema, ids are unique and carry their file's
source and folder's kind, relations point at existing ids, citations name a source of sources.json, a material's examples
spell its steps, an instrument's range holds its examples, an ensemble's slots name instruments, a vocabulary entry's code is its
subtype's own, a rule's code is unique in its subtype with its message placeholders and parameters in the allowed form, a texture's category is a texture's code, a progression type's example chords follow its type, a formal function's
harmony ids are progression types and its contrast target a formal function, and a theme type's ideas, units and examples agree with
its functions, and a pattern's rhythm fills one bar of its meter and its accompaniment steps name real meters and members. One line per problem,
"knowledge: <file>: <id>: <what>". Exit 0 clean, 1 with problems, 2 on a missing file.
"""
import json
import os
import re
import sys
from fractions import Fraction

KINDS = {
    "material", "chord-type", "voicing", "vocabulary", "schema", "relation", "function", "theme-type", "form", "operation",
    "pattern", "instrument", "rule", "preset", "term",
}
LETTER = {"C": 0, "D": 2, "E": 4, "F": 5, "G": 7, "A": 9, "B": 11}
NAMES = ["C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"]
KEY_NAME = re.compile(r"^([A-G][#b]?) (.+)$")

problems = []


def problem(path, entry_id, text):
    problems.append("knowledge: %s: %s: %s" % (path, entry_id, text))


def load(path):
    try:
        with open(path, encoding="utf-8") as handle:
            return json.load(handle)
    except FileNotFoundError:
        print("knowledge: %s: missing file" % path)
        sys.exit(2)


def pitch_class(name):
    value = LETTER[name[0]]
    if len(name) > 1:
        value += 1 if name[1] == "#" else -1
    return value % 12


def check_material_examples(path, entry):
    content = entry.get("content", {})
    steps = content.get("steps")
    if not isinstance(steps, list) or not steps:
        return
    if steps != sorted(steps):
        problem(path, entry["id"], "steps are not ascending")
    descending = content.get("descending_steps", steps)
    for number, example in enumerate(entry.get("examples", []), 1):
        if not isinstance(example, dict) or "center" not in example or "pitches" not in example:
            continue
        try:
            center = pitch_class(example["center"])
            ascending = [pitch_class(name) for name in example["pitches"]]
            down = [pitch_class(name) for name in example.get("descending", [])]
        except (KeyError, IndexError, TypeError):
            continue
        if ascending != [(center + step) % 12 for step in steps]:
            problem(path, entry["id"], "example %d: pitches do not match steps" % number)
        if "descending" in example:
            expected = [center] + [(center + step) % 12 for step in reversed(descending[1:])]
            if down != expected:
                problem(path, entry["id"], "example %d: descending does not match the descending steps" % number)


def key_word(entry):
    """The word after the tonic in a key name: the entry's key_word, else its name."""
    return entry.get("content", {}).get("key_word", entry.get("name", "")).lower()


def material_words(materials):
    """Every lowercase word a key name may use for a material (key word, name, aliases) with the ids that use it."""
    words = {}
    for material_id, entry in materials.items():
        for word in [key_word(entry), entry.get("name", "")] + list(entry.get("aliases", [])):
            if isinstance(word, str):
                words.setdefault(word.lower(), set()).add(material_id)
    return words


def parse_key(text, words):
    """'C minor' -> (0, material id), or None when the tonic or the material word is unknown or ambiguous."""
    match = KEY_NAME.match(text) if isinstance(text, str) else None
    if match is None:
        return None
    ids = words.get(match.group(2).lower(), set())
    if len(ids) != 1:
        return None
    return pitch_class(match.group(1)), next(iter(ids))


def show_key(key, materials):
    if key is None:
        return "nothing"
    return "%s %s" % (NAMES[key[0]], key_word(materials[key[1]]))


def tonal_target(content, anchor, materials, words):
    """The key a tonal relation gives from the anchor, or None when it is refused."""
    center, kind = anchor
    steps = materials[kind]["content"]["steps"]
    if "degree" in content:
        if content["degree"] > len(steps):
            return None
        center = (center + steps[content["degree"] - 1]) % 12
    else:
        center = (center + content["root_semitones"]) % 12
    material = content["material"]
    major = words.get("major", set())
    minor = words.get("minor", set())
    if material == "same":
        return center, kind
    if material == "parallel":
        minor_forms = minor | {i for i, m in materials.items() if m.get("content", {}).get("family") == "minor"}
        if kind in major and len(minor) == 1:
            return center, next(iter(minor))
        if kind in minor_forms and len(major) == 1:
            return center, next(iter(major))
        return None
    if material == "diatonic":
        degree = content["degree"] - 1
        if degree >= len(steps) or len(major) != 1 or len(minor) != 1:
            return None
        count = len(steps)
        root = steps[degree]
        third = (steps[(degree + 2) % count] - root) % 12
        fifth = (steps[(degree + 4) % count] - root) % 12
        if (third, fifth) == (4, 7):
            return center, next(iter(major))
        if (third, fifth) == (3, 7):
            return center, next(iter(minor))
        return None
    return (center, material) if material in materials else None


def modal_target(content, anchor, materials):
    center, kind = anchor
    rank = materials[kind].get("content", {}).get("brightness_rank")
    if rank is None:
        return None
    wanted = rank + content["brightness_steps"]
    found = [i for i, m in materials.items() if m.get("content", {}).get("brightness_rank") == wanted]
    if len(found) != 1:
        return None
    if content["center"] != "same":
        center = (center + content["center"]) % 12
    return center, found[0]


def neo_riemannian_target(content, anchor, materials, words):
    major, minor = words.get("major", set()), words.get("minor", set())
    if len(major) != 1 or len(minor) != 1:
        return None
    major, minor = next(iter(major)), next(iter(minor))
    center, kind = anchor
    for move in content["moves"]:
        if kind == major:
            center, kind = {"P": (center, minor), "L": (center + 4, minor), "R": (center - 3, minor)}[move]
        elif kind == minor:
            center, kind = {"P": (center, major), "L": (center - 4, major), "R": (center + 3, major)}[move]
        else:
            return None
        center %= 12
    return center, kind


def check_relation_examples(path, entry, materials, words):
    content = entry.get("content")
    subtype = entry.get("subtype")
    if subtype not in ("tonal", "modal", "neo-riemannian") or not isinstance(content, dict):
        return
    for number, example in enumerate(entry.get("examples", []), 1):
        if not isinstance(example, dict):
            continue
        anchor, target = parse_key(example.get("anchor"), words), parse_key(example.get("target"), words)
        if anchor is None or target is None:
            problem(path, entry["id"], "example %d: '%s' and '%s' are not both keys of a known material" % (
                number, example.get("anchor"), example.get("target")))
            continue
        try:
            if subtype == "tonal":
                result = tonal_target(content, anchor, materials, words)
            elif subtype == "modal":
                result = modal_target(content, anchor, materials)
            else:
                result = neo_riemannian_target(content, anchor, materials, words)
        except (KeyError, TypeError, IndexError):
            continue
        if result is None:
            problem(path, entry["id"], "example %d: %s is refused by this relation" % (number, example["anchor"]))
        elif result != target:
            problem(path, entry["id"], "example %d: %s gives %s, not %s" % (
                number, example["anchor"], show_key(result, materials), example["target"]))


def check_materials(materials, locations):
    """Key words and brightness ranks are one material's own; a rotation-of target is the same collection rotated."""
    seen_words, seen_ranks = {}, {}
    for material_id, entry in sorted(materials.items()):
        path = locations[material_id]
        word = key_word(entry)
        if word in seen_words:
            problem(path, material_id, "key word '%s' is also the key word of %s" % (word, seen_words[word]))
        seen_words.setdefault(word, material_id)
        rank = entry.get("content", {}).get("brightness_rank")
        if rank is not None:
            if rank in seen_ranks:
                problem(path, material_id, "brightness rank %s is also the rank of %s" % (rank, seen_ranks[rank]))
            seen_ranks.setdefault(rank, material_id)
        steps = entry.get("content", {}).get("steps")
        for relation in entry.get("relations", []) if isinstance(entry.get("relations"), list) else []:
            if not isinstance(relation, dict) or relation.get("type") != "rotation-of" or relation.get("target") not in materials:
                continue
            other = materials[relation["target"]].get("content", {}).get("steps")
            if not isinstance(steps, list) or not isinstance(other, list):
                continue
            rotations = [sorted((step - start) % 12 for step in other) for start in other]
            if steps not in rotations:
                problem(path, material_id, "is not a rotation of %s" % relation["target"])


def check_instrument(path, entry):
    """A range runs low to high, a comfortable range sits inside it, and each example's in_range follows the range."""
    content = entry.get("content")
    if not isinstance(content, dict):
        return
    full = content.get("range")
    if not isinstance(full, dict) or not all(isinstance(full.get(k), int) for k in ("lowest", "highest")):
        return
    if full["lowest"] > full["highest"]:
        problem(path, entry["id"], "range lowest %d is above highest %d" % (full["lowest"], full["highest"]))
    comfortable = content.get("comfortable_range")
    if isinstance(comfortable, dict) and all(isinstance(comfortable.get(k), int) for k in ("lowest", "highest")):
        if comfortable["lowest"] > comfortable["highest"]:
            problem(path, entry["id"], "comfortable range lowest %d is above highest %d" % (
                comfortable["lowest"], comfortable["highest"]))
        elif comfortable["lowest"] < full["lowest"] or comfortable["highest"] > full["highest"]:
            problem(path, entry["id"], "comfortable range is not inside the range")
    for number, example in enumerate(entry.get("examples", []), 1):
        if not isinstance(example, dict) or not isinstance(example.get("midi"), int) or not isinstance(example.get("in_range"), bool):
            continue
        if example["in_range"] != (full["lowest"] <= example["midi"] <= full["highest"]):
            problem(path, entry["id"], "example %d: midi %d is %s the range" % (
                number, example["midi"], "not in" if example["in_range"] else "in"))


def check_ensemble(path, entry, instruments):
    """Each slot names an instrument of the folder, slot names are unique, and each example's performers are the slot names."""
    content = entry.get("content")
    slots = content.get("slots") if isinstance(content, dict) else None
    if not isinstance(slots, list):
        return
    names = []
    for slot in slots:
        if not isinstance(slot, dict):
            continue
        if slot.get("instrument") not in instruments:
            problem(path, entry["id"], "slot instrument '%s' is not an instrument entry" % slot.get("instrument"))
        name = slot.get("name")
        if name in names:
            problem(path, entry["id"], "slot name '%s' is used twice" % name)
        names.append(name)
    for number, example in enumerate(entry.get("examples", []), 1):
        if isinstance(example, dict) and isinstance(example.get("performers"), list) and example["performers"] != names:
            problem(path, entry["id"], "example %d: performers do not equal the slot names in order" % number)


PREDOMINANTS = {"ii", "ii6", "ii7", "ii65", "IV", "iv", "iv6"}
NUMERAL = re.compile(r"^(b|#)?(I|II|III|IV|V|VI|VII|i|ii|iii|iv|v|vi|vii)(o|\+)?(6|64|7|65|43|42)?$")


def numeral_base(chord):
    """The numeral without its inversion figure: 'V6' -> 'V', 'ii65' -> 'ii'."""
    return chord.rstrip("0123456789")


def check_progression_example(path, entry, number, example):
    chords = example.get("chords") if isinstance(example, dict) else None
    if not isinstance(chords, list) or not chords or not all(isinstance(chord, str) for chord in chords):
        return
    for chord in chords:
        if not NUMERAL.match(chord):
            problem(path, entry["id"], "example %d: '%s' is not a Roman numeral" % (number, chord))
            return
    code = entry["content"].get("code")
    if code == "prolongation" and numeral_base(chords[0]) != numeral_base(chords[-1]):
        problem(path, entry["id"], "example %d: a prolongation starts and ends on the same numeral" % number)
    if code == "cadential":
        if numeral_base(chords[-1]) not in ("V", "I"):
            problem(path, entry["id"], "example %d: a cadential progression ends on V or I" % number)
        dominants = [i for i, chord in enumerate(chords) if numeral_base(chord) == "V"]
        if not dominants or not any(chord in PREDOMINANTS for chord in chords[:dominants[-1]]):
            problem(path, entry["id"], "example %d: a cadential progression needs a predominant before its V" % number)


def check_vocabulary(vocabulary_entries):
    """Codes are unique within a subtype, a texture's category is a texture's code, and progression examples follow their type."""
    codes = {}
    for path, entry in vocabulary_entries:
        content = entry.get("content")
        code = content.get("code") if isinstance(content, dict) else None
        if not isinstance(code, str):
            continue
        key = (entry.get("subtype"), code)
        if key in codes:
            problem(path, entry["id"], "code '%s' is also the code of %s" % (code, codes[key]))
        codes.setdefault(key, entry["id"])
    for path, entry in vocabulary_entries:
        content = entry.get("content")
        if not isinstance(content, dict):
            continue
        if entry.get("subtype") == "texture" and isinstance(content.get("category"), str):
            if ("texture", content["category"]) not in codes:
                problem(path, entry["id"], "content.category: no texture entry has the code '%s'" % content["category"])
        if entry.get("subtype") == "progression-type":
            for number, example in enumerate(entry.get("examples", []), 1):
                check_progression_example(path, entry, number, example)


RULE_PLACEHOLDERS = {
    "bar", "beat", "first", "last", "count", "value", "limit", "planned", "heard", "idea", "against", "aspect", "amount",
}
RATIO = re.compile(r"^-?[0-9]+/[1-9][0-9]*$")


def check_rule(rule_entries):
    """Codes are unique within a subtype, a message names only the placeholders of the list, an evaluator has at most four parameters
    and each is an integer or n/d with a positive denominator."""
    codes = {}
    for path, entry in rule_entries:
        content = entry.get("content")
        if not isinstance(content, dict):
            continue
        code = content.get("code")
        if isinstance(code, str):
            key = (entry.get("subtype"), code)
            if key in codes:
                problem(path, entry["id"], "code '%s' is also the code of %s" % (code, codes[key]))
            codes.setdefault(key, entry["id"])
        message = content.get("message")
        for name in re.findall(r"\{([^}]*)\}", message) if isinstance(message, str) else []:
            if name not in RULE_PLACEHOLDERS:
                problem(path, entry["id"], "content.message: unknown placeholder {%s}" % name)
        parameters = content.get("parameters")
        if entry.get("subtype") == "evaluator" and isinstance(parameters, dict):
            if len(parameters) > 4:
                problem(path, entry["id"], "content.parameters: %d parameters, at most 4" % len(parameters))
            for name, value in parameters.items():
                whole = isinstance(value, int) and not isinstance(value, bool)
                if not whole and not (isinstance(value, str) and RATIO.match(value)):
                    problem(path, entry["id"], "content.parameters.%s: '%s' is not an integer or n/d with a positive denominator" % (name, value))


def level_of(functions, function_id):
    entry = functions.get(function_id)
    return entry[1].get("content", {}).get("level") if entry is not None else None


def check_formal_function(path, entry, progression_ids, functions, theme_types):
    """Harmony ids are progression-type entries, `against` is a formal function, and an example's theme type lists the idea."""
    content = entry.get("content")
    if not isinstance(content, dict):
        return
    for target in content.get("harmony", []) if isinstance(content.get("harmony"), list) else []:
        if target not in progression_ids:
            problem(path, entry["id"], "content.harmony: '%s' is not a progression-type vocabulary entry" % target)
    contrast = content.get("contrast")
    if isinstance(contrast, dict) and contrast.get("against") not in functions:
        problem(path, entry["id"], "content.contrast.against: '%s' is not a formal-function entry" % contrast.get("against"))
    for number, example in enumerate(entry.get("examples", []), 1):
        if not isinstance(example, dict):
            continue
        theme_type = theme_types.get(example.get("theme_type"))
        if theme_type is None:
            problem(path, entry["id"], "example %d: '%s' is not a theme-type entry" % (number, example.get("theme_type")))
            continue
        ideas = theme_type[1].get("content", {}).get("ideas", [])
        if not any(isinstance(idea, dict) and idea.get("name") == example.get("idea") and idea.get("function") == entry["id"]
                   for idea in ideas):
            problem(path, entry["id"], "example %d: the theme type does not list the idea '%s' with this function" % (
                number, example.get("idea")))


def check_idea_expect(path, entry, idea, functions):
    """K8 slot expectations (R-749): `jobs` and each `chords` list have one item per unit of the idea, a climax or sound unit lies in
    the idea, and a harmony template's type is one of the idea function's progression types."""
    expect = idea.get("expect")
    units = idea.get("units")
    if not isinstance(expect, dict) or not isinstance(units, int):
        return
    where = "content.ideas: idea '%s'" % idea.get("name")
    jobs = expect.get("jobs")
    if isinstance(jobs, list) and len(jobs) != units:
        problem(path, entry["id"], "%s: expect.jobs has %d lists, not one per unit (%d)" % (where, len(jobs), units))
    for number, template in enumerate(expect.get("harmony", []) if isinstance(expect.get("harmony"), list) else [], 1):
        if not isinstance(template, dict):
            continue
        chords = template.get("chords")
        if isinstance(chords, list) and len(chords) != units:
            problem(path, entry["id"], "%s: expect.harmony %d has %d chord texts, not one per unit (%d)" % (
                where, number, len(chords), units))
        function = functions.get(idea.get("function"))
        allowed = function[1].get("content", {}).get("harmony", []) if function is not None else []
        if template.get("type") not in allowed:
            problem(path, entry["id"], "%s: harmony type %s is not one of %s's" % (where, template.get("type"), idea.get("function")))
    for unit in expect.get("climax", []) if isinstance(expect.get("climax"), list) else []:
        if isinstance(unit, int) and unit >= units:
            problem(path, entry["id"], "%s: expect.climax unit %d is outside the idea's %d units" % (where, unit, units))
    for place in expect.get("sounds", []) if isinstance(expect.get("sounds"), list) else []:
        if isinstance(place, dict) and isinstance(place.get("unit"), int) and place["unit"] >= units:
            problem(path, entry["id"], "%s: expect.sounds unit %d is outside the idea's %d units" % (where, place["unit"], units))


def check_theme_type(path, entry, functions):
    """Beginning and ending are formal functions above idea level, each idea's function is of idea level, the units sum to 8 and
    each example's ideas follow in order with bars = units x bars / 8."""
    content = entry.get("content")
    if not isinstance(content, dict) or not isinstance(content.get("ideas"), list):
        return
    for field in ("beginning", "ending"):
        if level_of(functions, content.get(field)) not in ("subphrase", "phrase"):
            problem(path, entry["id"], "content.%s: '%s' is not a formal function of subphrase or phrase level" % (
                field, content.get(field)))
    ideas = [idea for idea in content["ideas"] if isinstance(idea, dict)]
    for idea in ideas:
        if level_of(functions, idea.get("function")) != "idea":
            problem(path, entry["id"], "content.ideas: the function '%s' of '%s' is not a formal function of idea level" % (
                idea.get("function"), idea.get("name")))
    for idea in ideas:
        check_idea_expect(path, entry, idea, functions)
    units = [idea.get("units") for idea in ideas]
    if all(isinstance(unit, int) for unit in units) and sum(units) != 8:
        problem(path, entry["id"], "content.ideas: the units sum to %d, not 8" % sum(units))
        return
    for number, example in enumerate(entry.get("examples", []), 1):
        if not isinstance(example, dict) or not isinstance(example.get("ideas"), list) or not isinstance(example.get("bars"), int):
            continue
        rows = example["ideas"]
        if [row.get("name") if isinstance(row, dict) else None for row in rows] != [idea.get("name") for idea in ideas]:
            problem(path, entry["id"], "example %d: the ideas are not the entry's ideas in order" % number)
            continue
        at = 0
        for row, unit in zip(rows, units):
            bars = unit * example["bars"] / 8
            if not isinstance(row.get("start"), (int, float)) or not isinstance(row.get("end"), (int, float)) or \
                    row["start"] != at or row["end"] - row["start"] != bars:
                problem(path, entry["id"], "example %d: bars of '%s' disagree with its %d units of %d bars" % (
                    number, row.get("name"), unit, example["bars"]))
                break
            at = row["end"]


ROMAN_NUMERALS = ["I", "II", "III", "IV", "V", "VI", "VII"]
CHORD_NUMERAL = re.compile(r"^(I|II|III|IV|V|VI|VII|i|ii|iii|iv|v|vi|vii)o?$")


def stacked_numeral(steps, degree):
    """The numeral of the triad stacked on `degree` of a seven-step collection: upper case major, lower case minor, a trailing o for
    diminished; None for any other stack."""
    if len(steps) != 7 or not 1 <= degree <= 7:
        return None
    root = steps[degree - 1]
    stack = ((steps[(degree + 1) % 7] - root) % 12, (steps[(degree + 3) % 7] - root) % 12)
    name = ROMAN_NUMERALS[degree - 1]
    return {(4, 7): name, (3, 7): name.lower(), (3, 6): name.lower() + "o"}.get(stack)


def numeral_degree(text):
    if not isinstance(text, str) or not CHORD_NUMERAL.match(text):
        return None
    return ROMAN_NUMERALS.index(text.rstrip("o").upper()) + 1


def check_numerals(path, entry_id, field, numerals, steps):
    """Each numeral's case and o are the stacked triad's on that degree."""
    for text in numerals if isinstance(numerals, list) else []:
        degree = numeral_degree(text)
        if degree is None:
            continue
        expected = stacked_numeral(steps, degree)
        if expected != text:
            problem(path, entry_id, "content.%s: '%s' does not match the stacked triad on degree %d ('%s')" % (
                field, text, degree, expected))


def check_modes(materials, locations, vocabulary_entries):
    """A mode's primary and avoided chords are the stacked triads on their degrees, its cadence names a cadence kind of this mode,
    and its characteristic degrees lie within its steps."""
    cadences = {entry["id"]: entry for _, entry in vocabulary_entries if entry.get("subtype") == "cadence-kind"}
    for material_id, entry in sorted(materials.items()):
        content = entry.get("content", {})
        path = locations[material_id]
        steps = content.get("steps")
        for field in ("primary_chords", "chords_to_avoid"):
            check_numerals(path, material_id, field, content.get(field), steps)
        for degree in content.get("characteristic_degrees", []) if isinstance(content.get("characteristic_degrees"), list) else []:
            if isinstance(degree, int) and degree > len(steps):
                problem(path, material_id, "content.characteristic_degrees: degree %d is above the %d steps" % (degree, len(steps)))
        cadence = content.get("cadence")
        if cadence is None:
            continue
        target = cadences.get(cadence)
        if target is None:
            problem(path, material_id, "content.cadence: '%s' is not a cadence-kind entry" % cadence)
        elif target.get("content", {}).get("material") != material_id:
            problem(path, material_id, "content.cadence: '%s' is a cadence of %s, not of this mode" % (
                cadence, target.get("content", {}).get("material")))


def check_cadence_kind(path, entry, materials):
    """A modal cadence kind's approach and arrival, read on its material, are the numerals of its examples."""
    content = entry.get("content", {})
    if "material" not in content:
        return
    material = materials.get(content["material"])
    if material is None:
        problem(path, entry["id"], "content.material: '%s' is not a material entry" % content["material"])
        return
    steps = material.get("content", {}).get("steps")
    numerals = (content.get("approach") if isinstance(content.get("approach"), list) else []) + [content.get("arrival")]
    check_numerals(path, entry["id"], "approach and arrival", numerals, steps)
    derived = []
    for text in numerals:
        degree = numeral_degree(text)
        derived.append(stacked_numeral(steps, degree) if degree is not None else None)
    for number, example in enumerate(entry.get("examples", []), 1):
        if isinstance(example, dict) and isinstance(example.get("numerals"), list) and example["numerals"] != derived:
            problem(path, entry["id"], "example %d: the numerals %s are not the approach and arrival read on %s (%s)" % (
                number, example["numerals"], content["material"], derived))


PATTERN_METERS = {"2/4": Fraction(2, 4), "3/4": Fraction(3, 4), "4/4": Fraction(4, 4), "6/8": Fraction(6, 8)}
PATTERN_DURATIONS = {"w": Fraction(1), "h": Fraction(1, 2), "q": Fraction(1, 4), "e": Fraction(1, 8), "s": Fraction(1, 16), "t": Fraction(1, 32)}


def pattern_duration(text):
    """The length in whole notes of a note-list token (x:q., r:e3, x:h~), or None when it is not one."""
    match = re.fullmatch(r"[xr]:([whqest])(\.{0,2}|3)~?", text)
    if match is None:
        return None
    value = PATTERN_DURATIONS[match.group(1)]
    suffix = match.group(2)
    if suffix == "3":
        return value * 2 / 3
    return value * (2 - Fraction(1, 2 ** len(suffix)))


def check_pattern(path, entry):
    """A rhythm cell's tokens fill one bar of its meter; an accompaniment's by_meter keys are meters and its members 0 to 7."""
    content = entry.get("content", {})
    if entry.get("subtype") == "rhythm":
        rhythm, meter = content.get("rhythm"), content.get("meter")
        if not isinstance(rhythm, str) or meter not in PATTERN_METERS:
            return
        total = Fraction(0)
        for token in rhythm.split():
            length = pattern_duration(token)
            if length is None:
                problem(path, entry["id"], "content.rhythm: '%s' is not a note-list token" % token)
                return
            total += length
        if total != PATTERN_METERS[meter]:
            problem(path, entry["id"], "content.rhythm: the durations fill %s of a whole note, not one bar of %s" % (total, meter))
    elif entry.get("subtype") == "accompaniment":
        lists = [content.get("steps")]
        by_meter = content.get("by_meter")
        if isinstance(by_meter, dict):
            for key, steps in by_meter.items():
                if key not in PATTERN_METERS:
                    problem(path, entry["id"], "content.by_meter: '%s' is not a meter of the schema" % key)
                lists.append(steps)
        for steps in lists:
            for step in steps if isinstance(steps, list) else []:
                if isinstance(step, dict) and "member" in step and not (isinstance(step["member"], int) and 0 <= step["member"] <= 7):
                    problem(path, entry["id"], "content.steps: member %s is not 0 to 7" % step["member"])


def check_voicing(path, entry, chord_type_ids):
    """Members ascend from the root at 0, an example's pitches are the members on its root, and every fit is a chord-type entry."""
    content = entry.get("content", {})
    members = content.get("members")
    semitones = [member.get("semitones") for member in members if isinstance(member, dict)] if isinstance(members, list) else []
    if semitones and all(isinstance(value, int) for value in semitones):
        if semitones[0] != 0 or any(low >= high for low, high in zip(semitones, semitones[1:])):
            problem(path, entry["id"], "content.members: the members are not ascending from 0")
        else:
            for number, example in enumerate(entry.get("examples", []), 1):
                if not isinstance(example, dict) or not isinstance(example.get("root"), str) or \
                        not isinstance(example.get("pitches"), list):
                    continue
                try:
                    root = pitch_class(example["root"])
                    pitches = [pitch_class(name) for name in example["pitches"]]
                except (KeyError, IndexError, TypeError):
                    continue
                if pitches != [(root + value) % 12 for value in semitones]:
                    problem(path, entry["id"], "example %d: pitches are not the members on %s" % (number, example["root"]))
    for fit in content.get("fits", []) if isinstance(content.get("fits"), list) else []:
        if fit not in chord_type_ids:
            problem(path, entry["id"], "content.fits: '%s' is not a chord-type entry" % fit)


def main(argv):
    if len(argv) not in (2, 3):
        print("usage: check-knowledge.py <knowledge dir> [<json-schema dir>]")
        return 2
    root = argv[1]
    schema_dir = argv[2] if len(argv) == 3 else os.path.join(root, "json-schema")
    try:
        import jsonschema
    except ImportError:
        print("knowledge: jsonschema is not installed")
        return 2
    if not os.path.isdir(root):
        print("knowledge: %s: missing file" % root)
        return 2
    sources = load(os.path.join(root, "sources.json"))
    entry_schema = load(os.path.join(schema_dir, "entry.schema.json"))
    source_ids = {source["id"] for source in sources.get("sources", [])}
    schema_cache = {}

    def kind_schema(kind, subtype):
        name = "%s.%s.schema.json" % (kind, subtype) if subtype else "%s.schema.json" % kind
        if name not in schema_cache:
            path = os.path.join(schema_dir, name)
            schema_cache[name] = load(path) if os.path.isfile(path) else None
        return name, schema_cache[name]

    ids = {}
    relations = []
    materials, locations, relation_entries = {}, {}, []
    instrument_ids, ensemble_entries = set(), []
    vocabulary_entries = []
    rule_entries = []
    function_entries, theme_type_entries = {}, {}
    chord_type_ids, voicing_entries, pattern_entries = set(), [], []
    for folder in sorted(os.listdir(root)):
        folder_path = os.path.join(root, folder)
        if not os.path.isdir(folder_path) or folder == "json-schema":
            continue
        files = []
        for directory, _, names in os.walk(folder_path):
            files += [os.path.join(directory, name) for name in names if name.endswith(".json")]
        if folder not in KINDS:
            for file in sorted(files) or [folder_path]:
                problem(os.path.relpath(file, root), "-", "unknown kind folder '%s'" % folder)
            continue
        for file in sorted(files):
            rel = os.path.relpath(file, root).replace(os.sep, "/")
            try:
                with open(file, encoding="utf-8") as handle:
                    document = json.load(handle)
            except ValueError as error:
                problem(rel, "-", "not valid JSON: %s" % error)
                continue
            if not isinstance(document, dict):
                problem(rel, "-", "the envelope is not an object")
                continue
            if document.get("schema") != "ursatz-knowledge":
                problem(rel, "-", "envelope schema is not 'ursatz-knowledge'")
            if document.get("schema_version") != 1:
                problem(rel, "-", "envelope schema_version is not 1")
            source = document.get("source")
            if source not in source_ids:
                problem(rel, "-", "envelope source '%s' is not in sources.json" % source)
            entries = document.get("entries")
            if not isinstance(entries, list):
                problem(rel, "-", "envelope entries is not an array")
                continue
            for entry in entries:
                entry_id = entry.get("id", "-") if isinstance(entry, dict) else "-"
                for error in sorted(jsonschema.Draft202012Validator(entry_schema).iter_errors(entry), key=lambda e: list(e.path)):
                    where = "/".join(str(part) for part in error.path)
                    problem(rel, entry_id, "entry%s: %s" % (" " + where if where else "", error.message))
                if not isinstance(entry, dict) or not isinstance(entry_id, str):
                    continue
                kind, subtype = entry.get("kind"), entry.get("subtype")
                if kind != folder:
                    problem(rel, entry_id, "kind '%s' is not the folder '%s'" % (kind, folder))
                parts = entry_id.split(".")
                if len(parts) == 3:
                    if parts[0] != source:
                        problem(rel, entry_id, "id source part '%s' is not the file's source '%s'" % (parts[0], source))
                    if parts[1] != folder:
                        problem(rel, entry_id, "id kind part '%s' is not the folder '%s'" % (parts[1], folder))
                if entry_id in ids:
                    problem(rel, entry_id, "duplicate id (also in %s)" % ids[entry_id])
                else:
                    ids[entry_id] = rel
                if kind in KINDS:
                    name, schema = kind_schema(kind, subtype)
                    if schema is None:
                        problem(rel, entry_id, "no schema %s for this kind and subtype" % name)
                    else:
                        for error in sorted(jsonschema.Draft202012Validator(schema).iter_errors(entry), key=lambda e: list(e.path)):
                            where = "/".join(str(part) for part in error.path)
                            problem(rel, entry_id, "%s %s: %s" % (name, where, error.message))
                for citation in entry.get("citations", []) if isinstance(entry.get("citations"), list) else []:
                    if isinstance(citation, dict) and citation.get("source") not in source_ids:
                        problem(rel, entry_id, "citation source '%s' is not in sources.json" % citation.get("source"))
                for relation in entry.get("relations", []) if isinstance(entry.get("relations"), list) else []:
                    if isinstance(relation, dict):
                        relations.append((rel, entry_id, relation.get("target")))
                if kind == "material" and subtype == "unordered":
                    check_material_examples(rel, entry)
                    steps = entry["content"].get("steps") if isinstance(entry.get("content"), dict) else None
                    if isinstance(steps, list) and steps and all(isinstance(step, int) for step in steps):
                        materials[entry_id] = entry
                        locations[entry_id] = rel
                if kind == "chord-type":
                    chord_type_ids.add(entry_id)
                if kind == "voicing" and isinstance(entry.get("content"), dict):
                    voicing_entries.append((rel, entry))
                if kind == "pattern" and isinstance(entry.get("content"), dict):
                    pattern_entries.append((rel, entry))
                if kind == "relation":
                    relation_entries.append((rel, entry))
                if kind == "instrument" and subtype == "instrument":
                    instrument_ids.add(entry_id)
                    check_instrument(rel, entry)
                if kind == "instrument" and subtype == "ensemble":
                    ensemble_entries.append((rel, entry))
                if kind == "vocabulary" and isinstance(entry.get("content"), dict):
                    vocabulary_entries.append((rel, entry))
                if kind == "rule" and isinstance(entry.get("content"), dict):
                    rule_entries.append((rel, entry))
                if kind == "function" and subtype == "formal-function":
                    function_entries[entry_id] = (rel, entry)
                if kind == "theme-type":
                    theme_type_entries[entry_id] = (rel, entry)
    for rel, entry_id, target in relations:
        if target not in ids:
            problem(rel, entry_id, "relation target '%s' is not an entry" % target)
    for rel, entry in ensemble_entries:
        check_ensemble(rel, entry, instrument_ids)
    check_vocabulary(vocabulary_entries)
    check_modes(materials, locations, vocabulary_entries)
    for rel, entry in vocabulary_entries:
        if entry.get("subtype") == "cadence-kind":
            check_cadence_kind(rel, entry, materials)
    for rel, entry in voicing_entries:
        check_voicing(rel, entry, chord_type_ids)
    for rel, entry in pattern_entries:
        check_pattern(rel, entry)
    check_rule(rule_entries)
    progression_ids = {entry["id"] for _, entry in vocabulary_entries if entry.get("subtype") == "progression-type"}
    for rel, entry in function_entries.values():
        check_formal_function(rel, entry, progression_ids, function_entries, theme_type_entries)
    for rel, entry in theme_type_entries.values():
        check_theme_type(rel, entry, function_entries)
    check_materials(materials, locations)
    words = material_words(materials)
    for rel, entry in relation_entries:
        check_relation_examples(rel, entry, materials, words)
    for line in problems:
        print(line)
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
