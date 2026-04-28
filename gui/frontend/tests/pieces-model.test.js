/* Covers the pure piece helpers (public/pieces-model.js): the body round trip, the structure problems, the beats
 * labels, the timeline, the default cadences, the compose, regenerate and variations requests and the result rows.
 * piece-summary.json is the output of `ursatz-composer piece --format json` for the request of the loop-and-ending
 * fixture (intro 2 bars, body of 8 and 4 bars from two motives, turnaround 2 bars, ending 2 bars), D minor at tempo
 * 170 from a style, seed 7 and --check.
 * Plain Node script: `node frontend/tests/pieces-model.test.js`. */
const fs = require("fs");
const path = require("path");
const P = require("../public/pieces-model.js");

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

function section(id, part, letter, beats, fill, extra) {
  return { id, part, letter, beats, pickup: "0", cadence: null, fill: fill || { kind: "empty" }, ...extra };
}

function filled() {
  const state = P.emptyState("pce-1");
  state.key = "D";
  state.mode = "minor";
  state.sections = [
    section("s0", "intro", "I", 8),
    section("s1", "body", "A", 32, { kind: "motif", item_id: "mot-1", version: 2, into: "sentence", hybrid: "" }),
    section("s2", "body", "B", 16, { kind: "motif", item_id: "mot-2", version: 1, into: "sentence", hybrid: "" }),
    section("s3", "turnaround", "T", 8),
    section("s4", "ending", "E", 8, { kind: "progression", numerals: "I  IV V I", cells: "2" }),
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
check(body.piece.structure.key === "D" && body.piece.structure.mode === "minor" && body.piece.structure.meter === "4/4", "the body holds key, mode and meter");
check(body.piece.structure.sections.length === 5 && body.piece.structure.sections[1].fill.item_id === "mot-1", "the body holds the sections in order");
check(body.piece.structure.sections[1].fill.into === "sentence" && body.piece.structure.sections[1].fill.hybrid === null, "a motif of 8 bars keeps its theme form");
check(body.piece.structure.sections[2].fill.into === "sentence", "a motif of 4 bars keeps its theme form");
const freeState = filled();
freeState.sections[2].beats = 14;
check(P.bodyFromState(freeState).piece.structure.sections[2].fill.into === undefined, "a motif of any other length is a free phrase and has no theme form");
check(same(body.piece.structure.sections[4].fill, { kind: "progression", numerals: "I IV V I", cells: 2 }), "numerals are tidied and cells are a number");
check(same(body.piece.motive, { item_id: "mot-9", version: 3 }) && body.piece.pool === "approved", "motive and pool are in the body");
check(same(body.piece.accompaniment, { pattern: "alberti" }), "the accompaniment is a pattern");
check(body.piece.brief.scopes.length === 1 && body.piece.brief.style_ids[0] === "sty-1", "the brief comes from the picks");
check(same(body.piece.without, ["motive-rhythm"]) && body.piece.style_weight === "1", "the switches and the weight are in the body");
check(body.piece.tempo === 170 && body.piece.seed === 7 && body.piece.check === true, "tempo, seed and check are in the body");
check(body.piece.result === undefined, "the result is left out");
const back = P.stateFromBody(body, "pce-1");
back.sections[1].fill.hybrid = "";
check(same(P.bodyFromState(back), body), "the body round trips");
check(back.tempo === "170" && back.seed === "7" && back.accompaniment.kind === "pattern", "the state reads text for tempo and seed");

/* placeholders and unfinished fills */
for (const placeholder of [{ uses: [], piece: { structure: null } }, { uses: [], piece: { structure: "x" } }, { uses: [], piece: { structure: [] } }, { uses: [], piece: { structure: { sections: null } } }, null]) {
  const s = P.stateFromBody(placeholder, "pce-2");
  check(s.sections.length === 0 && s.mode === "major" && s.key === "C", `a placeholder gives the defaults (${JSON.stringify(placeholder)})`);
}
const unfinished = filled();
unfinished.sections[1].fill = { kind: "motif", item_id: "", version: 0, into: "sentence" };
unfinished.sections[2].fill = { kind: "theme", item_id: "", version: 0 };
unfinished.sections[4].fill = { kind: "progression", numerals: "  ", cells: "" };
const unfinishedBody = P.bodyFromState(unfinished);
check(unfinishedBody.piece.structure.sections.every((entry) => entry.fill.kind === "empty"), "a fill that names nothing is saved empty");
check(P.formProblems(unfinished).length === 0, "an unfinished fill is no problem");
const old = P.stateFromBody({ uses: [], piece: { structure: { key: "F", mode: "minor", meter: "3/4", sections: [{ id: "s0", part: "body", letter: "A", beats: 6, pickup: "1", cadence: "HC", fill: { kind: "motif", item_id: "m", version: 1, into: null, hybrid: null } }] } } }, "x");
check(old.meter === "3/4" && old.sections[0].cadence === "HC" && old.sections[0].pickup === "1", "meter, pickup and cadence are read");

/* structure problems */
const ok = filled();
check(P.structureProblems(ok).length === 0 && P.formProblems(ok).length === 0, "a filled piece has no problems");
const noBody = filled();
noBody.sections = noBody.sections.filter((entry) => entry.part !== "body");
check(P.structureProblems(noBody).some((text) => /loop body/.test(text)), "no body section is a problem");
const unordered = filled();
unordered.sections = [unordered.sections[1], unordered.sections[0]];
check(P.structureProblems(unordered).some((text) => /order/.test(text)), "parts out of order are a problem");
const short = filled();
short.sections[1].beats = 0;
short.sections[2].beats = 1.5;
check(P.structureProblems(short).filter((text) => /at least a beat/.test(text)).length === 2, "a length under a beat is a problem");
const pickup = filled();
pickup.sections[1].pickup = "4";
pickup.sections[2].pickup = "x";
pickup.sections[3].pickup = "7/2";
check(P.structureProblems(pickup).filter((text) => /pickup/.test(text)).length === 2, "a pickup not under a bar is a problem, 7/2 beats is not");
const waltz = filled();
waltz.meter = "3/4";
waltz.sections[1].pickup = "3";
check(P.structureProblems(waltz).some((text) => /pickup/.test(text)), "the bar of the meter limits the pickup");
const themed = filled();
themed.sections[1].fill = { kind: "theme", item_id: "thm-1", version: 2 };
check(P.structureProblems(themed, { themeBeats: () => 32 }).length === 0, "a theme of the section's length is fine");
check(P.structureProblems(themed, { themeBeats: () => 16 }).some((text) => /theme is 4 bars/.test(text)), "a theme of another length is a problem");
check(P.structureProblems(themed, { themeBeats: () => null }).length === 0 && P.structureProblems(themed).length === 0, "an unknown theme length is no problem");
const many = filled();
many.sections = Array.from({ length: 33 }, (_, i) => section(`s${i}`, "body", `L${i}`, 4));
check(P.structureProblems(many).some((text) => /at most 32/.test(text)), "more than 32 sections is a problem");
const form = filled();
form.key = "H";
form.tempo = "10";
form.seed = "-1";
form.mode = "style";
form.sections[4].fill.numerals = "I $ V";
form.sections[4].fill.cells = "9";
check(P.formProblems(form).length === 6, "the form problems: key, tempo, seed, mode, numerals and cells");

/* beats labels */
check(P.beatsLabel(8, "4/4") === "2 bars" && P.beatsLabel(4, "4/4") === "1 bar", "whole bars");
check(P.beatsLabel(6, "4/4") === "1 bar 2 beats" && P.beatsLabel(3, "4/4") === "3 beats" && P.beatsLabel(1, "4/4") === "1 beat", "beats");
check(P.beatsLabel(6, "3/4") === "2 bars" && P.beatsLabel(6, "6/8") === "1 bar" && P.beatsLabel(7, "2/4") === "3 bars 1 beat", "other meters");

/* timeline */
const timeline = P.sectionTimeline(filled());
check(same(timeline.map((entry) => [entry.id, entry.start, entry.beats]), [["s0", 0, 8], ["s1", 8, 32], ["s2", 40, 16], ["s3", 56, 8], ["s4", 64, 8]]), "the timeline runs section after section");
check(same(P.sectionTimeline(P.emptyState()), []), "no sections give an empty timeline");

/* default cadences */
const cadences = filled();
check(same(cadences.sections.map((_, i) => P.defaultCadence(cadences, i)), ["HC", "IAC", "IAC", "HC", "PAC"]), "the intro and the loop end on HC, the ending on PAC, the rest IAC");
cadences.sections.splice(3, 1);
check(P.defaultCadence(cadences, 2) === "HC", "without a turnaround the last body section ends the loop");
cadences.sections[2].cadence = "PAC";
check(P.cadenceOf(cadences, 2) === "PAC" && P.cadenceOf(cadences, 1) === "IAC", "a chosen cadence wins over the default");

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
check(same(P.composeRequest(state, {}), { method: "POST", url: "/api/materials/items/pce-1/compose", headers: { "Content-Type": "application/json" }, body: JSON.stringify({ seed: 7 }) }), "compose takes the seed of the form");
check(JSON.parse(P.composeRequest(state, { seed: "12", update_uses: true }).body).seed === 12 && JSON.parse(P.composeRequest(state, { update_uses: true }).body).update_uses === true, "an option seed wins and update_uses is sent");
const blank = P.emptyState("pce-3");
check(P.composeRequest(blank, {}).body === "{}", "no seed gives an empty body");
const save = P.saveRequest(state, " first ");
check(save.url === "/api/materials/items/pce-1/versions" && JSON.parse(save.body).note === "first" && JSON.parse(save.body).body.piece.structure.sections.length === 5, "save posts a version with the body and the note");
check(P.versionFileUrl("pce-1", 3, "loop.mid") === "/api/materials/items/pce-1/versions/3/files/loop.mid", "version file url");
const regenerate = P.regenerateRequest("pce-1", 2, { section_id: "s1", lane: "melody", seed: "9" });
check(regenerate.request.url.endsWith("/versions/2/regenerate") && same(JSON.parse(regenerate.request.body), { section_id: "s1", lane: "melody", seed: 9 }), "regenerate request");
check(same(JSON.parse(P.regenerateRequest("pce-1", 2, { section_id: "s1", lane: "" }).request.body), { section_id: "s1" }), "regenerate without a lane or seed");
check(P.regenerateRequest("pce-1", 2, { section_id: "s1", seed: "x" }).error && P.regenerateRequest("pce-1", 2, {}).error, "regenerate refuses a bad seed or no section");
const variations = P.variationsRequest("pce-1", 2, { section_id: "s1", lane: "harmony", count: "3", seed: "" });
check(variations.request.url.endsWith("/versions/2/variations") && same(JSON.parse(variations.request.body), { section_id: "s1", lane: "harmony", count: 3 }), "variations request");
check(P.variationsRequest("pce-1", 2, { section_id: "s1", lane: "harmony", count: "9" }).error && P.variationsRequest("pce-1", 2, { section_id: "s1", lane: "", count: "2" }).error, "variations refuse a count over 8 or no lane");
check(same(JSON.parse(P.chooseRequest("pce-1", 2, 4, 1).body), { index: 1 }) && P.chooseRequest("pce-1", 2, 4, 1).url.endsWith("/versions/2/variations/4/choose"), "choose request");
check(P.variationFileUrl("pce-1", 2, 4, "variation-1.musicxml").endsWith("/versions/2/variations/4/variation-1.musicxml"), "variation file url");
check(same(P.lanesFor(section("s", "body", "A", 8)), ["harmony", "bass", "melody", "inner"]) && same(P.lanesFor(section("s", "body", "A", 8, { kind: "theme" })), ["harmony", "bass"]) && P.lanesFor(section("e", "ending", "E", 8)).length === 0, "lanes by section");
const cards = P.variationCards({ set: 1, variations: [{ index: 2, seed: 5, score: { numerator: 8, denominator: 12 }, seam: { held: 3 }, heard: "half" }] });
check(cards[0].title === "#2 · seed 5" && cards[0].score === "8 of 12" && cards[0].seam === "3 of 4 seam rules" && cards[0].heard === "half", "variation cards");

/* result */
const summary = JSON.parse(fs.readFileSync(path.join(__dirname, "fixtures", "piece-summary.json"), "utf8"));
const view = P.resultView({ seed: 7, summary, files: ["loop.mid"], brief: { piece_count: 2, skipped: ["x"] } }, { sourceNames: {} });
check(view.seed === 7 && view.key === "D minor" && view.loop === "bar 3 to bar 17" && view.loopLength === "14 bars", "result facts and the loop region");
check(view.brief.tempo === "170 (from the brief)" && view.brief.mode === "minor (set here)" && view.brief.density === "2 attacks per beat" && view.brief.without === "none", "result brief");
check(view.pieces === "2 pieces in the brief, 1 skipped", "result brief pieces");
const loop = view.versions.loop;
check(same(loop.rows.map((row) => row.id), ["intro", "a", "b", "t"]) && loop.rows[1].start === "bar 3" && loop.rows[1].length === "8 bars", "loop rows");
check(loop.rows[1].cadence === "IAC" && loop.rows[1].heard === "imperfect-authentic" && loop.rows[1].fill === "motif" && loop.rows[1].source === "own motive", "loop row cadence, fill and source");
check(loop.rows[3].part === "turnaround" && loop.rows[3].cadence === "HC" && loop.rows[3].heard === "half", "turnaround row");
check(loop.seam.held === 3 && loop.seam.label === "3 of 4 seam rules held" && loop.seam.rules.filter((rule) => !rule.held).length === 1, "seam rules");
check(loop.score.label === "16 of 21" && loop.check.label === "5 of 5 planned items heard", "score and Analyzer check");
check(loop.distance.length === 5 && loop.distance[4].label === "overall" && loop.distance[4].value === "0.5454", "style distance");
const ending = view.versions.ending;
check(ending.rows[3].part === "ending" && ending.rows[3].heard === "none (the piece ends)" && ending.seam === null && ending.score.label === "6 of 6", "ending version");
check(ending.rows[3].start === "bar 15", "ending starts after the body");
const withTheme = JSON.parse(JSON.stringify(summary));
withTheme.versions.loop.sections[1].fill = "theme";
withTheme.versions.loop.sections[1].source = "/data/materials/items/pce-1/v2/theme-a.plan.json";
check(P.resultView({ summary: withTheme }, { sourceNames: { a: "Theme One v2" } }).versions.loop.rows[1].source === "Theme One v2", "a theme source is named by its section");
check(
  JSON.stringify(view.playLoop) === JSON.stringify({ start: 2, end: 16, bodyEnd: 14 }) && P.resultView({ summary: { versions: {} } }).playLoop === null,
  "the playback loop is the loop region and the end of the last body section, in whole notes",
);
const empty = P.resultView(null);
check(empty.seed === null && empty.versions.loop === null && empty.loop === "" && empty.files.length === 0, "no result gives no rows");

/* sources and themes */
const updates = P.updatedUses({ uses_status: [{ item_id: "mot-1", name: "Motif A", version: 1, current_version: 2 }, { item_id: "thm-1", version: 2, current_version: 2 }, { item_id: "x", version: 1, current_version: 4, missing: true }] });
check(updates.length === 1 && updates[0].label === "Source updated: Motif A v2", "an updated source is offered");
const themeBody = { theme: { form: { into: "hybrid", hybrid: "presentation+continuation", key: "D", mode: "minor", meter: "4/4", bars: 8 }, result: { seed: 1 } } };
check(P.themeLengthBeats(themeBody) === 32 && P.themeLengthBeats({ theme: { form: { bars: 4, meter: "3/4" } } }) === 12 && P.themeLengthBeats({}) === null, "a theme's length in beats");
check(P.themeFormText(themeBody) === "hybrid (presentation+continuation) in D minor, 4/4, 8 bars" && P.themeComposed(themeBody) && !P.themeComposed({ theme: { form: {}, result: null } }), "a theme's form and whether it was composed");
check(P.motifIsFree(section("s", "body", "A", 6)) && !P.motifIsFree(section("s", "body", "A", 32)) && P.motifIsFree(section("s", "body", "A", 32, null, { pickup: "1" })), "a motif fill is free unless 4 or 8 bars without a pickup");

if (failures > 0) {
  console.error(`${failures} failure(s)`);
  process.exit(1);
}
