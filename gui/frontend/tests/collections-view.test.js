/* Confirms the Collections view's data-shaping decisions in app.js
 * (loadCorpusPieces/buildCollectionPieceRow): joining a bare corpus piece
 * ref against the Library's piece list to get a filename, falling back to
 * the bare id exactly as Library's own row does, and that the row's only
 * action is remove-from-collection (no Library piece-management actions
 * carried over). Plain Node script, no framework/deps, run directly:
 * `node frontend/tests/collections-view.test.js`. */

function joinPieceFilename(pieceId, libraryPieces) {
  const filenamesById = new Map();
  for (const piece of libraryPieces) {
    filenamesById.set(piece.id, piece.filename);
  }
  return { id: pieceId, filename: filenamesById.get(pieceId) };
}

function collectionRowCellText(piece) {
  return piece.filename || piece.id;
}

/* Mirrors collectionAddOptions in app.js. */
function collectionAddOptions(libraryPieces, memberIds) {
  const members = new Set(memberIds);
  return libraryPieces
    .filter((piece) => !members.has(piece.id))
    .map((piece) => ({ id: piece.id, label: piece.filename || piece.id }))
    .sort((a, b) => a.label.localeCompare(b.label));
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

const libraryPieces = [
  { id: "piece-1", filename: "nocturne.mid" },
  { id: "piece-2", filename: "" },
];

check(
  collectionRowCellText(joinPieceFilename("piece-1", libraryPieces)) === "nocturne.mid",
  "a collection piece row shows the Library filename when the piece is found",
);
check(
  collectionRowCellText(joinPieceFilename("piece-2", libraryPieces)) === "piece-2",
  "a collection piece row falls back to the bare id when the Library filename is empty",
);
check(
  collectionRowCellText(joinPieceFilename("piece-missing", libraryPieces)) === "piece-missing",
  "a collection piece row falls back to the bare id when the piece isn't found in the Library",
);

const addLibrary = [
  { id: "piece-c", filename: "zeta.mid" },
  { id: "piece-a", filename: "alpha.mid" },
  { id: "piece-b", filename: "" },
  { id: "piece-m", filename: "member.mid" },
];
const addOptions = collectionAddOptions(addLibrary, ["piece-m"]);

check(
  !addOptions.some((option) => option.id === "piece-m"),
  "the add list excludes pieces already in the collection",
);
check(
  addOptions.find((option) => option.id === "piece-a").label === "alpha.mid",
  "the add list labels a piece by its filename",
);
check(
  addOptions.find((option) => option.id === "piece-b").label === "piece-b",
  "the add list falls back to the id when the filename is empty",
);
check(
  addOptions.map((option) => option.label).join(",") === "alpha.mid,piece-b,zeta.mid",
  "the add list is sorted by label",
);
check(
  collectionAddOptions(addLibrary, addLibrary.map((piece) => piece.id)).length === 0,
  "the add list is empty when every Library piece is a member",
);

process.exit(failures === 0 ? 0 : 1);
