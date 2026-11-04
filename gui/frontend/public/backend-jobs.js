/* Backend jobs (P27 B7): the long Composer runs (theme and piece composes, regenerate, variations, choose, variants,
 * each-op, reroll) are jobs on the backend. runJob sends the request, polls GET /api/jobs/<id> every 500 ms and
 * resolves with {status, body}: the status and the body the route answered before jobs, so callers keep their reply
 * handling. No DOM. */
(function (root) {
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

  /* request: {method, url, headers, body}. options: {onProgress(progress), signal, fetch, wait, pollMs}. A request the
   * backend answers at once (not 202) resolves as it is. An aborted signal asks the backend to cancel (DELETE); the
   * result then is {status: 499, body: {ok: false, error: "Cancelled"}, cancelled: true}. */
  async function runJob(request, options) {
    const o = options || {};
    const fetchFn = o.fetch || ((url, init) => root.fetch(url, init));
    const wait = o.wait || ((ms) => new Promise((resolve) => setTimeout(resolve, ms)));
    const pollMs = o.pollMs === undefined ? POLL_MS : o.pollMs;
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
    for (;;) {
      await wait(pollMs);
      const polled = await send(fetchFn, { method: "GET", url: jobUrl(job.id) });
      const current = polled.body && polled.body.job;
      if (!current) {
        return { status: polled.status || 0, body: polled.body || { error: "The job is gone." } };
      }
      if (o.onProgress) {
        o.onProgress({ state: current.state, stage: current.stage, done: current.done, total: current.total });
      }
      if (current.state === "queued" || current.state === "running") {
        continue;
      }
      if (current.state === "cancelled") {
        return { status: CANCELLED_STATUS, body: { ok: false, error: "Cancelled" }, cancelled: true };
      }
      return { status: current.http_status, body: current.result };
    }
  }

  const api = { runJob, progressText, POLL_MS, CANCELLED_STATUS };
  if (typeof module !== "undefined" && module.exports) {
    module.exports = api;
  } else {
    root.BackendJobs = api;
  }
})(typeof window !== "undefined" ? window : globalThis);
