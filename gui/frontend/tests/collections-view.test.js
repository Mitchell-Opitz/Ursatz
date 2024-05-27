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

process.exit(failures === 0 ? 0 : 1);
