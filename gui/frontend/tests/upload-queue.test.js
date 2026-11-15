/* Covers upload-queue.js, the Library's multi-file upload queue: state
 * changes return new queues, the summary with and without failures, and
 * which file names are accepted. Plain Node script, no framework/deps, run
 * directly: `node frontend/tests/upload-queue.test.js`. */

const {
  createQueue,
  markAnalyzing,
  markDone,
  markFailed,
  queueSummary,
  acceptedFile,
} = require("../public/upload-queue.js");

let failures = 0;

function check(cond, what) {
  if (!cond) {
    console.error(`FAIL: ${what}`);
    failures++;
  }
}

const queue = createQueue([{ name: "a.mid" }, { name: "b.xml" }, { name: "c.mxl" }]);
check(queue.length === 3, "one entry per file");
check(queue.every((e) => e.state === "queued"), "new entries are queued");
check(queue[1].name === "b.xml", "entries keep the file name");

const analyzing = markAnalyzing(queue, 0);
check(analyzing[0].state === "analyzing", "markAnalyzing sets the state");
check(queue[0].state === "queued", "markAnalyzing leaves the old queue alone");
check(analyzing[1].state === "queued", "markAnalyzing leaves other entries alone");

const done = markDone(analyzing, 0, "a.mid (p1)");
check(done[0].state === "done" && done[0].label === "a.mid (p1)", "markDone keeps the label");

const failed = markFailed(done, 1, "bad file");
check(failed[1].state === "failed" && failed[1].message === "bad file", "markFailed keeps the message");

check(queueSummary(queue) === "0 of 3 uploaded", "summary of a fresh queue");
check(queueSummary(done) === "1 of 3 uploaded", "summary without failures");
check(queueSummary(failed) === "1 of 3 uploaded, 1 failed", "summary with a failure");
check(queueSummary(createQueue([])) === "0 of 0 uploaded", "summary of an empty queue");

for (const name of ["a.mid", "a.midi", "a.musicxml", "a.xml", "a.mxl", "A.MID", "x.MusicXML"]) {
  check(acceptedFile(name), `${name} is accepted`);
}
for (const name of ["a.txt", "a.mid.bak", "midi", "a.pdf", ""]) {
  check(!acceptedFile(name), `"${name}" is rejected`);
}

if (failures > 0) {
  console.error(`${failures} check(s) failed`);
  process.exit(1);
}
console.log("upload-queue: all checks passed");
