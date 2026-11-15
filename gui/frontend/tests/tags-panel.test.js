/* Covers the pure Tags panel helpers (public/tags-panel.js): group rows,
 * the Unassigned and Untagged rows, the 12-row limit with Show all, the
 * filter each row opens, and which row is active.
 * Plain Node script: `node frontend/tests/tags-panel.test.js`. */
const P = require("../public/tags-panel.js");

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

const groups = {
  composers: [
    { value: "John Field", count: 12 },
    { value: "Chopin", count: 3 },
  ],
  sources: [{ value: "Op. 1", count: 2 }],
  tags: [
    { value: "etude", count: 5 },
    { value: "waltz", count: 1 },
  ],
  unassigned: { composer: 4, source: 0 },
  untagged: 6,
  total: 19,
};

const composerRows = P.groupRows(groups, "composer");
check(
  same(
    composerRows.map((r) => [r.label, r.count]),
    [["John Field", 12], ["Chopin", 3], ["Unassigned", 4]],
  ),
  "composer rows: values by count, then Unassigned",
);
check(same(composerRows[0].filter, { composer: "John Field" }), "a composer row filters by composer");
check(same(composerRows[2].filter, { unassigned: ["composer"] }), "Unassigned composer filters by unassigned composer");

const sourceRows = P.groupRows(groups, "source");
check(same(sourceRows.map((r) => r.label), ["Op. 1"]), "Unassigned is left out when its count is 0");
check(same(sourceRows[0].filter, { source: "Op. 1" }), "a source row filters by source");

const tagRows = P.groupRows(groups, "tag");
check(same(tagRows.map((r) => [r.label, r.count]), [["etude", 5], ["waltz", 1], ["Untagged", 6]]), "tag rows end with Untagged");
check(same(tagRows[0].filter, { tag: "etude" }), "a tag row filters by tag");
check(same(tagRows[2].filter, { untagged: true }), "Untagged filters by untagged");

const noUntagged = P.groupRows({ ...groups, untagged: 0 }, "tag");
check(noUntagged.length === 2, "Untagged is left out when its count is 0");

const empty = { composers: [], sources: [], tags: [], unassigned: { composer: 0, source: 0 }, untagged: 0, total: 0 };
check(P.groupRows(empty, "composer").length === 0, "an empty group has no rows");
check(P.groupRows(null, "tag").length === 0, "missing groups have no rows");

const many = {
  ...empty,
  composers: Array.from({ length: 15 }, (_, i) => ({ value: `c${i}`, count: i + 1 })),
  unassigned: { composer: 2, source: 0 },
};
const limited = P.groupRows(many, "composer");
check(limited.length === 13, "limit keeps 12 values plus Unassigned");
check(limited[0].label === "c14" && limited[11].label === "c3", "the top 12 are chosen by count");
check(limited[12].label === "Unassigned", "Unassigned still follows the limited values");
const all = P.groupRows(many, "composer", { showAll: true });
check(all.length === 16, "showAll lists every value plus Unassigned");
check(P.groupRows(many, "composer", { limit: 5 }).length === 6, "a custom limit applies");
check(P.valueCount(many, "composer") === 15, "valueCount is the number of values before the limit");

const tie = { ...empty, tags: [{ value: "b", count: 2 }, { value: "a", count: 2 }] };
check(same(P.groupRows(tie, "tag").map((r) => r.label), ["b", "a"]), "equal counts keep the route's order");

const blank = { composer: "", source: "", tag: "", unassigned: [], untagged: false, search: "", sort: "added" };
check(P.isActiveFilter({ composer: "John Field" }, { ...blank, composer: "John Field" }), "the matching composer row is active");
check(P.isActiveFilter({ composer: "John Field" }, { ...blank, composer: "john field" }), "values match case-insensitively");
check(!P.isActiveFilter({ composer: "John Field" }, blank), "no row is active when the Library filter is clear");
check(!P.isActiveFilter({ composer: "John Field" }, { ...blank, composer: "John Field", tag: "etude" }), "a combined filter matches no single row");
check(P.isActiveFilter({ unassigned: ["composer"] }, { ...blank, unassigned: ["composer"] }), "Unassigned composer is active");
check(!P.isActiveFilter({ unassigned: ["composer"] }, { ...blank, unassigned: ["source"] }), "Unassigned source does not match Unassigned composer");
check(P.isActiveFilter({ untagged: true }, { ...blank, untagged: true }), "Untagged is active");
check(P.isActiveFilter({ tag: "etude" }, { ...blank, tag: "etude", search: "x", sort: "title" }), "search and sort do not affect the active row");
check(!P.isActiveFilter({ tag: "etude" }, null), "a missing Library filter matches nothing");

if (failures > 0) {
  console.error(`${failures} check(s) failed`);
  process.exit(1);
}
console.log("tags-panel checks passed");
