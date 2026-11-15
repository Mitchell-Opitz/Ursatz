/* The instrument layer: a registry of sound sources, a map from a MIDI
 * program to a source, and per-part overrides. A source is
 * {id, label, credit, load(ctx) -> Promise,
 *  start(ctx, destination, {pitch, velocity, when, duration}) -> {stop(when)}}.
 * Adding an instrument later is one register() and one mapProgram(), with no
 * change to the playback engine or the transport.
 *
 * Pure, no Web Audio access (sources do that), so Node tests load this file
 * directly. */
(function (root) {
  function createInstrumentRegistry(options) {
    const fallbackId = options && options.fallback;
    const byId = new Map();
    const programs = new Map();
    const parts = new Map();

    function register(source) {
      if (!source || typeof source.id !== "string" || source.id === "") {
        throw new Error("A sound source needs an id");
      }
      if (typeof source.load !== "function" || typeof source.start !== "function") {
        throw new Error(`Sound source ${source.id} needs load and start`);
      }
      byId.set(source.id, source);
    }

    function known(sourceId) {
      if (!byId.has(sourceId)) {
        throw new Error(`Unknown sound source ${sourceId}`);
      }
    }

    function mapProgram(program, sourceId) {
      known(sourceId);
      programs.set(program, sourceId);
    }

    function setPartSource(part, sourceId) {
      if (sourceId === null || sourceId === undefined) {
        parts.delete(part);
        return;
      }
      known(sourceId);
      parts.set(part, sourceId);
    }

    function sourceForProgram(program) {
      const id = programs.has(program) ? programs.get(program) : fallbackId;
      return byId.get(id) || null;
    }

    function sourceForPart(part, program) {
      if (parts.has(part)) {
        return byId.get(parts.get(part)) || sourceForProgram(program);
      }
      return sourceForProgram(program);
    }

    function sources() {
      return Array.from(byId.values());
    }

    return { register, mapProgram, sourceForProgram, setPartSource, sourceForPart, sources };
  }

  const api = { createInstrumentRegistry };
  if (typeof module !== "undefined" && module.exports) {
    module.exports = api;
  } else {
    root.Instruments = api;
  }
})(typeof window !== "undefined" ? window : globalThis);
