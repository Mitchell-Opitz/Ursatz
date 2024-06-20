/* Confirms the Score tab's claim-selection highlight matching (app.js's
 * buildScoreNoteIndex/findNotesForHighlightId/applyPendingHighlight):
 * base highlighted_note_ids match their own exact xml:id plus any
 * "<base>_<letter>" tie-split fragment, ids missing from the score are
 * silently skipped (not an error), and a claim with no
 * highlighted_note_ids (period/sentence/form/thematic_return/key/chord)
 * clears any previous highlight rather than leaving it stuck. Plain Node
 * script, no framework/deps, mirrors app.js's logic manually since these
 * are DOM-free scripts with no access to app.js's module scope (same
 * convention as analysis-view.test.js/score-view.test.js). Run directly:
 * `node frontend/tests/claim-highlight.test.js`. */

let failures = 0;

function check(cond, what) {
  if (!cond) {
    console.error(`FAIL: ${what}`);
    failures++;
  } else {
    console.log(`PASS: ${what}`);
  }
}

/* Mirrors app.js's buildScoreNoteIndex, given already-parsed parallel
 * id/note arrays (the DOMParser/OSMD traversal side of that function has
 * no meaningful DOM-free equivalent to test here). */
function buildIndex(ids, graphicalNotes) {
  const index = new Map();
  const count = Math.min(ids.length, graphicalNotes.length);
  for (let i = 0; i < count; i++) {
    const id = ids[i];
    if (!id) {
      continue;
    }
    if (!index.has(id)) {
      index.set(id, []);
    }
    index.get(id).push(graphicalNotes[i]);
  }
  return index;
}

/* Mirrors app.js's findNotesForHighlightId exactly. */
function findNotesForHighlightId(noteIndex, baseId) {
  const matches = [];
  if (noteIndex.has(baseId)) {
    matches.push(...noteIndex.get(baseId));
  }
  const fragmentPrefix = `${baseId}_`;
  for (const [id, notes] of noteIndex) {
    if (id !== baseId && id.startsWith(fragmentPrefix) && /^[a-z]$/.test(id.slice(fragmentPrefix.length))) {
      matches.push(...notes);
    }
  }
  return matches;
}

const noteA = { name: "noteA" };
const noteB1 = { name: "noteB_fragment1" };
const noteB2 = { name: "noteB_fragment2" };
const noteC = { name: "noteC" };

const index = buildIndex(
  ["note-a", "note-b_a", "note-b_b", "note-c"],
  [noteA, noteB1, noteB2, noteC],
);

check(
  findNotesForHighlightId(index, "note-a").length === 1 && findNotesForHighlightId(index, "note-a")[0] === noteA,
  "a base id with no tie-split fragments matches its own single note",
);

const bMatches = findNotesForHighlightId(index, "note-b");
check(
  bMatches.length === 2 && bMatches.includes(noteB1) && bMatches.includes(noteB2),
  "a base id matches every '<base>_<letter>' tie-split fragment",
);

check(
  findNotesForHighlightId(index, "note-does-not-exist").length === 0,
  "a highlighted_note_ids entry missing from the score's note index matches nothing (not an error)",
);

check(
  findNotesForHighlightId(index, "note-c-suffix").length === 0,
  "a base id is not treated as a prefix of an unrelated longer id",
);

/* Mirrors app.js's applyPendingHighlight's no-highlighted-ids branch:
 * period/sentence/form/thematic_return/key/chord claims (empty/absent
 * highlighted_note_ids) resolve to zero notes to highlight -- the
 * caller (clearScoreHighlight) is what actually clears the previous
 * selection, exercised here as pure data flow. */
function resolveHighlightNotes(noteIndex, claim) {
  if (!claim || !claim.highlighted_note_ids || claim.highlighted_note_ids.length === 0) {
    return [];
  }
  const notes = [];
  for (const baseId of claim.highlighted_note_ids) {
    notes.push(...findNotesForHighlightId(noteIndex, baseId));
  }
  return notes;
}

check(
  resolveHighlightNotes(index, { type: "period", highlighted_note_ids: [] }).length === 0,
  "a claim with an empty highlighted_note_ids (e.g. period) resolves to no notes",
);
check(
  resolveHighlightNotes(index, { type: "chord" }).length === 0,
  "a claim with no highlighted_note_ids field at all resolves to no notes",
);
check(
  resolveHighlightNotes(index, { type: "cadence", highlighted_note_ids: ["note-a", "note-b"] }).length === 3,
  "a cadence claim's highlighted_note_ids resolves to every matched note across all its target ids",
);

process.exit(failures === 0 ? 0 : 1);
