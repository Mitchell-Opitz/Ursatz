/* The workshop's events in the page (R-648): one EventSource on /api/events, with on(name, fn) for the events the views follow. The stream
 * reconnects by itself; onOpen(fn) runs each time it opens (the first time too), so a follower can read what it may have missed. A "reset" event
 * means this page's queue overflowed: onReset(fn) runs and the view reloads. No DOM. */
(function (root) {
  const NAMES = [
    "item-changed",
    "item-removed",
    "library-changed",
    "collection-changed",
    "subject-changed",
    "style-changed",
    "links-changed",
    "job-progress",
    "job-done",
    "analysis-stale",
    "reset",
  ];

  /* options: {EventSource (the class; default the page's), url (default /api/events)}. Nothing is opened until the first on(), onOpen() or open(). */
  function create(options) {
    const o = options || {};
    const Source = o.EventSource || root.EventSource;
    const url = o.url || "/api/events";
    const handlers = new Map(); // name -> Set of fn(data, name)
    const openHandlers = new Set();
    let source = null;
    let opened = false;

    function available() {
      return typeof Source === "function";
    }

    function emit(name, data) {
      for (const fn of Array.from(handlers.get(name) || [])) {
        try {
          fn(data, name);
        } catch (error) {
          // a follower's failure is its own
        }
      }
    }

    function open() {
      if (source || !available()) {
        return;
      }
      source = new Source(url);
      source.onopen = () => {
        opened = true;
        for (const fn of Array.from(openHandlers)) {
          try {
            fn();
          } catch (error) {
            // a follower's failure is its own
          }
        }
      };
      for (const name of NAMES) {
        source.addEventListener(name, (message) => {
          let data = {};
          try {
            data = message && message.data ? JSON.parse(message.data) : {};
          } catch (error) {
            data = {};
          }
          emit(name, data);
        });
      }
    }

    /* Calls fn(data, name) for each event of that name; returns the function that stops it. */
    function on(name, fn) {
      if (!handlers.has(name)) {
        handlers.set(name, new Set());
      }
      handlers.get(name).add(fn);
      open();
      return () => handlers.get(name).delete(fn);
    }

    function onOpen(fn) {
      openHandlers.add(fn);
      open();
      if (opened) {
        fn();
      }
      return () => openHandlers.delete(fn);
    }

    function close() {
      if (source) {
        source.close();
        source = null;
        opened = false;
      }
    }

    return { on, onOpen, open, close, available, NAMES };
  }

  const api = { create, NAMES };
  if (typeof module !== "undefined" && module.exports) {
    module.exports = api;
  } else {
    root.WorkshopEvents = api;
    root.Events = create();
  }
})(typeof window !== "undefined" ? window : globalThis);
