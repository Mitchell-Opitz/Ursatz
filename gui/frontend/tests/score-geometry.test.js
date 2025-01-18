/* Confirms the score geometry (score-geometry.js) on a hand-built two-system
 * layout: positions inside and across measures, span segments, the
 * pixel-to-time inverse, the timeline offset and the lane stack. Plain Node
 * script, no dependencies. Run directly:
 * `node frontend/tests/score-geometry.test.js`. */

const ScoreGeometry = require("../public/score-geometry.js");

let failures = 0;

function check(cond, what) {
  if (!cond) {
    console.error(`FAIL: ${what}`);
    failures++;
  } else {
    console.log(`PASS: ${what}`);
  }
}

function near(a, b) {
  return Math.abs(a - b) < 1e-6;
}

const layout = {
  unit: 10,
  systems: [
    {
      index: 0, top: 0, bottom: 40, left: 10, right: 210,
      measures: [
        { number: 1, start: 0, end: 1, x: 10, width: 100, entries: [{ t: 0, x: 20 }, { t: 0.5, x: 70 }] },
        { number: 2, start: 1, end: 2, x: 110, width: 100, entries: [] },
      ],
    },
    {
      index: 1, top: 100, bottom: 140, left: 10, right: 110,
      measures: [{ number: 3, start: 2, end: 3, x: 10, width: 100, entries: [] }],
    },
  ],
};

// xAt
check(near(ScoreGeometry.xAt(layout, 0).x, 20), "t at a measure start sits on its first entry");
check(near(ScoreGeometry.xAt(layout, 0.25).x, 45), "t between entries is interpolated");
check(near(ScoreGeometry.xAt(layout, 0.75).x, 90), "t between the last entry and the measure end is interpolated");
check(near(ScoreGeometry.xAt(layout, 1.5).x, 160), "a measure without entries interpolates over its width");
const boundary = ScoreGeometry.xAt(layout, 2);
check(boundary.system === 1 && near(boundary.x, 10), "a boundary time belongs to the measure starting there");
const end = ScoreGeometry.xAt(layout, 3);
check(end.system === 1 && near(end.x, 110), "t at the last measure's end maps to its right edge");
check(near(ScoreGeometry.xAt(layout, 9).x, 110), "t after the end maps to the last right edge");
check(near(ScoreGeometry.xAt(layout, -1).x, 10), "t before the start maps to the first measure's left");

// spanSegments
const within = ScoreGeometry.spanSegments(layout, 0.5, 1.5);
check(within.length === 1 && near(within[0].x1, 70) && near(within[0].x2, 160), "a span inside one system is one segment");
check(!within[0].openStart && !within[0].openEnd, "a span inside one system has no open ends");
const across = ScoreGeometry.spanSegments(layout, 0.5, 2.5);
check(across.length === 2, "a span across systems gives one segment per system");
check(across[0].openEnd && near(across[0].x2, 210) && !across[0].openStart, "the first segment runs to the system's right edge");
check(across[1].openStart && near(across[1].x1, 10) && near(across[1].x2, 60) && !across[1].openEnd, "the next segment starts at the system's left edge");
const toBoundary = ScoreGeometry.spanSegments(layout, 1, 2);
check(toBoundary.length === 1 && near(toBoundary[0].x2, 210), "a span ending on a system boundary stays on the earlier system");
const empty = ScoreGeometry.spanSegments(layout, 1.5, 1.5);
check(empty.length === 1 && near(empty[0].x1, empty[0].x2), "a zero-width span is one segment of width 0");

// tAt
for (const t of [0, 0.25, 0.5, 0.75, 1.25, 1.9]) {
  const place = ScoreGeometry.xAt(layout, t);
  check(near(ScoreGeometry.tAt(layout, place.system, place.x), t), `tAt inverts xAt at t=${t}`);
}
check(near(ScoreGeometry.tAt(layout, 1, 60), 2.5), "tAt works on the second system");
check(near(ScoreGeometry.tAt(layout, 0, -50), 0), "tAt left of the system gives its first time");
check(near(ScoreGeometry.tAt(layout, 0, 999), 2), "tAt right of the system gives its last time");

// calibrateOffset
const pairs = [];
for (let i = 0; i < 10; i++) pairs.push({ ursatz: i * 0.25, osmd: i * 0.25 + 0.25 });
pairs[4].osmd += 0.5;
const calibrated = ScoreGeometry.calibrateOffset(pairs);
check(near(calibrated.offset, 0.25), "offset is the median, ignoring the outlier");
check(calibrated.mismatches === 1, "one pair differing from the offset is counted");
check(ScoreGeometry.calibrateOffset([]).offset === 0, "no pairs give offset 0");
const many = [];
for (let i = 0; i < 60; i++) many.push({ ursatz: i, osmd: i + (i < 50 ? 1 : 7) });
const capped = ScoreGeometry.calibrateOffset(many);
check(near(capped.offset, 1) && capped.mismatches === 0, "only the first 50 pairs count");

// laneStack
const stack = ScoreGeometry.laneStack([
  { layerId: "keys", placement: "above", rows: 2 },
  { layerId: "form", placement: "above", rows: 1 },
  { layerId: "cadences", placement: "above", rows: 1 },
  { layerId: "other:pivot_chords", placement: "above", rows: 1 },
  { layerId: "chords", placement: "below", rows: 1 },
]);
check(
  stack.above.map((l) => l.layerId).join() === "cadences,other:pivot_chords,keys,form",
  "above lanes follow the fixed order, nearest first"
);
check(stack.above[0].offset === 8, "the first lane is 8 px from the staff");
check(stack.above[1].offset === 8 + 18 + 4, "lanes are 4 px apart");
check(stack.above[2].offset === 30 + 18 + 4, "a lane's offset follows the previous lane");
check(stack.aboveHeight === 110, "aboveHeight covers every lane");
check(stack.below.length === 1 && stack.below[0].offset === 8 && stack.belowHeight === 26, "chords sit below");
const none = ScoreGeometry.laneStack([]);
check(none.aboveHeight === 0 && none.belowHeight === 0, "no lanes, no height");

// systemSpacingUnits
const defaults = { minimumDistanceBetweenSystems: 4, pageTopMargin: 0 };
const base = ScoreGeometry.systemSpacingUnits(none, 1, defaults);
check(base.minimumDistanceBetweenSystems >= 4 && base.pageTopMargin >= 0, "spacing never drops below the defaults");
const big = ScoreGeometry.systemSpacingUnits(stack, 1, defaults);
check(near(big.minimumDistanceBetweenSystems, (110 + 26 + 16) / 10), "system distance grows with the lanes");
check(near(big.pageTopMargin, (110 + 8) / 10), "page top margin makes room for the top lane");
const zoomed = ScoreGeometry.systemSpacingUnits(stack, 2, defaults);
check(zoomed.minimumDistanceBetweenSystems < big.minimumDistanceBetweenSystems, "higher zoom needs fewer OSMD units for the same pixels");
const huge = ScoreGeometry.systemSpacingUnits(none, 1, { minimumDistanceBetweenSystems: 50, pageTopMargin: 30 });
check(huge.minimumDistanceBetweenSystems === 50 && huge.pageTopMargin === 30, "larger defaults are kept");

// chordLabelsToDraw
const spaced = ScoreGeometry.chordLabelsToDraw([0, 40, 80], [20, 20, 20], 4);
check(spaced.join() === "true,true,true", "labels with room between them are all kept");
const crowded = ScoreGeometry.chordLabelsToDraw([0, 10, 30, 31, 100], [20, 20, 20, 20, 20], 4);
check(crowded.join() === "true,false,true,false,true", "a label within the gap of the last drawn one is dropped");
const against = ScoreGeometry.chordLabelsToDraw([0, 22, 25], [20, 20, 20], 4);
check(against.join() === "true,false,true", "the gap is measured from the last drawn label, not a dropped one");
check(ScoreGeometry.chordLabelsToDraw([5, 6], [50, 50], 4)[0] === true, "the first label is always kept");
check(ScoreGeometry.chordLabelsToDraw([], [], 4).length === 0, "no labels, nothing to draw");

if (failures > 0) {
  console.error(`${failures} check(s) failed`);
  process.exit(1);
}
console.log("score-geometry: all checks passed");
