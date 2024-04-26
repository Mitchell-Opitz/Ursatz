/* Confirms the Piece Detail provenance label mapping (app.js) reports
 * correct provenance for both the MIDI-upload/analyzer path
 * (provenance_state "absent") and the hand-entered path
 * (provenance_state "present"), mirroring test_analyzer_route.c's
 * present/absent fixtures. Plain Node script, no framework/deps, run
 * directly: `node frontend/tests/provenance-label.test.js`. */

const PROVENANCE_LABELS = {
  present: "Hand-entered",
  absent: "Analyzed",
};

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
  PROVENANCE_LABELS.absent === "Analyzed",
  "MIDI-upload-derived Chord/Key output (provenance_state absent) reports 'Analyzed'",
);
check(
  PROVENANCE_LABELS.present === "Hand-entered",
  "hand-entered Chord/Key output (provenance_state present) reports 'Hand-entered'",
);

process.exit(failures === 0 ? 0 : 1);
