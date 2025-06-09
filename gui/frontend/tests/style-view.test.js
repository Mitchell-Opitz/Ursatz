/* Covers the pure style detail helper (public/style-view.js): rows by
 * dimension and category, order by count, "and n more" and the fraction
 * text. Plain Node script: `node frontend/tests/style-view.test.js`. */
const V = require("../public/style-view.js");

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

const statistics = {
  harmony: {
    chord: {
      labels: [
        { label: "I", count: 3, total: 10 },
        { label: "V", count: 5, total: 10 },
        { label: "IV", count: 3, total: 10 },
      ],
      more: 2,
    },
    cadence: { labels: [{ label: "authentic", count: 4, total: 4 }], more: 0 },
  },
  melody: { contour: { labels: [], more: 0 } },
  rhythm: { meter: { labels: [{ label: "4/4", count: 0, total: 0 }], more: 0 } },
};

const rows = V.styleSummaryRows(statistics);
check(same(rows.map((d) => d.dimension), ["harmony", "rhythm"]), "dimensions with nothing to show are left out");
check(rows[0].title === "Harmony", "dimension title is capitalized");
check(same(rows[0].categories.map((c) => c.category), ["chord", "cadence"]), "categories keep the given order");

const chord = rows[0].categories[0];
check(same(chord.rows.map((r) => r.label), ["V", "I", "IV"]), "rows are ordered by count then label");
check(chord.rows[0].text === "5/10", "fraction text is count/total");
check(chord.rows[0].percent === 50 && chord.rows[0].fraction === 0.5, "share is count over total");
check(chord.more === 2 && chord.moreText === "and 2 more", "more count reads 'and 2 more'");
check(rows[0].categories[1].moreText === "", "no more text when more is 0");
check(rows[0].categories[1].rows[0].percent === 100, "a full share is 100 percent");
check(rows[1].categories[0].rows[0].percent === 0 && rows[1].categories[0].rows[0].text === "0/0", "a zero total gives an empty bar");

check(same(V.styleSummaryRows(null), []), "no statistics gives no rows");
check(
  V.styleSummaryRows({ voice_leading: { motion_type: { labels: [], more: 3 } } })[0].categories[0].title === "Motion type",
  "underscores become spaces in titles and a category with only more is kept",
);

process.exit(failures === 0 ? 0 : 1);
