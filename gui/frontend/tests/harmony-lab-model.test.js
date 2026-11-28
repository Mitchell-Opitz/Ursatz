/* Covers the Harmony Lab's model (public/models/harmony-lab-model.js): the request built from the page's fields (the key, the palette's second mode,
 * spans, typed chords joined by bar, shapes, the accompaniment), the chip placed on a unit, the page's state read back from a stored request, the
 * commands and the field a refusal's path is about. Plain Node script, run through ctest: it reads the vocabulary the build generates. */
import Vocabulary from "../public/core/vocabulary.js";
import { generated, readJson } from "./support/fixtures.js";
import H from "../public/models/harmony-lab-model.js";
import { check, same, finish } from "./support/check.js";
import { params } from "./support/command.js";

Vocabulary.set(readJson(generated("vocabulary.json")));

function sad() {
  let state = H.initialState();
  state.material = "omt.material.natural-minor";
  state.withMaterial = "omt.material.dorian";
  state = H.resize(state, { bars: 16 });
  state.spans = [
    { from: 1, to: 12, chords: "shared", schema: "", loop: null },
    { from: 13, to: 13, chords: "color", schema: "", loop: null },
    { from: 14, to: 16, chords: "any", schema: "", loop: null },
  ];
  state.brightness = { shape: "step", at: 13 };
  state.accompaniment = { pattern: "block", voicing: "" };
  return state;
}

/* The key and the new page. */
const fresh = H.initialState();
check(fresh.bars === 8 && fresh.units.length === 8 && fresh.chordsPerBar === 1 && fresh.cadence === "none", "a new page has 8 bars, a chord a bar and a cadence of none");
const state = sad();
check(H.keyText(state) === "C minor", "the key is the tonic's name and the material's key word");
check(H.tonicChoices("omt.material.major").length === 12 && H.tonicName(3, "omt.material.natural-minor") !== "", "the tonics come from the vocabulary");

/* The request. */
const request = H.requestFromState(state);
check(request.key === "C minor" && request.meter === "4/4" && request.bars === 16 && request.cadence === "none" && request.seed === undefined, "the request names key, meter, bars and cadence, and no seed until one is typed");
check(request.palette.with === "dorian" && request.palette.chords_per_bar === undefined && request.palette.chords === undefined, "the second mode is its key word, and nothing typed adds no chords");
check(same(request.palette.spans, [{ bars: [1, 12], chords: "shared" }, { bars: [13, 13], chords: "color" }, { bars: [14, 16], chords: "any" }]), "the spans are the rows");
check(same(request.palette.brightness, { shape: "step", at: 13 }) && request.palette.tension === undefined, "a step shape has its bar and a shape of none is left out");
check(same(request.accompaniment, { pattern: "block" }), "the accompaniment is the pattern");
state.seed = "7";
check(H.requestFromState(state, { suggest: 4 }).seed === "7" && H.requestFromState(state, { suggest: 4 }).suggest === 4, "a seed and the number of variants are added");
state.seed = "-3";
check(H.requestFromState(state).seed === undefined, "a seed that is not a whole number from 0 is left out");

/* Chips and typed chords. */
let typed = H.applyChip(sad(), 1, { text: "III" });
typed = H.applyChip(typed, 12, { text: "IV:dorian" });
check(typed.units[1] === "III" && typed.units[12] === "IV:dorian" && typed.units[0] === "", "a chip puts its text on the unit");
check(same(H.requestFromState(typed).palette.chords, [{ bar: 2, text: "III" }, { bar: 13, text: "IV:dorian" }]), "a typed unit becomes the chord of its bar");
check(H.applyChip(typed, 99, { text: "I" }) === typed && H.applyChip(typed, 0, { text: " " }) === typed, "a unit that is not there and a chip with no text change nothing");
let halves = H.resize(H.initialState(), { chordsPerBar: 2 });
check(halves.units.length === 16 && halves.chordsPerBar === 2, "two chords a bar make two units a bar");
halves = H.applyChip(H.applyChip(halves, 2, { text: "I" }), 3, { text: "V" });
check(same(H.requestFromState(halves).palette.chords, [{ bar: 2, text: "I V@2" }]) && H.requestFromState(halves).palette.chords_per_bar === 2, "units of a bar are joined with their beats");
halves = H.applyChip(H.resize(H.initialState(), { chordsPerBar: 2 }), 1, { text: "V" });
check(same(H.requestFromState(halves).palette.chords, [{ bar: 1, text: "V@2" }]), "a bar whose first unit is open starts its chord on the unit's beat");
check(same(H.chordsPerBarChoices("4/4"), [1, 2, 4]) && same(H.chordsPerBarChoices("3/4"), [1, 3]) && same(H.chordsPerBarChoices("6/8"), [1, 2]), "a meter's choices come from its vocabulary entry");
check(H.resize(H.resize(H.initialState(), { meter: "3/4", chordsPerBar: 3 }), { meter: "4/4", chordsPerBar: 3 }).chordsPerBar === 1, "a chords-a-bar the meter does not take falls back to one");

/* The state read back. */
const back = H.stateFromRequest(H.requestFromState(typed), { id: "hrm-1", version: 2 });
check(back.material === "omt.material.natural-minor" && back.tonic === 0 && back.withMaterial === "omt.material.dorian" && back.bars === 16 && back.item.id === "hrm-1", "a stored request gives the page's fields");
check(same(back.units, typed.units) && same(back.spans, typed.spans) && same(back.brightness, typed.brightness) && back.accompaniment.pattern === "block", "and its chords, spans, shape and accompaniment");
const twice = H.stateFromRequest(H.requestFromState(halves));
check(same(twice.units, halves.units) && twice.chordsPerBar === 2, "chords a bar and typed units survive the round trip");

/* The commands. */
const create = H.createCommand(state, "prj-1");
check(create.url === "/api/workshop/command" && params(create).type === "harmony" && params(create).project_id === "prj-1" && params(create).body.request.key === "C minor" && same(params(create).body.uses, []), "item.create sends the harmony body");
state.item = { id: "hrm-1", version: 1 };
const save = H.saveCommand(state, "Lift");
check(JSON.parse(save.body).target === "hrm-1" && params(save).note === "Lift", "item.save sends it to the item");
const compose = H.composeCommand("hrm-1", "3");
check(compose.job === true && params(compose).seed === 3 && !("seed" in params(H.composeCommand("hrm-1", ""))), "item.compose is a job with the seed when one is typed");
check(same(params(H.keepSchemaCommand("hrm-1", 2, " Sad lift ", "sad-lift", true)), { item: "hrm-1", version: 2, name: "Sad lift", slug: "sad-lift", loop: true }), "knowledge.keep-schema names the version, name, slug and loop");
check(H.slugOf("The Sad Lift!") === "the-sad-lift" && H.slugOf("  ") === "", "a slug is words and hyphens");
const apply = H.applyCommand("thm-1", "hrm-1", 2, "");
check(JSON.parse(apply.body).command === "item.apply-harmony" && JSON.parse(apply.body).target === "thm-1" && !("section" in params(apply)) && params(H.applyCommand("pce-1", "hrm-1", 2, "A")).section === "A", "item.apply-harmony names the plan, its version and, for a piece, the section");
check(same(H.paletteParams(state), { key: "C minor", with: "dorian" }), "the palette's params are the key and the second mode");

/* The refusal paths. */
check(same(H.fieldForPath("/palette/chords/1", typed), { field: "unit", bar: 13 }) && same(H.fieldForPath("/palette/spans/2/schema", typed), { field: "span", index: 2 }), "a chord's path names its bar and a span's its row");
check(H.fieldForPath("/palette/with", typed).field === "with" && H.fieldForPath("/cadence", typed).field === "cadence" && H.fieldForPath("/accompaniment/pattern", typed).field === "accompaniment" && H.fieldForPath("/nothing", typed) === null, "other paths name their field");

/* The palette's chips and the bars' numbers. */
const chips = H.chipsFrom({
  materials: [
    { material: "omt.material.natural-minor", word: "minor", chords: [{ text: "I", numeral: "i", name: "Cm", shared: true, color: false }, { text: "", numeral: "iio", name: "Ddim", shared: false, color: true }] },
    { material: "omt.material.dorian", word: "dorian", chords: [{ text: "IV:dorian", numeral: "IV", name: "F", shared: false, color: true }, { text: "v", numeral: "v", name: "Gm", shared: false, color: false }] },
  ],
});
check(chips.length === 3 && same(chips.map((chip) => chip.kind), ["shared", "color", "own"]) && chips[1].word === "dorian", "a chip is a chord with a text, marked shared, color or own");
check(same(H.measureRows([{ bar: 1, tension: "1", brightness: "9/2" }, { bar: 2, tension: null, brightness: null }]), [{ bar: 1, tension: "1", brightness: "9/2" }, { bar: 2, tension: "-", brightness: "-" }]) && same(H.measureRows(null), []), "a bar's numbers show a dash where it has none");
finish();
