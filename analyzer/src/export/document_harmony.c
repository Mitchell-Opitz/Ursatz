/* The harmony areas of the analysis document: cadences and prolongations, progressions, harmonic rhythm and the outer voices. */

#include "export/analysis_document_internal.h"

/* {"span": ...} and {"span": ..., "location": ...} for the parts of a cadence. */
static cJSON *span_only_json(Rational start, Rational end) {
    cJSON *object = cJSON_CreateObject();
    if (object && !common_json_add(object, "span", doc_span_json(start, end))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

static cJSON *span_and_location_json(DocBuild *build, const ImportedScore *notes, Rational start, Rational end) {
    cJSON *object = span_only_json(start, end);
    if (object && !common_json_add(object, "location", doc_location_json(build, notes->meter.meter_map, start, end))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

/* The competing reading of a cadence: {"reading", "type" (null for a prolongation), "measure", "beat" (where its
   arrival begins; null when the meter cannot place it), "share", "note_ids" (the notes of its arrival)}. */
static cJSON *cadence_alternative_json(DocBuild *build, const ImportedScore *notes, const ChainResult *report,
                                       const AnalysisCadenceAlternative *alternative) {
    size_t id_count = 0;
    size_t *ids = doc_entry_range_note_ids(report, alternative->arrival_first, alternative->arrival_last, &id_count);
    if (!ids) {
        return NULL;
    }
    cJSON *position = doc_position_json(build, notes->meter.meter_map, doc_entry_start_time(build, &report->entries[alternative->arrival_first]));
    cJSON *object = cJSON_CreateObject();
    cJSON *measure = position ? cJSON_GetObjectItemCaseSensitive(position, "measure") : NULL;
    cJSON *beat = position ? cJSON_GetObjectItemCaseSensitive(position, "beat") : NULL;
    bool ok = object &&
              common_json_add(object, "reading", cJSON_CreateString(alternative->reading)) &&
              common_json_add(object, "type", doc_optional_string_json(alternative->type)) &&
              common_json_add(object, "measure", measure ? cJSON_CreateNumber(measure->valuedouble) : cJSON_CreateNull()) &&
              common_json_add(object, "beat", beat ? cJSON_CreateNumber(beat->valuedouble) : cJSON_CreateNull()) &&
              common_json_add(object, "share", cJSON_CreateNumber((double)alternative->share)) &&
              common_json_add(object, "note_ids", doc_note_ids_json(ids, id_count));
    free(ids);
    cJSON_Delete(position);
    if (!ok) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

/* One claim per cadence, in time order. The claim's span runs from the dominant's start to the arrival's end;
   arrival has the arrival's span and location, dominant its span. soprano_degree is the outer voice's label at the
   arrival (null when absent); region the key region of the arrival (null outside every region); boundary marks a half
   cadence the boundary step added before a section start. share is the thousandths of this reading (1000 with no
   other); alternative, present only for a cadence with a competing reading, is the other one. Everything else is the
   library's: the type, the beat strength, the uncertainty. */
cJSON *doc_cadences_json(DocBuild *build, const ImportedScore *notes, const ChainResult *report) {
    cJSON *list = cJSON_CreateArray();
    if (!list) {
        return NULL;
    }
    const char *framework = framework_name(standard_harmony_framework());
    for (size_t c = 0; c < reading_cadence_count(build->reading); c++) {
        const AnalysisCadence *cadence = &report->cadences[reading_cadence_at(build->reading, c)->detail];
        const ChainEntry *dominant = &report->entries[cadence->dominant_first];
        const ChainEntry *arrival_first = &report->entries[cadence->arrival_first];
        const ChainEntry *arrival_last = &report->entries[cadence->arrival_last];
        Rational start = doc_entry_start_time(build, dominant);
        Rational end = doc_entry_end_time(build, arrival_last);
        Rational arrival_start = doc_entry_start_time(build, arrival_first);
        Rational dominant_end = doc_entry_end_time(build, &report->entries[cadence->dominant_last]);
        size_t id_count = 0;
        size_t *ids = doc_entry_range_note_ids(report, cadence->dominant_first, cadence->arrival_last, &id_count);
        cJSON *claim = ids ? doc_claim_json_with(build, framework, notes, ids, id_count, start, end, cadence->type,
                                             cJSON_CreateNumber(cadence->uncertainty))
                           : NULL;
        free(ids);
        if (!(claim && common_json_add(claim, "type", cJSON_CreateString(cadence->type)) &&
              common_json_add(claim, "arrival", span_and_location_json(build, notes, arrival_start, end)) &&
              common_json_add(claim, "dominant", span_only_json(start, dominant_end)) &&
              common_json_add(claim, "soprano_degree", doc_optional_string_json(cadence->soprano_degree)) &&
              common_json_add(claim, "arrival_beat_strength", cJSON_CreateString(cadence->arrival_beat_strength)) &&
              common_json_add(claim, "boundary", cJSON_CreateBool(cadence->boundary)) &&
              common_json_add(claim, "region",
                  cadence->in_region ? cJSON_CreateNumber((double)cadence->region) : cJSON_CreateNull()) &&
              common_json_add(claim, "share", cJSON_CreateNumber((double)cadence->share)) &&
              (!cadence->has_alternative ||
               common_json_add(claim, "alternative", cadence_alternative_json(build, notes, report, &cadence->alternative))))) {
            cJSON_Delete(claim);
            cJSON_Delete(list);
            return NULL;
        }
        if (!common_json_append(list, claim)) {
            cJSON_Delete(list);
            return NULL;
        }
    }
    return list;
}

/* One claim per prolongation, in order of the prolonged chord. The claim's span runs from the prolonged chord's start
   to the end of the closing chord's first segment; first_chord and last_chord are the indices of those chords in
   "chords" and numeral is the prolonged chord's Roman numeral (null when the library gave it none). reasons has one
   library reason name per interior chord. Everything else is the library's: the function, the reasons. */
cJSON *doc_prolongations_json(DocBuild *build, const ImportedScore *notes, const ChainResult *report) {
    cJSON *list = cJSON_CreateArray();
    if (!list) {
        return NULL;
    }
    const char *framework = framework_name(standard_harmony_framework());
    for (size_t i = 0; i < report->prolongation_count; i++) {
        const AnalysisProlongation *prolongation = &report->prolongations[i];
        const ChainEntry *first = &report->entries[prolongation->first_entry];
        const ChainEntry *last = &report->entries[prolongation->last_entry];
        Rational start = doc_entry_start_time(build, first);
        Rational end = doc_entry_end_time(build, last);
        size_t id_count = 0;
        size_t *ids = doc_entry_range_note_ids(report, prolongation->first_entry, prolongation->last_entry, &id_count);
        cJSON *claim = ids ? doc_claim_json_with(build, framework, notes, ids, id_count, start, end, prolongation->function,
                                             cJSON_CreateNumber(prolongation->uncertainty))
                           : NULL;
        free(ids);
        cJSON *reasons = cJSON_CreateArray();
        bool reasons_ok = reasons != NULL;
        for (size_t r = 0; reasons_ok && r < prolongation->interior_count; r++) {
            reasons_ok = common_json_append(reasons, cJSON_CreateString(prolongation->reasons[r]));
        }
        bool reasons_taken = false; /* common_json_add() takes the reasons over, and frees them itself when it fails */
        if (!(claim && reasons_ok && common_json_add(claim, "function", cJSON_CreateString(prolongation->function)) &&
              common_json_add(claim, "numeral", doc_optional_string_json(first->roman.numeral)) &&
              common_json_add(claim, "first_chord", cJSON_CreateNumber((double)prolongation->first_entry)) &&
              common_json_add(claim, "last_chord", cJSON_CreateNumber((double)prolongation->last_entry)) &&
              (reasons_taken = true, common_json_add(claim, "reasons", reasons)) &&
              common_json_add(claim, "region",
                  prolongation->in_region ? cJSON_CreateNumber((double)prolongation->region) : cJSON_CreateNull()))) {
            if (!reasons_taken) {
                cJSON_Delete(reasons);
            }
            cJSON_Delete(claim);
            cJSON_Delete(list);
            return NULL;
        }
        if (!common_json_append(list, claim)) {
            cJSON_Delete(list);
            return NULL;
        }
    }
    return list;
}

/* The pattern as a JSON array of its base numerals. */
static cJSON *progression_pattern_json(const AnalysisProgression *progression) {
    cJSON *pattern = cJSON_CreateArray();
    for (size_t n = 0; pattern && n < progression->length; n++) {
        if (!common_json_append(pattern, cJSON_CreateString(progression->pattern[n]))) {
            cJSON_Delete(pattern);
            return NULL;
        }
    }
    return pattern;
}

/* The indices into "chords" of every chord from first to last, held repeats included. */
static cJSON *chord_range_json(size_t first, size_t last) {
    cJSON *indices = cJSON_CreateArray();
    for (size_t i = first; indices && i <= last; i++) {
        if (!common_json_append(indices, cJSON_CreateNumber((double)i))) {
            cJSON_Delete(indices);
            return NULL;
        }
    }
    return indices;
}

/* One occurrence of a progression, a claim over the chords of the run: its span and location run from the first
   chord's start to the last chord's end, note_ids are the notes sounding in them, the label is the pattern's, and
   the uncertainty is the least certain Roman numeral in the run (higher is less certain). region is the key region
   all the chords lie in; chords are their indices in "chords". */
static cJSON *progression_occurrence_json(DocBuild *build, const ImportedScore *notes, const ChainResult *report, const char *label,
                                          const AnalysisProgressionOccurrence *occurrence) {
    double uncertainty = 0.0;
    for (size_t i = occurrence->first_entry; i <= occurrence->last_entry; i++) {
        if (report->entries[i].roman.labelled && report->entries[i].roman.uncertainty > uncertainty) {
            uncertainty = report->entries[i].roman.uncertainty;
        }
    }
    size_t id_count = 0;
    size_t *ids = doc_entry_range_note_ids(report, occurrence->first_entry, occurrence->last_entry, &id_count);
    cJSON *claim = ids ? doc_claim_json_with(build, doc_library_framework_name(), notes, ids, id_count,
                                         doc_entry_start_time(build, &report->entries[occurrence->first_entry]),
                                         doc_entry_end_time(build, &report->entries[occurrence->last_entry]), label,
                                         cJSON_CreateNumber(uncertainty))
                       : NULL;
    free(ids);
    if (!(claim && common_json_add(claim, "region", cJSON_CreateNumber((double)report->entries[occurrence->first_entry].region)) &&
          common_json_add(claim, "chords", chord_range_json(occurrence->first_entry, occurrence->last_entry)))) {
        cJSON_Delete(claim);
        return NULL;
    }
    return claim;
}

/* One entry per recurring progression, in rank order (most occurrences, then longest, then earliest): pattern (the
   base numerals), label ("I > IV > V > I"), length, count and its occurrences in time order. The progressions are
   the analyzer's count over the library's Roman numerals; nothing here is a claim of its own. */
cJSON *doc_progressions_json(DocBuild *build, const ImportedScore *notes, const ChainResult *report) {
    cJSON *list = cJSON_CreateArray();
    if (!list) {
        return NULL;
    }
    for (size_t i = 0; i < report->progression_count; i++) {
        const AnalysisProgression *progression = &report->progressions[i];
        char *label = chain_progression_label(progression);
        cJSON *entry = label ? cJSON_CreateObject() : NULL;
        cJSON *occurrences = entry ? cJSON_CreateArray() : NULL;
        bool ok = occurrences != NULL;
        for (size_t o = 0; ok && o < progression->occurrence_count; o++) {
            ok = common_json_append(occurrences, progression_occurrence_json(build, notes, report, label, &progression->occurrences[o]));
        }
        bool taken = false; /* common_json_add() takes the occurrences over, and frees them itself when it fails */
        ok = ok && common_json_add(entry, "pattern", progression_pattern_json(progression)) &&
             common_json_add(entry, "label", cJSON_CreateString(label)) &&
             common_json_add(entry, "length", cJSON_CreateNumber((double)progression->length)) &&
             common_json_add(entry, "count", cJSON_CreateNumber((double)progression->occurrence_count)) &&
             (taken = true, common_json_add(entry, "occurrences", occurrences));
        free(label);
        if (!(ok && common_json_append(list, entry))) {
            if (!taken) {
                cJSON_Delete(occurrences);
            }
            if (!ok) {
                cJSON_Delete(entry);
            }
            cJSON_Delete(list);
            return NULL;
        }
    }
    return list;
}

#define RHYTHM_WINDOW_MEASURES 4

/* The mean number of changes per measure over report->harmonic_rhythm[first..first + measures), a rational. */
static Rational rhythm_mean(DocBuild *build, const ChainResult *report, size_t first, size_t measures) {
    long long changes = 0;
    for (size_t m = first; m < first + measures; m++) {
        changes += (long long)report->harmonic_rhythm[m].changes;
    }
    return doc_rational(build, changes, measures);
}

static cJSON *rhythm_per_measure_json(const ChainResult *report) {
    cJSON *list = cJSON_CreateArray();
    for (size_t m = 0; list && m < report->harmonic_rhythm_count; m++) {
        cJSON *entry = cJSON_CreateObject();
        if (!(entry && common_json_add(entry, "measure", cJSON_CreateNumber(report->harmonic_rhythm[m].measure)) &&
              common_json_add(entry, "changes", cJSON_CreateNumber((double)report->harmonic_rhythm[m].changes)) &&
              common_json_append(list, entry))) {
            cJSON_Delete(entry);
            cJSON_Delete(list);
            return NULL;
        }
    }
    return list;
}

/* The mean per window of RHYTHM_WINDOW_MEASURES consecutive measures from the first, the last window shorter when
   the measures run out. */
static cJSON *rhythm_per_window_json(DocBuild *build, const ChainResult *report) {
    cJSON *list = cJSON_CreateArray();
    for (size_t first = 0; list && first < report->harmonic_rhythm_count; first += RHYTHM_WINDOW_MEASURES) {
        size_t measures = report->harmonic_rhythm_count - first;
        measures = measures < RHYTHM_WINDOW_MEASURES ? measures : RHYTHM_WINDOW_MEASURES;
        cJSON *entry = cJSON_CreateObject();
        if (!(entry && common_json_add(entry, "first_measure", cJSON_CreateNumber(report->harmonic_rhythm[first].measure)) &&
              common_json_add(entry, "last_measure", cJSON_CreateNumber(report->harmonic_rhythm[first + measures - 1].measure)) &&
              common_json_add(entry, "mean_per_measure", doc_rational_json(rhythm_mean(build, report, first, measures))) &&
              common_json_append(list, entry))) {
            cJSON_Delete(entry);
            cJSON_Delete(list);
            return NULL;
        }
    }
    return list;
}

/* The mean of changes over measures measures, or null when the piece has none of them. */
static cJSON *window_mean_json(DocBuild *build, size_t changes, size_t measures) {
    return measures > 0 ? doc_rational_json(doc_rational(build, (long long)changes, measures)) : cJSON_CreateNull();
}

/* One entry per cadence, in the cadences' order: its arrival measure, the mean changes per measure over the two
   measures before the arrival (approach) and over the two before those (before), and whether the approach is faster.
   A window the start of the piece clips to nothing has a null mean, and then accelerates is false. */
static cJSON *cadence_approaches_json(DocBuild *build, const ChainResult *report) {
    cJSON *list = cJSON_CreateArray();
    for (size_t c = 0; list && c < report->cadence_approach_count; c++) {
        const AnalysisCadenceApproach *approach = &report->cadence_approaches[c];
        bool accelerates = approach->before_measures > 0 && approach->approach_measures > 0 &&
                           approach->approach_changes * approach->before_measures >
                               approach->before_changes * approach->approach_measures;
        cJSON *entry = cJSON_CreateObject();
        if (!(entry && common_json_add(entry, "cadence", cJSON_CreateNumber((double)approach->cadence)) &&
              common_json_add(entry, "arrival_measure", cJSON_CreateNumber(approach->arrival_measure)) &&
              common_json_add(entry, "before", window_mean_json(build, approach->before_changes, approach->before_measures)) &&
              common_json_add(entry, "approach", window_mean_json(build, approach->approach_changes, approach->approach_measures)) &&
              common_json_add(entry, "accelerates", cJSON_CreateBool(accelerates)) && common_json_append(list, entry))) {
            cJSON_Delete(entry);
            cJSON_Delete(list);
            return NULL;
        }
    }
    return list;
}

/* The harmonic rhythm: the changes of harmony in each measure (a chord segment whose base numeral or key region
   differs from the one before), their mean over the piece, the mean per window of four measures, from the first, and
   how the rhythm moves into each cadence (cadence_approaches). Null when the piece has no chords the meter can place. */
cJSON *doc_harmonic_rhythm_json(DocBuild *build, const ChainResult *report) {
    if (report->harmonic_rhythm_count == 0) {
        return cJSON_CreateNull();
    }
    cJSON *object = cJSON_CreateObject();
    if (!(object && common_json_add(object, "per_measure", rhythm_per_measure_json(report)) &&
          common_json_add(object, "mean_per_measure", doc_rational_json(rhythm_mean(build, report, 0, report->harmonic_rhythm_count))) &&
          common_json_add(object, "per_window", rhythm_per_window_json(build, report)) &&
          common_json_add(object, "cadence_approaches", cadence_approaches_json(build, report)))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

/* The note's own timing, apart from the chord segment it sounds in: where it is attacked, as a time and a
   measure and beat, and how long it lasts in whole notes and in beats of the signature it is attacked in.
   Null members when no note sounds as the line. */
static bool add_outer_note_timing(DocBuild *build, cJSON *claim, const ImportedScore *notes, const AnalysisOuterNote *voice) {
    if (voice->status != ANALYSIS_OUTER_PRESENT) {
        return common_json_add(claim, "onset", cJSON_CreateNull()) && common_json_add(claim, "duration", cJSON_CreateNull());
    }
    const NoteEvent *event = notes->events[voice->note_id];
    Rational onset = doc_note_onset(event);
    Rational end = doc_note_end(build, event);
    cJSON *onset_object = cJSON_CreateObject();
    cJSON *duration = cJSON_CreateObject();
    bool ok = onset_object && duration && common_json_add(onset_object, "time", doc_rational_json(onset)) &&
              common_json_add(onset_object, "location", doc_position_json(build, notes->meter.meter_map, onset));
    if (ok) {
        Rational length = doc_sub(build, end, onset);
        ok = common_json_add(duration, "whole_notes", doc_rational_json(length)) &&
             common_json_add(duration, "beats", doc_span_beats_json(build, notes->meter.meter_map, onset, end));
    }
    if (!ok) {
        cJSON_Delete(onset_object);
        cJSON_Delete(duration);
        return false;
    }
    return common_json_add(claim, "onset", onset_object) && common_json_add(claim, "duration", duration);
}

/* {"name", "midi"}: the note's pitch name with octave and its MIDI note number; null when no note sounds. */
static cJSON *outer_pitch_json(const AnalysisOuterNote *voice) {
    if (voice->status != ANALYSIS_OUTER_PRESENT) {
        return cJSON_CreateNull();
    }
    cJSON *pitch = cJSON_CreateObject();
    if (pitch && !(common_json_add(pitch, "name", cJSON_CreateString(voice->pitch_name)) &&
                   common_json_add(pitch, "midi", cJSON_CreateNumber(voice->pitch)))) {
        cJSON_Delete(pitch);
        return NULL;
    }
    return pitch;
}

/* One entry of an outer line: the claim over the voice segment's window (note_ids is the line's note, or empty
   when none sounds), then the chord whose segment holds the voice segment's start, the status, the note, its
   pitch, its scale degree in that chord's local key, and its timing. The label is the degree; failing that the
   pitch name (the segment lies in no key region), or the status when no note sounds. held is relative to the
   voice segment. */
static cJSON *outer_voice_note_json(DocBuild *build, const ImportedScore *notes, const ChainResult *report,
                                    const AnalysisOuterLineEntry *line, const AnalysisOuterNote *voice) {
    const ChainEntry *entry = &report->entries[line->chord_index];
    bool present = voice->status == ANALYSIS_OUTER_PRESENT;
    Rational start = doc_rational(build, line->time.num, line->time.den);
    Rational end = doc_rational(build, line->end.num, line->end.den);
    const char *label = voice->degree ? voice->degree
                                       : (present ? voice->pitch_name : doc_outer_status_name(voice->status));
    cJSON *claim = doc_claim_json_with(build, doc_library_framework_name(), notes, present ? &voice->note_id : NULL,
                                   present ? 1 : 0, start, end, label, doc_outer_uncertainty_json(report, entry, voice));
    if (!(claim && common_json_add(claim, "chord_index", cJSON_CreateNumber((double)line->chord_index)) &&
          common_json_add(claim, "status", cJSON_CreateString(doc_outer_status_name(voice->status))) &&
          common_json_add(claim, "key_label", cJSON_CreateString(entry->key_label)) &&
          common_json_add(claim, "note_id", present ? cJSON_CreateNumber((double)voice->note_id) : cJSON_CreateNull()) &&
          common_json_add(claim, "pitch", outer_pitch_json(voice)) && common_json_add(claim, "degree", doc_optional_string_json(voice->degree)) &&
          common_json_add(claim, "held", present ? cJSON_CreateBool(voice->held) : cJSON_CreateNull()) &&
          add_outer_note_timing(build, claim, notes, voice))) {
        cJSON_Delete(claim);
        return NULL;
    }
    return claim;
}

/* One line of the outer voices: an entry for every voice segment (one per melody note), in time order. Several
   entries share a chord on a harmonic span, so a line lines up with the chords by chord_index, many to one. */
static cJSON *outer_line_json(DocBuild *build, const ImportedScore *notes, const ChainResult *report, bool soprano) {
    cJSON *line = cJSON_CreateArray();
    if (!line) {
        return NULL;
    }
    for (size_t i = 0; i < report->outer_line_count; i++) {
        const AnalysisOuterLineEntry *entry = &report->outer_lines[i];
        if (!common_json_append(line, outer_voice_note_json(build, notes, report, entry, soprano ? &entry->soprano : &entry->bass))) {
            cJSON_Delete(line);
            return NULL;
        }
    }
    return line;
}

/* The outer_voices section: the soprano and the bass, the shared input for voice-leading, melody and schema
   analysis. */
cJSON *doc_outer_voices_json(DocBuild *build, const ImportedScore *notes, const ChainResult *report) {
    cJSON *outer = cJSON_CreateObject();
    if (outer && !(common_json_add(outer, "soprano", outer_line_json(build, notes, report, true)) &&
                   common_json_add(outer, "bass", outer_line_json(build, notes, report, false)))) {
        cJSON_Delete(outer);
        return NULL;
    }
    return outer;
}
