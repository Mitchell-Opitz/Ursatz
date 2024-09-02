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

process.exit(failures === 0 ? 0 : 1);
