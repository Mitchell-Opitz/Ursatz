/* Confirms the pure helpers of export-control.js: the download file name and the measure range query. Plain Node
 * script: `node frontend/tests/export-control.test.js`. */

const ExportControl = require("../public/export-control.js");

let failures = 0;

function check(cond, what) {
  if (!cond) {
    console.error(`FAIL: ${what}`);
    failures++;
  } else {
    console.log(`PASS: ${what}`);
  }
}

const name = ExportControl.exportFilename;
check(name("Mozart 545") === "mozart-545.txt", "a name becomes a lowercase slug with .txt");
check(name("Sonata No. 1 (Op. 2)") === "sonata-no-1-op-2.txt", "runs of other characters become one dash");
check(name("--Bach__114--") === "bach-114.txt", "leading and trailing dashes are dropped");
check(name("") === "export.txt", "an empty name gives export.txt");
check(name("???") === "export.txt", "a name with no letters or digits gives export.txt");
check(name(null) === "export.txt" && name(undefined) === "export.txt", "a missing name gives export.txt");

const range = ExportControl.rangeQuery;
check(range("", "") === "", "both blank gives an empty query");
check(range(" ", null) === "", "blank and missing give an empty query");
check(range("3", "7") === "?from=3&to=7", "both ends give from and to");
check(range("3", "") === "?from=3", "from alone");
check(range("", "7") === "?to=7", "to alone");
check(range("5", "5") === "?from=5&to=5", "equal ends are a range of one measure");
check(range(" 4 ", "9") === "?from=4&to=9", "spaces around a number are ignored");
check(range("007", "9") === "?from=7&to=9", "leading zeros are dropped");
check(range("8", "3") === null, "from greater than to is null");
check(range("a", "3") === null, "a non-integer from is null");
check(range("3", "1.5") === null, "a non-integer to is null");
check(range("-1", "3") === null, "a negative number is null");

process.exit(failures === 0 ? 0 : 1);
