/* Covers the pure materials helpers (public/materials-model.js): request
 * builders, new item bodies, the project filter, item and version rows.
 * Plain Node script, run through ctest: it reads the vocabulary the build generates (support/fixtures.js). */
import path from "path";
import { fileURLToPath } from "url";
import Vocabulary from "../public/core/vocabulary.js";
import { generated, readJson } from "./support/fixtures.js";
import M from "../public/models/materials-model.js";
import { check, same, finish } from "./support/check.js";
import { params, isCommand } from "./support/command.js";

const __dirname = path.dirname(fileURLToPath(import.meta.url));

Vocabulary.set(readJson(generated("vocabulary.json")));


const projects = [
  { id: "prj-a", name: "Sonata" },
  { id: "prj-b", name: "Etudes" },
];

check(same(M.TYPES.map((t) => t.label), ["Motifs", "Themes", "Pieces"]), "three types in sidebar order");
check(M.typeInfo("theme").singular === "theme" && M.typeInfo("x") === null, "typeInfo finds a type or null");

const create = M.buildCreateProjectRequest("  Sonata ", "first");
check(isCommand(create, "project.create"), "create project is the project.create command");
check(same(params(create), { name: "Sonata", description: "first" }), "create project trims the name");
check(!("description" in params(M.buildCreateProjectRequest("a"))), "create project leaves out an empty description");
const rename = M.buildRenameProjectRequest("prj-a", " Op. 1 ");
check(isCommand(rename, "project.update", "prj-a"), "rename project is project.update of the project");
check(same(params(rename), { name: "Op. 1" }), "rename project sends the trimmed name");
check(isCommand(M.buildDeleteProjectRequest("prj-a"), "project.delete", "prj-a") && same(params(M.buildDeleteProjectRequest("prj-a")), {}), "delete project is project.delete without params");

check(M.buildItemListRequest("motif", "all").url === "/api/materials/items?type=motif", "item list for all projects has no project");
check(
  M.buildItemListRequest("piece", "prj-a").url === "/api/materials/items?type=piece&project=prj-a",
  "item list for a project adds it",
);
check(M.buildVersionGetRequest("mot-1", 2).url === "/api/materials/items/mot-1/versions/2", "version route");

const motif = M.newItemBody("motif").motif;
check(motif.notes !== "" && motif.pickup === "0" && same(motif.features, []) && motif.derived === null, "new motif body is valid");
check(same(M.newItemBody("theme"), { uses: [], request: {} }), "new theme body");
check(same(M.newItemBody("piece"), { uses: [], request: {} }), "new piece body");
const item = params(M.buildCreateItemRequest("theme", " Main ", "prj-a"));
check(item.type === "theme" && item.name === "Main" && item.project_id === "prj-a" && item.body.request, "create item body");
check(same(params(M.buildUpdateItemRequest("x", { name: " B ", project_id: "" })), { name: "B" }), "update item skips empty fields");

check(M.errorText({ error: "project is not empty" }) === "project is not empty", "error text from the answer");
check(M.errorText(null, "fallback") === "fallback", "error text fallback");

check(same(M.projectOptions(projects).map((o) => o.label), ["All projects", "Sonata", "Etudes"]), "project options start with All projects");
check(M.resolveFilter("prj-b", projects) === "prj-b", "a known filter is kept");
check(M.resolveFilter("prj-gone", projects) === "all", "a deleted project falls back to all");
check(M.defaultProject("prj-b", projects) === "prj-b" && M.defaultProject("all", projects) === "prj-a", "new item project");
check(M.defaultProject("all", []) === "", "no projects, no default");
check(M.readFilter() === "all", "filter reads as all without storage");
M.writeFilter("prj-a");

const rows = M.itemRows(
  [
    { id: "mot-1", name: "Old", project_id: "prj-a", current_version: 1, updated_at: "2026-10-01T10:00:00Z" },
    { id: "mot-2", name: "New", project_id: "prj-b", current_version: 3, updated_at: "2026-10-20T09:00:00Z" },
  ],
  projects,
);
check(rows[0].id === "mot-2" && rows[0].project === "Etudes" && rows[0].version === "v3", "item rows newest first with project and version");
check(rows[0].updated === "2026-10-20", "item row date");
check(M.itemRows(null, projects).length === 0, "no items, no rows");

const vrows = M.versionRows({
  current_version: 2,
  versions: [
    { number: 1, created_at: "2026-10-01T10:00:00Z", note: "first" },
    { number: 2, created_at: "2026-10-02T10:00:00Z" },
  ],
});
check(vrows[0].number === 2 && vrows[0].current && vrows[0].note === "", "version rows newest first, current marked");
check(vrows[1].note === "first" && !vrows[1].current, "older version row");

const uses = M.useRows([{ item_id: "mot-1", name: "Hook", version: 2, missing: false }, { item_id: "mot-9", version: 1, missing: true }]);
check(uses[0].label === "Hook v2" && uses[1].missing && uses[1].label === "mot-9 v1", "use rows");

// Motif editor helpers.
const body = {
  uses: [{ item_id: "mot-9", version: 1, copy: {} }],
  motif: {
    role: "accompaniment",
    form: "full",
    notes: "D4:q F4:e",
    meter: "3/4",
    material: "omt.material.natural-minor",
    pickup: "1/2",
    features: [{ label: "a", first: 0, last: 1 }],
    analysis: { analysis: {} },
    derived: { source_item: "mot-1" },
  },
};
const values = M.motifFormValues(body);
check(values.role === "accompaniment" && values.material === "omt.material.natural-minor" && values.pickup === "1/2", "form values read the motif and its material");
check(M.motifFormValues({ motif: { material: "omt.material.major" } }).material === "omt.material.major", "a motif in C major reads as major");
check(M.motifFormValues({ motif: { material: null } }).material === "omt.material.major" && M.motifFormValues(null).material === "omt.material.major", "the material defaults to major");
check(same(M.MOTIF_MATERIALS.map((entry) => [entry.value, entry.label]), [["omt.material.major", "major"], ["omt.material.natural-minor", "minor"]]) && M.TONIC_HINT.startsWith("Write the notes with C as the tonic"), "the materials and the tonic hint");
check(same(values.features, [{ label: "a", first: 1, last: 2 }]), "features show note numbers from 1");
check(values.harmony === "" && M.motifFormValues({ motif: { harmony: "I V6@2" } }).harmony === "I V6@2", "form values read the harmony");
check(M.buildMotifBody({ ...values, harmony: " I V6@2 " }, null).motif.harmony === "I V6@2", "the body holds the trimmed harmony");
check(M.buildMotifBody({ ...values, harmony: "  " }, null).motif.harmony === null, "an empty harmony is stored as null");
check(M.newItemBody("motif").motif.harmony === null, "a new motif has no harmony");
const withHarmony = M.renderRequest({ motif: { form: "full", degrees: "1:q", meter: "4/4", harmony: "I V@2", features: [{ label: "a", first: 0, last: 1 }] } });
check(JSON.parse(withHarmony.body).harmony === "I V@2" && JSON.parse(withHarmony.body).features.length === 1, "a render passes the harmony and features");
check(JSON.parse(M.renderRequest({ motif: { form: "full", degrees: "1:q", meter: "4/4" } }).body).harmony === undefined, "a render without harmony leaves it out");
check(JSON.parse(M.randomRequest({ meter: "4/4", role: "accompaniment", seed: 1 }).body).role === undefined, "random motifs have no role option");
const blank = M.motifFormValues(null);
check(blank.notes === "" && blank.meter === "4/4" && blank.pickup === "0" && blank.form === "full", "a new motif form has defaults");

const rebuilt = M.buildMotifBody({ ...values, notes: " C4:q ", pickup: "" }, body);
check(rebuilt.motif.notes === "C4:q" && rebuilt.motif.material === "omt.material.natural-minor" && !("key" in rebuilt.motif) && rebuilt.motif.pickup === "0", "body trims notes, stores the material and no key text, and an empty pickup is 0");
check(M.buildMotifBody({ ...values, material: "omt.material.major" }, null).motif.material === "omt.material.major" && M.buildMotifBody({ ...values, material: "omt.material.dorian" }, null).motif.material === "omt.material.major", "the body material is the chosen one, major for anything else");
check(M.buildMotifBody({ ...values, form: "rhythm", material: "omt.material.natural-minor" }, null).motif.material === "omt.material.natural-minor", "every form is written in C");
check(same(rebuilt.motif.features, [{ label: "a", first: 0, last: 1 }]), "body stores features from 0");
check(rebuilt.uses.length === 1 && rebuilt.motif.derived.source_item === "mot-1", "body keeps uses and derived");
check(rebuilt.motif.analysis === null, "body leaves the analysis to the backend");
check(M.buildMotifBody(blank, null).uses.length === 0 && M.buildMotifBody(blank, null).motif.derived === null, "a new body has no uses or derived");
check(M.motifForms().map((f) => f.value).join() === "full,rhythm,contour" && M.motifRoles().map((r) => r.value).join() === "melodic,accompaniment", "the roles and forms are the vocabulary's");

const saveNew = M.buildSaveMotifRequest({ name: " Hook ", project_id: "prj-a", body: rebuilt });
check(isCommand(saveNew, "item.create") && params(saveNew).type === "motif", "a new motif is item.create");
check(params(saveNew).name === "Hook", "new motif name is trimmed");
const saveVersion = M.buildSaveMotifRequest({ id: "mot-1", note: " tweak ", body: rebuilt });
check(isCommand(saveVersion, "item.save", "mot-1") && params(saveVersion).note === "tweak", "an existing motif is item.save with its note");
check(!("note" in params(M.buildSaveMotifRequest({ id: "mot-1", note: " ", body: rebuilt }))), "an empty note is left out");

// Features.
check(M.newFeatureRow([]).label === "a" && M.newFeatureRow([{ label: "a" }]).label === "b", "a new feature row takes the next free letter");

// Analysis rows.
const heard = {
  analysis: {
    motive: {},
    analysis: {
      scale_degrees: { all: [{ degree: 1, alteration: 0 }, { degree: 3, alteration: 0 }, { degree: 4, alteration: 1 }] },
      intervals: [{ semitones: 3, steps: 2 }, { semitones: -1, steps: -1 }],
      contour: { shape: "arch", directions: ["up", "down"] },
      rhythm: [
        { bar: 0, beat: { num: 0, den: 1 }, duration: { num: 1, den: 4 }, on_beat: true },
        { bar: 1, beat: { num: 3, den: 2 }, duration: { num: 1, den: 8 }, on_beat: false },
      ],
      implied_harmony: { key_known: true, candidates: [{ roman_numeral: "i", function: "tonic", fit: 0.8333 }] },
    },
  },
};
const info = M.analysisRows(heard);
check(info.analyzed && info.rows.map((r) => r.label).join() === "Scale degrees,Intervals,Contour,Rhythm,Implied harmony", "analysis rows in order");
const text = (label) => info.rows.find((r) => r.label === label).text;
check(text("Scale degrees") === "1 3 #4", "scale degrees show raised degrees");
check(text("Intervals") === "+3 (+2 steps), -1 (-1 steps)", "intervals show semitones and steps");
check(text("Contour") === "arch (up, down)", "contour shows shape and directions");
check(text("Rhythm") === "bar 1 beat 1, length 1/4; bar 2 beat 2+1/2, length 1/8 (off the beat)", "rhythm shows bar, beat and length");
check(text("Implied harmony") === "i (tonic) 83%", "harmony shows numeral, function and fit");
const unkeyed = M.analysisRows({
  analysis: { analysis: { scale_degrees: null, intervals: [{ semitones: 2, steps: null }], contour: { shape: "single note", directions: [] }, rhythm: [], implied_harmony: { key_known: false, candidates: [] } } },
});
check(unkeyed.rows.every((r) => r.label !== "Scale degrees"), "no scale degrees without a key");
check(unkeyed.rows.find((r) => r.label === "Intervals").text === "+2", "intervals without a key are semitones");
check(unkeyed.rows.find((r) => r.label === "Implied harmony").text === "needs a key", "harmony needs a key");
const none = M.analysisRows({ analysis: null }, "composer not found");
check(!none.analyzed && none.message === "Not analyzed: composer not found", "a null analysis names the reason");
check(M.analysisRows(null).message === "Not analyzed", "no motif is not analyzed");

// Motif forms: hints, MIDI import, degrees.
check(M.formHint("full") === M.NOTE_HINT, "the full form shows the note list hint");
check(M.formHint("rhythm").includes("x:q x:e x:e x:h"), "the rhythm form shows the rhythm syntax");
check(M.formHint("contour").includes("0 2 1 3"), "the contour form shows the contour syntax");
check(M.formHint("rhythm") !== M.formHint("contour"), "each form has its own hint");
const imported = M.importToFormValues({ notes_text: "C4:q Eb4:q", meter: "3/4", key: "C minor", material: "omt.material.natural-minor", moved_from: "D minor", key_found: true, pickup: "1" });
check(imported.form === "full" && imported.notes === "C4:q Eb4:q" && imported.meter === "3/4", "an import fills a full motif");
check(imported.material === "omt.material.natural-minor" && imported.pickup === "1", "an import fills the material and pickup");
const unkeyedImport = M.importToFormValues({ notes_text: "D4:q", meter: "4/4", key: "C major", material: "omt.material.major", moved_from: null, key_found: false, pickup: "0" });
check(unkeyedImport.material === "omt.material.major" && unkeyedImport.pickup === "0", "an import without a key reads as C major");
check(M.importNotice({ key: "C minor", material: "omt.material.natural-minor", moved_from: "D minor", key_found: true }) === "Moved from D minor to C minor.", "the notice names the key the notes moved from");
check(M.importNotice({ key: "C major", material: "omt.material.major", moved_from: null, key_found: false }) === "No key found: read as C major.", "the notice says when no key was found");
check(M.importNotice({ key: "C minor", material: "omt.material.natural-minor", moved_from: null, key_found: true }) === "Filled from the MIDI file.", "a MIDI file already in C is just filled");
check(M.importToFormValues(null).meter === "4/4" && M.importToFormValues(null).notes === "", "an empty import answer gives defaults");
const importRequest = M.buildImportMidiRequest("file");
check(importRequest.method === "POST" && importRequest.url === "/api/materials/motifs/import" && importRequest.body === "file", "the import posts the file");
check(M.buildMotifBody(values, { ...body, motif: { ...body.motif, degrees: "1:q 3:e" } }).motif.degrees === "1:q 3:e", "body keeps the degrees");
check(M.buildMotifBody(blank, null).motif.degrees === null, "a new body has no degrees");
const withDegrees = M.analysisRows({ form: "full", degrees: "1:q 3:e", analysis: heard.analysis });
check(withDegrees.rows[0].label === "Degree list" && withDegrees.rows[0].text === "1:q 3:e", "the degree list is the first analysis row");
const rhythmRows = M.analysisRows({
  form: "rhythm",
  degrees: "x:q x:e",
  analysis: { analysis: { intervals: null, contour: null, rhythm: [{ bar: 0, beat: { num: 0, den: 1 }, duration: { num: 1, den: 4 }, on_beat: true }], implied_harmony: { key_known: false, candidates: [] } } },
});
check(rhythmRows.rows.map((r) => r.label).join() === "Rhythm list,Rhythm", "a rhythm motif shows its list and rhythm only");
const contourRows = M.analysisRows({
  form: "contour",
  degrees: "0 2 1",
  analysis: { analysis: { intervals: null, contour: { shape: "arch", directions: ["up", "down"] }, rhythm: null, implied_harmony: { key_known: false, candidates: [] } } },
});
check(contourRows.rows.map((r) => r.label).join() === "Contour list,Contour", "a contour motif shows its list and contour only");

// Item references.
const statuses = [
  { item_id: "mot-1", version: 1, current_version: 3, name: "Hook", type: "motif", project_id: "prj-a", missing: false },
  { item_id: "mot-2", version: 2, current_version: 2, name: "Tail", type: "motif", project_id: "prj-b", missing: false },
  { item_id: "mot-9", version: 1, name: "Gone", missing: true },
];
const useInfo = M.useRows(statuses, projects);
check(useInfo[0].stale && useInfo[0].note === "source updated to v3" && useInfo[0].current === 3, "a newer source version flags the use");
check(!useInfo[1].stale && useInfo[1].note === "", "an unchanged source is not flagged");
check(useInfo[2].missing && !useInfo[2].stale && useInfo[2].note === "source deleted", "a missing source says deleted, not updated");
check(useInfo[0].project === "Sonata" && useInfo[0].type === "motif", "use rows carry project and type");
check(M.hasUpdatedUse(statuses) && !M.hasUpdatedUse([statuses[1], statuses[2]]) && !M.hasUpdatedUse(null), "badge only for a newer source");
const upd = M.buildUpdateUseRequest("thm-1", "mot-1");
check(isCommand(upd, "item.update-use", "thm-1") && same(params(upd), { source_id: "mot-1" }), "update use is item.update-use with the source");

const all = [
  { id: "mot-1", type: "motif", name: "Hook", project_id: "prj-a", current_version: 3 },
  { id: "mot-2", type: "motif", name: "Tail", project_id: "prj-b", current_version: 1 },
  { id: "thm-1", type: "theme", name: "Main hook", project_id: "prj-b", current_version: 2 },
];
check(same(M.pickerItems(all, projects, {}).map((i) => i.id), ["mot-1", "thm-1", "mot-2"]), "picker lists items of every project by name");
check(same(M.pickerItems(all, projects, { query: "HOOK" }).map((i) => i.id), ["mot-1", "thm-1"]), "picker searches by name, any case");
check(same(M.pickerItems(all, projects, { type: "theme" }).map((i) => i.id), ["thm-1"]), "picker filters by type");
check(same(M.pickerItems(all, projects, { project: "prj-b" }).map((i) => i.id), ["thm-1", "mot-2"]), "picker filters by project");
check(same(M.pickerItems(all, projects, { type: "all", project: "all" }).map((i) => i.id), ["mot-1", "thm-1", "mot-2"]), "all means no filter");
check(same(M.pickerItems(all, projects, { exclude: ["mot-1", "mot-2"] }).map((i) => i.id), ["thm-1"]), "picker leaves out excluded items");
check(M.pickerItems(all, projects, {})[0].label === "Hook (motif, Sonata) v3", "picker label");
check(M.pickerItems(null, projects, {}).length === 0, "no items, no choices");

const base = { uses: [{ item_id: "mot-1", version: 1, copy: {} }], motif: { notes: "x" } };
const added = M.bodyWithUse(base, "mot-2", 4, true);
check(added.uses.length === 2 && same(added.uses[1], { item_id: "mot-2", version: 4 }), "adding a use appends it at that version");
check(base.uses.length === 1, "the original body is not changed");
check(M.bodyWithUse(base, "mot-1", 3, true).uses.length === 1 && M.bodyWithUse(base, "mot-1", 3, true).uses[0].version === 3, "adding an existing use replaces its version");
check(M.bodyWithUse(base, "mot-1", 1, false).uses.length === 0, "removing a use drops it");
check(M.bodyWithUse(base, "mot-1", 1, false).motif.notes === "x", "the rest of the body is kept");
check(M.bodyWithUse(null, "mot-1", 1, true).uses.length === 1, "a missing body gets a uses array");
const vreq = M.buildAddVersionRequest("thm-1", added, " Added use ");
check(isCommand(vreq, "item.save", "thm-1") && params(vreq).note === "Added use", "version request with note");
check(!("note" in params(M.buildAddVersionRequest("thm-1", added))), "version request without note");

const users = M.usedByRows([{ id: "thm-1", name: "Main", type: "theme", project_id: "prj-b", version: 2 }], projects);
check(users[0].label === "Main (theme) uses v2" && users[0].project === "Etudes", "used by rows");
check(M.usedByRows(null, projects).length === 0, "no users, no rows");

check(isCommand(M.buildDeleteItemRequest("mot-1", false), "item.delete", "mot-1") && same(params(M.buildDeleteItemRequest("mot-1", false)), {}), "delete without force");
check(same(params(M.buildDeleteItemRequest("mot-1", true)), { force: true }), "delete with force");
const warn = M.deleteWarning("Hook", 409, { ok: false, error: "item is used", used_by: [{ id: "a", name: "Main" }, { id: "b" }] });
check(warn && warn.users.join() === "Main,b" && warn.message.includes("used by 2 items: Main, b"), "warning names the users");
check(M.deleteWarning("Hook", 409, { used_by: [{ id: "a", name: "Main" }] }).message.includes("1 item:"), "warning uses the singular");
check(M.deleteWarning("Hook", 404, { error: "not found" }) === null, "another failure is not the warning");
check(M.deleteWarning("Hook", 409, null) === null, "a 409 without users is not the warning");

const renderBody = {
  motif: { role: "accompaniment", form: "full", meter: "3/4", material: "omt.material.natural-minor", pickup: "1", degrees: "1:q 3:e 5:e" },
};
const rendered = M.renderRequest(renderBody);
const renderedBody = JSON.parse(rendered.body);
check(rendered.method === "POST" && rendered.url === "/api/materials/render", "render request is a POST to /render");
check(renderedBody.degrees === "1:q 3:e 5:e" && renderedBody.key === "C minor" && renderedBody.pickup === "1", "render request carries the stored motif, in C");
check(JSON.parse(M.renderRequest(renderBody, "D minor").body).key === "D minor", "a random motif renders in the key it was drawn in");
check(!("render_key" in renderedBody) && renderedBody.role === "accompaniment", "render request carries the role and no render key");
const plain = JSON.parse(M.renderRequest({ motif: { form: "rhythm", degrees: "x:q x:q", meter: "4/4", material: null } }).body);
check(!("key" in plain) && !("render_key" in plain) && plain.pickup === "0" && plain.role === "melodic", "render request leaves out a missing key and defaults the pickup and role");
check(M.RENDER_KEYS === undefined, "the render keys are gone");
const urls = M.versionScoreUrls("mot-1", 2, "D minor");
check(urls.musicxmlUrl === "/api/materials/items/mot-1/versions/2/score", "score url has no key query");
check(urls.playbackUrl === "/api/materials/items/mot-1/versions/2/playback", "playback url has no key query");

const random = M.randomRequest({ mode: "minor", meter: " 3/4 ", key: "D major", form: "contour", style_id: "sty-1", count: 20, seed: "7" });
const randomBody = JSON.parse(random.body);
check(random.method === "POST" && random.url === "/api/materials/motifs/random", "random request is a POST to /motifs/random");
check(randomBody.material === "random" && !("mode" in randomBody) && randomBody.meter === "random" && !("key" in randomBody) && randomBody.form === "full", "random request leaves the mode and meter to the Composer and sends no key");
check(randomBody.count === 1 && randomBody.seed === 7 && !("style_id" in randomBody) && M.RANDOM_DEFAULT_COUNT === undefined && M.randomProblems === undefined, "random request draws one motif from the seed with no style and no count option");
check(M.randomSettings === undefined, "the draw of the mode and meter is not computed here");
check(same(M.drawnSettings({ drawn: { material: "omt.material.natural-minor", meter: "6/8" } }), { material: "omt.material.natural-minor", meter: "6/8" }), "the drawn material and meter come from the answer");
for (const captured of readJson(path.join(__dirname, "../../backend/tests/fixtures/composer/b11_captures.json")).random) {
  const material = captured.mode === "minor" ? "omt.material.natural-minor" : "omt.material.major";
  const answer = { drawn: { material, meter: captured.meter } };
  check(same(M.drawnSettings(answer), { material, meter: captured.meter }) && JSON.parse(M.randomRequest({ seed: captured.seed }).body).seed === captured.seed, `seed ${captured.seed} shows the captured ${captured.mode} ${captured.meter}`);
}
check(M.drawnSettings({ drawn: { material: "omt.material.natural-minor" } }) === null && M.drawnSettings({}) === null && M.drawnSettings(null) === null, "an answer that names no draw gives none");

const candidate = {
  seed: 3,
  motive: {
    motive: { key: { tonic_pitch_class: 3, material: "omt.material.natural-minor" } },
    analysis: { notes_text: "c4:q eb4:e g4:e", degrees_text: "1:q b3:e 5:e", rhythm_text: "x:q x:e x:e", pickup: "1" },
  },
};
check(M.candidateKeyText(candidate) === "D# minor", "the candidate key is written as the Composer writes keys");
const saved = M.randomCandidateToBody(candidate, { form: "full", meter: "3/4", role: "accompaniment" });
check(saved.motif.notes === "c4:q eb4:e g4:e" && saved.motif.material === "omt.material.natural-minor" && saved.motif.meter === "3/4", "a saved random motif keeps the notes, material and meter");
check(saved.motif.pickup === "1" && saved.motif.role === "accompaniment" && saved.motif.form === "full" && saved.motif.degrees === null, "a saved random motif keeps the pickup and role and leaves the degrees to the backend");
const savedRhythm = M.randomCandidateToBody(candidate, { form: "rhythm", meter: "4/4" });
check(savedRhythm.motif.notes === "x:q x:e x:e" && savedRhythm.motif.role === "melodic", "a rhythm candidate is saved from its rhythm text");
check(M.randomCandidateToBody({ motive: { analysis: {} } }, {}).motif.material === null, "a candidate without a key saves a null material");
const drawnMotive = { motive: { key: { tonic_pitch_class: 0, material: "omt.material.major" }, notes: [] }, analysis: { notes_text: "C4:q E4:e G4:e", degrees_text: "1:q 3:e 5:e", pickup: "0" } };
const wrapped = M.randomCandidate(drawnMotive, 5);
check(wrapped.seed === 5 && wrapped.motive.analysis.notes_text === "C4:q E4:e G4:e" && M.candidateKeyText(wrapped) === "C major", "a motive as the Composer draws it is wrapped for the page");
check(M.randomCandidateToBody(wrapped, { form: "full", meter: "4/4" }).motif.notes === "C4:q E4:e G4:e", "a drawn candidate gives its notes to the editor");
check(M.randomCandidate(candidate.motive ? candidate : null, 7).motive.analysis.notes_text === "c4:q eb4:e g4:e", "an already wrapped candidate is kept");
check(M.rerollSeed === undefined, "the model has no reroll seed (one Random button)");
const segmentedCandidate = {
  seed: 3,
  motive: {
    motive: { key: { tonic_pitch_class: 0, material: "omt.material.major" } },
    analysis: {
      notes_text: "c4:q d4:q e4:h", pickup: "0",
      base_harmony: { given: true, text: "I V" },
      segments: { given: true, items: [{ label: "a", first: 0, last: 0 }, { label: "a'", first: 1, last: 2 }] },
    },
  },
};
const segmentedBody = M.randomCandidateToBody(segmentedCandidate, { form: "full", meter: "4/4" });
check(same(segmentedBody.motif.features, [{ label: "a", first: 0, last: 0 }, { label: "a'", first: 1, last: 2 }]) && segmentedBody.motif.segments_proposed === true,
  "a random motif keeps its segments, marked proposed");
check(segmentedBody.motif.harmony === "I V", "a random motif keeps its base harmony");
check(M.randomCandidateToBody(candidate, {}).motif.harmony === null && same(M.randomCandidateToBody(candidate, {}).motif.features, []), "a candidate without segments or harmony saves none");
const proposedBody = { motif: { notes: "C4:q", features: [{ label: "a", first: 0, last: 0 }], segments_proposed: true } };
check(M.motifFormValues(proposedBody).segmentsProposed === true && M.motifFormValues({ motif: {} }).segmentsProposed === false && M.motifFormValues(null).segmentsProposed === false, "form values read segments_proposed, false when absent");
check(M.buildMotifBody(M.motifFormValues(proposedBody), proposedBody).motif.segments_proposed === true, "the body carries segments_proposed");
check(M.buildMotifBody({ ...M.motifFormValues(proposedBody), segmentsProposed: false }, proposedBody).motif.segments_proposed === false, "an edited segment list is not proposed");
check(M.buildMotifBody(M.motifFormValues(null), null).motif.segments_proposed === false, "a new motif body is not proposed");
check(M.motifBodyChanged({ ...M.motifFormValues(proposedBody), segmentsProposed: false }, proposedBody), "taking over proposed segments is a body change");

check(M.analysisRows({ analysis: null, analysis_error: "composer not found" }).message === "Not analyzed: composer not found", "the stored reason is read when no argument is given");
check(M.analysisRows({ analysis: null, analysis_error: "stored" }, "given").message === "Not analyzed: given", "an argument wins over the stored reason");
check(M.analysisRows({ analysis: null, analysis_error: null }).message === "Not analyzed", "no stored reason gives a bare status");
check(M.analysisRows(heard).message === "Analyzed", "an analyzed motif reads Analyzed");
check(isCommand(M.buildReanalyzeRequest("mot-1"), "item.reanalyze", "mot-1"), "re-analyze is the item.reanalyze command");
check(M.composerAvailable({ ok: true, data: { available: true } }) && !M.composerAvailable({ ok: true, data: { available: false } }) && !M.composerAvailable({ ok: false }), "the Composer counts as available only when the status says so");

/* The motif tabs, the single Save and the Used by table (P18 B3). */
check(same(M.MOTIF_TABS.map((t) => t.label), ["Motif", "Variants", "Used by", "Other"]), "four motif tabs in order");
check(M.readMotifTab() === "motif", "the motif tab falls back to Motif without storage");
global.localStorage = { getItem: () => "other", setItem: () => {} };
check(M.readMotifTab() === "other", "a remembered tab is read");
global.localStorage = { getItem: () => "nope", setItem: () => {} };
check(M.readMotifTab() === "motif", "an unknown remembered tab falls back to Motif");
global.localStorage = {
  getItem: () => {
    throw new Error("blocked");
  },
  setItem: () => {
    throw new Error("blocked");
  },
};
check(M.readMotifTab() === "motif", "a throwing storage falls back to Motif");
let wrote = true;
try {
  M.writeMotifTab("variants");
} catch (err) {
  wrote = false;
}
check(wrote, "a throwing storage does not break writing the tab");
delete global.localStorage;

const stored = {
  uses: [],
  motif: {
    role: "melodic",
    form: "full",
    notes: "C4:q E4:e G4:e",
    meter: "4/4",
    material: "omt.material.major",
    pickup: "0",
    harmony: null,
    features: [{ label: "head", first: 0, last: 1 }],
    analysis: { analysis: {} },
    degrees: "1:q 3:e 5:e",
    derived: null,
  },
};
const editorValues = () => M.motifFormValues(stored);
check(!M.motifBodyChanged(editorValues(), stored), "an untouched editor is not a change");
check(M.motifBodyChanged({ ...editorValues(), notes: "C4:q E4:e A4:e" }, stored), "other notes are a change");
check(M.motifBodyChanged({ ...editorValues(), material: "omt.material.natural-minor" }, stored), "another material is a change");
check(M.motifBodyChanged({ ...editorValues(), features: [{ label: "tail", first: 1, last: 2 }] }, stored), "another segment is a change");
check(!M.motifBodyChanged({ ...editorValues(), notes: "  C4:q E4:e G4:e  " }, stored), "spaces around the notes are not a change");
check(!M.motifBodyChanged({ ...editorValues(), pickup: "" }, { motif: { ...stored.motif, pickup: "0" } }), "an empty pickup reads as 0");

check(M.motifSaveDecision({}, "x").kind === "none" && M.motifSaveDecision({}, "x").message === "Nothing to save.", "no change says Nothing to save.");
check(
  M.motifSaveDecision({ body: true }, "").kind === "blocked" && M.motifSaveDecision({ body: true }, "  ").message === "Give a version note.",
  "a body change without a note is blocked",
);
check(M.motifSaveDecision({ body: true }, "Shorter").kind === "save", "a body change with a note saves");
check(M.motifSaveDecision({ name: true }, "").kind === "save" && M.motifSaveDecision({ project: true }, "").kind === "save", "a rename or move needs no note");
check(M.motifSaveDecision({ nothing: true }, "note").kind === "none", "a note alone is not a change");

const createdRequest = M.buildSaveMotifRequest({ name: " New ", project_id: "prj-a", note: "Created", body: stored });
check(isCommand(createdRequest, "item.create") && params(createdRequest).note === "Created", "a new motif sends the Created note");
check(!("note" in params(M.buildSaveMotifRequest({ name: "n", project_id: "prj-a", body: stored }))), "a new motif without a note sends none");

const usedByUsers = [
  { id: "t1", name: "Theme", type: "theme", project_id: "prj-a", version: 1 },
  { id: "t2", name: "Piece", type: "piece", project_id: "prj-b", version: 3 },
];
const table = M.usedByTable(usedByUsers, projects, 3);
check(table.length === 2 && table[0].updateAvailable && !table[1].updateAvailable, "a use behind the current version has an update available");
check(table[0].project === "Sonata" && table[0].version === 1 && table[0].type === "theme", "the Used by row carries project, version and type");
check(M.usedByTable(null, projects, 1).length === 0, "no usedByUsers, no rows");

check(/head and tail/.test(M.SEGMENTS_HINT) && /Liquidation and motive-forms/.test(M.SEGMENTS_HINT), "the segments hint names the use");

/* The status area of a motif tab: sections write to it, an empty text clears it, no element is a no-op. */
const statusEl = { textContent: "", classList: new Set() };
statusEl.classList.add = Set.prototype.add;
statusEl.classList.remove = (name) => statusEl.classList.delete(name);
const tabStatus = M.motifTabStatus(statusEl);
tabStatus.show("Saving...");
check(statusEl.textContent === "Saving..." && !statusEl.classList.has("error"), "tab status shows progress without the error style");
tabStatus.show("Could not save the motif", true);
check(statusEl.textContent === "Could not save the motif" && statusEl.classList.has("error"), "tab status shows an error with the error style");
tabStatus.show("Saved");
check(statusEl.textContent === "Saved" && !statusEl.classList.has("error"), "the next message replaces the error style");
tabStatus.show("Could not save the motif", true);
tabStatus.clear();
check(statusEl.textContent === "" && !statusEl.classList.has("error"), "clear empties the tab status and drops the error style");
tabStatus.show("", true);
check(!statusEl.classList.has("error"), "an empty error message leaves no error style");
check(M.motifTabStatus(null).show("x", true) === undefined, "tab status without an element does nothing");

/* Variants belong to their motif (R-187): lists and pickers leave them out, and the delete counts them. */
const withVariants = [
  { id: "mot-1", name: "Hook", type: "motif", project_id: "prj-a", current_version: 1, updated_at: "2026-01-01T00:00:00Z", variant_of: null },
  { id: "mot-v1", name: "Hook inverted", type: "motif", project_id: "prj-a", current_version: 1, updated_at: "2026-01-02T00:00:00Z", variant_of: "mot-1" },
  { id: "mot-v2", name: "Hook inverted augmented", type: "motif", project_id: "prj-a", current_version: 1, updated_at: "2026-01-03T00:00:00Z", variant_of: "mot-v1" },
  { id: "mot-3", name: "Promoted", type: "motif", project_id: "prj-a", current_version: 1, updated_at: "2026-01-04T00:00:00Z", variant_of: null },
  { id: "mot-4", name: "Other", type: "motif", project_id: "prj-a", current_version: 1, updated_at: "2026-01-05T00:00:00Z" },
];
check(same(M.pickerItems(withVariants, projects, {}).map((i) => i.id), ["mot-1", "mot-4", "mot-3"]), "the picker leaves variants out");
check(same(M.pickerItems(withVariants, projects, { query: "inverted" }), []), "searching for a variant's name finds nothing");
check(same(M.itemRows(withVariants, projects).map((r) => r.id), ["mot-4", "mot-3", "mot-1"]), "the Motifs list leaves variants out");
check(M.isVariantItem(withVariants[1]) && !M.isVariantItem(withVariants[0]) && !M.isVariantItem(withVariants[4]) && !M.isVariantItem(null),
      "an item is a variant when variant_of is set");
check(M.variantCount(withVariants, "mot-1") === 2, "variants are counted through variants of variants");
check(M.variantCount(withVariants, "mot-v1") === 1 && M.variantCount(withVariants, "mot-3") === 0 && M.variantCount(null, "mot-1") === 0,
      "a leaf, a motif without variants and no list count zero or their own");
check(M.deleteConfirmText("motif", "Hook", 2) === 'Delete "Hook" and its 2 variants?', "the confirm names the variant count");
check(M.deleteConfirmText("motif", "Hook", 1) === 'Delete "Hook" and its 1 variant?', "the confirm says variant for one");
check(M.deleteConfirmText("theme", "Song", 0) === 'Delete theme "Song" and all its versions?', "no variants keeps the old confirm");

/* Base harmony placeholder and tone chips (R-195, R-196). */
const toneBody = (tones, extra) => ({
  motif: {
    harmony: null,
    analysis: { analysis: { base_harmony: { text: "I V@2", given: false }, tones, warnings: ["Note 3 (B5) does not resolve"], ...(extra || {}) } },
  },
});
check(M.harmonyPlaceholder(toneBody([])) === "Proposed: I V@2", "the placeholder is the proposed base harmony");
check(M.harmonyPlaceholder(null) === "I (proposed)" && M.harmonyPlaceholder({ motif: { analysis: null } }) === "I (proposed)", "before an analysis the placeholder is I (proposed)");
check(M.harmonyPlaceholder({ motif: { analysis: { analysis: { base_harmony: { text: "VI", given: true } } } } }) === "I (proposed)", "a given harmony is not shown as proposed");
const tones = [
  { note: 1, role: "chord", type: null, resolves_to: null, unresolved: false, overridden: false },
  { note: 2, role: "non-chord", type: "passing", resolves_to: 3, unresolved: false, overridden: false },
  { note: 3, role: "non-chord", type: "changing-tone", resolves_to: null, unresolved: true, overridden: true },
  { note: 4, role: "unclassified", type: null, resolves_to: null, unresolved: true, overridden: false },
  { note: 5, role: "non-chord", type: "double-neighbor", resolves_to: 6, unresolved: false, overridden: false },
];
const chips = M.toneChips(toneBody(tones));
check(same(chips.map((c) => c.text), ["1 CT", "2 PT -> 3", "3 CT2", "4 ?", "5 DN -> 6"]), "chips read note, label and resolution");
check(chips[2].unresolved && chips[2].overridden && !chips[1].unresolved && chips[3].unresolved, "chips carry unresolved and overridden");
const abbreviations = ["passing", "neighbor", "appoggiatura", "suspension", "retardation", "escape", "anticipation", "double-neighbor", "changing-tone", "pedal"];
check(same(abbreviations.map((t) => M.toneChips(toneBody([{ note: 1, role: "non-chord", type: t, resolves_to: null, unresolved: false }]))[0].label), ["PT", "NT", "APP", "SUS", "RET", "ET", "ANT", "DN", "CT2", "PED"]), "the ten type abbreviations");
check(same(M.toneChips(null), []) && same(M.toneChips({ motif: { analysis: { analysis: { tones: null } } } }), []), "no tones, no chips");
check(M.toneWarningText(toneBody([])) === "Note 3 (B5) does not resolve" && M.toneWarningText(null) === "", "the warnings become one status line");
check(same(M.TONE_ROLE_OPTIONS.map((o) => o.value), ["", "chord", ...abbreviations]), "the override select offers Automatic, chord and each type");
check(M.toneRoleLabel("chord") === "CT" && M.toneRoleLabel("changing-tone") === "CT2" && M.toneRoleLabel("") === null, "override values have labels");
check(same(M.withToneOverride({ 2: "passing" }, 3, "chord"), { 2: "passing", 3: "chord" }), "an override is added");
check(same(M.withToneOverride({ 2: "passing", 3: "chord" }, 2, ""), { 3: "chord" }), "Automatic removes an override");
const overridden = { motif: { role: "melodic", form: "full", notes: "C4:q", meter: "4/4", key: "C major", pickup: "0", harmony: null, features: [], tone_overrides: { 2: "passing" } } };
check(same(M.motifFormValues(overridden).toneOverrides, { 2: "passing" }) && same(M.motifFormValues({ motif: {} }).toneOverrides, {}), "form values read the overrides");
check(same(M.buildMotifBody({ ...M.motifFormValues(overridden), toneOverrides: { 2: "pedal" } }, overridden).motif.tone_overrides, { 2: "pedal" }), "the body holds the overrides");
check(!("tone_overrides" in M.buildMotifBody(M.motifFormValues({ motif: {} }), null).motif), "a body without overrides leaves the member out");
check(M.motifBodyChanged({ ...M.motifFormValues(overridden), toneOverrides: {} }, overridden) && !M.motifBodyChanged(M.motifFormValues(overridden), overridden), "changing an override is a body change");

finish();
