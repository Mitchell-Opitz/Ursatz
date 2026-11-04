/* Covers the job client (public/backend-jobs.js) with a fake fetch: the request is sent, the job is polled until it
 * ends, the result is the status and body the route answered before jobs, progress is reported, a request answered at
 * once passes through, and an aborted signal cancels. Plain Node script: `node frontend/tests/backend-jobs.test.js`. */
const J = require("../public/backend-jobs.js");

let failures = 0;
function check(cond, what) {
  if (!cond) {
    console.error(`FAIL: ${what}`);
    failures++;
  } else {
    console.log(`PASS: ${what}`);
  }
}
const same = (a, b) => JSON.stringify(a) === JSON.stringify(b);

/* A fake fetch: script is a list of [status, body] answers, one for each call in order; calls records them. */
function fakeFetch(script) {
  const calls = [];
  const fetch = async (url, init) => {
    calls.push({ url, method: init && init.method, body: init && init.body });
    const next = script.shift() || [500, { error: "no answer scripted" }];
    return { status: next[0], json: async () => next[1] };
  };
  return { fetch, calls };
}
const request = { method: "POST", url: "/api/materials/items/thm-1/compose", headers: { "Content-Type": "application/json" }, body: "{}", job: true };
const instant = () => Promise.resolve();

(async () => {
  // The job runs: 202, a poll that is running, a poll that is done.
  let progress = [];
  let fake = fakeFetch([
    [202, { ok: true, job: { id: "job-1", kind: "theme", state: "queued" } }],
    [200, { ok: true, job: { id: "job-1", state: "running", stage: "melody", done: 6, total: 9 } }],
    [200, { ok: true, job: { id: "job-1", state: "done", stage: "export", done: 1, total: 1, http_status: 200, result: { ok: true, item: { id: "thm-1" } } } }],
  ]);
  let result = await J.runJob(request, { fetch: fake.fetch, wait: instant, onProgress: (p) => progress.push(p) });
  check(result.status === 200 && same(result.body, { ok: true, item: { id: "thm-1" } }), "the result is the status and body the route answered");
  check(fake.calls[0].method === "POST" && fake.calls[0].url === request.url && fake.calls[0].body === "{}", "the request is sent as it is");
  check(fake.calls[1].method === "GET" && fake.calls[1].url === "/api/jobs/job-1" && fake.calls.length === 3, "the job is polled until it ends");
  check(progress.length === 2 && J.progressText(progress[0]) === "melody 6/9", "progress is reported as the stage and its count");

  // A failed job passes its status and body on.
  fake = fakeFetch([
    [202, { ok: true, job: { id: "job-2", kind: "piece", state: "queued" } }],
    [200, { ok: true, job: { id: "job-2", state: "failed", http_status: 422, result: { ok: false, error: "stuck" } } }],
  ]);
  result = await J.runJob(request, { fetch: fake.fetch, wait: instant });
  check(result.status === 422 && result.body.error === "stuck", "a failed job gives its status and error");

  // A request answered at once (a refusal before the queue) is the answer.
  fake = fakeFetch([[400, { ok: false, error: "seed is a whole number" }]]);
  result = await J.runJob(request, { fetch: fake.fetch, wait: instant });
  check(result.status === 400 && result.body.error === "seed is a whole number" && fake.calls.length === 1, "a request answered at once passes through");

  // A cancelled job.
  fake = fakeFetch([
    [202, { ok: true, job: { id: "job-3", kind: "theme", state: "queued" } }],
    [200, { ok: true, job: { id: "job-3", state: "cancelled" } }],
  ]);
  result = await J.runJob(request, { fetch: fake.fetch, wait: instant });
  check(result.status === 499 && result.cancelled === true, "a cancelled job gives 499 and cancelled");

  // An aborted signal sends DELETE once.
  fake = fakeFetch([
    [202, { ok: true, job: { id: "job-4", kind: "theme", state: "queued" } }],
    [204, null],
    [200, { ok: true, job: { id: "job-4", state: "running", stage: "grow", done: 1, total: 3 } }],
    [200, { ok: true, job: { id: "job-4", state: "cancelled" } }],
  ]);
  const controller = new AbortController();
  let polls = 0;
  const abortAfterFirstPoll = () => {
    if (++polls === 1) {
      controller.abort();
    }
    return Promise.resolve();
  };
  result = await J.runJob(request, { fetch: fake.fetch, wait: abortAfterFirstPoll, signal: controller.signal });
  const deletes = fake.calls.filter((call) => call.method === "DELETE");
  check(deletes.length === 1 && deletes[0].url === "/api/jobs/job-4", "an aborted signal asks the backend to cancel, once");
  check(result.cancelled === true, "the aborted job ends cancelled");

  // A job that is gone and a backend that cannot be reached.
  fake = fakeFetch([
    [202, { ok: true, job: { id: "job-5", kind: "theme", state: "queued" } }],
    [404, { ok: false, error: "job not found" }],
  ]);
  result = await J.runJob(request, { fetch: fake.fetch, wait: instant });
  check(result.status === 404, "a job that is gone is the 404");
  result = await J.runJob(request, {
    fetch: async () => {
      throw new Error("down");
    },
    wait: instant,
  });
  check(result.status === 0 && /Could not reach backend/.test(result.body.error), "an unreachable backend is status 0");

  check(J.progressText({ state: "queued" }) === "waiting" && J.progressText({ state: "running", stage: "seed", done: 2, total: 0 }) === "seed" && J.progressText({ state: "running" }) === "working", "progress text");
  check(J.POLL_MS === 500, "the poll interval is 500 ms");

  if (failures > 0) {
    console.error(`${failures} check(s) failed`);
    process.exit(1);
  }
  console.log("all checks passed");
})();
