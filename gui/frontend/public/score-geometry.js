/* Where things sit on the rendered score (R-101). OSMD is read once into a
 * plain layout object; every other function works on that object, so Node
 * tests use hand-built layouts.
 *
 * Layout, in CSS pixels from the OSMD SVG's top-left:
 *   { unit, systems: [{ index, top, bottom, left, right,
 *       measures: [{ number, start, end, x, width, entries: [{ t, x }] }] }] }
 *
 * Times in a layout are OSMD absolute timestamps in whole notes. A piece
 * position from the Analyzer plus the calibrated offset gives the OSMD time. */
(function (root) {
  const STAFF_HEIGHT_UNITS = 4;
  const T_EPSILON = 1e-9;

  // OSMD stores a measure column as an array of staff measures (one per staff).
  function firstOfColumn(column) {
    return Array.isArray(column) ? column[0] : column;
  }

  function columnMeasures(column) {
    return Array.isArray(column) ? column : [column];
  }

  function staffEntriesOf(measure) {
    return (measure && (measure.staffEntries || measure.StaffEntries)) || [];
  }

  function mergeEntries(column, measureStart, unit) {
    const byTime = new Map();
    for (const measure of columnMeasures(column)) {
      for (const entry of staffEntriesOf(measure)) {
        if (!entry || !entry.relInMeasureTimestamp || !entry.PositionAndShape) continue;
        const t = measureStart + entry.relInMeasureTimestamp.RealValue;
        const x = entry.PositionAndShape.AbsolutePosition.x * unit;
        const key = Math.round(t / T_EPSILON);
        const seen = byTime.get(key);
        if (!seen || x < seen.x) byTime.set(key, { t, x });
      }
    }
    return Array.from(byTime.values()).sort((a, b) => a.t - b.t);
  }

  function extractLayout(osmd) {
    const unit = 10 * (osmd.zoom || 1);
    const systems = [];
    for (const page of osmd.GraphicSheet.MusicPages) {
      for (const system of page.MusicSystems) {
        const measures = [];
        for (const column of system.GraphicalMeasures) {
          const measure = firstOfColumn(column);
          if (!measure || !measure.parentSourceMeasure) continue;
          const start = measure.parentSourceMeasure.AbsoluteTimestamp.RealValue;
          const shape = measure.PositionAndShape;
          measures.push({
            number: measure.MeasureNumber,
            start,
            end: start + measure.parentSourceMeasure.Duration.RealValue,
            x: shape.AbsolutePosition.x * unit,
            width: shape.Size.width * unit,
            entries: mergeEntries(column, start, unit),
          });
        }
        if (measures.length === 0) continue;
        const staffLines = system.StaffLines;
        const first = staffLines[0].PositionAndShape.AbsolutePosition.y;
        const last = staffLines[staffLines.length - 1].PositionAndShape.AbsolutePosition.y;
        const lastMeasure = measures[measures.length - 1];
        systems.push({
          index: systems.length,
          top: first * unit,
          bottom: (last + STAFF_HEIGHT_UNITS) * unit,
          left: measures[0].x,
          right: lastMeasure.x + lastMeasure.width,
          measures,
        });
      }
    }
    return { unit, systems };
  }

  // The OSMD time of a rendered note (a GraphicalNote), in whole notes.
  function osmdNoteTime(graphicalNote) {
    return graphicalNote.sourceNote.getAbsoluteTimestamp().RealValue;
  }

  const api = {
    extractLayout,
    osmdNoteTime,
  };
  if (typeof module !== "undefined" && module.exports) {
    module.exports = api;
  } else {
    root.ScoreGeometry = api;
  }
})(typeof window !== "undefined" ? window : globalThis);
