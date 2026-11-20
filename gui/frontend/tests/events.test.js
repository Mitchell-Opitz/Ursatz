/* Covers the event stream in the page (public/events.js) and the job client that follows it (public/backend-jobs.js) with a fake EventSource and a fake
 * fetch: events reach the followers of their name, the stream opens once, runJob resolves on job-done and reads the job again when the stream opens or
 * reconnects (no poll), progress events are reported, and a page without EventSource polls as before. Plain Node script, run through ctest. */
import * as E from "../public/core/events.js";
import J from "../public/core/backend-jobs.js";
import { check, same, finish } from "./support/check.js";

class FakeSource {
  constructor(url) {
    this.url = url;
    this.listeners = {};
    this.closed = false;
    FakeSource.last = this;
    FakeSource.count++;
  }
  addEventListener(name, fn) {
    (this.listeners[name] = this.listeners[name] || []).push(fn);
  }
  close() {
    this.closed = true;
  }
  open() {
    this.onopen();
  }
  send(name, data) {
    for (const fn of this.listeners[name] || []) {
      fn({ data: JSON.stringify(data) });
    }
  }
}
FakeSource.count = 0;

(async () => {
  const events = E.create({ EventSource: FakeSource });
  check(events.available() && FakeSource.count === 0, "nothing is opened before a follower asks");
  const seen = [];
  const stop = events.on("item-changed", (data) => seen.push(data));
  check(FakeSource.count === 1 && FakeSource.last.url === "/api/events", "the first follower opens /api/events");
  events.on("library-changed", () => seen.push("library"));
  check(FakeSource.count === 1, "the stream is opened once");
  FakeSource.last.send("item-changed", { item_id: "thm-1", version: 2 });
  FakeSource.last.send("library-changed", { piece_id: "p" });
  check(same(seen, [{ item_id: "thm-1", version: 2 }, "library"]), "an event reaches the followers of its name");
  stop();
  FakeSource.last.send("item-changed", { item_id: "thm-2", version: 1 });
  check(seen.length === 2, "a follower that stopped hears nothing");
  let opened = 0;
  events.onOpen(() => opened++);
  FakeSource.last.open();
  FakeSource.last.open();
  check(opened === 2, "onOpen runs at each opening of the stream (a reconnect too)");

  // runJob follows job-done; the stream's opening makes a read too.
  const jobEvents = E.create({ EventSource: FakeSource });
  const calls = [];
  let polls = 0;
  const fetchFn = async (url, init) => {
    calls.push(`${init.method} ${url}`);
    if (init.method === "POST") {
      return { status: 202, json: async () => ({ ok: true, job: { id: "job-1", kind: "theme", state: "queued" } }) };
    }
    polls++;
    const done = polls >= 3;
    return {
      status: 200,
      json: async () => ({ ok: true, job: done ? { id: "job-1", state: "done", http_status: 200, result: { ok: true, item: { id: "thm-1" } } } : { id: "job-1", state: "running", stage: "melody", done: 1, total: 3 } }),
    };
  };
  const progress = [];
  const running = J.runJob({ method: "POST", url: "/api/x", body: "{}" }, { fetch: fetchFn, events: jobEvents, wait: () => new Promise(() => {}), onProgress: (p) => progress.push(p.stage) });
  await new Promise((r) => setTimeout(r, 5));
  check(polls === 1, "the job is read once when it starts (nothing is polled)");
  FakeSource.last.send("job-progress", { id: "job-1", kind: "theme", stage: "grow", done: 2, total: 9 });
  FakeSource.last.send("job-progress", { id: "other", stage: "no", done: 0, total: 1 });
  check(same(progress.slice(-1), ["grow"]) && !progress.includes("no"), "progress events of the job are reported, others are not");
  FakeSource.last.open(); // the stream opened or reconnected: the job is read again
  await new Promise((r) => setTimeout(r, 5));
  check(polls === 2, "the job is read again when the stream opens");
  FakeSource.last.send("job-done", { id: "job-1", kind: "theme", state: "done", http_status: 200 });
  const result = await running;
  check(result.status === 200 && result.body.item.id === "thm-1", "runJob resolves with the job's status and answer on job-done");
  check(calls.filter((c) => c.startsWith("GET")).length === 3, "the answer is the third read");
  check(FakeSource.last.listeners["job-done"].length >= 1, "the follower of job events is set up");

  // Without an EventSource the job is polled.
  const none = E.create({ EventSource: undefined });
  let plain = 0;
  const poll = async (url, init) =>
    init.method === "POST"
      ? { status: 202, json: async () => ({ ok: true, job: { id: "job-2", state: "queued" } }) }
      : { status: 200, json: async () => ({ ok: true, job: ++plain < 2 ? { id: "job-2", state: "running" } : { id: "job-2", state: "done", http_status: 200, result: { ok: true } } }) };
  const polled = await J.runJob({ method: "POST", url: "/api/y" }, { fetch: poll, events: none, wait: () => Promise.resolve() });
  check(polled.status === 200 && plain === 2, "a page without EventSource polls the job");
  finish();
})();
