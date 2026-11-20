/* Covers the pure helpers of the Motive links tool (public/models/motive-links-model.js): the scope body of a run for each kind of picker selection,
 * the progress and skipped lines, the rows of links, groups, earlier runs and a piece's motive families, and the feature rows of a motive. Plain Node
 * script, run through ctest. */
import M from "../public/models/motive-links-model.js";
import { check, same, finish } from "./support/check.js";

/* The scope of a run. */
check(same(M.scopeBody({ kind: "all" }), { scope: { kind: "all" } }), "all pieces need no choice");
check(same(M.scopeBody({ kind: "collection", collectionId: "col-1" }), { scope: { kind: "collection", collection_id: "col-1" } }), "a collection scope names the collection");
check(M.scopeBody({ kind: "collection", collectionId: " " }) === null && M.scopeBody({ kind: "subject" }) === null && M.scopeBody({ kind: "project", projectId: "" }) === null, "a scope that names nothing yet is null");
check(same(M.scopeBody({ kind: "filter", composer: "Field", source: "", tag: "slow" }), { scope: { kind: "filter", filter: { composer: "Field", tag: "slow" } } }), "a filter scope keeps only the fields given");
check(M.scopeBody({ kind: "filter", composer: "", source: "", tag: "" }) === null, "a filter with no field is null");
check(M.scopeBody(null) === null && M.scopeBody({ kind: "zzz" }) === null, "no selection and an unknown kind are null");
check(same(M.scopeBody({ kind: "subject", subjectId: "s1" }), { scope: { kind: "subject", subject_id: "s1" } }) && same(M.scopeBody({ kind: "project", projectId: "p1" }), { scope: { kind: "project", project_id: "p1" } }), "subject and project scopes name their id");
check(M.scopeTitle("collection") === "Collection" && M.scopeTitle("zzz") === "zzz", "a kind has a title");

/* The progress line. */
check(M.progressText({ status: "running", done: 3, total: 10 }) === "Comparing 3 of 10 pairs", "a running run shows the count");
check(M.progressText({ status: "failed", message: "disk full" }) === "Run failed: disk full" && M.progressText({ status: "failed" }) === "Run failed", "a failed run says so, with its message");
check(M.progressText({ status: "done", result: { compared: 4, cached: 6, pairs: 10 } }) === "compared 4, cached 6 of 10 pairs", "a finished run says what it compared and took from the cache");
check(M.progressText(null) === "", "no run has no line");
check(same(M.skippedLines({ skipped: [{ id: "p1", reason: "no motives" }] }), ["p1: no motives"]) && same(M.skippedLines(null), []), "skipped pieces are lines of id and reason");

/* Links and groups. */
const a = { piece: "p1", family: 0, title: "Nocturne", measures: { first: 1, last: 2 }, degrees: "1 2 3" };
const b = { piece: "p2", family: 1, measures: { first: 5, last: 5 } };
const result = {
  links: [{ a, b, reversed: true, steps: ["transposed", "inverted"], distance: { value: 0.234 }, similarity: { rhythm: 1, intervals: 0.5, contour: 1, harmony: 0 } }],
  groups: [{ members: [{ piece: "p1", family: 0 }, { piece: "p2", family: 1, title: "Etude" }] }],
};
const link = M.linkRows(result)[0];
check(link.a.measures === "m1-2" && link.b.measures === "m5" && link.b.title === "p2", "a link end shows its measures and falls back to the piece id for a title");
check(link.steps === "transposed then inverted" && link.distance === "23%" && link.reversed === true, "a link shows its steps joined, the distance as a percent and whether it is reversed");
check(same(link.kept, ["rhythm", "contour"]) && same(link.changed, ["intervals", "harmony"]), "a link lists the dimensions kept and changed");
check(same(M.linkRows(null), []) && M.measuresText(null) === "-" && M.measuresText({ first: 3 }) === "m3", "no links is no rows, and measures read as text");
const group = M.groupRows(result)[0];
check(group.members.length === 2 && group.members[0].title === "Nocturne" && group.members[1].title === "Etude", "a group member is resolved against the link ends, and keeps a title it brings");

/* Earlier runs. */
const names = { collections: { "col-1": "Set" } };
check(M.scopeText({ kind: "collection", collection_id: "col-1" }, names) === "Collection Set" && M.scopeText({ kind: "subject", subject_id: "s9" }) === "Subject s9", "a scope reads as a phrase with the name of what it points at");
check(M.scopeText({ kind: "filter", filter: { composer: "Field", tag: "slow" } }) === "composer Field, tag slow" && M.scopeText({ kind: "filter", filter: {} }) === "Composer / Source / Tag" && M.scopeText(null) === "All Library pieces", "a filter lists its fields and an empty one is its title");
const rows = M.runRows(
  [
    { id: "r1", status: "running", scope: { kind: "all" }, created_at: "2026-11-03T10:15:00Z", done: 1, total: 4 },
    { id: "r2", status: "failed", scope: { kind: "collection", collection_id: "col-1" }, created_at: "2026-11-02 09:00:00" },
    { id: "r3", status: "done", scope: { kind: "all" }, created_at: "later" },
  ],
  names,
);
check(rows[0].label === "All Library pieces · 2026-11-03 10:15" && rows[0].progress === "Comparing 1 of 4 pairs", "a running run shows its progress");
check(rows[1].label === "Collection Set · 2026-11-02 09:00" && rows[1].progress === "failed" && rows[2].progress === "" && rows[2].label.endsWith("later"), "a failed run says failed, a finished one nothing, and a date that cannot be read stays as it is");
check(same(M.runRows(null), []), "no runs is no rows");

/* One piece's families. */
const entries = [{ run: "r1", link: { a, b, steps: ["same"], distance: { value: 0 } } }, { run: "r1", link: { a: { piece: "p3", family: 0 }, b: { piece: "p4", family: 0 } } }, { run: "r2" }];
const mine = M.pieceLinkRows(entries, "p1", 0);
check(mine.length === 1 && mine[0].piece === "p2" && mine[0].measures === "m5" && mine[0].steps === "same" && mine[0].distance === "0%", "the links of a family name the other end");
check(M.pieceLinkRows(entries, "p2", 1)[0].piece === "p1" && M.pieceLinkRows(entries, "p1", 7).length === 0, "either end can be the piece, and another family has none");

/* Features. */
const features = M.motiveFeatureRows({ features: { degrees: [1, 2], intervals: [1], rhythm: ["q", "e"], ncts: ["passing", null, "neighbor"] } });
check(same(features.map((row) => row[0]), ["Degrees", "Intervals", "Rhythm", "Non-chord tones"]) && features[3][1] === "note 1: passing, note 3: neighbor", "a motive's features are labelled rows, with the non-chord tones by note");
check(M.motiveFeatureRows({ features: { ncts: [null] } })[0][1] === "none" && same(M.motiveFeatureRows({}), []), "no non-chord tones read none, and no features are no rows");

finish();
