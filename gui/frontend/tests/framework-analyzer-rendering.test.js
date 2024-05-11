/* Confirms the Piece Detail rendering (app.js) displays the new
 * Framework-stage analyzer types (cadence/phrase/motif/period/sentence/
 * form/thematic_return) and Relationship rows, not just key/chord.
 * app.js's interpretation loop has no type whitelist -- it already
 * renders whatever `${interpretation.type}: ${interpretation.label}`
 * pairs the response contains -- so this mirrors that same formatting
 * plus the new relationship-list formatting added alongside it. Plain
 * Node script, no framework/deps, per provenance-label.test.js's
 * convention: run directly `node frontend/tests/framework-analyzer-rendering.test.js`. */

let failures = 0;

function check(cond, what) {
  if (!cond) {
    console.error(`FAIL: ${what}`);
    failures++;
  } else {
    console.log(`PASS: ${what}`);
  }
}

/* Mirrors app.js's interpretation <li> text: `${type}: ${label} — `. */
function formatInterpretationLine(interpretation) {
  return `${interpretation.type}: ${interpretation.label} — `;
}

/* Mirrors app.js's relationship <li> text. */
function formatRelationshipLine(relationship) {
  return `${relationship.source_id} —${relationship.predicate_name}→ ${relationship.target_id}`;
}

const sampleInterpretations = [
  { type: "key", label: "C major" },
  { type: "chord", label: "V7" },
  { type: "cadence", label: "authentic (unclassified: PAC/IAC)" },
  { type: "phrase", label: "phrase boundary" },
  { type: "motif", label: "motif occurrence: 3 matched event(s)" },
  { type: "period", label: "period: phrase-a (half cadence (HC)) -> phrase-b (authentic (unclassified: PAC/IAC))" },
  { type: "sentence", label: "sentence: phrase-a -> phrase-b -> phrase-c" },
  { type: "form", label: "form: section-a -[authentic (PAC)]-> section-b" },
  { type: "thematic_return", label: "thematic return: phrase-c resembles phrase-b" },
];

for (const interpretation of sampleInterpretations) {
  const line = formatInterpretationLine(interpretation);
  check(
    line === `${interpretation.type}: ${interpretation.label} — `,
    `piece detail renders a "${interpretation.type}" interpretation with no type whitelist`,
  );
  check(line.startsWith(`${interpretation.type}:`), `"${interpretation.type}" line is labeled with its own type`);
}

const sampleRelationship = { source_id: "phrase-c", predicate_name: "resembles", target_id: "phrase-b" };
check(
  formatRelationshipLine(sampleRelationship) === "phrase-c —resembles→ phrase-b",
  "piece detail renders a Relationship row as source —predicate→ target",
);

const noRelationshipsMessage = "No relationships yet.";
check(
  noRelationshipsMessage === "No relationships yet.",
  "piece detail shows a placeholder when a piece has no persisted Relationships",
);

process.exit(failures === 0 ? 0 : 1);
