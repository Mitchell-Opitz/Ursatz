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

  // Piecewise-linear points of one measure: (time, x), ascending in both.
  function measurePoints(measure) {
    const entries = measure.entries || [];
    const points = [{ t: measure.start, x: entries.length ? entries[0].x : measure.x }];
    for (const entry of entries) {
      if (entry.t > measure.start + T_EPSILON && entry.t < measure.end - T_EPSILON) {
        points.push({ t: entry.t, x: entry.x });
      }
    }
    points.push({ t: measure.end, x: measure.x + measure.width });
    return points;
  }

  function interpolateX(measure, t) {
    const points = measurePoints(measure);
    for (let i = 1; i < points.length; i++) {
      const a = points[i - 1];
      const b = points[i];
      if (t <= b.t) {
        if (b.t - a.t < T_EPSILON) return b.x;
        return a.x + ((t - a.t) / (b.t - a.t)) * (b.x - a.x);
      }
    }
    return points[points.length - 1].x;
  }

  function flatMeasures(layout) {
    const flat = [];
    layout.systems.forEach((system, si) => {
      system.measures.forEach((measure) => flat.push({ si, measure }));
    });
    return flat;
  }

  // The place of OSMD time t. With `endSide`, a time on a measure boundary
  // belongs to the measure that ends there (an exclusive span end).
  function locate(layout, t, endSide) {
    const flat = flatMeasures(layout);
    if (flat.length === 0) return { si: 0, x: 0 };
    const first = flat[0];
    if (t < first.measure.start || (endSide && t <= first.measure.start)) {
      return { si: first.si, x: layout.systems[first.si].measures[0].x };
    }
    for (const { si, measure } of flat) {
      const inside = endSide
        ? t > measure.start && t <= measure.end
        : t >= measure.start && t < measure.end;
      if (inside) return { si, x: interpolateX(measure, t) };
    }
    const last = flat[flat.length - 1];
    if (!endSide && t === last.measure.end) {
      return { si: last.si, x: last.measure.x + last.measure.width };
    }
    for (const { si, measure } of flat) {
      if (t < measure.start) return { si, x: measure.x };
    }
    return { si: last.si, x: layout.systems[last.si].right };
  }

  // {system, x} for an OSMD time t.
  function xAt(layout, t) {
    const place = locate(layout, t, false);
    return { system: layout.systems[place.si].index, x: place.x };
  }

  // One segment per system the span touches. A segment that continues on the
  // next system is open at that end.
  function spanSegments(layout, t1, t2) {
    const a = locate(layout, t1, false);
    const b = t2 > t1 ? locate(layout, t2, true) : a;
    const segments = [];
    for (let si = a.si; si <= b.si; si++) {
      const system = layout.systems[si];
      const openStart = si > a.si;
      const openEnd = si < b.si;
      segments.push({
        system: system.index,
        x1: openStart ? system.left : a.x,
        x2: openEnd ? system.right : b.x,
        openStart,
        openEnd,
      });
    }
    return segments;
  }

  // The inverse of xAt within one system: the OSMD time at pixel x.
  function tAt(layout, systemIndex, x) {
    const system = layout.systems.find((s) => s.index === systemIndex);
    if (!system || system.measures.length === 0) return 0;
    const measures = system.measures;
    let measure = measures[measures.length - 1];
    for (const m of measures) {
      if (x < m.x + m.width) {
        measure = m;
        break;
      }
    }
    const points = measurePoints(measure);
    if (x <= points[0].x) return points[0].t;
    for (let i = 1; i < points.length; i++) {
      const a = points[i - 1];
      const b = points[i];
      if (x <= b.x) {
        if (b.x - a.x < T_EPSILON) return b.t;
        return a.t + ((x - a.x) / (b.x - a.x)) * (b.t - a.t);
      }
    }
    return points[points.length - 1].t;
  }

  // OSMD time = Ursatz position + offset. The offset absorbs pickup measure
  // numbering (R-101); it is the median over the first 50 matched notes.
  function calibrateOffset(pairs) {
    const used = pairs.slice(0, 50);
    if (used.length === 0) return { offset: 0, mismatches: 0 };
    const deltas = used.map((p) => p.osmd - p.ursatz).sort((a, b) => a - b);
    const mid = deltas.length >> 1;
    const offset = deltas.length % 2 ? deltas[mid] : (deltas[mid - 1] + deltas[mid]) / 2;
    const mismatches = used.filter((p) => Math.abs(p.osmd - p.ursatz - offset) > 1e-6).length;
    return { offset, mismatches };
  }

  const ROW_HEIGHT = 18;
  const LANE_GAP = 4;
  const STAFF_PADDING = 8;
  const SYSTEM_MARGIN = 16;
  // Above-lanes from the staff upward, nearest first. Unlisted layers
  // (other:* and any layer the Analyzer adds later) share the other:* slot.
  const ABOVE_ORDER = [
    "cadences", "other", "progressions", "prolongations", "key-changes",
    "keys", "texture", "schemas", "phrases", "themes", "form",
  ];
  const BELOW_ORDER = ["chords"];

  function laneRank(layerId, order) {
    const i = order.indexOf(layerId);
    if (i >= 0) return i;
    const other = order.indexOf("other");
    return other >= 0 ? other : order.length;
  }

  function stackLanes(lanes, order) {
    const sorted = lanes
      .map((lane, i) => ({ lane, i, rank: laneRank(lane.layerId, order) }))
      .sort((a, b) => a.rank - b.rank || a.i - b.i);
    let offset = STAFF_PADDING;
    let height = 0;
    const stacked = sorted.map(({ lane }) => {
      const entry = { layerId: lane.layerId, offset, rows: lane.rows };
      height = offset + lane.rows * ROW_HEIGHT;
      offset = height + LANE_GAP;
      return entry;
    });
    return { stacked, height };
  }

  // Stacks the enabled, non-empty lanes ({layerId, placement, rows}) against
  // the staff. `offset` is the distance from the staff edge to the lane's
  // nearest row edge.
  function laneStack(lanes) {
    const above = stackLanes(lanes.filter((l) => l.placement === "above"), ABOVE_ORDER);
    const below = stackLanes(lanes.filter((l) => l.placement === "below"), BELOW_ORDER);
    return {
      above: above.stacked,
      below: below.stacked,
      aboveHeight: above.height,
      belowHeight: below.height,
    };
  }

  // OSMD spacing (in OSMD units) that leaves room for the lanes. Never below
  // `defaults`, the EngravingRules values read before any change.
  function systemSpacingUnits(stack, zoom, defaults) {
    const px = 10 * zoom;
    return {
      minimumDistanceBetweenSystems: Math.max(
        defaults.minimumDistanceBetweenSystems,
        (stack.aboveHeight + stack.belowHeight + SYSTEM_MARGIN) / px
      ),
      pageTopMargin: Math.max(defaults.pageTopMargin, (stack.aboveHeight + STAFF_PADDING) / px),
    };
  }

  const api = {
    extractLayout,
    osmdNoteTime,
    xAt,
    tAt,
    spanSegments,
    calibrateOffset,
    laneStack,
    systemSpacingUnits,
    ROW_HEIGHT,
  };
  if (typeof module !== "undefined" && module.exports) {
    module.exports = api;
  } else {
    root.ScoreGeometry = api;
  }
})(typeof window !== "undefined" ? window : globalThis);
