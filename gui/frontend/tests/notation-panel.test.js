/* Confirms the notation panel model (notation-panel.js): the meter check
 * summary with and without an alternative, an override shown as applied,
 * the PATCH body and the reading text.
 *
 * Plain Node script: `node frontend/tests/notation-panel.test.js`. */

const panel = require("../public/notation-panel.js");

let failures = 0;

function check(cond, what) {
  if (!cond) {
    console.error(`FAIL: ${what}`);
    failures++;
  } else {
    console.log(`PASS: ${what}`);
  }
}

function reading(num, den, scale, pickup, score) {
  return {
    meter: { numerator: num, denominator: den },
    scale: { num: scale, den: 1 },
    pickup: { num: pickup, den: pickup === 0 ? 1 : 4 },
    score,
  };
}

const withAlternative = {
  meter: { source: "file" },
  rhythm: [
    { label: "meter-fit", declared: reading(4, 4, 1, 0, 0.41), alternative: reading(3, 4, 2, 0, 0.93) },
    { label: "density" },
  ],
};
const summary = panel.meterFitSummary(withAlternative);
check(summary.declared === "4/4" && summary.declaredScore === 0.41, "declared meter and score");
check(summary.overridden === false, "a file meter is not an override");
check(
  summary.alternative.text === "3/4, values doubled" &&
    summary.alternative.meter === "3/4" &&
    summary.alternative.value_scale === "2" &&
    summary.alternative.pickup === "" &&
    summary.alternative.score === 0.93,
  "alternative reading",
);
check(summary.line === "Declared 4/4 fits 41%. 3/4 with values doubled fits 93%.", `check line: ${summary.line}`);

const noAlternative = {
  meter: { source: "file" },
  rhythm: [{ label: "meter-fit", declared: reading(3, 4, 1, 0, 0.87), alternative: null }],
};
const plain = panel.meterFitSummary(noAlternative);
check(plain.alternative === null, "no alternative");
check(plain.line === "Declared 3/4 fits 87%.", `line without alternative: ${plain.line}`);

const applied = panel.meterFitSummary({
  meter: { source: "override" },
  rhythm: [{ label: "meter-fit", declared: reading(3, 4, 2, 1, 0.93), alternative: null }],
});
check(applied.overridden === true, "override source is shown as overridden");
check(applied.reading === "3/4, values doubled, pickup of a quarter", `effective reading: ${applied.reading}`);
check(applied.line.startsWith("Applied 3/4 with values doubled, pickup of a quarter fits 93%"), `applied line: ${applied.line}`);

check(panel.meterFitSummary({ meter: {}, rhythm: [{ label: "density" }] }) === null, "no meter-fit claim gives null");
check(panel.meterFitSummary({}) === null, "document without rhythm gives null");

check(panel.readingText("3/4", "2", "") === "3/4, values doubled", "reading text, doubled");
check(panel.readingText("", "1/2", "") === "values halved", "reading text, halved");
check(panel.readingText("", "1", "1/4") === "pickup of a quarter", "reading text, pickup only");
check(panel.readingText("4/4", "4", "5/16") === "4/4, values x4, pickup of 5/16", "reading text, other scale and pickup");

check(
  JSON.stringify(panel.notationBody({ meter: " 3/4 ", value_scale: "2", pickup: "" })) ===
    JSON.stringify({ notation: { meter: "3/4", value_scale: "2" } }),
  "body leaves blank fields out",
);
check(JSON.stringify(panel.notationBody({ clear: true })) === '{"notation":null}', "clear body");
check(JSON.stringify(panel.notationBody({ meter: "", value_scale: "", pickup: "" })) === '{"notation":null}', "empty fields clear");

if (failures > 0) {
  console.error(`${failures} check(s) failed`);
  process.exit(1);
}
