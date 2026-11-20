/* Backend jobs (P27 B7): the long Composer runs (theme and piece composes, regenerate, variations, choose, variants,
 * each-op, reroll) are jobs on the backend. runJob sends the request, follows the job through the event stream (events.js; GET /api/jobs/<id>
 * every 500 ms when there is none) and resolves with {status, body}: the status and the body the route answered before jobs, so callers keep their reply
 * handling. No DOM. */

import { events as Events } from "./events.js";

const POLL_MS = 500;
const CANCELLED_STATUS = 499;

async function send(fetchFn, request) {
  try {
    const res = await fetchFn(request.url, { method: request.method, headers: request.headers, body: request.body });
    let body = null;
    try {
      body = await res.json();
    } catch (err) {
      body = null;
    }
    return { status: res.status, body };
  } catch (err) {
    return { status: 0, body: { error: `Could not reach backend: ${err}` } };
  }
}

function jobUrl(id) {
  return `/api/jobs/${encodeURIComponent(id)}`;
}

/* "melody 6/9" while a stage counts, the stage alone before, "waiting" while queued. */
function progressText(progress) {
  const p = progress || {};
  if (p.state === "queued") {
    return "waiting";
  }
  const stage = String(p.stage || "");
  if (stage && Number(p.total) > 0) {
    return `${stage} ${Number(p.done) || 0}/${Number(p.total)}`;
  }
  return stage || "working";
}

/* request: {method, url, headers, body}. options: {onProgress(progress), signal, fetch, wait, pollMs, events}. A request the backend answers at once (not
 * 202) resolves as it is. An aborted signal asks the backend to cancel (DELETE); the result then is {status: 499, body: {ok: false, error: "Cancelled"},
 * cancelled: true}. The job is followed through the event stream (events, default the page's Events): job-progress reports its stage, job-done and each
 * (re)opening of the stream make a read of GET /api/jobs/<id>. Without an event stream it is polled every pollMs. */
async function runJob(request, options) {
  const o = options || {};
  const fetchFn = o.fetch || ((url, init) => globalThis.fetch(url, init));
  const wait = o.wait || ((ms) => new Promise((resolve) => setTimeout(resolve, ms)));
  const pollMs = o.pollMs === undefined ? POLL_MS : o.pollMs;
  const events = o.events === undefined ? Events : o.events;
  const started = await send(fetchFn, request);
  const job = started.body && started.body.job;
  if (started.status !== 202 || !job || !job.id) {
    return started;
  }
  let cancelSent = false;
  const cancel = () => {
    if (!cancelSent) {
      cancelSent = true;
      send(fetchFn, { method: "DELETE", url: jobUrl(job.id) });
    }
  };
  if (o.signal) {
    if (o.signal.aborted) {
      cancel();
    } else {
      o.signal.addEventListener("abort", cancel, { once: true });
    }
  }
  /* One read of the job: its result when it ended, null while it runs (progress is reported). */
  async function read() {
    const polled = await send(fetchFn, { method: "GET", url: jobUrl(job.id) });
    const current = polled.body && polled.body.job;
    if (!current) {
      return { status: polled.status || 0, body: polled.body || { error: "The job is gone." } };
    }
    if (o.onProgress) {
      o.onProgress({ state: current.state, stage: current.stage, done: current.done, total: current.total });
    }
    if (current.state === "queued" || current.state === "running") {
      return null;
    }
    if (current.state === "cancelled") {
      return { status: CANCELLED_STATUS, body: { ok: false, error: "Cancelled" }, cancelled: true };
    }
    return { status: current.http_status, body: current.result };
  }
  if (events && events.available && events.available()) {
    return new Promise((resolve) => {
      const stops = [];
      let finished = false;
      let reading = false;
      let again = false;
      const finish = (result) => {
        if (!finished) {
          finished = true;
          stops.forEach((stop) => stop());
          resolve(result);
        }
      };
      const check = async () => {
        if (reading) {
          again = true;
          return;
        }
        reading = true;
        do {
          again = false;
          const result = await read();
          if (result) {
            finish(result);
            return;
          }
        } while (again && !finished);
        reading = false;
      };
      stops.push(
        events.on("job-progress", (data) => {
          if (data && data.id === job.id && o.onProgress && !finished) {
            o.onProgress({ state: "running", stage: data.stage, done: data.done, total: data.total });
          }
        }),
        events.on("job-done", (data) => {
          if (data && data.id === job.id) {
            check();
          }
        }),
        events.onOpen(() => check()),
      );
      check();
    });
  }
  for (;;) {
    await wait(pollMs);
    const result = await read();
    if (result) {
      return result;
    }
  }
}

const api = { runJob, progressText, POLL_MS, CANCELLED_STATUS };
export default api;
