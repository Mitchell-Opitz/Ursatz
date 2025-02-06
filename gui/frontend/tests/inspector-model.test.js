/* Confirms the Inspector's model (inspector-model.js) on the Mozart K. 545
 * fixture: item picking and neighbors, filtering, value formatting, and the
 * header, readings, evidence and scores of a claim. Plain Node script, no
 * dependencies. Run directly: `node frontend/tests/inspector-model.test.js`. */

const InspectorModel = require("../public/inspector-model.js");
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
const model = (id, n) => InspectorModel.buildInspectorModel(layer(id).items[n], layer(id));
const row = (m, key) => m.evidence.find((r) => r.key === key);

/* formatting */
check(InspectorModel.humanizeKey("arrival_beat_strength") === "Arrival beat strength", "a key is humanized");
check(InspectorModel.humanizeKey("region") === "Region", "a one-word key is capitalized");
check(InspectorModel.formatValue("strong") === "strong", "a string stays as it is");
check(InspectorModel.formatValue(0.8951492546) === "0.895", "a number is cut to 3 decimals");
check(InspectorModel.formatValue(4) === "4" && InspectorModel.formatValue(0.5) === "0.5", "short numbers keep no padding");
check(InspectorModel.formatValue(true) === "yes" && InspectorModel.formatValue(false) === "no", "true is yes and false is no");
check(InspectorModel.formatValue(null) === "none" && InspectorModel.formatValue(undefined) === "none", "null is none");
check(InspectorModel.formatValue([43, 44]) === "43, 44" && InspectorModel.formatValue([]) === "none", "a list joins its values");

/* header */
const cadence = model("cadences", 0);
const cadenceClaim = layer("cadences").items[0].claim;
check(cadence.header.layerLabel === "Cadences" && cadence.header.color === layer("cadences").color, "the header names the layer and its color");
check(cadence.header.text === "PAC", "the header title is the item text");
check(cadence.header.label === "perfect-authentic", "the claim label shows when it differs from the title");
check(cadence.header.location === "m26 b4 to m27 b2", "the header has the formatted location");
check(cadence.header.measures === null, "no measure count without a measures field");
const firstPhrase = layer("phrases").items.find((i) => i.claim.label === "phrase");
const phraseHeader = InspectorModel.buildInspectorModel(firstPhrase, layer("phrases")).header;
check(
  phraseHeader.measures === firstPhrase.claim.measures.last - firstPhrase.claim.measures.first + 1,
  "the measure count is last minus first plus one",
);
const sameLabel = InspectorModel.buildInspectorModel({ key: "x/0", text: "same", claim: { label: "same" } }, layer("keys"));
check(sameLabel.header.label === null, "a label equal to the title is null");

/* readings */
check(model("keys", 0).readings === null && model("chords", 0).readings === null, "only cadences have readings");
check(cadence.readings.primary.abbr === "PAC" && cadence.readings.primary.type === "perfect-authentic", "a cadence has its primary reading");
check(cadence.readings.alternative === null, "a cadence without an alternative has none");
check(cadence.readings.primary.share === null, "a missing share is null");
const competing = JSON.parse(JSON.stringify(cadenceClaim));
competing.type = "half";
competing.share = 625;
competing.alternative = { reading: "prolongation", type: null, measure: 27, beat: 1, share: 375, note_ids: [4, 5] };
const withAlt = InspectorModel.buildInspectorModel({ key: "cadences/0", text: "HC", claim: competing }, layer("cadences"));
check(withAlt.readings.primary.abbr === "HC" && withAlt.readings.primary.share === 62.5, "the share is shown in percent");
check(
  withAlt.readings.alternative.abbr === "PROL" && withAlt.readings.alternative.reading === "prolongation" && withAlt.readings.alternative.share === 37.5,
  "a prolongation reading is abbreviated PROL",
);
check(
  withAlt.readings.alternative.noteIds.join() === "4,5" && withAlt.readings.alternative.measure === 27 && withAlt.readings.alternative.beat === 1,
  "the alternative keeps its notes, measure and beat",
);
check(withAlt.evidence.every((r) => r.key !== "alternative" && r.key !== "share"), "readings are not repeated as evidence");

/* evidence */
const keys = cadence.evidence.map((r) => r.key);
check(
  !keys.some((k) => ["framework", "span", "location", "note_ids", "label", "share", "alternative", "uncertainty", "score", "confidence"].includes(k)),
  "evidence leaves out the fields shown elsewhere",
);
check(keys.join() === Object.keys(cadenceClaim).filter((k) => keys.includes(k)).join(), "evidence keeps the claim's field order");
const arrival = row(cadence, "arrival");
check(arrival.kind === "span" && arrival.start === 26 && arrival.end === 26.25, "arrival is a span row with numeric bounds");
check(arrival.location === "m27 b1 to m27 b2", "a span row has its formatted location");
check(row(cadence, "dominant").kind === "span" && row(cadence, "dominant").location === "-", "dominant is a span row without a location");
check(row(cadence, "soprano_degree").kind === "value" && row(cadence, "soprano_degree").text === "1", "a plain field is a value row");
check(row(cadence, "boundary").text === "no", "a boolean field reads yes or no");
check(row(cadence, "arrival_beat_strength").label === "Arrival beat strength", "a row label is humanized");

const phraseClaim = layer("phrases").items.find((i) => i.claim.label === "phrase").claim;
const phraseRows = InspectorModel.buildInspectorModel({ key: "phrases/0", text: "Phrase 1", claim: phraseClaim }, layer("phrases")).evidence;
const subphrases = phraseRows.find((r) => r.key === "subphrases");
check(subphrases.kind === "claims" && subphrases.count === phraseClaim.subphrases.length, "subphrases are a claims row with their count");
check(subphrases.items[0].claim === phraseClaim.subphrases[0], "a claims row keeps the raw sub-claim");
check(subphrases.items[0].start === 0 && subphrases.items[0].end === 4, "a sub-claim has numeric bounds");
check(subphrases.items[0].location === "m1 b1 to m5 b1", "a sub-claim has its formatted location");
const measures = phraseRows.find((r) => r.key === "measures");
check(measures.kind === "group" && measures.rows.map((r) => r.key).join() === "first,last", "an object without a span is a group of rows");
check(phraseRows.find((r) => r.key === "cadence").text === "none", "a null field reads none");
check(phraseRows.find((r) => r.key === "phrase_model").text === "none", "an empty list reads none");

const progression = layer("progressions").items[0];
const progressionModel = InspectorModel.buildInspectorModel(progression, layer("progressions"));
const chordsRow = row(progressionModel, "chords");
check(chordsRow.kind === "value" && /^\d+, \d+/.test(chordsRow.text), "a list of numbers is a value row");

/* scores */
check(cadence.scores.length === 1 && cadence.scores[0].name === "uncertainty" && cadence.scores[0].text === "0.895", "uncertainty is a score under its own name");
const chordModel = model("chords", 0);
check(chordModel.scores.some((s) => s.name === "score"), "a score field is shown as score");
const scored = InspectorModel.buildInspectorModel(
  { key: "x/0", text: "t", claim: { uncertainty: null, confidence: { melodic: 0.5, label: "high" } } },
  layer("keys"),
);
check(scored.scores.length === 1 && scored.scores[0].name === "confidence.melodic", "each numeric confidence is its own score and null uncertainty is skipped");

/* raw */
check(cadence.raw === JSON.stringify(cadenceClaim, null, 2), "raw is the claim as indented JSON");

/* picking, neighbors, filtering */
const spans = [
  { key: "a/0", layerId: "a", start: 0, end: 8, text: "wide", label: null },
  { key: "b/0", layerId: "b", start: 2, end: 4, text: "short", label: "Short one" },
  { key: "c/0", layerId: "c", start: 2, end: 4, text: "tie", label: null },
];
check(InspectorModel.pickItemForNote(spans).key === "b/0", "the shortest span wins, the earlier layer wins a tie");
check(InspectorModel.pickItemForNote([]) === null, "no items pick nothing");
check(InspectorModel.pickItemForNote([spans[0]]).key === "a/0", "one item is picked");

const cadenceItems = layer("cadences").items;
check(InspectorModel.neighborItem(cadenceItems, cadenceItems[0].key, -1) === null, "the first item has no previous");
check(InspectorModel.neighborItem(cadenceItems, cadenceItems[0].key, 1) === cadenceItems[1], "the next item follows by start");
check(InspectorModel.neighborItem(cadenceItems, cadenceItems[2].key, -1) === cadenceItems[1], "the previous item comes before");
const last = cadenceItems[cadenceItems.length - 1];
check(InspectorModel.neighborItem(cadenceItems, last.key, 1) === null, "the last item has no next");
check(InspectorModel.neighborItem(cadenceItems, "missing/0", 1) === null, "an unknown key has no neighbor");
const mixed = [...layer("phrases").items, ...cadenceItems];
check(
  InspectorModel.neighborItem(mixed, cadenceItems[0].key, -1) === null,
  "neighbors stay inside the item's layer",
);

check(InspectorModel.filterItems(spans, "SHORT").length === 1, "filtering ignores case and matches text");
check(InspectorModel.filterItems(spans, "short one").length === 1, "filtering matches the label");
check(InspectorModel.filterItems(spans, "  ").length === 3, "a blank query keeps everything");
check(InspectorModel.filterItems(spans, "zzz").length === 0, "no match gives nothing");
check(InspectorModel.locationOf(cadenceItems[0]) === "m26 b4 to m27 b2", "locationOf formats an item's location");

process.exit(failures === 0 ? 0 : 1);
