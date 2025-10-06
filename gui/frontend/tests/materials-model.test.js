/* Covers the pure materials helpers (public/materials-model.js): request
 * builders, new item bodies, the project filter, item and version rows.
 * Plain Node script: `node frontend/tests/materials-model.test.js`. */
const M = require("../public/materials-model.js");

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

const projects = [
  { id: "prj-a", name: "Sonata" },
  { id: "prj-b", name: "Etudes" },
];

check(same(M.TYPES.map((t) => t.label), ["Motifs", "Themes", "Pieces"]), "three types in sidebar order");
check(M.typeInfo("theme").singular === "theme" && M.typeInfo("x") === null, "typeInfo finds a type or null");

const create = M.buildCreateProjectRequest("  Sonata ", "first");
check(create.method === "POST" && create.url === "/api/materials/projects", "create project posts to the projects route");
check(same(JSON.parse(create.body), { name: "Sonata", description: "first" }), "create project trims the name");
check(!("description" in JSON.parse(M.buildCreateProjectRequest("a").body)), "create project leaves out an empty description");
const rename = M.buildRenameProjectRequest("prj-a", " Op. 1 ");
check(rename.method === "PATCH" && rename.url === "/api/materials/projects/prj-a", "rename project patches the project");
check(same(JSON.parse(rename.body), { name: "Op. 1" }), "rename project sends the trimmed name");
check(M.buildDeleteProjectRequest("prj-a").method === "DELETE" && !M.buildDeleteProjectRequest("prj-a").body, "delete project has no body");

check(M.buildItemListRequest("motif", "all").url === "/api/materials/items?type=motif", "item list for all projects has no project");
check(
  M.buildItemListRequest("piece", "prj-a").url === "/api/materials/items?type=piece&project=prj-a",
  "item list for a project adds it",
);
check(M.buildVersionGetRequest("mot-1", 2).url === "/api/materials/items/mot-1/versions/2", "version route");

const motif = M.newItemBody("motif").motif;
check(motif.notes !== "" && motif.pickup === "0" && same(motif.features, []) && motif.derived === null, "new motif body is valid");
check(same(M.newItemBody("theme"), { uses: [], theme: { form: null, slots: [] } }), "new theme body");
check(same(M.newItemBody("piece"), { uses: [], piece: { structure: null } }), "new piece body");
const item = JSON.parse(M.buildCreateItemRequest("theme", " Main ", "prj-a").body);
check(item.type === "theme" && item.name === "Main" && item.project_id === "prj-a" && item.body.theme, "create item body");
check(same(JSON.parse(M.buildUpdateItemRequest("x", { name: " B ", project_id: "" }).body), { name: "B" }), "update item skips empty fields");

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
    key: "D minor",
    pickup: "1/2",
    features: [{ label: "a", first: 0, last: 1 }],
    analysis: { analysis: {} },
    derived: { source_item: "mot-1" },
  },
};
const values = M.motifFormValues(body);
check(values.role === "accompaniment" && values.key === "D minor" && values.pickup === "1/2", "form values read the motif");
check(same(values.features, [{ label: "a", first: 1, last: 2 }]), "features show note numbers from 1");
const blank = M.motifFormValues(null);
check(blank.notes === "" && blank.meter === "4/4" && blank.pickup === "0" && blank.form === "full", "a new motif form has defaults");
check(M.motifFormProblems(blank).length === 1 && M.motifFormProblems(blank)[0].field === "notes", "empty notes are a problem");
check(M.motifFormProblems(values).length === 0, "a filled form has no problems");
check(M.motifFormProblems({ ...values, meter: "four" })[0].field === "meter", "a bad meter is a problem");
check(M.motifFormProblems({ ...values, key: "Dm" })[0].field === "key", "a bad key is a problem");
check(M.motifFormProblems({ ...values, key: "" }).length === 0, "an empty key is fine");
check(M.motifFormProblems({ ...values, pickup: "x" })[0].field === "pickup", "a bad pickup is a problem");

const rebuilt = M.buildMotifBody({ ...values, notes: " D4:q ", key: " ", pickup: "" }, body);
check(rebuilt.motif.notes === "D4:q" && rebuilt.motif.key === null && rebuilt.motif.pickup === "0", "body trims notes, empty key is null, empty pickup is 0");
check(same(rebuilt.motif.features, [{ label: "a", first: 0, last: 1 }]), "body stores features from 0");
check(rebuilt.uses.length === 1 && rebuilt.motif.derived.source_item === "mot-1", "body keeps uses and derived");
check(rebuilt.motif.analysis === null, "body leaves the analysis to the backend");
check(M.buildMotifBody(blank, null).uses.length === 0 && M.buildMotifBody(blank, null).motif.derived === null, "a new body has no uses or derived");
check(M.MOTIF_FORMS.filter((f) => f.enabled).map((f) => f.value).join() === "full,rhythm,contour", "all three forms are enabled");

const saveNew = M.buildSaveMotifRequest({ name: " Hook ", project_id: "prj-a", body: rebuilt });
check(saveNew.url === "/api/materials/items" && JSON.parse(saveNew.body).type === "motif", "a new motif posts an item");
check(JSON.parse(saveNew.body).name === "Hook", "new motif name is trimmed");
const saveVersion = M.buildSaveMotifRequest({ id: "mot-1", note: " tweak ", body: rebuilt });
check(saveVersion.url === "/api/materials/items/mot-1/versions" && JSON.parse(saveVersion.body).note === "tweak", "an existing motif posts a version with its note");
check(!("note" in JSON.parse(M.buildSaveMotifRequest({ id: "mot-1", note: " ", body: rebuilt }).body)), "an empty note is left out");

// Features.
check(M.featureProblems([{ label: "a", first: 1, last: 2 }]).length === 0, "a good feature row passes");
check(M.featureProblems([{ label: "", first: 1, last: 1 }]).length === 1, "an empty label fails");
check(M.featureProblems([{ label: "abcdefghi", first: 1, last: 1 }]).length === 1, "a label over 8 characters fails");
check(M.featureProblems([{ label: "a", first: 1, last: 1 }, { label: "a", first: 2, last: 2 }]).length === 1, "a repeated label fails");
check(M.featureProblems([{ label: "a", first: 3, last: 2 }]).length === 1, "first after last fails");
check(M.featureProblems([{ label: "a", first: 0, last: 1 }]).length === 1, "note numbers start at 1");
check(M.featureProblems([{ label: "a", first: NaN, last: 1 }]).length === 1, "a blank number fails");
check(M.motifFormProblems({ ...values, features: [{ label: "", first: 1, last: 1 }] })[0].field === "features", "feature problems show on the form");
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
const imported = M.importToFormValues({ notes_text: "D4:q F4:q", meter: "3/4", key: "D minor", pickup: "1" });
check(imported.form === "full" && imported.notes === "D4:q F4:q" && imported.meter === "3/4", "an import fills a full motif");
check(imported.key === "D minor" && imported.pickup === "1", "an import fills the key and pickup");
const unkeyedImport = M.importToFormValues({ notes_text: "D4:q", meter: "4/4", key: null, pickup: "0" });
check(unkeyedImport.key === "" && unkeyedImport.pickup === "0", "an import without a key leaves it empty");
check(M.importToFormValues(null).meter === "4/4" && M.importToFormValues(null).notes === "", "an empty import answer gives defaults");
const importRequest = M.buildImportMidiRequest("file");
check(importRequest.method === "POST" && importRequest.url === "/api/materials/motifs/import" && importRequest.body === "file", "the import posts the file");
check(M.buildMotifBody(values, { ...body, motif: { ...body.motif, degrees: "1:q 3:e" } }).motif.degrees === "1:q 3:e", "body keeps the degrees");
check(M.buildMotifBody(blank, null).motif.degrees === null, "a new body has no degrees");
check(M.motifFormProblems({ ...values, pickup: "123" })[0].field === "pickup", "a pickup of three digits is a problem");
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
check(upd.method === "POST" && upd.url === "/api/materials/items/thm-1/uses/mot-1/update", "update use route");

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
check(vreq.url === "/api/materials/items/thm-1/versions" && JSON.parse(vreq.body).note === "Added use", "version request with note");
check(!("note" in JSON.parse(M.buildAddVersionRequest("thm-1", added).body)), "version request without note");

const users = M.usedByRows([{ id: "thm-1", name: "Main", type: "theme", project_id: "prj-b", version: 2 }], projects);
check(users[0].label === "Main (theme) uses v2" && users[0].project === "Etudes", "used by rows");
check(M.usedByRows(null, projects).length === 0, "no users, no rows");

check(M.buildDeleteItemRequest("mot-1", false).url === "/api/materials/items/mot-1", "delete without force");
check(M.buildDeleteItemRequest("mot-1", true).url === "/api/materials/items/mot-1?force=1", "delete with force");
check(M.buildDeleteItemRequest("mot-1", true).method === "DELETE", "delete uses DELETE");
const warn = M.deleteWarning("Hook", 409, { ok: false, error: "item is used", used_by: [{ id: "a", name: "Main" }, { id: "b" }] });
check(warn && warn.users.join() === "Main,b" && warn.message.includes("used by 2 items: Main, b"), "warning names the users");
check(M.deleteWarning("Hook", 409, { used_by: [{ id: "a", name: "Main" }] }).message.includes("1 item:"), "warning uses the singular");
check(M.deleteWarning("Hook", 404, { error: "not found" }) === null, "another failure is not the warning");
check(M.deleteWarning("Hook", 409, null) === null, "a 409 without users is not the warning");

if (failures > 0) {
  process.exit(1);
}
