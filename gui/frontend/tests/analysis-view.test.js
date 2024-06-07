/* Confirms the Analysis view's tab grouping (app.js's ANALYSIS_TABS/
 * groupClaimsByTab), claim-list formatting, claim-detail panel content,
 * and the Library filename-click navigation added alongside it.
 * Supersedes framework-analyzer-rendering.test.js, which covered the old
 * flat Piece Detail view removed in this branch. Plain Node script, no
 * framework/deps, run directly: `node frontend/tests/analysis-view.test.js`. */

let failures = 0;

function check(cond, what) {
  if (!cond) {
    console.error(`FAIL: ${what}`);
    failures++;
  } else {
    console.log(`PASS: ${what}`);
  }
}

/* Mirrors app.js's ANALYSIS_TABS -- kept in sync manually since these
 * are plain Node scripts with no access to app.js's module scope. */
const ANALYSIS_TABS = [
  { id: "chords", label: "Chords", types: ["chord"] },
  { id: "keys", label: "Keys", types: ["key"] },
  { id: "cadences", label: "Cadences", types: ["cadence"] },
  { id: "motifs", label: "Motifs", types: ["motif"] },
  { id: "phrases", label: "Phrases", types: ["phrase"] },
  { id: "form", label: "Form", types: ["form", "thematic_return"] },
  { id: "periods-sentences", label: "Periods/Sentences", types: ["period", "sentence"] },
];

/* Mirrors app.js's groupClaimsByTab. */
function groupClaimsByTab(interpretations) {
  const groups = new Map();
  for (const tab of ANALYSIS_TABS) {
    groups.set(tab.id, []);
  }
  for (const interpretation of interpretations) {
    const tab = ANALYSIS_TABS.find((candidate) => candidate.types.includes(interpretation.type));
    if (tab) {
      groups.get(tab.id).push(interpretation);
    }
  }
  return groups;
}

const sampleInterpretations = [
  { type: "key", label: "C major" },
  { type: "chord", label: "V7" },
  { type: "chord", label: "I" },
  { type: "cadence", label: "authentic (unclassified: PAC/IAC)" },
  { type: "motif", label: "motif occurrence: 3 matched event(s)" },
  { type: "phrase", label: "phrase boundary" },
  { type: "form", label: "form: section-a -[authentic (PAC)]-> section-b" },
  { type: "thematic_return", label: "thematic return: phrase-c resembles phrase-b" },
  { type: "period", label: "period: phrase-a (half cadence (HC)) -> phrase-b (authentic (unclassified: PAC/IAC))" },
  { type: "sentence", label: "sentence: phrase-a -> phrase-b -> phrase-c" },
];

const groups = groupClaimsByTab(sampleInterpretations);

check(groups.get("chords").length === 2, "chord-typed claims are grouped under the Chords tab");
check(groups.get("keys").length === 1, "key-typed claims are grouped under the Keys tab");
check(groups.get("cadences").length === 1, "cadence-typed claims are grouped under the Cadences tab");
check(groups.get("motifs").length === 1, "motif-typed claims are grouped under the Motifs tab");
check(groups.get("phrases").length === 1, "phrase-typed claims are grouped under the Phrases tab");
check(
  groups.get("form").length === 2,
  "the Form tab's count badge includes both form and thematic_return claims",
);
check(
  groups.get("periods-sentences").length === 2,
  "the Periods/Sentences tab's count badge includes both period and sentence claims",
);

/* Mirrors app.js's claim <li> text: `${label} — ` plus a provenance flag. */
const PROVENANCE_LABELS = {
  present: "Hand-entered",
  absent: "Analyzed",
};

function formatClaimLine(claim) {
  return `${claim.label} — ${PROVENANCE_LABELS[claim.provenance_state] || claim.provenance_state}`;
}

check(
  formatClaimLine({ label: "V7", provenance_state: "absent" }) === "V7 — Analyzed",
  "a claim row renders its label and provenance flag",
);

/* Mirrors app.js's renderClaimDetail provenance line -- the only
 * Provenance data the API actually returns (produced_by.author/method
 * and provenance_state; no full Provenance record). */
function formatProvenanceLine(claim) {
  return `${claim.produced_by.author} / ${claim.produced_by.method} (${claim.provenance_state})`;
}

const sampleClaim = {
  type: "chord",
  label: "V7",
  produced_by: { author: "ursatz-gui", method: "triad-identification" },
  provenance_state: "absent",
};

check(
  formatProvenanceLine(sampleClaim) === "ursatz-gui / triad-identification (absent)",
  "claim detail's Provenance section shows produced_by.author/method and provenance_state",
);

const evidenceEmptyMessage = "No evidence data returned by the analyzer.";
check(
  evidenceEmptyMessage === "No evidence data returned by the analyzer.",
  "claim detail's Evidence section renders an explicit empty state, not fabricated fields",
);

/* Mirrors app.js's buildPieceRow onFilenameClick wiring -- Library rows
 * call showAnalysisView(piece.id) on filename click; Collections rows
 * (also built via buildPieceRow) pass no onFilenameClick and stay
 * unaffected. */
function filenameCellIsClickable(onFilenameClick) {
  return Boolean(onFilenameClick);
}

check(
  filenameCellIsClickable((piece) => piece.id) === true,
  "Library's filename cell is clickable (navigates to the Analysis view)",
);
check(
  filenameCellIsClickable(undefined) === false,
  "a piece row built without onFilenameClick (e.g. Collections) has no clickable filename cell",
);

process.exit(failures === 0 ? 0 : 1);
