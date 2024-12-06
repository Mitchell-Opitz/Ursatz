/* Finds a claim's notes in the rendered score (R-085). OSMD does not keep
 * xml:id, so a note is identified by where and what it is:
 *
 *   <part index>|<measure number>|<staff>|<voice>|<onset>|<pitch>
 *
 * built once from the MusicXML text and once from OSMD's graphical notes;
 * equal keys are the same note. Onset is the position in the measure as a
 * reduced fraction of a whole note ("1/4"), pitch is step, alter and
 * octave ("E-1/5" is E flat in octave 5). Rests are left out.
 *
 * Checked against the vendored OSMD 3.10.1: its voice ids equal the
 * MusicXML <voice> numbers, part index is ParentInstrument.Id (0-based),
 * staff is Staff.Id (1-based), and Pitch.Octave is the MusicXML octave
 * minus 3. So the voice is part of the key and no voice-less fallback is
 * used. On an OSMD version where voice ids differ, drop the voice from
 * both key builders; chords of one voice share it anyway.
 *
 * Pure functions, no DOM access of their own: buildMusicXmlNoteKeys takes
 * the parsed document's root element (any object with tagName, children,
 * getAttribute and textContent), so Node tests can run it. */
(function (root) {
  const OSMD_STEPS = { 0: "C", 2: "D", 4: "E", 5: "F", 7: "G", 9: "A", 11: "B" };
  const OSMD_OCTAVE_OFFSET = 3;

  function gcd(a, b) {
    a = Math.abs(a);
    b = Math.abs(b);
    while (b) {
      [a, b] = [b, a % b];
    }
    return a || 1;
  }

  function fractionText(numerator, denominator) {
    const g = gcd(numerator, denominator);
    return `${numerator / g}/${denominator / g}`;
  }

  function noteKey(part, measure, staff, voice, onset, pitch) {
    return `${part}|${measure}|${staff}|${voice}|${onset}|${pitch}`;
  }

  function pitchText(step, alter, octave) {
    return `${step}${alter}/${octave}`;
  }

  function childNamed(el, name) {
    for (const child of el.children) {
      if (child.tagName === name) {
        return child;
      }
    }
    return null;
  }

  function childNumber(el, name, fallback) {
    const child = childNamed(el, name);
    if (!child) {
      return fallback;
    }
    const value = Number(child.textContent.trim());
    return Number.isFinite(value) ? value : fallback;
  }

  /* Every non-rest <note> in the document as {id, key}; id is null for a
   * note without xml:id. Onset comes from <divisions>, <duration>,
   * <backup>, <forward> and <chord/>. */
  function buildMusicXmlNoteKeys(rootEl) {
    const entries = [];
    const parts = Array.from(rootEl.children).filter((el) => el.tagName === "part");
    parts.forEach((partEl, partIndex) => {
      let divisions = 1;
      let measureIndex = 0;
      for (const measureEl of Array.from(partEl.children).filter((el) => el.tagName === "measure")) {
        measureIndex += 1;
        const measureNumber = parseInt(measureEl.getAttribute("number"), 10);
        const measure = Number.isFinite(measureNumber) ? measureNumber : measureIndex;
        let cursor = 0;
        let chordOnset = 0;
        for (const el of measureEl.children) {
          if (el.tagName === "attributes") {
            divisions = childNumber(el, "divisions", divisions);
          } else if (el.tagName === "backup") {
            cursor -= childNumber(el, "duration", 0);
          } else if (el.tagName === "forward") {
            cursor += childNumber(el, "duration", 0);
          } else if (el.tagName === "note") {
            const isChord = childNamed(el, "chord") !== null;
            const isGrace = childNamed(el, "grace") !== null;
            const duration = isGrace ? 0 : childNumber(el, "duration", 0);
            const onset = isChord ? chordOnset : cursor;
            if (!isChord) {
              chordOnset = cursor;
              cursor += duration;
            }
            const pitchEl = childNamed(el, "pitch");
            if (!pitchEl || childNamed(el, "rest")) {
              continue;
            }
            const step = childNamed(pitchEl, "step").textContent.trim();
            const alter = childNumber(pitchEl, "alter", 0);
            const octave = childNumber(pitchEl, "octave", 0);
            entries.push({
              id: el.getAttribute("xml:id") || null,
              key: noteKey(
                partIndex,
                measure,
                childNumber(el, "staff", 1),
                childNumber(el, "voice", 1),
                fractionText(onset, 4 * divisions),
                pitchText(step, alter, octave),
              ),
            });
          }
        }
      }
    });
    return entries;
  }

  /* The same key from one OSMD GraphicalNote; null for a rest or a note
   * OSMD gave no pitch. */
  function osmdNoteKey(graphicalNote) {
    const source = graphicalNote && graphicalNote.sourceNote;
    if (!source || !source.Pitch || (source.isRest && source.isRest())) {
      return null;
    }
    const staff = source.ParentStaffEntry.ParentStaff;
    const voiceEntry = source.ParentVoiceEntry;
    const time = voiceEntry.Timestamp;
    const onset = fractionText(time.Numerator + time.WholeValue * time.Denominator, time.Denominator);
    const step = OSMD_STEPS[source.Pitch.FundamentalNote];
    return noteKey(
      staff.ParentInstrument.Id,
      source.SourceMeasure.MeasureNumber,
      staff.Id,
      voiceEntry.ParentVoice.VoiceId,
      onset,
      pitchText(step, source.Pitch.AccidentalHalfTones, source.Pitch.Octave + OSMD_OCTAVE_OFFSET),
    );
  }

  /* All GraphicalNotes of a rendered OSMD instance. */
  function collectGraphicalNotes(osmd) {
    const notes = [];
    for (const measureRow of (osmd.GraphicSheet && osmd.GraphicSheet.MeasureList) || []) {
      for (const measure of measureRow) {
        if (!measure || !measure.staffEntries) {
          continue;
        }
        for (const staffEntry of measure.staffEntries) {
          for (const voiceEntry of staffEntry.graphicalVoiceEntries || []) {
            notes.push(...(voiceEntry.notes || []));
          }
        }
      }
    }
    return notes;
  }

  /* xml:id -> GraphicalNote[] by equal keys. Returns {index, unmatched}:
   * unmatched counts the score's notes whose key matched no xml:id. */
  function buildNoteIndex(xmlEntries, graphicalNotes) {
    const byKey = new Map();
    for (const entry of xmlEntries) {
      if (entry.id) {
        if (!byKey.has(entry.key)) {
          byKey.set(entry.key, []);
        }
        byKey.get(entry.key).push(entry.id);
      }
    }
    const index = new Map();
    let unmatched = 0;
    for (const note of graphicalNotes) {
      const key = osmdNoteKey(note);
      if (key === null) {
        continue;
      }
      const ids = byKey.get(key);
      if (!ids) {
        unmatched += 1;
        continue;
      }
      for (const id of ids) {
        if (!index.has(id)) {
          index.set(id, []);
        }
        index.get(id).push(note);
      }
    }
    return { index, unmatched };
  }

  /* A claim's note id matches its own xml:id and the barline fragments
   * Ursatz writes for a note split across bars: "<id>_<letter>" and
   * "<id>_<number>". */
  function findNotesForHighlightId(index, baseId) {
    const matches = [];
    if (index.has(baseId)) {
      matches.push(...index.get(baseId));
    }
    const prefix = `${baseId}_`;
    for (const [id, notes] of index) {
      if (id.startsWith(prefix) && /^([a-z]|[0-9]+)$/.test(id.slice(prefix.length))) {
        matches.push(...notes);
      }
    }
    return matches;
  }

  /* The caption under the score: only when the file declares no time
   * signature (meter_defaulted from /musicxml/meta), else null. */
  function meterCaption(meta) {
    if (meta && meta.ok && meta.meter_defaulted === true) {
      return "This file declares no time signature; shown in 4/4";
    }
    return null;
  }

  /* Why a score response cannot be handed to OSMD, or null when it is XML.
   * A JSON error body contributes its message and, when present, its code. */
  function scoreResponseProblem(status, contentType, bodyText) {
    const body = typeof bodyText === "string" ? bodyText.trim() : "";
    const isXml = typeof contentType === "string" && contentType.includes("xml") && body.startsWith("<");
    if (status >= 200 && status < 300 && isXml) {
      return null;
    }
    try {
      const data = JSON.parse(body);
      if (data && typeof data.message === "string" && data.message) {
        return data.code ? `${data.message} (${data.code})` : data.message;
      }
    } catch (err) {
      /* not JSON: fall through to the status message */
    }
    return `score request failed (${status})`;
  }

  const api = {
    scoreResponseProblem,
    meterCaption,
    buildMusicXmlNoteKeys,
    osmdNoteKey,
    collectGraphicalNotes,
    buildNoteIndex,
    findNotesForHighlightId,
  };
  if (typeof module !== "undefined" && module.exports) {
    module.exports = api;
  } else {
    root.ScoreNotes = api;
  }
})(typeof window !== "undefined" ? window : globalThis);
