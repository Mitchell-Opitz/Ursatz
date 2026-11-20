/* Covers the pure piece helpers (public/pieces-model.js): the body round trip, the structure problems, the beats
 * labels, the timeline, the default cadences, the compose, regenerate and variations requests and the result rows.
 * piece-summary.json is the output of `ursatz-composer piece --format json` for the request of the loop-and-ending
 * fixture (intro 2 bars, body of 8 and 4 bars from two motives, turnaround 2 bars, ending 2 bars), D minor at tempo
 * 170 from a style, seed 7 and --check.
 * Plain Node script, run through ctest: it reads the vocabulary the build generates (support/fixtures.js). */
const fs = require("fs");
const path = require("path");
const { generated } = require("./support/fixtures.js");
require("../public/vocabulary.js").set(require(generated("vocabulary.json")), require(generated("forms.json")));
const P = require("../public/pieces-model.js");

const { check, same, finish } = require("./support/check.js");

function section(id, part, letter, beats, fill, extra) {
  return { id, part, letter, beats, pickup: "0", cadence: null, fill: fill || { kind: "empty" }, ...extra };
}

function filled() {
  const state = P.emptyState("pce-1");
  state.key = "D";
  state.material = "omt.material.natural-minor";
  state.sections = [
    section("s0", "intro", "I", 8),
    section("s1", "body", "A", 32, { kind: "motif", item_id: "mot-1", version: 2, into: "sentence", hybrid: "" }),
    section("s2", "body", "B", 16, { kind: "motif", item_id: "mot-2", version: 1, into: "sentence", hybrid: "" }),
    section("s3", "turnaround", "T", 8),
    section("s4", "ending", "E", 8, null, { harmony: { kind: "progression", numerals: "I  IV V I", cells: "2", schema: "", chords: [] } }),
  ];
  return state;
}

/* body round trip */
const state = filled();
state.motive = { item_id: "mot-9", version: 3 };
state.pool = "approved";
state.accompaniment = { kind: "pattern", pattern: "alberti" };
state.picks = [{ kind: "collection", collectionId: "col-1" }, { kind: "style", id: "sty-1" }];
state.switches["motive-rhythm"] = false;
state.weight = "1";
state.tempo = "170";
state.check = true;
state.seed = "7";
state.uses = [{ item_id: "mot-1", version: 2 }];
const body = P.bodyFromState(state);
check(Array.isArray(body.uses) && body.uses.length === 1, "the body keeps the uses");
check(P.pieceOfBody(body).structure.key === "D" && P.pieceOfBody(body).structure.material === "omt.material.natural-minor" && !("mode" in P.pieceOfBody(body).structure) && P.pieceOfBody(body).structure.meter === "4/4", "the body holds key, material and meter");
check(P.pieceOfBody(body).structure.sections.length === 5 && P.pieceOfBody(body).structure.sections[1].fill.item_id === "mot-1", "the body holds the sections in order");
check(P.pieceOfBody(body).structure.sections[1].fill.into === "sentence" && P.pieceOfBody(body).structure.sections[1].fill.hybrid === undefined, "a motif of 8 bars keeps its theme form");
check(P.pieceOfBody(body).structure.sections[2].fill.into === "sentence", "a motif of 4 bars keeps its theme form");
const freeState = filled();
freeState.sections[2].beats = 14;
check(P.pieceOfBody(P.bodyFromState(freeState)).structure.sections[2].fill.into === undefined, "a motif of any other length is a free phrase and has no theme form");
check(same(P.pieceOfBody(body).structure.sections[4].harmony, { progression: "I IV V I", cells: 2 }), "numerals are tidied and cells are a number");
check(same(P.pieceOfBody(body).motive, { item_id: "mot-9", version: 3 }) && P.pieceOfBody(body).pool === "approved", "motive and pool are in the body");
check(same(P.pieceOfBody(body).accompaniment, { pattern: "alberti" }), "the accompaniment is a pattern");
check(P.pieceOfBody(body).brief.scopes.length === 1 && P.pieceOfBody(body).brief.style_ids[0] === "sty-1", "the brief comes from the picks");
check(same(P.pieceOfBody(body).without, ["motive-rhythm"]) && P.pieceOfBody(body).style_weight === "1", "the switches and the weight are in the body");
check(P.pieceOfBody(body).tempo === 170 && P.pieceOfBody(body).seed === 7 && P.pieceOfBody(body).check === true, "tempo, seed and check are in the body");
check(P.pieceOfBody(body).result === undefined, "the result is left out");
const back = P.stateFromBody(body, "pce-1");
back.sections[1].fill.hybrid = "";
check(same(P.bodyFromState(back), body), "the body round trips");
check(back.tempo === "170" && back.seed === "7" && back.accompaniment.kind === "pattern", "the state reads text for tempo and seed");

/* placeholders and unfinished fills */
for (const placeholder of [P.requestBodyOf([], { structure: null }), P.requestBodyOf([], { structure: "x" }), P.requestBodyOf([], { structure: [] }), P.requestBodyOf([], { structure: { sections: null } }), null]) {
  const s = P.stateFromBody(placeholder, "pce-2");
  check(s.sections.length === 0 && s.material === "omt.material.major" && s.key === "C", `a placeholder gives the defaults (${JSON.stringify(placeholder)})`);
}
const unfinished = filled();
unfinished.sections[1].fill = { kind: "motif", item_id: "", version: 0, into: "sentence" };
unfinished.sections[2].fill = { kind: "theme", item_id: "", version: 0 };
unfinished.sections[4].harmony = { kind: "progression", numerals: "  ", cells: "", schema: "", chords: [] };
const unfinishedBody = P.bodyFromState(unfinished);
check(P.pieceOfBody(unfinishedBody).structure.sections.every((entry) => entry.fill.kind === "empty" && entry.harmony === undefined), "a fill that names nothing is saved empty, and so is a harmony");
const old = P.stateFromBody(P.requestBodyOf([], { structure: { key: "F", mode: "minor", meter: "3/4", sections: [{ id: "s0", part: "body", letter: "A", beats: 6, pickup: "1", cadence: "HC", fill: { kind: "motif", item_id: "m", version: 1, into: null, hybrid: null } }] } }), "x");
check(old.meter === "3/4" && old.sections[0].cadence === "HC" && old.sections[0].pickup === "1", "meter, pickup and cadence are read");

/* structure problems */
const ok = filled();
const noBody = filled();
noBody.sections = noBody.sections.filter((entry) => entry.part !== "body");
const unordered = filled();
unordered.sections = [unordered.sections[1], unordered.sections[0]];
const short = filled();
short.sections[1].beats = 0;
short.sections[2].beats = 1.5;
const themed = filled();
themed.sections[1].fill = { kind: "theme", item_id: "thm-1", version: 2 };
const form = filled();
form.key = "H";
form.tempo = "10";
form.seed = "-1";
form.material = "style";
form.sections[4].fill.numerals = "I $ V";
form.sections[4].fill.cells = "9";
check(P.pieceOfBody(P.bodyFromState({ ...form, seed: "5", tempo: "10" })).tempo === 10, "a tempo out of range is sent for the Composer to refuse");
check(P.fieldForPath("/key") === "key" && P.fieldForPath("/sections/2/pickup") === "sections" && P.fieldForPath("/x") === null, "request paths name the form fields");
check(P.pickups("2/4").join() === "0,1/4,1/2,1,3/2" && P.pickups("4/4").includes("3"), "the pickups are the Composer's for each meter");
check(P.barBeats("6/8") === 6 && P.barBeats("3/4") === 3, "the beats in a bar come from the meters");

/* beats labels */
check(P.beatsLabel(8, "4/4") === "2 bars" && P.beatsLabel(4, "4/4") === "1 bar", "whole bars");
check(P.beatsLabel(6, "4/4") === "1 bar 2 beats" && P.beatsLabel(3, "4/4") === "3 beats" && P.beatsLabel(1, "4/4") === "1 beat", "beats");
check(P.beatsLabel(6, "3/4") === "2 bars" && P.beatsLabel(6, "6/8") === "1 bar" && P.beatsLabel(7, "2/4") === "3 bars 1 beat", "other meters");

/* timeline */
const timeline = P.sectionTimeline(filled());
check(same(timeline.map((entry) => [entry.id, entry.start, entry.beats]), [["s0", 0, 8], ["s1", 8, 32], ["s2", 40, 16], ["s3", 56, 8], ["s4", 64, 8]]), "the timeline runs section after section");
check(same(P.sectionTimeline(P.emptyState()), []), "no sections give an empty timeline");

/* cadences: the Composer serves those a section leaves unchosen */
const cadences = filled();
const servedRequest = { sections: ["HC", "IAC", "IAC", "HC", "PAC"].map((cadence) => ({ cadence })) };
const served = P.servedCadences(servedRequest, cadences);
check(same(served, ["HC", "IAC", "IAC", "HC", "PAC"]), "the served cadences follow the sections of the reply");
check(same(P.servedCadences(servedRequest, { sections: cadences.sections.slice(1) }), []) && same(P.servedCadences(null, cadences), []), "a reply for other sections, or none, serves nothing");
check(P.cadenceOf(cadences, 4, served) === "PAC" && P.cadenceOf(cadences, 4) === "", "the served cadence shows when none is chosen, and nothing before the reply");
cadences.sections[2].cadence = "PAC";
check(P.cadenceOf(cadences, 2, served) === "PAC" && P.cadenceOf(cadences, 1, served) === "IAC", "a chosen cadence wins over the served one");
check(P.defaultCadence === undefined, "the cadence defaults are not computed here");
const captured = require("../../backend/tests/fixtures/composer/b11_captures.json");
const capturedState = { sections: captured.cadences.parts.map((part, i) => ({ id: `s${i}`, part, cadence: null })) };
const capturedServed = P.servedCadences({ sections: captured.cadences.defaults.map((cadence) => ({ cadence })) }, capturedState);
check(same(capturedState.sections.map((_, i) => P.cadenceOf(capturedState, i, capturedServed)), captured.cadences.defaults), "the cadences shown for an intro, a loop of three sections and two endings equal the captured ones");

/* editing the structure */
let edit = P.emptyState("e");
edit = P.addSection(edit, "body");
edit = P.addSection(edit, "intro");
edit = P.addSection(edit, "body");
edit = P.addSection(edit, "ending");
check(same(edit.sections.map((entry) => entry.part), ["intro", "body", "body", "ending"]), "a section is added at the end of its part");
check(new Set(edit.sections.map((entry) => entry.id)).size === 4 && new Set(edit.sections.map((entry) => entry.letter)).size === 4, "ids and letters are new");
const second = edit.sections[2].id;
edit = P.moveSection(edit, second, -1);
check(edit.sections[1].id === second, "a section moves left inside its part");
edit = P.moveSection(edit, second, -1);
check(edit.sections[1].id === second, "a section does not leave its part");
edit = P.removeSection(edit, second);
check(edit.sections.length === 3, "a section is removed");

/* requests */
check(same(P.composeRequest(state, {}), { method: "POST", url: "/api/materials/items/pce-1/compose", headers: { "Content-Type": "application/json" }, body: JSON.stringify({ seed: 7 }), job: true }), "compose takes the seed of the form, as a job");
check(JSON.parse(P.composeRequest(state, { seed: "12", update_uses: true }).body).seed === 12 && JSON.parse(P.composeRequest(state, { update_uses: true }).body).update_uses === true, "an option seed wins and update_uses is sent");
const blank = P.emptyState("pce-3");
check(P.composeRequest(blank, {}).body === "{}", "no seed gives an empty body");
const save = P.saveRequest(state, " first ");
check(save.url === "/api/materials/items/pce-1/versions" && JSON.parse(save.body).note === "first" && P.pieceOfBody(JSON.parse(save.body).body).structure.sections.length === 5, "save posts a version with the body and the note");
check(P.versionFileUrl("pce-1", 3, "loop.mid") === "/api/materials/items/pce-1/versions/3/files/loop.mid", "version file url");
const regenerate = P.regenerateRequest("pce-1", 2, { section_id: "s1", lane: "melody", seed: "9" });
check(regenerate.request.url.endsWith("/versions/2/regenerate") && same(JSON.parse(regenerate.request.body), { section_id: "s1", lane: "melody", seed: 9 }), "regenerate request");
check(same(JSON.parse(P.regenerateRequest("pce-1", 2, { section_id: "s1", lane: "" }).request.body), { section_id: "s1" }), "regenerate without a lane or seed");
check(P.regenerateRequest("pce-1", 2, { section_id: "s1", seed: "x" }).error && P.regenerateRequest("pce-1", 2, {}).error, "regenerate refuses a bad seed or no section");
const variations = P.variationsRequest("pce-1", 2, { section_id: "s1", lane: "harmony", count: "3", seed: "" });
check(variations.request.url.endsWith("/versions/2/variations") && same(JSON.parse(variations.request.body), { section_id: "s1", lane: "harmony", count: 3 }), "variations request");
check(P.variationsRequest("pce-1", 2, { section_id: "s1", lane: "harmony", count: "9" }).error && P.variationsRequest("pce-1", 2, { section_id: "s1", lane: "", count: "2" }).error, "variations refuse a count over 8 or no lane");
check(P.regenerateRequest("pce-1", 2, { section_id: "s1" }).request.job === true && P.variationsRequest("pce-1", 2, { section_id: "s1", lane: "harmony", count: "3" }).request.job === true && P.chooseRequest("pce-1", 2, 4, 1).job === true, "regenerate, variations and choose are jobs");
check(same(JSON.parse(P.chooseRequest("pce-1", 2, 4, 1).body), { index: 1 }) && P.chooseRequest("pce-1", 2, 4, 1).url.endsWith("/versions/2/variations/4/choose"), "choose request");
check(P.variationFileUrl("pce-1", 2, 4, "variation-1.musicxml").endsWith("/versions/2/variations/4/variation-1.musicxml"), "variation file url");
check(same(P.lanesFor(section("s", "body", "A", 8)), ["harmony", "bass", "melody", "inner"]) && same(P.lanesFor(section("s", "body", "A", 8, { kind: "theme" })), ["harmony", "bass"]) && P.lanesFor(section("e", "ending", "E", 8), [{ id: "s" }]).length === 0, "lanes by section");
const cards = P.variationCards({ set: 1, variations: [{ index: 2, seed: 5, score: { numerator: 8, denominator: 12 }, seam: { held: 3 }, heard: "half" }] });
check(cards[0].title === "#2 · seed 5" && cards[0].score === "8 of 12" && cards[0].seam === "3 seam rules held" && cards[0].heard === "half", "variation cards");

/* result */
const summary = JSON.parse(fs.readFileSync(path.join(__dirname, "fixtures", "piece-summary.json"), "utf8"));
const view = P.resultView({ seed: 7, summary, files: ["loop.mid"], brief: { piece_count: 2, skipped: ["x"] } }, { sourceNames: {} });
check(view.seed === 7 && view.key === "D minor" && view.loop === "bar 3 to bar 17" && view.loopLength === "14 bars", "result facts and the loop region");
check(view.brief.tempo === "170 (from the brief)" && view.brief.mode === "minor (set here)" && view.brief.density === "2 attacks per beat" && view.brief.without === "none", "result brief");
check(view.pieces === "2 pieces in the brief, 1 skipped", "result brief pieces");
const loop = view.versions[0].view;
check(same(loop.rows.map((row) => row.id), ["intro", "a", "b", "t"]) && loop.rows[1].start === "bar 3" && loop.rows[1].length === "8 bars", "loop rows");
check(loop.rows[1].cadence === "IAC" && loop.rows[1].heard === "imperfect-authentic" && loop.rows[1].fill === "motif" && loop.rows[1].source === "own motive", "loop row cadence, fill and source");
check(loop.rows[3].part === "turnaround" && loop.rows[3].cadence === "HC" && loop.rows[3].heard === "half", "turnaround row");
check(loop.seam.held === 3 && loop.seam.label === "3 of 4 seam rules held" && loop.seam.rules.filter((rule) => !rule.held).length === 1, "seam rules");
check(loop.score.label === "16 of 21" && loop.check.label === "5 of 5 planned items heard", "score and Analyzer check");
check(loop.distance.length === 5 && loop.distance[4].label === "overall" && loop.distance[4].value === "0.5454", "style distance");
const ending = view.versions[1].view;
check(ending.rows[3].part === "ending" && ending.rows[3].heard === "none (the piece ends)" && ending.seam === null && ending.score.label === "6 of 6", "ending version");
check(ending.rows[3].start === "bar 15", "ending starts after the body");
const withTheme = JSON.parse(JSON.stringify(summary));
withTheme.versions.loop.sections[1].fill = "theme";
withTheme.versions.loop.sections[1].source = "/data/materials/items/pce-1/v2/theme-a.plan.json";
check(P.resultView({ summary: withTheme }, { sourceNames: { a: "Theme One v2" } }).versions[0].view.rows[1].source === "Theme One v2", "a theme source is named by its section");
check(
  JSON.stringify(view.playLoop) === JSON.stringify({ start: 2, end: 16, bodyEnd: 14 }) && P.resultView({ summary: { versions: {} } }).playLoop === null,
  "the playback loop is the loop region and the end of the last body section, in whole notes",
);
const empty = P.resultView(null);
check(empty.seed === null && empty.versions.length === 0 && empty.loop === "" && empty.files.length === 0, "no result gives no rows");

/* forms (forms.json is generated from the Composer's forms route answer) */
const roundedState = P.withForm(P.emptyState("pce-2"), "rounded-binary");
roundedState.key = "A";
roundedState.material = "omt.material.natural-minor";
roundedState.meter = "3/4";
roundedState.bars = 24;
roundedState.modulate = true;
roundedState.motive = { item_id: "mot-9", version: 3 };
const roundedBody = P.bodyFromState(roundedState);
check(
  same(P.pieceOfBody(roundedBody).structure, { form: "rounded-binary", key: "A", material: "omt.material.natural-minor", meter: "3/4", bars: 24, count: 0, modulate: true }),
  "a sized form's body holds its form, size and modulate and no sections",
);
const roundedBack = P.stateFromBody(roundedBody, "pce-2");
check(
  roundedBack.form === "rounded-binary" && roundedBack.bars === 24 && roundedBack.count === 0 && roundedBack.modulate === true && roundedBack.sections.length === 0 &&
    roundedBack.key === "A" && roundedBack.material === "omt.material.natural-minor" && roundedBack.meter === "3/4" && roundedBack.motive.item_id === "mot-9",
  "a rounded binary state round-trips through its body",
);
check(same(P.bodyFromState(roundedBack), roundedBody), "a rounded binary body round-trips through its state");
const noModulate = P.withForm(roundedState, "loop");
check(noModulate.form === "loop" && noModulate.modulate === false && noModulate.bars === 0, "a form that cannot modulate drops modulate and the size");
const gameBody = P.bodyFromState(filled());
check(P.pieceOfBody(gameBody).structure.form === "game-loop" && P.pieceOfBody(gameBody).structure.sections.length === 5 && P.pieceOfBody(gameBody).structure.bars === undefined, "a game loop body names its form and keeps its sections");
const oldBody = JSON.parse(JSON.stringify(gameBody));
delete P.pieceOfBody(oldBody).structure.form;
const oldState = P.stateFromBody(oldBody, "pce-1");
check(oldState.form === "game-loop" && oldState.sections.length === 5 && oldState.sections[4].part === "ending", "a body without a form is the game loop");
const sizes = P.sizeOptions(P.formOf("variations"));
check(sizes.length === 8 && sizes[2].label === "3 variations, 32 bars" && sizes[2].default && sizes[0].label === "1 variation, 16 bars", "the sizes of the variations form");
check(same(P.sizeOptions(P.formOf("rounded-binary")).map((size) => size.label), ["16 bars", "24 bars"]) && P.sizeOptions(P.formOf("game-loop")).length === 0, "sizes of other forms");
check(P.sectionsOf(P.formOf("rounded-binary"), 0, 0).length === 3 && P.sectionsOf(P.formOf("variations"), 0, 4).length === 5 && P.sectionsOf(P.formOf("game-loop"), 0, 0).length === 0, "the sections of a size");
const noBody2 = P.emptyState("pce-3");
const reordered = filled();
reordered.sections.reverse();
check(P.addSection(P.emptyState("pce-4"), "intro").sections.length === 1 && P.rolesOf(P.formOf("game-loop")).length === 4 && P.defaultRole(P.formOf("game-loop")) === "body", "the roles are the form's");
const oneVersion = { meter: "4/4", key: "C major", versions: { piece: { plan: "plan.json", sections: [{ id: "A", role: "theme", letter: "A", start: "0/1", beats: 32, cadence: "PAC", heard: "perfect-authentic" }] } } };
const oneView = P.resultView({ summary: oneVersion }, { form: "rounded-binary" });
check(oneView.versions.length === 1 && oneView.versions[0].name === "piece" && oneView.versions[0].label === "Piece" && !oneView.versions[0].loop && oneView.playLoop === null, "a one-version summary has no loop and one version piece");
check(oneView.versions[0].view.rows[0].partLabel === "Theme" && oneView.versions[0].view.rows[0].part === "theme", "a row of a sized form names its role");
check(same(view.versions.map((version) => version.name), ["loop", "ending"]) && view.versions[0].loop && view.versions[1].after === "loop" && view.versions[1].label === "Ending version", "the game loop has its loop and its ending after it");
const ternarySummary = JSON.parse(fs.readFileSync(path.join(__dirname, "fixtures", "ternary-summary.json"), "utf8"));
const ternaryRows = P.resultView({ seed: 3, summary: ternarySummary }, { form: "ternary" }).versions[0].view.rows;
check(
  ternaryRows.length === 3 && ternaryRows[0].keyHeard === "C major" && ternaryRows[1].keyHeard === "G major" && ternaryRows[0].pedal === "" && ternaryRows[1].pedal === "Dominant pedal" && ternaryRows[2].pedal === "",
  "a sized piece row carries the key heard and the label of the pedal of its section",
);
check(
  ternaryRows[0].returns === "" && ternaryRows[0].changes === "" && ternaryRows[2].returns === "A" && ternaryRows[2].changes === "varied: 6 notes added, 2 of 8 chords changed",
  "a returning row names its section and how it differs",
);
check(
  view.versions[0].view.rows.every((row) => row.keyHeard === "" && row.pedal === "" && row.returns === "" && row.changes === "") && view.versions[0].view.rows[2].heard === "perfect-authentic",
  "a game loop summary's rows have no section details",
);
const actions = P.actionSections(filled(), [{ id: "s0" }, { id: "s1" }, { id: "s2" }, { id: "s3" }]);
check(same(actions.map((entry) => entry.id), ["s0", "s1", "s2", "s3"]) && actions[1].lanes.length === 4 && actions[1].length === "8 bars", "regenerate is offered for the sections of the first version only");
check(same(P.actionSections(roundedState, oneView.versions[0].view.rows).map((entry) => entry.id), ["A"]), "a sized form offers the sections of its result");

/* sources and themes */
const updates = P.updatedUses({ uses_status: [{ item_id: "mot-1", name: "Motif A", version: 1, current_version: 2 }, { item_id: "thm-1", version: 2, current_version: 2 }, { item_id: "x", version: 1, current_version: 4, missing: true }] });
check(updates.length === 1 && updates[0].label === "Source updated: Motif A v2", "an updated source is offered");
const themeBody = { request: { into: "hybrid", hybrid: "presentation+continuation", key: "D", material: "omt.material.natural-minor", meter: "4/4", bars: 8 }, result: { seed: 1 } };
check(P.themeLengthBeats(themeBody) === 32 && P.themeLengthBeats({ request: { bars: 4, meter: "3/4" } }) === 12 && P.themeLengthBeats({}) === null, "a theme's length in beats");
check(P.themeFormText(themeBody) === "hybrid (presentation+continuation) in D minor, 4/4, 8 bars" && P.themeComposed(themeBody) && !P.themeComposed({ request: {}, result: null }), "a theme's form and whether it was composed");
check(P.motifIsFree(section("s", "body", "A", 6)) && !P.motifIsFree(section("s", "body", "A", 32)) && P.motifIsFree(section("s", "body", "A", 32, null, { pickup: "1" })), "a motif fill is free unless 4 or 8 bars without a pickup");

/* the construction of a motif fill (P47, R-468) */
const sixBars = (fill) => section("s", "body", "A", 24, { kind: "motif", item_id: "m", version: 1, into: "sentence", hybrid: "", ...fill });
check(P.motifIsFree(sixBars({})) && !P.motifIsFree(sixBars({ construction: "auto" })), "6 bars are a free phrase until a construction is chosen");
check(!P.motifIsFree(sixBars({ construction: { ideas: {}, extensions: [] } })), "6 bars with a given construction are a theme");
check(P.motifIsFree({ ...sixBars({ construction: "auto" }), pickup: "1" }), "a section with a pickup is a free phrase whatever its construction");
check(P.motifIsFree(section("s", "body", "A", 26, { kind: "motif", item_id: "m", version: 1, construction: "auto" })), "a section of part bars is a free phrase whatever its construction");
check(P.motifIsFree(section("s", "body", "A", 68, { kind: "motif", item_id: "m", version: 1, construction: "auto" })), "17 bars are a free phrase whatever its construction");
check(P.constructionPossible(sixBars({})) && !P.constructionPossible(section("s", "body", "A", 8)), "a section of 3 to 16 whole bars can have a construction");
check(P.constructionChoice(sixBars({}).fill) === "default" && P.constructionChoice(sixBars({ construction: "auto" }).fill) === "auto" && P.constructionChoice(sixBars({ construction: { ideas: {}, extensions: [] } }).fill) === "given", "the three choices read from the fill");
const constructed = P.emptyState("c");
constructed.sections = [sixBars({ construction: "auto" })];
check(P.pieceOfBody(P.bodyFromState(constructed)).structure.sections[0].fill.construction === "auto", "a chosen construction is sent as auto");
constructed.sections = [sixBars({ construction: { ideas: { "basic idea": 2, repetition: 1, "fragment 1": 1, "fragment 2": 1, cadential: 1, stray: 9 }, extensions: [{ kind: "evaded", idea: "", bars: 0, varied: false }] } })];
const givenFill = P.pieceOfBody(P.bodyFromState(constructed)).structure.sections[0].fill;
check(same(givenFill.construction, { ideas: { "basic idea": 2, repetition: 1, "fragment 1": 1, "fragment 2": 1, cadential: 1 }, extensions: [{ kind: "evaded", bars: 0 }] }), "a given construction is sent as the ideas of the pairing and its extensions");
check(same(P.bodyFromState(P.stateFromBody(P.bodyFromState(constructed), "c")), P.bodyFromState(constructed)), "a given construction round trips through the body");
constructed.sections = [sixBars({ construction: "auto", into: "period" })];
check(P.stateFromBody(P.bodyFromState(constructed), "c").sections[0].fill.construction === "auto", "a chosen construction reads back");
constructed.sections = [sixBars({ construction: "auto", pickup: "1" })];
constructed.sections[0].pickup = "1";
check(P.pieceOfBody(P.bodyFromState(constructed)).structure.sections[0].fill.construction === undefined, "a free phrase sends no construction");
constructed.sections = [section("s", "body", "A", 32, { kind: "motif", item_id: "m", version: 1, into: "sentence", hybrid: "" })];
check(P.pieceOfBody(P.bodyFromState(constructed)).structure.sections[0].fill.construction === undefined, "a theme of 8 bars with no construction sends no member");
const oldPiece = P.requestBodyOf([], { structure: { form: "game-loop", key: "C", mode: "minor", meter: "4/4", sections: [{ id: "s0", part: "body", letter: "A", beats: 32, pickup: "0", cadence: null, fill: { kind: "motif", item_id: "m", version: 1, into: "sentence", hybrid: null } }] } });
check(P.stateFromBody(oldPiece, "o").sections[0].fill.construction === null, "a saved piece without the member reads as no construction");
check(P.pieceOfBody(P.bodyFromState(P.stateFromBody(oldPiece, "o"))).structure.sections[0].fill.construction === undefined, "a saved piece without the member is sent as before");
check(same(P.sectionConstruction({ construction: { halves: [4, 6], text: "A 10-bar period." } }), { halves: "4+6", text: "A 10-bar period." }), "a section's construction reads its halves and reason");
check(P.sectionConstruction({ construction: "auto" }).text === "Chosen by the Composer" && P.sectionConstruction({}).text === "" && P.sectionConstruction(null).halves === "", "a section without a construction shows nothing");
const rowView = P.resultView({ seed: 1, summary: { meter: "4/4", versions: { loop: { sections: [{ id: "s0", part: "body", start: "0", beats: 24, construction_text: "A 6-bar sentence." }, { id: "s1", part: "ending", start: "6", beats: 8 }] } } } }).versions[0].view;
check(rowView.rows[0].construction === "A 6-bar sentence." && rowView.rows[1].construction === "", "a section row with construction_text shows it and one without shows nothing");

/* The minuet and the trio (R-485): the form's meters and tempo, the performed rows. */
const minuetForm = P.formOf("minuet");
const ternaryForm = P.formOf("ternary");
check(same(P.meterChoices(minuetForm), ["3/4"]) && same(P.meterChoices(ternaryForm), P.METERS) && P.meterChoices(ternaryForm).length === 4, "a form that lists its meters offers those, any other offers every meter");
check(same(P.meterChoices(null), P.METERS), "no form offers every meter");
check(P.formTempo(minuetForm) === 112 && P.formTempo(ternaryForm) === null && P.formTempo(null) === null, "a form gives its tempo or none");
check(P.isPerformed(minuetForm) && P.isPerformed(P.formOf("minuet-and-trio")) && !P.isPerformed(ternaryForm) && !P.isPerformed(P.formOf("game-loop")) && !P.isPerformed(null), "a form with written-out repeats or parts is performed");
check(
  same(P.performedParts(P.formOf("minuet-and-trio")).map((part) => part.name), ["Minuet", "Trio", "Minuet da capo"]) &&
    same(P.performedParts(P.formOf("minuet-and-trio")).map((part) => part.form), ["minuet", "trio", ""]) &&
    P.performedParts(P.formOf("minuet-and-trio"))[2].again === "Minuet" &&
    P.performedParts(P.formOf("minuet-and-trio"))[1].text.includes("in the trio key") &&
    P.performedParts(ternaryForm).length === 0,
  "a compound form lists its parts and a form without parts lists none",
);
const minuetSummary = JSON.parse(fs.readFileSync(path.join(__dirname, "fixtures", "minuet-summary.json"), "utf8"));
const minuetRows = P.resultView({ seed: 1, summary: minuetSummary }, { form: "minuet-and-trio", bars: 24, count: 0 }).versions[0].view.rows;
check(
  minuetRows.length === 15 &&
    same(minuetRows.map((row) => row.partName), [...Array(6).fill("Minuet"), ...Array(6).fill("Trio"), ...Array(3).fill("Minuet da capo")]) &&
    minuetRows[0].repeatOf === "" &&
    minuetRows[1].repeatOf === "A" &&
    minuetRows[7].repeatOf === "Trio A" &&
    minuetRows[2].upbeat === "V to I" &&
    minuetRows[8].upbeat === "V to i" &&
    minuetRows[0].upbeat === "",
  "a minuet and trio result gives the part, what a row plays again and the upbeat chord of each row",
);
check(minuetRows[0].part === "theme" && minuetRows[0].partLabel === "Theme" && minuetRows[3].part === "return", "a performed row still names its role");
const connectiveRows = P.resultView(
  { seed: 1, summary: { meter: "3/4", versions: { piece: { sections: [{ id: "A", role: "theme", start: "0/1", beats: 24, connective: null }, { id: "B", role: "contrast", start: "8/1", beats: 24, connective: true }, { id: "C", role: "contrast", start: "16/1", beats: 24, connective: false }] } } } },
  { form: "minuet" },
).versions[0].view.rows;
check(connectiveRows[0].connective === "" && connectiveRows[1].connective === "yes" && connectiveRows[2].connective === "no", "the connective reads yes, no or nothing");
check(
  ternaryRows.every((row) => row.partName === "" && row.repeatOf === "" && row.upbeat === "" && row.connective === "") && ternaryRows[0].part === "theme" && ternaryRows[1].part === "contrast",
  "the rows of a ternary result are as before",
);

/* the scherzo (P49): a form that always modulates, and the planned key of each row */
const scherzoForm = P.formOf("scherzo");
check(P.isModulating(scherzoForm) && !P.isModulating(P.formOf("minuet")) && !P.isModulating(null), "the scherzo always modulates and the minuet does not");
check(scherzoForm.settings.modulate === false && P.withForm(P.emptyState("pce-5"), "scherzo").modulate === false, "the scherzo offers no modulate choice and its state does not send one");
check(P.roleLabel(scherzoForm, "liquidation") === "Liquidation" && P.roleLabel(scherzoForm, "pattern") === "Pattern" && P.roleLabel(scherzoForm, "codetta") === "Codetta", "the scherzo's new roles have their labels");
const scherzoSummary = JSON.parse(fs.readFileSync(path.join(__dirname, "fixtures", "scherzo-summary.json"), "utf8"));
const scherzoRows = P.resultView({ seed: 1, summary: scherzoSummary }, { form: "scherzo", bars: 44, count: 0 }).versions[0].view.rows;
check(
  scherzoRows.length === 15 && scherzoRows[2].id === "P" && scherzoRows[2].plannedKey === "G major" && scherzoRows[3].id === "S1" && scherzoRows[3].plannedKey === "F major" && scherzoRows[4].plannedKey === "C major",
  "a scherzo result gives the planned key of each row",
);
check(scherzoRows[5].partLabel === "Liquidation" && scherzoRows[12].partLabel === "Episode", "a scherzo row names its role");
check(ternaryRows.every((row) => row.plannedKey === "") && minuetRows.every((row) => row.plannedKey === ""), "a row without key has no planned key");

/* figured variations (P50): the figure of each variation in the structure table and the result rows */
const figuredEntry = P.formOf("figured-variations");
const figuredSections = figuredEntry.sizes.find((size) => size.bars === 56).sections;
check(
  same(figuredSections.map((entry) => entry.labels.figure), [
    "",
    "neighbor tones in eighths",
    "neighbor tones in sixteenths",
    "scale line in eighths, reharmonized, parallel mode",
    "scale line in sixteenths, reharmonized, root changed at bar 3",
    "broken chord in sixteenths, climax",
    "",
    "",
  ]),
  "the figure of each variation of the figured variations reads in words and its theme and codettas have none",
);
check(
  P.formOf("minuet").sizes.every((size) => size.sections.every((entry) => entry.labels.figure === "")),
  "a section with no figure has no label",
);
const figuredSummary = JSON.parse(fs.readFileSync(path.join(__dirname, "fixtures", "figured-variations-summary.json"), "utf8"));
const figuredRows = P.resultView({ seed: 1, summary: figuredSummary }, { form: "figured-variations", bars: 56, count: 5 }).versions[0].view.rows;
check(
  figuredRows.length === 8 &&
    figuredRows.slice(1, 6).every((row) => row.figure.includes("over the skeleton") && row.skeleton.split(" ").length === 16) &&
    figuredRows[1].figure.startsWith("neighbor tones in eighths") &&
    figuredRows[3].figure.includes("parallel mode") &&
    figuredRows[0].figure === "" &&
    figuredRows[0].skeleton === "" &&
    figuredRows.slice(6).every((row) => row.figure === "" && row.skeleton === ""),
  "a figured variations result gives the figure and skeleton of V1 to V5 and none for the theme and codettas",
);
check(minuetRows.every((row) => row.figure === "" && row.skeleton === ""), "a row without a figure has none");

/* The parts of larger forms (P51, R-510): the structure table's role label, the function a game loop section may have and the
 * function column of a result. */
const partsEntry = P.formOf("main-and-subordinate");
const partsSections = partsEntry.sizes.find((size) => size.bars === 57).sections;
check(
  same(partsSections.map((entrySection) => entrySection.labels.role), ["Theme", "Transition", "Subordinate theme", "Subordinate theme (lyric)", "Codetta", "Retransition", "Return", "Coda"]),
  "the structure table's role label names a lyric theme and the other roles by their label",
);
check(
  same(P.functionsFor("body").map((entry) => entry.name), ["transition"]) &&
    same(P.functionsFor("intro").map((entry) => entry.name), ["transition"]) &&
    same(P.functionsFor("ending").map((entry) => entry.name), ["coda"]) &&
    P.functionsFor("turnaround").length === 0,
  "a game loop part offers the section functions the vocabulary allows it",
);
const functionState = filled();
functionState.sections[1].function = "transition";
functionState.sections[2].function = "coda";
const functionBody = P.bodyFromState(functionState);
check(
  P.pieceOfBody(functionBody).structure.sections[1].function === "transition" && !("function" in P.pieceOfBody(functionBody).structure.sections[2]) && !("function" in P.pieceOfBody(functionBody).structure.sections[0]),
  "a section with a function writes it, a section without one or with one its part may not have writes none",
);
const functionBack = P.stateFromBody(functionBody, "pce-1");
check(
  functionBack.sections[1].function === "transition" && functionBack.sections[0].function === "" && same(P.bodyFromState(functionBack), functionBody),
  "a game loop state with a transition body section round trips",
);
const unknownFunction = JSON.parse(JSON.stringify(functionBody));
unknownFunction.request.sections[1].function = "bridge";
unknownFunction.request.sections[4].function = "transition";
const unknownBack = P.stateFromBody(unknownFunction, "pce-1");
check(unknownBack.sections[1].function === "" && unknownBack.sections[4].function === "", "a function the vocabulary lacks or the part may not have is dropped");
check(P.newSection(functionState, "body").function === "", "a new section has no function");
const partsSummary = JSON.parse(fs.readFileSync(path.join(__dirname, "fixtures", "main-and-subordinate-summary.json"), "utf8"));
const partsRows = P.resultView({ seed: 1, summary: partsSummary }, { form: "main-and-subordinate", bars: 57, count: 0 }).versions[0].view.rows;
check(
  same(partsRows.map((row) => row.letter), ["A", "T", "B", "B", "C", "R", "A", "K"]) &&
    same(partsRows.map((row) => row.function !== ""), [false, true, true, true, false, true, false, true]) &&
    partsRows[1].function.includes("transition") && partsRows[5].function.includes("retransition") && partsRows[7].function.includes("coda") && partsRows[3].function.includes("lyric"),
  "a main and subordinate result gives the function of T, S1, S2, R and K and none for A, C and the return",
);
check(minuetRows.every((row) => row.function === ""), "a row without a function has none");

/* The rondo labels (P52, R-521). */
const rondoSections = require(generated("forms.json")).forms.find((form) => form.name === "small-rondo-ababa").sizes[0].sections;
const rondoSection = (name) => rondoSections.find((section) => section.name === name);
check(
  rondoSection("A1").labels.return === "A (embellish)" &&
    rondoSection("B1").labels.return === "B (exact)" &&
    rondoSection("A2").labels.return === "A (setting: arpeggiated)" &&
    rondoSection("A").labels.return === "",
  "a return names its variation, and its setting for a setting return",
);
check(
  rondoSection("B1").labels.keys.endsWith(" (tonic major)") &&
    rondoSection("B").labels.keys === "subordinate to subordinate" &&
    rondoSection("A1").labels.keys === "home to home",
  "a section in the tonic major says so and any other shows its keys alone",
);
const mainSections = require(generated("forms.json")).forms.find((form) => form.name === "main-and-subordinate").sizes[0].sections;
const mainReturn = mainSections.find((section) => section.returns);
check(
  mainReturn.labels.return === `${mainReturn.returns} (${mainReturn.variation})` && mainReturn.labels.return === "A (exact)" && (mainSections.find((section) => section.name === "T")).labels.keys === "home to subordinate",
  "a main and subordinate section's labels are as the inline text gave",
);
const rondoSummary = JSON.parse(fs.readFileSync(path.join(__dirname, "fixtures", "small-rondo-ababa-summary.json"), "utf8"));
const rondoRows = P.resultView({ seed: 1, summary: rondoSummary }, { form: "small-rondo-ababa", bars: 68, count: 0 }).versions[0].view.rows;
check(rondoRows[8].letter === "A" && rondoRows[8].changes.includes("new setting"), "the second return of A in a small rondo says its new setting");

/* The sonatina labels (P53, R-525). */
const sonatinaForms = require(generated("forms.json")).forms;
const sonatinaSections = sonatinaForms.find((form) => form.name === "sonatina").sizes[0].sections;
const sonatinaSection = (name) => sonatinaSections.find((section) => section.name === name);
check(
  sonatinaSection("B1").labels.keys === "home to home (home mode)" &&
    sonatinaSection("E").labels.keys === "elaboration to elaboration" &&
    sonatinaSection("B1").labels.return === "B (reharmonize)" &&
    sonatinaSection("E").labels.return === "B (embellish)",
  "a sonatina section in the home mode says so, the elaboration shows its keys and returns of B",
);
const sonatinaMajorSections = sonatinaForms.find((form) => form.name === "sonatina-tonic-major").sizes[0].sections;
check(
  (sonatinaMajorSections.find((section) => section.name === "B1")).labels.keys.endsWith(" (tonic major)"),
  "a sonatina-tonic-major section in the tonic major says so",
);
const sonatinaSummary = JSON.parse(fs.readFileSync(path.join(__dirname, "fixtures", "sonatina-summary.json"), "utf8"));
const sonatinaRows = P.resultView({ seed: 1, summary: sonatinaSummary }, { form: "sonatina", bars: 92, count: 0 }).versions[0].view.rows;
check(
  sonatinaRows[11].letter === "B" && sonatinaRows[11].returns === "B" && sonatinaRows[11].changes.includes("mode: mapped into minor") && !sonatinaRows[5].changes.includes("mode:"),
  "the home mode return of B in a sonatina says it was mapped into minor and the elaboration says no mode",
);

/* Regions, harmony and overlays (P60, R-610, R-612). */
const placed = filled();
placed.sections[1].region = { kind: "relation", relation: "omt.relation.dominant", anchor: "home", tonic: "", material: "" };
placed.sections[2].region = { kind: "key", tonic: "Eb", material: "omt.material.major", relation: "", anchor: "home" };
placed.sections[3].region = { kind: "home", tonic: "", material: "", relation: "", anchor: "home" };
placed.sections[1].harmony = { kind: "schema", numerals: "", cells: "", schema: "prinner", chords: [] };
placed.sections[2].harmony = { kind: "chords", numerals: "", cells: "", schema: "", chords: [{ bar: "2", text: "V" }, { bar: "", text: "I" }, { bar: "3", text: "" }] };
const placedBody = P.pieceOfBody(P.bodyFromState(placed)).structure.sections;
check(same(placedBody[1].region, { relation: "omt.relation.dominant", anchor: "home" }) && same(placedBody[2].region, { tonic: "Eb", material: "omt.material.major" }) && placedBody[3].region === "home", "a section's region is null, home, a key or a relation");
check(placedBody[0].region === undefined && placedBody[0].harmony === undefined, "a section without a region or harmony has no member");
check(same(placedBody[1].harmony, { schema: "prinner" }) && same(placedBody[2].harmony, { chords: [{ bar: 2, text: "V" }] }), "a schema is a name and chords with no bar or no text are left out");
const placedBack = P.stateFromBody(P.requestBodyOf([], { structure: { key: "D", material: "omt.material.natural-minor", meter: "4/4", sections: placedBody } }), "pce-3");
check(placedBack.sections[1].region.kind === "relation" && placedBack.sections[2].region.tonic === "Eb" && placedBack.sections[3].region.kind === "home", "a region round trips through the state");
check(placedBack.sections[1].harmony.schema === "prinner" && placedBack.sections[2].harmony.chords[0].bar === "2", "a harmony round trips through the state");
const unfinishedPlace = filled();
unfinishedPlace.sections[1].region = { kind: "key", tonic: "", material: "", relation: "", anchor: "home" };
unfinishedPlace.sections[2].harmony = { kind: "progression", numerals: " ", cells: "", schema: "", chords: [] };
check(P.pieceOfBody(P.bodyFromState(unfinishedPlace)).structure.sections.every((entry) => entry.region === undefined), "a region that names nothing is saved as none");

/* a stored progression fill is the section's harmony */
/* the overlays of a sized form */
const ternaryState = P.emptyState("pce-5");
ternaryState.form = "ternary";
ternaryState.overlays = [
  { name: "B", region: { kind: "relation", relation: "omt.relation.mediant", anchor: "A", tonic: "", material: "" }, harmony: P.emptyHarmony(), fill: { kind: "theme", item_id: "th-1", version: 2 } },
  { name: "A", region: P.emptyRegion(), harmony: P.emptyHarmony(), fill: P.emptyFill() },
  { name: "A return", region: P.emptyRegion(), harmony: { kind: "schema", numerals: "", cells: "", schema: "prinner", chords: [] }, fill: P.emptyFill() },
];
const ternaryBody = P.pieceOfBody(P.bodyFromState(ternaryState)).structure;
check(ternaryBody.sections.length === 2 && ternaryBody.sections[0].name === "B" && same(ternaryBody.sections[0].fill, { kind: "theme", item_id: "th-1", version: 2 }) && same(ternaryBody.sections[1], { name: "A return", harmony: { schema: "prinner" } }), "a sized form's sections are the overlays that have something");
const ternaryBack = P.stateFromBody(P.requestBodyOf([], { structure: { form: "ternary", key: "C", material: "omt.material.major", meter: "4/4", bars: 0, count: 0, modulate: false, sections: ternaryBody.sections } }), "pce-5");
check(ternaryBack.overlays.length === 2 && ternaryBack.overlays[0].region.anchor === "A" && ternaryBack.overlays[0].fill.item_id === "th-1" && ternaryBack.overlays[1].harmony.schema === "prinner", "the overlays round trip through the state");
const bare = P.pieceOfBody(P.bodyFromState(Object.assign(P.emptyState("pce-6"), { form: "ternary" }))).structure;
check(bare.sections === undefined && P.stateFromBody(P.requestBodyOf([], { structure: { form: "ternary", key: "C", material: "omt.material.major", meter: "4/4", bars: 0, count: 0 } }), "pce-6").overlays.length === 0, "a sized form with no overlay has no sections member, and a body saved before reads the same");

/* the rows of a result show the region and the harmony */
const placedRows = P.resultView({ summary: { versions: { loop: { sections: [
  { id: "a", part: "body", letter: "A", start: "0/1", beats: 8, region: "dominant of home", harmony: "i iv V i (2 cells each)", fill: "empty", source: "basic motive" },
  { id: "b", part: "body", letter: "B", start: "2/1", beats: 8, region: null, harmony: null, fill: "empty", source: "basic motive" },
] } } } }).versions[0].view.rows;
check(placedRows[0].region === "dominant of home" && placedRows[0].harmony === "i iv V i (2 cells each)" && placedRows[1].region === "" && placedRows[1].harmony === "", "the rows show the region and the harmony the request gave");

/* The rows of a result are the Composer's words (P63 B16): the summaries the build composes for a piece of each form with sizes. */
for (const name of ["ternary", "rounded-binary", "minuet", "scherzo", "variations", "figured-variations", "sonatina", "small-rondo-ababa"]) {
  const composed = JSON.parse(fs.readFileSync(generated(`piece-${name}.json`), "utf8"));
  const view = P.resultView({ seed: composed.seed, summary: composed }, { form: name });
  const planned = composed.versions.piece.sections;
  const rows = view.versions[0].view.rows;
  check(
    rows.length === planned.length &&
      rows.every((row, at) => row.start === planned[at].start_text && row.length === planned[at].length_text && row.start !== "" && row.length !== ""),
    `${name}: the rows show the position and the length the Composer wrote`,
  );
  check(rows[0].start === "bar 1", `${name}: the first section starts at bar 1`);
  const forms = P.formOf(name);
  check(forms.sizes.every((size) => typeof size.label === "string" && size.label !== "") && forms.sizes.every((size) => size.sections.every((section) => section.labels && typeof section.labels.role === "string")), `${name}: the form's sizes and sections carry the Composer's labels`);
}

finish();
