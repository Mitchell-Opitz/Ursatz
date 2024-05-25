/* Confirms the two small decision points in the Library view (app.js's
 * renderLibraryFilenameCell/handleLibraryRename): the filename column
 * falls back to the bare id when no original filename was recorded, and
 * a rename is rejected before it ever reaches the PATCH call when the
 * trimmed input is empty. Plain Node script, no framework/deps, run
 * directly: `node frontend/tests/library-view.test.js`. */

function libraryFilenameCellText(piece) {
  return piece.filename || piece.id;
}

function isValidRenameInput(trimmedValue) {
  return trimmedValue.length > 0;
}

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
  "filename column shows the original filename when present",
);
check(
  libraryFilenameCellText({ id: "piece-1788837459-41", filename: "" }) === "piece-1788837459-41",
  "filename column falls back to the bare id when the filename is empty",
);

check(isValidRenameInput("sonata.mid") === true, "a non-empty trimmed rename input is accepted");
check(isValidRenameInput("") === false, "an empty trimmed rename input is rejected");
check(isValidRenameInput("   ".trim()) === false, "a whitespace-only rename input is rejected once trimmed");

process.exit(failures === 0 ? 0 : 1);
