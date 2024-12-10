/* The Library's multi-file upload queue (issue 1). Pure functions, no DOM:
 * a queue is an array of entries {name, state}, where state is "queued",
 * "analyzing", "done" (with a label) or "failed" (with a message). Every
 * change returns a new queue, so the view can keep the old one for
 * comparison and Node tests can run these directly. */
(function (root) {
  const ACCEPTED_EXTENSIONS = [".mid", ".midi", ".musicxml", ".xml", ".mxl"];

  function createQueue(files) {
    return Array.from(files, (file) => ({ name: file.name, state: "queued" }));
  }

  function withEntry(queue, i, change) {
    return queue.map((entry, index) =>
      index === i ? { name: entry.name, ...change } : entry
    );
  }

  function markAnalyzing(queue, i) {
    return withEntry(queue, i, { state: "analyzing" });
  }

  function markDone(queue, i, label) {
    return withEntry(queue, i, { state: "done", label });
  }

  function markFailed(queue, i, message) {
    return withEntry(queue, i, { state: "failed", message });
  }

  function queueSummary(queue) {
    const done = queue.filter((entry) => entry.state === "done").length;
    const failed = queue.filter((entry) => entry.state === "failed").length;
    const summary = `${done} of ${queue.length} uploaded`;
    return failed > 0 ? `${summary}, ${failed} failed` : summary;
  }

  function acceptedFile(name) {
    const lower = String(name).toLowerCase();
    return ACCEPTED_EXTENSIONS.some((ext) => lower.endsWith(ext));
  }

  const api = {
    createQueue,
    markAnalyzing,
    markDone,
    markFailed,
    queueSummary,
    acceptedFile,
  };
  if (typeof module !== "undefined" && module.exports) {
    module.exports = api;
  } else {
    root.UploadQueue = api;
  }
})(typeof window !== "undefined" ? window : globalThis);
