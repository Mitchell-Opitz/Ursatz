/* Confirms the Score tab's meter caption (score-notes.js meterCaption):
 * shown only when /musicxml/meta reports that the file declares no time
 * signature. Plain Node script, no dependencies. Run directly:
 * `node frontend/tests/score-view.test.js`. */

const ScoreNotes = require("../public/score-notes.js");

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
  ScoreNotes.meterCaption({ ok: true, meter_defaulted: true }) === "This file declares no time signature; shown in 4/4",
  "the caption is shown when meter_defaulted is true",
);
check(ScoreNotes.meterCaption({ ok: true, meter_defaulted: false }) === null, "no caption when the file declares its meter");
check(ScoreNotes.meterCaption({ ok: false, meter_defaulted: true }) === null, "no caption when the meta fetch reports ok:false");
check(ScoreNotes.meterCaption({ ok: true }) === null, "no caption when meter_defaulted is absent");
check(ScoreNotes.meterCaption(null) === null, "no caption without a meta response");

const XML = "<?xml version=\"1.0\"?><score-partwise/>";
check(ScoreNotes.scoreResponseProblem(200, "application/xml", XML) === null, "an XML score has no problem");
check(
  ScoreNotes.scoreResponseProblem(
    500,
    "application/json",
    "{\"ok\":false,\"message\":\"failed to export piece\",\"code\":\"MUSICXML_EXPORT_ERR_ALLOC\"}",
  ) === "failed to export piece (MUSICXML_EXPORT_ERR_ALLOC)",
  "a JSON error shows its message and code",
);
check(
  ScoreNotes.scoreResponseProblem(422, "application/json", "{\"ok\":false,\"message\":\"could not import the stored source\"}") ===
    "could not import the stored source",
  "a JSON error without a code shows its message only",
);
check(
  ScoreNotes.scoreResponseProblem(404, "text/plain", "Not Found") === "score request failed (404)",
  "a non-JSON error shows the status",
);
check(
  ScoreNotes.scoreResponseProblem(200, "application/json", "{\"ok\":false}") === "score request failed (200)",
  "a 200 that is not XML is a problem",
);

process.exit(failures === 0 ? 0 : 1);
