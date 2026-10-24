/* Covers the pure theme helpers (public/themes-model.js): the body round trip, the form problems, the without list
 * of the motive switches, the brief from picks, the compose request, the result rows and the MIDI playback.
 * theme-summary.json is the output of `ursatz-composer theme --format json` for a sentence with two slot motives,
 * a style, `without: ["motive-nct-figure"]`, mode style, tempo 132 and --check.
 * Plain Node script: `node frontend/tests/themes-model.test.js`. */
const fs = require("fs");
const path = require("path");
const T = require("../public/themes-model.js");

let failures = 0;
function check(cond, what) {
  if (!cond) {
    console.error(`FAIL: ${what}`);
    failures++;
  } else {
    console.log(`PASS: ${what}`);
  }
}
const same = (a, b) => JSON.stringify(a) === JSON.stringify(b);

function filled() {
  const state = T.emptyState("thm-1");
  state.mode = "minor";
  state.slots = [
    { slot: "cadential", item_id: "mot-2", version: 3 },
    { slot: "basic", item_id: "mot-1", version: 2 },
  ];
  return state;
}

/* body round trip */
const state = filled();
state.into = "hybrid";
state.hybrid = "antecedent+continuation";
state.firstCadence = "IAC";
state.accompaniment = { kind: "motive", pattern: "any", item_id: "mot-9", version: 1 };
state.picks = [{ kind: "collection", collectionId: "col-1" }, { kind: "piece", id: "pie-1" }];
state.switches["motive-rhythm"] = false;
state.weight = "1";
state.tempo = "132";
state.check = true;
state.seed = "97";
state.uses = [{ item_id: "mot-1", version: 2 }];
const body = T.bodyFromForm(state);
check(body.theme.form.hybrid === "antecedent+continuation" && body.theme.form.first_cadence === "IAC", "a hybrid keeps its pairing and first cadence");
check(same(body.theme.slots.map((s) => s.slot), ["basic", "cadential"]), "slots are written in slot order");
check(same(body.theme.accompaniment, { pattern: "motive", item_id: "mot-9", version: 1 }), "a motif accompaniment names the item and version");
check(body.theme.tempo === 132 && body.theme.seed === 97 && body.theme.check === true, "tempo, seed and check are written");
check(!("result" in body.theme), "a saved body has no result");
const back = T.formFromBody(body, "thm-1");
const again = T.bodyFromForm(back);
check(same(again, body), "form to body to form to body is stable");
check(back.slots.length === 2 && back.accompaniment.kind === "motive" && back.picks.length === 2, "the form reads slots, accompaniment and picks back");
check(back.switches["motive-rhythm"] === false && back.switches["motive-degrees"] === true, "the form reads the switches back");
const sentence = T.bodyFromForm({ ...filled(), into: "sentence", hybrid: "cbi+cadential", firstCadence: "HC" });
check(sentence.theme.form.hybrid === null && sentence.theme.form.first_cadence === null, "a sentence has no pairing and no first cadence");
check(T.formFromBody({ uses: [], theme: { form: null, slots: [] } }, "thm-2").mode === "major", "a placeholder theme gives the defaults");
check(T.bodyFromForm(T.emptyState("x")).theme.brief === null && T.bodyFromForm(T.emptyState("x")).theme.accompaniment === null, "the defaults have no brief and no accompaniment");

/* problems */
check(same(T.formProblems(filled()), []), "a filled form has no problems");
check(T.formProblems(T.emptyState("x")).some((p) => /basic/.test(p)), "a missing basic slot is a problem");
const twice = filled();
twice.slots = [{ slot: "basic", item_id: "mot-1", version: 1 }, { slot: "continuation", item_id: "mot-1", version: 1 }];
check(same(T.formProblems(twice), []), "one motif in two slots is allowed");
const many = filled();
many.slots = [1, 2, 3, 4, 5].map((n) => ({ slot: "basic", item_id: `mot-${n}`, version: 1 }));
check(T.formProblems(many).some((p) => /at most 4/.test(p)), "more than 4 slots is a problem");
const keyed = T.bodyFromForm({ ...filled(), key: "D", pool: "approved" });
check(!("key" in keyed.theme.form) && !("pool" in keyed.theme), "the body has no key and no pool");
check(!("key" in T.emptyState("x")) && !("pool" in T.emptyState("x")), "the form has no key and no pool");
const old = T.formFromBody({ uses: [], theme: { form: { into: "sentence", key: "D" }, pool: "approved", slots: [] } }, "thm-3");
check(!("key" in old) && !("pool" in old), "an old key and pool are not read");
const badMeter = filled();
badMeter.meter = "5/4";
check(T.formProblems(badMeter).some((p) => /meter/.test(p)), "a bad meter is a problem");
for (const tempo of ["19", "401", "fast", "12.5"]) {
  const bad = filled();
  bad.tempo = tempo;
  check(T.formProblems(bad).some((p) => /tempo/.test(p)), `the tempo ${tempo} is a problem`);
}
const edge = filled();
edge.tempo = "400";
check(T.formProblems(edge).length === 0 && T.bodyFromForm(edge).theme.tempo === 400, "the tempo 400 is accepted");
const noBrief = filled();
noBrief.mode = "style";
check(T.formProblems(noBrief).some((p) => /brief/.test(p)), "the mode from the brief needs a brief");
noBrief.picks = [{ kind: "style", id: "sty-1" }];
check(T.formProblems(noBrief).length === 0, "the mode from the brief works with a brief");
const hybridBad = filled();
hybridBad.into = "hybrid";
hybridBad.hybrid = "cbi+nothing";
check(T.formProblems(hybridBad).some((p) => /hybrid/i.test(p)), "a bad hybrid pairing is a problem");
const noAcc = filled();
noAcc.accompaniment = { kind: "motive", pattern: "any", item_id: "", version: 0 };
check(T.formProblems(noAcc).some((p) => /accompaniment/.test(p)), "a motif accompaniment without a motif is a problem");

/* switches */
check(same(T.withoutFromSwitches(T.emptyState("x").switches), []), "all switches on give an empty without list");
check(
  same(T.withoutFromSwitches({ "motive-degrees": false, "motive-intervals": true, "motive-rhythm": false, "motive-nct-figure": false }), [
    "motive-degrees",
    "motive-rhythm",
    "motive-nct-figure",
  ]),
  "unchecked switches go to without, in order",
);
check(same(T.switchesFromWithout(["motive-intervals"]), { "motive-degrees": true, "motive-intervals": false, "motive-rhythm": true, "motive-nct-figure": true }), "without reads back as switches");
const off = filled();
off.picks = [{ kind: "style", id: "sty-1" }];
off.switches = { "motive-degrees": true, "motive-intervals": false, "motive-rhythm": true, "motive-nct-figure": false };
check(same(T.bodyFromForm(off).theme.without, ["motive-intervals", "motive-nct-figure"]), "the body carries the without list");

/* brief */
check(T.briefFromPicks([]) === null && T.briefFromPicks(undefined) === null, "no picks give no brief");
check(T.briefFromPicks([{ kind: "filter", composer: "", source: "", tag: " " }]) === null, "an empty filter names nothing");
const picks = [
  { kind: "collection", collectionId: "col-1" },
  { kind: "filter", composer: "Bach", source: "", tag: "chorale" },
  { kind: "subject", subjectId: "sub-1" },
  { kind: "project", projectId: "prj-1" },
  { kind: "piece", id: "pie-1" },
  { kind: "piece", id: "pie-1" },
  { kind: "style", id: "sty-1" },
  { kind: "collection", collectionId: "col-1" },
];
const brief = T.briefFromPicks(picks);
check(
  same(brief, {
    scopes: [
      { kind: "collection", collection_id: "col-1" },
      { kind: "filter", filter: { composer: "Bach", tag: "chorale" } },
      { kind: "subject", subject_id: "sub-1" },
      { kind: "project", project_id: "prj-1" },
    ],
    piece_ids: ["pie-1"],
    style_ids: ["sty-1"],
  }),
  "picks make scopes, pieces and styles without repeats",
);
check(T.briefLabel(brief) === "4 scopes, 1 piece, 1 style", "the brief label counts scopes, pieces and styles");
check(T.briefLabel(null) === "No brief", "no brief is labeled");
check(T.picksFromBrief(brief).length === 6, "a brief reads back as picks");
check(T.pickLabel({ kind: "collection", collectionId: "col-1" }, { collections: { "col-1": "Chorales" } }) === "Collection: Chorales", "a pick shows its name");
check(T.pickLabel({ kind: "filter", composer: "Bach", tag: "x" }) === "Composer: Bach, Tag: x", "a filter pick lists its values");

/* requests */
const compose = T.composeRequest(filled(), {});
check(compose.method === "POST" && compose.url === "/api/materials/items/thm-1/compose" && compose.body === "{}", "compose posts an empty body without a seed");
const seeded = filled();
seeded.seed = "97";
check(same(JSON.parse(T.composeRequest(seeded, {}).body), { seed: 97 }), "the form's seed is sent");
check(same(JSON.parse(T.composeRequest(seeded, { seed: 5, update_uses: true }).body), { seed: 5, update_uses: true }), "an option seed wins and update_uses is sent");
check(same(JSON.parse(T.composeRequest(filled(), { seed: "x" }).body), {}), "a seed that is not a number is left out");
const save = T.saveRequest(filled(), " note ");
check(save.url === "/api/materials/items/thm-1/versions" && JSON.parse(save.body).note === "note" && !("key" in JSON.parse(save.body).body.theme.form), "save posts a new version");
check(T.versionFileUrl("thm-1", 3, "theme.musicxml") === "/api/materials/items/thm-1/versions/3/files/theme.musicxml", "version file route");

/* pool */
const tree = {
  id: "mot-1",
  children: [
    { id: "a", derived: { flag: "approved" }, children: [{ id: "b", derived: { flag: "unflagged" }, children: [] }] },
    { id: "c", derived: { flag: "rejected" }, children: [] },
    { id: "d", derived: { flag: "approved" }, children: [] },
  ],
};
check(same(T.poolCounts(tree, "mot-1"), { approved: 2, unflagged: 1, rejected: 1 }), "pool counts the forms below a motif by flag");
check(same(T.poolCounts(tree, "a"), { approved: 0, unflagged: 1, rejected: 0 }), "pool counts below an inner motif");
check(T.poolText({ approved: 2, unflagged: 1 }) === "2 approved, 1 unflagged", "pool text");

/* result */
const summary = JSON.parse(fs.readFileSync(path.join(__dirname, "fixtures", "theme-summary.json"), "utf8"));
const view = T.resultView(
  { seed: 97, summary, brief: { piece_count: 12, skipped: [{ id: "x", reason: "bad" }] }, files: ["theme.mid"] },
  { motiveNames: { A: "Rising line", B: "Falling line" }, check: true },
);
check(view.seed === 97 && view.rows.length === 5, "the result has the seed and a row for each idea");
check(view.rows[0].idea === "basic idea" && view.rows[0].motive === "Rising line" && view.rows[0].connection === "", "the basic idea row has no connection");
check(view.rows[1].connection === "87%" && view.rows[1].source === "slot motif, exact", "a restated idea shows its connection");
check(view.rows[2].source === "registry form" && view.rows[2].chain === "liquidate-features", "a registry form shows its chain");
check(view.rows[4].slot === "Cadential" && view.rows[4].motive === "Falling line", "the cadential row names its slot and motif");
check(view.brief.tempo === "132 (set here)" && view.brief.mode === "major (from the brief)", "the brief tempo and mode say where they came from");
check(view.brief.density === "not set" && view.brief.without === "motive-nct-figure", "the brief density and without list");
check(same(view.distance.map((d) => d.label), ["harmony", "rhythm", "melody", "form", "overall"]) && view.distance[4].value === "0.6640", "the style distance rows");
check(view.heard === "heard as sentence" && view.heardAsRequested === true, "the form heard");
check(view.check === "no contradiction found" && T.resultView({ summary }, {}).check === "not run", "the Analyzer check line");
check(view.pieces === "12 pieces in the brief, 1 skipped", "the brief pieces line");
check(T.resultView(null).rows.length === 0 && T.resultView({}).distance.length === 0, "an empty result gives empty rows");
check(same(T.motiveNames(filled(), (id) => `n-${id}`), { M1: "n-mot-1", M2: "n-mot-2" }), "motive ids follow the slot order, M1 on");

/* sources */
const item = {
  uses_status: [
    { item_id: "mot-1", name: "Alpha", version: 1, current_version: 2 },
    { item_id: "mot-2", name: "Beta", version: 3, current_version: 3 },
    { item_id: "mot-3", name: "Gone", version: 1, current_version: 4, missing: true },
  ],
};
const updates = T.updatedUses(item);
check(updates.length === 1 && updates[0].label === "Source updated: Alpha v2", "an updated source gets a notice");
check(T.updatedUses({}).length === 0, "no uses give no notice");
const moved = T.formWithUpdates(filled(), [{ item_id: "mot-1", current: 5 }]);
check(moved.slots.find((s) => s.slot === "basic").version === 5 && moved.slots.find((s) => s.slot === "cadential").version === 3, "updating moves the named slot motif");

/* MIDI playback */
const midi = fs.readFileSync(path.join(__dirname, "fixtures", "theme.mid"));
const playback = T.playbackFromMidi(midi);
check(playback && playback.ok && playback.parts.length === 2 && playback.notes.length > 0, "a theme MIDI file gives parts and notes");
check(playback && playback.tempo[0].qpm === 132 && same(playback.end, [15360, 1920]), "the MIDI tempo and the 8 bars of 4/4");
check(T.playbackFromMidi(Buffer.from("not midi at all, no")) === null, "bytes that are not MIDI give null");

/* Theme tabs (P19 B5) */
check(same(T.THEME_TABS.map((tab) => tab.id), ["theme", "structure", "used-by", "other"]), "the theme tabs are Theme, Structure, Used by, Other");
const store = {};
globalThis.localStorage = { getItem: (k) => (k in store ? store[k] : null), setItem: (k, v) => { store[k] = v; } };
check(T.readThemeTab() === "theme", "the theme tab defaults to Theme");
T.writeThemeTab("other");
check(T.readThemeTab() === "other" && store["ursatz.theme.tab"] === "other", "a written tab is read back under its own key");
store["ursatz.theme.tab"] = "bogus";
check(T.readThemeTab() === "theme", "an unknown stored tab gives Theme");
delete globalThis.localStorage;
check(T.readThemeTab() === "theme", "no storage gives Theme");

/* The one Save reuses the motif decision */
const M = require("../public/materials-model.js");
check(M.motifSaveDecision({ body: true }, "").kind === "blocked", "a changed theme body without a note is blocked");
check(M.motifSaveDecision({ name: true }, "").kind === "save", "a rename alone saves without a note");
check(M.motifSaveDecision({}, "x").message === "Nothing to save.", "no change says Nothing to save.");

/* Mode default from the basic motif (R-205) */
const minorBody = { motif: { key: "C minor" } };
const majorBody = { motif: { key: "C major" } };
check(T.motifMode(minorBody) === "minor" && T.motifMode(majorBody) === "major" && T.motifMode({}) === null, "a motif body gives its mode");
const fresh = T.emptyState("thm-2");
check(T.applyBasicMode(fresh, false, minorBody) && fresh.mode === "minor", "the first basic motif sets the mode");
const chosen = T.emptyState("thm-2");
chosen.modeChosen = true;
check(!T.applyBasicMode(chosen, false, minorBody) && chosen.mode === "major", "a mode set by hand is kept");
const second = T.emptyState("thm-2");
check(!T.applyBasicMode(second, true, minorBody) && second.mode === "major", "a second basic motif leaves the mode");
check(!T.applyBasicMode(T.emptyState("x"), false, { motif: {} }), "a motif without a key leaves the mode");
check(T.formFromBody(T.bodyFromForm(filled()), "thm-1").modeChosen === true, "a saved theme with a basic motif keeps its mode");
check(T.formFromBody({ uses: [], theme: { form: null, slots: [] } }, "thm-3").modeChosen === false, "a new theme has not chosen its mode");

if (failures > 0) {
  console.error(`${failures} check(s) failed`);
  process.exit(1);
}
console.log("themes-model tests passed");
