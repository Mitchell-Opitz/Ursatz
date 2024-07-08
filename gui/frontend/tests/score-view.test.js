/* Confirms the Analysis view's Score tab (app.js's renderAnalysisTabs/
 * renderScoreTab): the tab is a sibling alongside the claim-list tabs
 * (not folded into their group/count loop), and the synthetic-meter
 * caption is shown only when the /musicxml/meta endpoint reports
 * meter_defaulted:true. Plain Node script, no framework/deps, mirrors
 * app.js's logic manually since these are DOM-free scripts with no
 * access to app.js's module scope (same convention as
 * analysis-view.test.js). Run directly:
 * `node frontend/tests/score-view.test.js`. */

let failures = 0;

function check(cond, what) {
  if (!cond) {
    console.error(`FAIL: ${what}`);
    failures++;
  } else {
    console.log(`PASS: ${what}`);
  }
}

/* Mirrors app.js's ANALYSIS_TABS plus the Score tab button appended
 * after it in renderAnalysisTabs. */
const ANALYSIS_TABS = [
  { id: "chords", label: "Chords" },
  { id: "keys", label: "Keys" },
  { id: "cadences", label: "Cadences" },
  { id: "motifs", label: "Motifs" },
  { id: "phrases", label: "Phrases" },
  { id: "form", label: "Form" },
  { id: "periods-sentences", label: "Periods/Sentences" },
];

function tabIds() {
  return [...ANALYSIS_TABS.map((tab) => tab.id), "score"];
}

check(tabIds().includes("score"), "the Score tab is present alongside the claim-list tabs");
check(
  tabIds().indexOf("score") === ANALYSIS_TABS.length,
  "the Score tab is appended after the claim-list tabs, not mixed into their ordering",
);
check(
  ANALYSIS_TABS.every((tab) => tab.id !== "score"),
  "the Score tab is not one of the claim-grouped tabs (it has no claim list/count of its own)",
);

/* Mirrors app.js's renderScoreTab caption logic: the caption is shown
 * only when the meta fetch succeeds and reports meter_defaulted:true,
 * never presented as if it were authored meter with no caveat. */
function resolveScoreCaption(meta) {
  if (meta && meta.ok && meta.meter_defaulted) {
    return "Barlines are approximate, not authored meter.";
  }
  return null;
}

check(
  resolveScoreCaption({ ok: true, meter_defaulted: true }) === "Barlines are approximate, not authored meter.",
  "the synthetic-meter caption is shown when /musicxml/meta reports meter_defaulted:true",
);
check(
  resolveScoreCaption({ ok: true, meter_defaulted: false }) === null,
  "no caption is shown when meter_defaulted is false",
);
check(resolveScoreCaption({ ok: false }) === null, "no caption is shown when the meta fetch reports ok:false");

process.exit(failures === 0 ? 0 : 1);
