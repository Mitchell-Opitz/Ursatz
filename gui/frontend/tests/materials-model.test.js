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

if (failures > 0) {
  process.exit(1);
}
