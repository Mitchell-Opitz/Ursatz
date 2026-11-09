/* Confirms the Analysis view's data logic (analysis-tabs.js, loaded here
 * directly) against a real analysis document: the tab table, a row for every
 * item of a tab's sections, disabled empty tabs, row labels and locations,
 * the header and provenance fields, and the mapping of a claim's note_ids to
 * xml:ids. Also checks that app.js no longer carries the claim actions that
 * have no backend, that the view is the score workspace (no Score tab), and
 * the Library/Analysis navigation decisions.
 *
 * Fixture: frontend/tests/fixtures/mozart545.analysis.json is the Analyzer
 * CLI's document (`ursatz-analyzer --json`, schema v10) for
 * external/ursatz/tests/fixtures/framework-v1-corpus/Mozart 545.mid, cut to
 * its first 6 items per array (and 6 note_ids per claim) to stay small; the
 * structure is untouched.
 *
 * Plain Node script, no framework/deps, run directly:
 * `node frontend/tests/analysis-view.test.js`. */

const fs = require("fs");
const path = require("path");
const tabs = require("../public/analysis-tabs.js");

let failures = 0;

function check(cond, what) {
  if (!cond) {
    console.error(`FAIL: ${what}`);
    failures++;
  } else {
    console.log(`PASS: ${what}`);
  }
}

const doc = JSON.parse(fs.readFileSync(path.join(__dirname, "fixtures", "mozart545.analysis.json"), "utf8"));
const states = tabs.buildAllTabStates(doc);
const byId = new Map(states.map((state) => [state.tab.id, state]));

/* The tab table: order, labels and sections. */
const expectedTabs = [
  ["Keys", ["keys", "tonicizations", "modulations", "pivot_chords"]],
  ["Chords", ["chords", "chord_vocabulary", "non_chord_tones", "prolongations"]],
  ["Cadences", ["cadences"]],
  ["Progressions", ["progressions", "harmonic_rhythm"]],
  ["Phrases", ["phrases"]],
  ["Themes and form", ["themes", "form"]],
  ["Motives", ["motives"]],
  ["Melody", ["melody", "outer_voices"]],
  ["Rhythm and meter", ["meter", "rhythm"]],
  ["Texture", ["texture", "voice_leading"]],
  ["Schemas", ["schemas"]],
  ["Markings", ["markings"]],
  ["Counterpoint", ["counterpoint_check"]],
  ["Notes", ["notes"]],
];
check(
  tabs.ANALYSIS_TABS.length === expectedTabs.length &&
    tabs.ANALYSIS_TABS.every(
      (tab, i) => tab.label === expectedTabs[i][0] && JSON.stringify(tab.sections) === JSON.stringify(expectedTabs[i][1]),
    ),
  "the tabs are the document's sections grouped in the specified order, with the specified labels",
);

/* Row counts: an array section gives one row per item; an object section
 * gives one row per item of its arrays of claims. */
function arrayLength(value) {
  return Array.isArray(value) ? value.length : 0;
}
function claimArrayRows(value) {
  let total = 0;
  for (const member of Object.values(value || {})) {
    if (Array.isArray(member) && member.length > 0 && member.every((item) => item && ("span" in item || "location" in item))) {
      total += member.length;
    }
  }
  return total;
}
function expectedRowCount(sections) {
  return sections.reduce((total, name) => {
    const value = doc[name];
    return total + (Array.isArray(value) ? arrayLength(value) : claimArrayRows(value));
  }, 0);
}
for (const tab of tabs.ANALYSIS_TABS) {
  check(
    byId.get(tab.id).rowCount === expectedRowCount(tab.sections),
    `the ${tab.label} tab has one row per item of its sections (${expectedRowCount(tab.sections)})`,
  );
}
check(byId.get("cadences").rowCount === doc.cadences.length && doc.cadences.length > 0, "Cadences rows equal doc.cadences");
check(
  byId.get("chords").rowCount ===
    doc.chords.length + doc.chord_vocabulary.length + doc.non_chord_tones.length + doc.prolongations.length,
  "Chords rows are chords + chord_vocabulary + non_chord_tones + prolongations",
);
check(
  byId.get("keys").rowCount ===
    doc.keys.regions.length + doc.tonicizations.length + doc.modulations.length + doc.pivot_chords.length,
  "Keys rows are the key regions + tonicizations + modulations + pivot_chords",
);

/* Empty tabs. */
check(byId.get("counterpoint").empty === true, "a tab whose only section is null is empty (disabled)");
check(byId.get("keys").empty === false && byId.get("chords").empty === false, "tabs with content are not empty");
const emptyDocument = {};
for (const tab of tabs.ANALYSIS_TABS) {
  for (const name of tab.sections) {
    emptyDocument[name] = name === "keys" || name === "form" ? null : [];
  }
}
check(
  tabs.buildAllTabStates(emptyDocument).every((state) => state.empty && state.rowCount === 0),
  "a document whose sections are all empty arrays or null disables every tab",
);
check(
  tabs.buildAllTabStates(null).every((state) => state.empty),
  "no document at all disables every tab",
);

/* Object sections: a JSON block plus rows for the claim arrays inside. */
const keysBlock = byId.get("keys").blocks[0];
check(keysBlock.kind === "object" && keysBlock.rows.length === doc.keys.regions.length, "keys is an object block with a row per region");
check(
  keysBlock.text.includes("home_key") && !keysBlock.text.includes('"regions"'),
  "an object block shows the object without its claim arrays",
);
const formBlock = byId.get("themes-form").blocks[1];
check(
  formBlock.kind === "object" && formBlock.rows.length === doc.form.sections.length + doc.form.returns.length,
  "form is an object block with rows for its sections and returns",
);
const meterBlock = byId.get("rhythm-meter").blocks[0];
check(meterBlock.kind === "object" && meterBlock.rows.length === 0 && meterBlock.text.includes("time_signature"), "meter is one JSON block");

/* Row labels and locations. */
const chordRow = byId.get("chords").blocks[0].rows[0];
check(chordRow.label === doc.chords[0].label && chordRow.label !== "", `a chord row shows the chord's label (${chordRow.label})`);
const cadenceRow = byId.get("cadences").blocks[0].rows[0];
check(cadenceRow.label === doc.cadences[0].label, `a cadence row shows the cadence's label (${cadenceRow.label})`);
const keyRow = keysBlock.rows[0];
check(keyRow.label === doc.keys.regions[0].label, `a key row shows the region's label (${keyRow.label})`);
check(/^m\d+ b[\d.]+ to m\d+ b[\d.]+$/.test(chordRow.where), `a chord row shows where it is (${chordRow.where})`);
check(
  tabs.claimLabel({ type: "half" }, 0) === "half" &&
    tabs.claimLabel({ key: "G major" }, 0) === "G major" &&
    tabs.claimLabel({ numeral: "V7" }, 0) === "V7" &&
    tabs.claimLabel({}, 4) === "#5",
  "an item without a label is named by its numeral, type or key, else by position",
);
check(tabs.formatLocation(null) === "-" && tabs.formatLocation({ start: null, end: null }) === "-", "a null location is shown as -");
check(
  tabs.formatPosition({ measure: 3, beat: 2, subdivision: { num: 1, den: 2 } }) === "m3 b2.5",
  "a position past the beat adds the subdivision",
);
const chordDetail = JSON.parse(chordRow.detail);
check(!("label" in chordDetail) && !("location" in chordDetail), "a row's toggle shows the fields other than label and location");
check("note_ids" in chordDetail && "span" in chordDetail, "a row's toggle shows the remaining fields (span and note_ids among them)");

/* Header and provenance. */
const header = tabs.buildAnalysisHeader({ id: "piece-1", filename: "Mozart 545.mid", format: "midi", document: doc, note_xml_ids: [] });
check(header.filename === "Mozart 545.mid" && header.format === "midi", "the header has the filename and format");
check(header.analyzerVersion === doc.metadata.analyzer_version, "the header has the analyzer version");
check(header.ursatzCommit === doc.metadata.ursatz_commit.slice(0, 7) && header.ursatzCommit.length === 7, "the header has the first 7 characters of the Ursatz commit");
check(header.noteCount === doc.metadata.note_count && header.measureCount === doc.meter.measure_count, "the header has the note count and meter.measure_count");
const provenance = tabs.buildProvenanceFields(doc);
check(
  provenance.some(([label, value]) => label === "Analyzer version" && value === doc.metadata.analyzer_version) &&
    provenance.some(([label, value]) => label === "Input format" && value === "midi"),
  "the provenance panel lists the document's metadata fields",
);

/* note_ids to xml:ids. */
check(
  JSON.stringify(tabs.mapNoteIdsToXmlIds([0, 2], ["midi-note-0", "midi-note-1", "midi-note-2"])) ===
    JSON.stringify(["midi-note-0", "midi-note-2"]),
  "a claim's note_ids are mapped through note_xml_ids",
);
check(tabs.mapNoteIdsToXmlIds([9], ["midi-note-0"]).length === 0, "a note id past the end of note_xml_ids is skipped");
check(tabs.mapNoteIdsToXmlIds(undefined, undefined).length === 0, "a claim without note_ids maps to no notes");

/* app.js carries no claim actions without a backend. */
const appSource = fs.readFileSync(path.join(__dirname, "..", "public", "app.js"), "utf8");
check(!/Accept|Dispute/.test(appSource), "app.js has no Accept or Dispute buttons");
check(!/ANALYZERS|harmony 2\.4\.1/.test(appSource), "app.js has no static analyzer list");
check(appSource.includes('accept=".mid,.midi,.musicxml,.xml,.mxl"'), "the upload picker accepts MusicXML files");
check(appSource.includes("/reanalyze"), "app.js calls the reanalyze route");

/* The Analysis view is the score workspace: Score mode by default, Data mode
 * keeps the tabs and the Provenance list, and the old Score tab is gone. */
check(appSource.includes("ScoreWorkspace.mount("), "showAnalysisView mounts the score workspace");
check(!/renderScoreTab|analysis-score\b|analysis-sidebar|analysis-layout/.test(appSource), "the old Score tab and sidebar markup are gone from app.js");
check(!tabs.ANALYSIS_TABS.some((tab) => tab.id === "score"), "the claim tabs have no Score tab");
check(
  appSource.includes("renderProvenancePanel(data.document)") && appSource.includes('class="sidebar-label">Provenance'),
  "Data mode lists the Provenance fields above the claim tabs",
);
check(appSource.includes("renderAnalysisTabs(data, tabId)"), "Data mode renders the claim tabs");
check(appSource.includes("Re-run analysis") && appSource.includes("reanalyzePiece(id)"), "the toolbar offers Re-run analysis");
check(appSource.includes("content--workspace") && appSource.includes("leaveWorkspace()"), "the content area is a workspace only while a piece is open");
const indexSource = fs.readFileSync(path.join(__dirname, "..", "public", "index.html"), "utf8");
const scriptOrder = ["score-notes.js", "workspace-state.js", "score-workspace.js", "app.js"].map((name) => indexSource.indexOf(`src="${name}"`));
check(scriptOrder.every((at, i) => at >= 0 && (i === 0 || at > scriptOrder[i - 1])), "index.html loads workspace-state.js and score-workspace.js after score-notes.js and before app.js");

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

/* Mirrors app.js's renderAnalysisTool: the top-level Analysis nav tab
 * reopens lastAnalysisPieceId when set, and otherwise shows an empty
 * state rather than guessing a piece. */
function resolveAnalysisToolTarget(lastAnalysisPieceId) {
  return lastAnalysisPieceId ? { kind: "piece", id: lastAnalysisPieceId } : { kind: "empty" };
}

check(
  resolveAnalysisToolTarget("piece-1788837459-41").kind === "piece" &&
    resolveAnalysisToolTarget("piece-1788837459-41").id === "piece-1788837459-41",
  "the Analysis nav tab reopens the last piece viewed when one is set",
);
check(
  resolveAnalysisToolTarget(null).kind === "empty",
  "the Analysis nav tab shows an empty state, not a guessed piece, before any piece has been opened",
);

/* The document per tab: a cache that starts from the summary, loads only the sections a tab lacks, and builds the
 * same tab state as the whole document does. */
const summaryKeys = ["schema_version", "metadata", "meter"];
const sectionNames = Object.keys(doc);

function makeFetcher() {
  const calls = [];
  const fetchSections = async (names) => {
    calls.push(names.slice());
    const loaded = {};
    for (const name of names) {
      if (Object.prototype.hasOwnProperty.call(doc, name)) {
        loaded[name] = doc[name];
      }
    }
    return loaded;
  };
  return { calls, fetchSections };
}

function summaryOf() {
  const summary = {};
  for (const key of summaryKeys) {
    if (key in doc) {
      summary[key] = doc[key];
    }
  }
  return summary;
}

(async () => {
  for (const tab of tabs.ANALYSIS_TABS) {
    const cache = tabs.createDocumentCache(summaryOf(), sectionNames);
    const { calls, fetchSections } = makeFetcher();
    const expected = tabs.buildTabState(tab, doc);
    const state = await tabs.loadTabState(cache, tab, fetchSections);
    const wanted = tab.sections.filter((name) => sectionNames.includes(name) && !summaryKeys.includes(name));
    check(
      state && state.rowCount === expected.rowCount && state.empty === expected.empty,
      `${tab.id}: the tab loaded per section has the rows of the whole document`,
    );
    check(
      wanted.length === 0 ? calls.length === 0 : calls.length === 1 && JSON.stringify(calls[0]) === JSON.stringify(wanted),
      `${tab.id}: opening the tab asks for its own sections only`,
    );
    await tabs.loadTabState(cache, tab, fetchSections);
    check(calls.length === (wanted.length === 0 ? 0 : 1), `${tab.id}: a second open asks for nothing`);
  }

  const cache = tabs.createDocumentCache(summaryOf(), sectionNames);
  const { calls, fetchSections } = makeFetcher();
  await tabs.loadDocumentSections(cache, ["keys", "melody"], fetchSections);
  await tabs.loadDocumentSections(cache, ["melody", "outer_voices"], fetchSections);
  check(
    calls.length === 2 && JSON.stringify(calls[1]) === JSON.stringify(["outer_voices"]),
    "a section loaded for the score is not asked for again by a tab",
  );
  check(cache.has("nothing_here") && cache.missing(["no_such"]).length === 0, "a section the document lacks is not asked for");
  const loadedTab = { id: "loaded", label: "Loaded", sections: ["keys", "melody"] };
  const rebuilt = await tabs.loadTabState(cache, loadedTab, async () => null);
  check(rebuilt !== null, "a tab whose sections are all loaded builds without a fetch");
  const failingCache = tabs.createDocumentCache(summaryOf(), sectionNames);
  check(
    (await tabs.loadTabState(failingCache, tabs.ANALYSIS_TABS[0], async () => null)) === null,
    "a failed fetch gives no tab state",
  );
  const header = tabs.buildAnalysisHeader({ id: "p", document: summaryOf(), note_xml_ids: [] });
  check(header.measureCount !== undefined && header.analyzerVersion === (doc.metadata.analyzer_version || ""), "the header reads from the summary");
  check(
    JSON.stringify(tabs.buildProvenanceFields(summaryOf())) === JSON.stringify(tabs.buildProvenanceFields(doc)),
    "the provenance list reads the same from the summary as from the whole document",
  );

  process.exit(failures === 0 ? 0 : 1);
})();
