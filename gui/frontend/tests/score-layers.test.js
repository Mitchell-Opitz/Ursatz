/* Confirms the score layer catalog (score-layers.js) on the Mozart K. 545
 * fixture: each layer's items come from the right sections, cadences sit
 * at their arrival, and unknown sections become Other layers. Plain Node
 * script, no dependencies. Run directly:
 * `node frontend/tests/score-layers.test.js`. */

const ScoreLayers = require("../public/score-layers.js");
const fixture = require("./fixtures/mozart545.analysis.json");

let failures = 0;

function check(cond, what) {
  if (!cond) {
    console.error(`FAIL: ${what}`);
    failures++;
  } else {
    console.log(`PASS: ${what}`);
  }
}

const doc = JSON.parse(JSON.stringify(fixture));
const catalog = ScoreLayers.buildLayerCatalog(doc);
const layer = (id) => catalog.find((l) => l.id === id);
const count = (id) => layer(id).count;
const sum = (list, f) => list.reduce((n, x) => n + f(x), 0);

check(ScoreLayers.LAYERS.length === 14, "the catalog has 14 layers");
check(
  catalog.slice(0, 14).map((l) => l.id).join() === ScoreLayers.LAYERS.map((l) => l.id).join(),
  "catalog layers come first, in table order",
);
check(count("keys") === doc.keys.regions.length, "keys has one item per key region");
check(count("key-changes") === doc.tonicizations.length + doc.modulations.length, "key changes has tonicizations plus modulations");
check(count("chords") === doc.chords.length, "chords has one item per chord");
check(count("cadences") === doc.cadences.length, "cadences has one item per cadence");
check(count("non-chord-tones") === doc.non_chord_tones.length, "non-chord tones has one item per tone");
check(count("prolongations") === doc.prolongations.length, "prolongations has one item per prolongation");
check(
  count("progressions") === sum(doc.progressions, (p) => (p.occurrences || []).length),
  "progressions has one item per occurrence",
);
check(count("form") === doc.form.sections.length, "form has one item per section");
check(count("themes") === doc.themes.length, "themes has one item per theme");
check(
  count("phrases") === doc.phrases.length + sum(doc.phrases, (p) => (p.subphrases || []).length),
  "phrases has phrases plus subphrases",
);
check(count("motives") === sum(doc.motives, (m) => (m.occurrences || []).length), "motives has one item per occurrence");
check(count("schemas") === doc.schemas.length, "schemas has one item per schema");
check(count("texture") === doc.texture.length, "texture has one item per passage");
check(count("melody") === doc.melody.length, "melody has one item per claim");

const keyChanges = layer("key-changes").items;
check(
  keyChanges.filter((i) => i.style === "dashed").length === doc.tonicizations.length &&
    keyChanges.filter((i) => i.style === "solid").length === doc.modulations.length,
  "tonicizations are dashed and modulations solid",
);

check(
  layer("cadences").items.every((i) => i.at === ScoreLayers.rat(i.claim.arrival.span.start)),
  "a cadence mark sits at its arrival start",
);
const noArrival = ScoreLayers.collectLayerItems(
  { cadences: [{ span: { start: { num: 3, den: 2 }, end: { num: 2, den: 1 } }, type: "half", note_ids: [1] }] },
  layer("cadences"),
);
check(noArrival[0].at === 1.5 && noArrival[0].text === "HC", "a cadence without an arrival sits at its span start");
check(
  layer("chords").items.every((i) => i.at === i.start),
  "a chord mark sits at its span start",
);
check(layer("keys").items.every((i) => i.at === undefined), "ribbons have no at");

const phrases = layer("phrases").items;
check(
  phrases.filter((i) => i.parent === null).every((i) => /^Phrase \d+$/.test(i.text)),
  "phrases read Phrase <n>",
);
check(
  phrases.filter((i) => i.parent !== null).every((i) => i.style === "thin" && i.text === i.claim.label),
  "subphrases are thin and read their label",
);
check(layer("motives").items.every((i) => /^Motive \d+$/.test(i.text) && i.parent), "motive occurrences read Motive <n>");
check(layer("progressions").items.every((i) => i.text === i.parent.label), "progression occurrences read the pattern label");

check(
  catalog.every((l) =>
    l.items.every((item, n) => {
      const next = l.items[n + 1];
      return item.key === `${l.id}/${n}` && (!next || item.start < next.start || (item.start === next.start && item.end <= next.end));
    }),
  ),
  "items are sorted by start then end and keyed in that order",
);
check(layer("keys").items[0].noteIds === doc.keys.regions[0].note_ids, "noteIds are the claim's note_ids");

check(ScoreLayers.abbreviateCadence("perfect-authentic") === "PAC", "PAC");
check(ScoreLayers.abbreviateCadence("imperfect-authentic") === "IAC", "IAC");
check(ScoreLayers.abbreviateCadence("half") === "HC", "HC");
check(ScoreLayers.abbreviateCadence("subverted") === "EV", "EV");
check(ScoreLayers.abbreviateCadence("plagal") === "PC", "PC");
check(ScoreLayers.abbreviateCadence("deceptive") === "DC", "DC");
check(ScoreLayers.abbreviateCadence("authentic-unspecified") === "AC", "AC");
check(ScoreLayers.abbreviateCadence("unknown") === "?", "unknown is ?");
check(ScoreLayers.abbreviateCadence(null) === "PROL", "a prolongation reading is PROL");
check(ScoreLayers.abbreviateCadence("evaded-phrygian-half") === "EPH", "an unlisted type gives its initials");

check(ScoreLayers.rat({ num: 3, den: 4 }) === 0.75 && ScoreLayers.rat([5, 2]) === 2.5, "rat reads both forms");
check(
  ScoreLayers.ratCompare([1, 3], { num: 2, den: 6 }) === 0 &&
    ScoreLayers.ratCompare([1, 3], [1, 2]) < 0 &&
    ScoreLayers.ratCompare({ num: 3, den: 4 }, [2, 3]) > 0,
  "ratCompare is exact",
);

const extra = JSON.parse(JSON.stringify(fixture));
extra.cadence_rhymes = [{ span: { start: { num: 0, den: 1 }, end: { num: 2, den: 1 } }, label: "rhyme", note_ids: [0] }];
extra.harmonic_rhythm = { windows: [{ span: { start: { num: 0, den: 1 }, end: { num: 1, den: 1 } } }] };
const extraCatalog = ScoreLayers.buildLayerCatalog(extra);
const rhyme = extraCatalog.find((l) => l.id === "other:cadence_rhymes");
check(
  rhyme && rhyme.label === "Cadence rhymes" && rhyme.group === "other" && rhyme.kind === "ribbon" &&
    rhyme.placement === "above" && rhyme.color === "#6b7280" && rhyme.count === 1,
  "an extra synthetic section becomes an Other layer",
);
check(extraCatalog[extraCatalog.length - 1] === rhyme, "Other layers follow in document key order");
const ignored = ["schema_version", "metadata", "meter", "notes", "chord_vocabulary", "harmonic_rhythm",
  "outer_voices", "markings", "voice_leading", "rhythm"];
check(
  ignored.every((s) => !extraCatalog.some((l) => l.id === `other:${s}`)),
  "ignored sections stay out",
);
check(
  ScoreLayers.discoverOtherLayers(doc).map((l) => l.id).join() === "other:pivot_chords,other:repeats",
  "the fixture's Other layers are pivot chords and repeats, in document key order",
);

const empty = ScoreLayers.buildLayerCatalog({});
check(empty.length === 14 && empty.every((l) => l.empty && l.count === 0), "a document without sections gives empty layers");
check(
  ScoreLayers.collectLayerItems({ keys: { regions: [{ label: "no span" }] } }, layer("keys")).length === 0,
  "items without a span are skipped",
);

/* Packing */
const spans = (list) => list.map(([start, end]) => ({ start, end }));
const nested = spans([[0, 8], [1, 3], [3, 5], [2, 2.5]]);
check(ScoreLayers.packRows(nested).rows === 3, "nested spans use three rows");
check(nested.map((i) => i.row).join() === "0,1,1,2", "a touching item reuses the row whose end equals its start");
const sequential = spans([[0, 1], [1, 2], [2, 3]]);
check(ScoreLayers.packRows(sequential).rows === 1 && sequential.every((i) => i.row === 0), "back-to-back spans share one row");
const crowd = spans([[0, 10], [1, 10], [2, 10], [3, 10], [4, 10], [10, 12]]);
const crowdResult = ScoreLayers.packRows(crowd);
check(crowdResult.rows === 4, "rows stop at four");
check(crowd[4].row === null && crowd[4].overflow === true, "a fifth overlapping item overflows");
check(crowd[5].row === 0 && crowd[5].overflow === false, "a later item fits again");
const two = spans([[0, 4], [1, 4], [2, 4]]);
ScoreLayers.packRows(two, 2);
check(two[2].overflow === true && two[2].row === null, "maxRows is honored");
check(ScoreLayers.packRows([]).rows === 0, "no items use no rows");

/* Note owners */
const notesLayer = (id, ids) => ({ id, items: [{ noteIds: ids }] });
const owners = ScoreLayers.noteColorOwners([
  notesLayer("melody", [1, 2, 3]),
  notesLayer("motives", [2, 3, 4]),
  notesLayer("non-chord-tones", [3, 5]),
]);
check(owners.get(3) === "non-chord-tones", "non-chord tones win over motives and melody");
check(owners.get(2) === "motives" && owners.get(4) === "motives", "motives win over melody");
check(owners.get(1) === "melody" && owners.get(5) === "non-chord-tones", "unshared notes keep their layer");
check(owners.size === 5, "one owner per note");

/* Layer state */
function fakeStorage(initial) {
  const data = { ...initial };
  return {
    data,
    getItem: (k) => (k in data ? data[k] : null),
    setItem: (k, v) => {
      data[k] = String(v);
    },
  };
}
const store = fakeStorage();
const state = ScoreLayers.createLayerState(store);
check(state.enabled().length === 0, "nothing is on by default");
let changes = 0;
const stop = state.onChange(() => changes++);
state.toggle("keys");
check(state.isOn("keys") && changes === 1, "toggle turns a layer on and reports it");
state.setOn("keys", true);
check(changes === 1, "setting an on layer on again reports nothing");
state.setOn("chords", true);
check(JSON.parse(store.data[ScoreLayers.STORAGE_KEY]).join() === "keys,chords", "enabled ids are stored as a JSON array");
const reloaded = ScoreLayers.createLayerState(store);
check(reloaded.enabled().join() === "keys,chords", "a new state reads the stored ids back");
state.toggle("keys");
check(!state.isOn("keys"), "toggle turns a layer off");
stop();
state.toggle("keys");
check(changes === 3, "an unsubscribed listener hears nothing");

state.applyPreset("harmony");
check(state.enabled().join() === "keys,key-changes,chords,cadences", "the harmony preset");
state.applyPreset("structure");
check(state.enabled().join() === "form,themes,phrases,cadences", "the structure preset");
state.applyPreset("none");
check(state.enabled().length === 0, "the none preset turns everything off");

const kept = ScoreLayers.createLayerState(fakeStorage({ [ScoreLayers.STORAGE_KEY]: '["keys","other:cadence_rhymes"]' }));
kept.setCatalog(["keys", "chords"]);
check(kept.enabled().join() === "keys" && !kept.isOn("other:cadence_rhymes"), "an id missing from the catalog is not on");
kept.setOn("chords", true);
kept.setCatalog(["keys", "chords", "other:cadence_rhymes"]);
check(kept.isOn("other:cadence_rhymes") && kept.enabled().length === 3, "it is kept for a catalog that has it");

const throwing = {
  getItem: () => {
    throw new Error("blocked");
  },
  setItem: () => {
    throw new Error("blocked");
  },
};
const guarded = ScoreLayers.createLayerState(throwing);
check(guarded.enabled().length === 0, "a throwing storage reads as empty");
guarded.toggle("keys");
check(guarded.isOn("keys"), "a throwing storage still toggles");
const missing = ScoreLayers.createLayerState(undefined);
missing.toggle("chords");
check(missing.isOn("chords"), "a missing storage behaves as empty");
check(
  ScoreLayers.createLayerState(fakeStorage({ [ScoreLayers.STORAGE_KEY]: "not json" })).enabled().length === 0,
  "unreadable stored text reads as empty",
);

/* Palette: every layer color is readable on the white score paper and
 * distinct from every other (CIE76 distance in Lab, D65 white). */
function srgbToLinear(c) {
  const v = c / 255;
  return v <= 0.04045 ? v / 12.92 : ((v + 0.055) / 1.055) ** 2.4;
}
function linearRgb(hex) {
  return [1, 3, 5].map((i) => srgbToLinear(parseInt(hex.slice(i, i + 2), 16)));
}
function contrastOnWhite(hex) {
  const [r, g, b] = linearRgb(hex);
  const luminance = 0.2126 * r + 0.7152 * g + 0.0722 * b;
  return 1.05 / (luminance + 0.05);
}
function lab(hex) {
  const [r, g, b] = linearRgb(hex);
  const x = (0.4124 * r + 0.3576 * g + 0.1805 * b) / 0.95047;
  const y = 0.2126 * r + 0.7152 * g + 0.0722 * b;
  const z = (0.0193 * r + 0.1192 * g + 0.9505 * b) / 1.08883;
  const f = (t) => (t > 216 / 24389 ? Math.cbrt(t) : (24389 / 27 * t + 16) / 116);
  return [116 * f(y) - 16, 500 * (f(x) - f(y)), 200 * (f(y) - f(z))];
}
function cie76(a, b) {
  const [la, lb] = [lab(a), lab(b)];
  return Math.hypot(la[0] - lb[0], la[1] - lb[1], la[2] - lb[2]);
}
const palette = [...ScoreLayers.LAYERS.map((l) => l.color), extraCatalog.find((l) => l.group === "other").color];
for (const color of palette) {
  const ratio = contrastOnWhite(color);
  check(ratio >= 4.0, `${color} has contrast ${ratio.toFixed(2)} against #ffffff`);
}
let closest = Infinity;
let closestPair = "";
for (let i = 0; i < palette.length; i++) {
  for (let j = i + 1; j < palette.length; j++) {
    const d = cie76(palette[i], palette[j]);
    if (d < closest) {
      closest = d;
      closestPair = `${palette[i]} and ${palette[j]}`;
    }
  }
}
check(closest >= 15, `every pair of colors is at least 15 apart (closest ${closestPair}, ${closest.toFixed(1)})`);

{
  const names = ["schema_version", "metadata", "meter", "notes", "keys", "rhythm", "chords", "harmonic_rhythm", "new_section"];
  const wanted = ScoreLayers.scoreSectionNames(names);
  check(
    JSON.stringify(wanted) === JSON.stringify(["keys", "chords", "new_section", "rhythm"]),
    "the score asks for its layer sections, a section it does not know, and rhythm for the meter check",
  );
  check(!wanted.includes("notes") && !wanted.includes("metadata"), "the score does not ask for the notes or the metadata");
}

if (failures > 0) {
  console.error(`${failures} check(s) failed`);
  process.exit(1);
}
console.log("all score layer checks passed");
