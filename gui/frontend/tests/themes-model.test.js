/* Covers the pure theme helpers (public/themes-model.js): the body round trip, the form problems, the without list
 * of the motive switches, the brief from picks, the compose request, the result rows and the MIDI playback.
 * theme-summary.json is the output of `ursatz-composer theme --format json` for a sentence with two slot motives,
 * a style, `without: ["motive-nct-figure"]`, mode style, tempo 132 and --check.
 * Plain Node script: `node frontend/tests/themes-model.test.js`. */
const fs = require("fs");
const path = require("path");
const Vocabulary = require("../public/vocabulary.js");
Vocabulary.set(require("./fixtures/vocabulary.json"));
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

/* development setting */
check(T.emptyState("x").development === "balanced" && body.theme.form.development === "balanced", "the default development is balanced");
const developed = T.bodyFromForm({ ...filled(), development: "theme" });
check(developed.theme.form.development === "theme", "bodyFromForm writes the development");
check(T.formFromBody(developed, "thm-1").development === "theme", "a stored body with a development reads it back");
const older = JSON.parse(JSON.stringify(developed));
delete older.theme.form.development;
check(T.formFromBody(older, "thm-1").development === "balanced", "a stored body without a development reads as balanced");
const odd = JSON.parse(JSON.stringify(developed));
odd.theme.form.development = "wild";
check(T.formFromBody(odd, "thm-1").development === "balanced", "an unknown stored development reads as balanced");
check(T.bodyFromForm({ ...filled(), development: "wild" }).theme.form.development === "balanced", "an unknown development is written as balanced");
check(same(T.DEVELOPMENTS.map((entry) => entry.value), ["melody", "balanced", "theme"]), "the development choices");
check(T.fieldForPath("/development") === "development", "/development marks the development");
check(T.tabOfField("development") === "structure", "the development is on the structure tab");
const devView = T.resultView({
  summary: {
    themes: [
      {
        developments: [
          { motive: "m1", idea: "repetition", development: "restated, exact", segment: null, summary: "repetition: restates the basic idea" },
          { motive: "m2", idea: "fragment 1", development: "fragmented", segment: "b", summary: "fragment 1: segment b, sequenced" },
        ],
      },
    ],
  },
});
check(devView.developments.length === 2 && devView.developments[1].segment === "b" && devView.developments[0].segment === "", "the result lists each idea's segment");
check(devView.developments[0].text === "repetition: restates the basic idea", "a development row shows its summary");
const oldView = T.resultView({ summary: { themes: [{ developments: [{ motive: "m1", idea: "repetition", development: "restated, exact" }] }] } });
check(oldView.developments[0].text === "restated, exact" && oldView.developments[0].segment === "", "a version composed before summaries shows the development text");
check(T.resultView({}).developments.length === 0, "an empty result has no developments");
const fidelityOf = (entry) =>
  T.resultView({ summary: { themes: [{ developments: [{ motive: "m1", idea: "repetition", development: "restated", summary: "repetition: restates the basic idea", ...entry }] }] } }).developments[0];
const agreed = fidelityOf({ fidelity: 0.82, minimum: 0.75, heard: "exact", heard_agrees: true });
check(agreed.intended === "repetition: restates the basic idea" && agreed.heard === "exact", "a development row shows intended against heard");
check(agreed.fidelity === "82%" && agreed.minimum === "75%" && agreed.mismatch === false, "an agreeing idea at or over its minimum is no mismatch");
check(fidelityOf({ fidelity: 0.82, minimum: 0.75, heard: "transposed", heard_agrees: false }).mismatch === true, "a heard that does not agree is a mismatch");
check(fidelityOf({ fidelity: 0.5, minimum: 0.75, heard: "exact", heard_agrees: true }).mismatch === true, "a fidelity under the minimum is a mismatch");
const unmeasured = fidelityOf({ fidelity: null, minimum: null, heard: null, heard_agrees: null });
check(unmeasured.heard === "" && unmeasured.fidelity === "" && unmeasured.minimum === "" && unmeasured.mismatch === false, "an idea that was not measured shows no score and no mismatch");
check(unmeasured.text === "repetition: restates the basic idea", "an idea without a heard keeps its single line");
check(oldView.developments[0].heard === "" && oldView.developments[0].mismatch === false, "a version composed before API 1.10 has no heard and no mismatch");
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
const fiveSlots = JSON.parse(JSON.stringify(require("./fixtures/vocabulary.json")));
fiveSlots.theme_slots.push({ value: "coda", label: "Coda", required: false });
Vocabulary.set(fiveSlots);
check(!T.formProblems(many).some((p) => /at most/.test(p)), "a vocabulary with a fifth slot allows five slots");
check(T.REQUIRED_SLOT === "basic" && T.SLOT_NAMES.length === 5, "the required slot and the slot names are the vocabulary's");
Vocabulary.set(require("./fixtures/vocabulary.json"));
const keyed = T.bodyFromForm({ ...filled(), key: "D", pool: "approved" });
check(!("key" in keyed.theme.form) && !("pool" in keyed.theme), "the body has no key and no pool");
check(!("key" in T.emptyState("x")) && !("pool" in T.emptyState("x")), "the form has no key and no pool");
const old = T.formFromBody({ uses: [], theme: { form: { into: "sentence", key: "D" }, pool: "approved", slots: [] } }, "thm-3");
check(!("key" in old) && !("pool" in old), "an old key and pool are not read");
const edge = filled();
edge.tempo = "400";
check(T.formProblems(edge).length === 0 && T.bodyFromForm(edge).theme.tempo === 400, "the tempo 400 is sent");
edge.tempo = "19";
check(T.formProblems(edge).length === 0 && T.bodyFromForm(edge).theme.tempo === 19, "a tempo out of range is sent for the Composer to refuse");
const noBrief = filled();
noBrief.mode = "style";
check(T.formProblems(noBrief).some((p) => /brief/.test(p)), "the mode from the brief needs a brief");
noBrief.picks = [{ kind: "style", id: "sty-1" }];
check(T.formProblems(noBrief).length === 0, "the mode from the brief works with a brief");
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
check(T.composeRequest(seeded, {}).job === true, "compose is a job");
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

/* Playback is served by the backend from the version's MIDI file (R-275); the fixture is the served JSON of theme.mid */
const playback = JSON.parse(fs.readFileSync(path.join(__dirname, "fixtures", "theme.playback.json"), "utf8"));
check(playback.ok && playback.parts.length === 2 && playback.notes.length > 0, "a served theme playback has parts and notes");
check(playback.tempo[0].qpm === 132 && same(playback.end, [8, 1]), "the served tempo and the 8 bars of 4/4");
check(T.playbackFromMidi === undefined, "the browser no longer reads MIDI files");
check(T.versionPlaybackUrl("thm-1", 2, "theme") === "/api/materials/items/thm-1/versions/2/playback?file=theme", "a version's playback is asked of the backend by file");

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

/* slots and ideas of each form */
function formOf(into, hybrid) {
  const f = T.emptyState("thm-1");
  f.into = into;
  if (hybrid) {
    f.hybrid = hybrid;
  }
  return f;
}
const slotsOf = (f) => T.formSlots(f).join(",");
const ideasOf = (f) => T.formIdeas(f).map((entry) => `${entry.idea}>${entry.slot}`).join(",");
check(slotsOf(formOf("sentence")) === "basic,continuation,cadential", "a sentence has basic, continuation and cadential slots");
check(slotsOf(formOf("period")) === "basic,contrasting", "a period has basic and contrasting slots");
check(slotsOf(formOf("hybrid", "antecedent+continuation")) === "basic,contrasting,continuation,cadential", "antecedent+continuation has all four");
check(slotsOf(formOf("hybrid", "cbi+cadential")) === "basic,contrasting,cadential", "cbi+cadential has no continuation slot");
check(slotsOf(formOf("hybrid", "presentation+consequent")) === "basic,contrasting", "presentation+consequent has basic and contrasting");
check(slotsOf(formOf("hybrid", "presentation+cadential")) === "basic,cadential", "presentation+cadential has basic and cadential");
check(
  ideasOf(formOf("sentence")) === "basic idea>basic,repetition>basic,fragment 1>continuation,fragment 2>continuation,cadential>cadential",
  "the ideas of a sentence",
);
check(
  ideasOf(formOf("period")) ===
    "basic idea>basic,contrasting idea>contrasting,consequent basic idea>basic,consequent contrasting idea>contrasting",
  "the ideas of a period",
);
check(ideasOf(formOf("hybrid", "cbi+cadential")) === "basic idea>basic,contrasting idea>contrasting,cadential 1>cadential,cadential 2>cadential", "the ideas of cbi+cadential");

/* a variant in a slot, the same motif shared */
const shared = filled();
shared.slots = [
  { slot: "basic", item_id: "mot-1", version: 2 },
  { slot: "continuation", item_id: "var-7", version: 4 },
  { slot: "cadential", item_id: "mot-1", version: 2 },
];
const sharedBody = T.bodyFromForm(shared);
check(
  same(sharedBody.theme.slots, [
    { slot: "basic", item_id: "mot-1", version: 2 },
    { slot: "continuation", item_id: "var-7", version: 4 },
    { slot: "cadential", item_id: "mot-1", version: 2 },
  ]),
  "a variant and the same motif in two slots are kept",
);
check(T.formProblems(shared).length === 0, "the same motif in two slots is not a problem");
check(
  T.rootMotif([{ id: "var-7", variant_of: "var-6" }, { id: "var-6", variant_of: "mot-1" }, { id: "mot-1" }], "var-7") === "mot-1",
  "a variant's motif is the end of its variant_of chain",
);
check(
  same(
    T.variantChoices([
      { id: "mot-1", isRoot: true, depth: 0, name: "A", version: 2, derivation: "" },
      { id: "var-6", isRoot: false, depth: 1, name: "A up", version: 3, derivation: "transpose" },
      { id: "var-7", isRoot: false, depth: 2, name: "A up slow", version: 1, derivation: "transpose then augment" },
    ]),
    [
      { value: "var-6", version: 3, label: "A up", title: "transpose" },
      { value: "var-7", version: 1, label: "- A up slow", title: "transpose then augment" },
    ],
  ),
  "the Use choices are the variants below the root",
);

/* ideas in the body, dropped with the form */
const withIdeas = formOf("sentence");
withIdeas.slots = [{ slot: "basic", item_id: "mot-1", version: 2 }];
withIdeas.ideas = [
  { idea: "fragment 2", item_id: "mot-3", version: 1 },
  { idea: "repetition", item_id: "mot-2", version: 5 },
  { idea: "consequent basic idea", item_id: "mot-4", version: 1 },
  { idea: "basic idea", item_id: "mot-9", version: 1 },
];
check(
  same(T.bodyFromForm(withIdeas).theme.ideas, [
    { idea: "repetition", item_id: "mot-2", version: 5 },
    { idea: "fragment 2", item_id: "mot-3", version: 1 },
  ]),
  "the body holds the ideas of the form in order, without the basic idea or other forms' ideas",
);
withIdeas.into = "period";
check(same(T.bodyFromForm(withIdeas).theme.ideas, [{ idea: "consequent basic idea", item_id: "mot-4", version: 1 }]), "changing the form drops the ideas it does not have");
withIdeas.into = "sentence";
const loaded = T.formFromBody(T.bodyFromForm(withIdeas), "thm-1");
check(same(loaded.ideas.map((entry) => entry.idea), ["repetition", "fragment 2"]), "the ideas come back from the body");
check(same(T.bodyFromForm(formOf("sentence")).theme.ideas, []), "a form without ideas has an empty list");
check(T.formProblems({ ...withIdeas, ideas: [{ idea: "repetition", item_id: "", version: 1 }] }).length > 0, "an idea without a motif is a problem");
check(
  same(T.motiveNames(withIdeas, (id) => `n-${id}`), { M1: "n-mot-1", M2: "n-mot-2", M3: "n-mot-3" }),
  "motive ids go over the slots, then the ideas, one per distinct motif",
);

/* the result names idea sources */
const ideaRow = T.resultView({ summary: { themes: [{ slots: [{ idea: "repetition", slot: "basic", motive: "M2", source: "idea" }] }] } }, { motiveNames: { M2: "Two" } });
check(ideaRow.rows[0].source === "From its own motif" && ideaRow.rows[0].motive === "Two", "an idea source reads From its own motif");

/* the chord row (R-208): the cell grid of B2 allows a second chord at @2 in 4/4, @3 in 6/8 and none in 2/4 and 3/4 */
function chordState(chords, meter, bars) {
  const state = T.emptyState("c");
  state.meter = meter || "4/4";
  state.bars = bars || 8;
  Object.entries(chords).forEach(([bar, value]) => {
    state.chords[Number(bar)] = value;
  });
  return state;
}
const eightToFour = chordState({ 1: "IV", 5: "V", 7: "I" });
eightToFour.bars = 4;
check(T.dropChords(eightToFour).join() === "6,8", "bars 8 to 4 drops the chords of bars 6 and 8");
check(eightToFour.chords.slice(4).every((value) => value === ""), "the dropped bars are blank");
check(JSON.stringify(T.bodyFromForm(eightToFour).theme.chords) === '[{"bar":1,"text":"IV"}]', "the body keeps the chords of the bars left");
const chordBody = T.bodyFromForm(chordState({ 0: "I V@2", 2: " IV " }));
check(JSON.stringify(chordBody.theme.chords) === '[{"bar":0,"text":"I V@2"},{"bar":2,"text":"IV"}]', "the body chords are {bar, text} for filled bars only");
check(T.bodyFromForm(T.emptyState("c")).theme.chords.length === 0, "no chords typed gives an empty list");
check(T.formFromBody(chordBody, "c").chords[2] === "IV", "the chords round trip through the body");
check(T.chordsText(T.formFromBody(chordBody, "c")) === "Bar 1: I V@2; Bar 3: IV", "the facts row lists the given chords");

/* refusal paths and their tabs */
check(T.fieldForPath("/slots/basic") === "slot:basic", "a path under /slots/basic marks the basic slot row");
check(T.fieldForPath("/slots/contrasting") === "slot:contrasting", "a path under /slots/contrasting marks the contrasting slot row");
check(T.fieldForPath("/ideas/basic idea") === "idea:basic idea", "a path under /ideas names the idea row");
check(T.fieldForPath("/ideas/a~1b/item_id") === "idea:a/b", "an idea name is read from its JSON Pointer segment");
check(T.fieldForPath("/slots") === "slots" && T.fieldForPath("/slots/9") === "slots", "a slot list or an unknown slot marks the slots");
check(T.fieldForPath("/chords/3") === "chords", "a path under /chords/<index> marks the chord row");
check(T.fieldForPath("/voicing") === "accompaniment", "/voicing marks the accompaniment");
check(T.fieldForPath("/first_cadence") === "firstCadence", "/first_cadence marks the first cadence");
const chordsGiven = chordState({ 0: "I", 3: "V", 7: "V" });
check(T.chordBarOfPath(chordsGiven, "/chords/1") === 3 && T.chordBarOfPath(chordsGiven, "/chords/2/text") === 7, "a chord index names the bar of that entry");
check(T.chordBarOfPath(chordsGiven, "/chords/5") === null && T.chordBarOfPath(chordsGiven, "/bars") === null, "an unknown entry or another path names no bar");
check(T.tabOfField("slot:basic") === "structure" && T.tabOfField("idea:x") === "structure" && T.tabOfField("chords") === "structure", "slots, ideas and chords are on the Structure tab");
check(T.tabOfField("meter") === "theme" && T.tabOfField("accompaniment") === "theme" && T.tabOfField("nothing") === null, "meter and accompaniment are on the Theme tab");
check(T.refusalText("V cannot end a PAC", "/chords/3", "theme") === "on Structure: V cannot end a PAC", "a refusal on another tab names that tab");
check(T.refusalText("V cannot end a PAC", "/chords/3", "structure") === "V cannot end a PAC", "a refusal on the open tab has no tab name");
check(T.refusalText("too slow", "/tempo", "structure") === "on Theme: too slow", "a Theme field refused while Structure is open names the Theme tab");
check(T.refusalText("no", "", "theme") === "no" && T.refusalText("no", "/unknown", "other") === "no", "a refusal without a field keeps its text");

if (failures > 0) {
  console.error(`${failures} check(s) failed`);
  process.exit(1);
}
console.log("themes-model tests passed");
