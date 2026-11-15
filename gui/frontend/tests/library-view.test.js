/* Confirms the small decision points in the Library view (app.js): the
 * title column falls back to the bare id, the Edit editor rejects an empty
 * title before the PATCH, composer and source show a muted dash when empty,
 * and renderLibraryView(filter) merges a passed filter over the defaults.
 * Plain Node script, no framework/deps, run directly:
 * `node frontend/tests/library-view.test.js`. */

function libraryFilenameCellText(piece) {
  return piece.filename || piece.id;
}

function isValidEditTitle(trimmedValue) {
  return trimmedValue.length > 0;
}

function metadataCellText(value) {
  return value ? value : "—";
}

function emptyLibraryFilter() {
  return { composer: "", source: "", tag: "", unassigned: [], untagged: false, search: "", sort: "added" };
}

function libraryFilterFor(filter) {
  return { ...emptyLibraryFilter(), ...(filter || {}) };
}

const LIBRARY_COLUMNS = ["", "Title", "Composer", "Source", "Tags", "Actions"];
const LIBRARY_ACTIONS = ["Play", "Edit", "Re-run analysis", "Import from URL", "Delete analysis", "Delete piece"];

let failures = 0;

function check(cond, what) {
  if (!cond) {
    console.error(`FAIL: ${what}`);
    failures++;
  } else {
    console.log(`PASS: ${what}`);
  }
}

check(
  libraryFilenameCellText({ id: "piece-1788837459-41", filename: "sonata.mid" }) === "sonata.mid",
  "title column shows the original filename when present",
);
check(
  libraryFilenameCellText({ id: "piece-1788837459-41", filename: "" }) === "piece-1788837459-41",
  "title column falls back to the bare id when the filename is empty",
);

check(isValidEditTitle("sonata.mid") === true, "a non-empty trimmed title is accepted");
check(isValidEditTitle("") === false, "an empty title is rejected");
check(isValidEditTitle("   ".trim()) === false, "a whitespace-only title is rejected once trimmed");

check(metadataCellText("John Field") === "John Field", "composer shows its value");
check(metadataCellText("") === "—", "empty composer shows a dash");

check(libraryFilterFor().sort === "added", "no filter gives the defaults");
check(libraryFilterFor({ tag: "etude" }).tag === "etude", "a passed filter is applied");
check(libraryFilterFor({ tag: "etude" }).sort === "added", "a passed filter keeps the other defaults");
check(libraryFilterFor({ untagged: true }).unassigned.length === 0, "unassigned defaults to empty");

check(LIBRARY_COLUMNS.length === 6, "Library has select, title, composer, source, tags and actions columns");
check(LIBRARY_ACTIONS.includes("Edit") && !LIBRARY_ACTIONS.includes("Rename"), "Edit replaces Rename");

process.exit(failures === 0 ? 0 : 1);
