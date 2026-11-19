/* The motive areas of the analysis document: motives and the melody. */

#include "export/analysis_document_internal.h"

static bool add_owned(cJSON *object, const char *key, cJSON **item) {
    bool ok = common_json_add(object, key, *item);
    *item = NULL;
    return ok;
}

/* An exact fraction of a whole note as "n/d". */
static cJSON *fraction_string_json(unsigned long long numerator, unsigned long long denominator) {
    char text[48];
    snprintf(text, sizeof text, "%llu/%llu", numerator, denominator);
    return cJSON_CreateString(text);
}

static cJSON *strings_json(const char *const *labels, size_t count) {
    cJSON *list = cJSON_CreateArray();
    for (size_t i = 0; list && i < count; i++) {
        if (!common_json_append(list, labels[i] ? cJSON_CreateString(labels[i]) : cJSON_CreateNull())) {
            return NULL;
        }
    }
    return list;
}

static cJSON *numbers_json(const long *values, size_t count) {
    cJSON *list = cJSON_CreateArray();
    for (size_t i = 0; list && i < count; i++) {
        if (!common_json_append(list, cJSON_CreateNumber((double)values[i]))) {
            return NULL;
        }
    }
    return list;
}

/* One occurrence of a motive family as a claim over its soprano notes. The label is the library's kind of the
   occurrence relative to the family's prototype (the transformation: "exact", "real-transposed" for the same semitone
   intervals from another pitch, "tonal-transposed" for the same scale steps, "inverted", "retrograde", "augmented",
   "diminished", "fragmented" or "varied"). measures run from the measure of its first note to the measure of its last;
   beat is the 1-based beat of the first measure its first note starts on. score is the library's match score and
   mismatches the interval mismatches of a varied occurrence out of intervals compared. A fragmented occurrence
   names the part of the prototype it is (fragment: "prefix" or "suffix") and says whether it lies in a continuation
   (in_continuation). phrase is the index into "phrases" of the phrase the whole occurrence lies in, and idea the
   index (within that phrase's "ideas") of the idea it lies in; each is null when the occurrence crosses a boundary.
   Both are placed by span against the report's phrases and ideas. */
static cJSON *motive_occurrence_json(DocBuild *build, const ImportedScore *notes, const ChainResult *report, const char *framework,
                                     const AnalysisMotiveOccurrence *occurrence) {
    Rational start = doc_rational(build, occurrence->start.num, occurrence->start.den);
    Rational end = doc_rational(build, occurrence->end.num, occurrence->end.den);
    cJSON *claim = doc_claim_json_with(build, framework, notes, occurrence->note_ids, occurrence->note_id_count, start, end,
                                   occurrence->kind, cJSON_CreateNumber(occurrence->uncertainty));
    if (!(claim && common_json_add(claim, "index", cJSON_CreateNumber((double)occurrence->index)) &&
          common_json_add(claim, "measures", doc_measures_json(occurrence->first_measure, occurrence->last_measure)) &&
          common_json_add(claim, "beat", cJSON_CreateNumber(occurrence->first_beat)) &&
          common_json_add(claim, "score", cJSON_CreateNumber(occurrence->score)) &&
          common_json_add(claim, "mismatches", cJSON_CreateNumber(occurrence->mismatches)) &&
          common_json_add(claim, "intervals", cJSON_CreateNumber(occurrence->intervals)) &&
          common_json_add(claim, "fragment", doc_string_or_null_json(occurrence->fragment)) &&
          common_json_add(claim, "in_continuation", cJSON_CreateBool(occurrence->in_continuation)) &&
          common_json_add(claim, "phrase", occurrence->has_phrase ? cJSON_CreateNumber((double)occurrence->phrase) : cJSON_CreateNull()) &&
          common_json_add(claim, "idea",
              occurrence->has_idea ? cJSON_CreateNumber(report->ideas[occurrence->idea].index) : cJSON_CreateNull()) &&
          (!occurrence->degrees ||
           common_json_add(claim, "degrees", strings_json(occurrence->degrees, occurrence->note_id_count))))) {
        cJSON_Delete(claim);
        return NULL;
    }
    return claim;
}

/* What a family is made of, read from its prototype: its notes (id, MIDI pitch, onset from the first note and duration
   as "n/d" whole notes), the key region's tonic and mode at the first note, the bar and beat there and the first
   note's position in its bar (pickup, "0/1" on a downbeat), the degrees of the notes against the tonic's major scale
   ("1 b2 2 b3 3 4 #4 5 b6 6 b7 7"), the semitone and scale-step intervals between neighbours, the rhythm (the durations),
   the non-chord-tone type of the segment each note starts in ("chord-tone" for a chord tone, null when the segment has
   no chord or the library left the tone unclassified) and the sketch's fingerprint (16 hex digits). */
static cJSON *motive_features_json(const AnalysisMotiveFeatures *features) {
    cJSON *object = cJSON_CreateObject();
    cJSON *notes = cJSON_CreateArray();
    cJSON *rhythm = cJSON_CreateArray();
    cJSON *intervals = cJSON_CreateArray();
    bool ok = object && notes && rhythm && intervals;
    for (size_t k = 0; ok && k < features->note_count; k++) {
        const AnalysisMotiveNote *note = &features->notes[k];
        cJSON *entry = cJSON_CreateObject();
        ok = entry && common_json_add(entry, "note_id", cJSON_CreateNumber((double)note->note_id)) &&
             common_json_add(entry, "pitch", cJSON_CreateNumber(note->pitch)) &&
             common_json_add(entry, "onset", fraction_string_json(note->onset.num, note->onset.den)) &&
             common_json_add(entry, "duration", fraction_string_json(note->duration.num, note->duration.den));
        ok = ok ? common_json_append(notes, entry) : (cJSON_Delete(entry), false);
        ok = ok && common_json_append(rhythm, fraction_string_json(note->duration.num, note->duration.den));
        if (ok && k > 0) {
            ok = common_json_append(intervals, cJSON_CreateNumber(features->intervals[k - 1]));
        }
    }
    size_t gaps = features->note_count > 0 ? features->note_count - 1 : 0;
    /* common_json_add() takes an item over, and frees it itself when it fails, so each is cleared once handed over */
    ok = ok && add_owned(object, "notes", &notes) &&
         common_json_add(object, "tonic_pc", cJSON_CreateNumber(features->tonic_pc)) &&
         common_json_add(object, "material", cJSON_CreateString(features->material->id)) &&
         common_json_add(object, "bar", fraction_string_json(features->bar.num, features->bar.den)) &&
         common_json_add(object, "beat", fraction_string_json(features->beat.num, features->beat.den)) &&
         common_json_add(object, "pickup", fraction_string_json(features->pickup.num, features->pickup.den)) &&
         common_json_add(object, "degrees", strings_json(features->degrees, features->note_count)) &&
         add_owned(object, "intervals", &intervals) && common_json_add(object, "steps", numbers_json(features->steps, gaps)) &&
         add_owned(object, "rhythm", &rhythm) &&
         common_json_add(object, "ncts", strings_json((const char *const *)features->ncts, features->note_count)) &&
         common_json_add(object, "fingerprint", cJSON_CreateString(features->fingerprint));
    cJSON_Delete(notes);
    cJSON_Delete(rhythm);
    cJSON_Delete(intervals);
    if (!ok) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

/* The development of a family: a step per occurrence in time order. transformation is the library's kind of that
   occurrence against the prototype, null for the prototype itself (which is not always the earliest: the library
   searches continuations for fragments first). from is the occurrence before it in time (null for the first);
   occurrence and from are indices into the family's "occurrences". */
static cJSON *motive_development_json(const AnalysisMotive *motive) {
    cJSON *steps = cJSON_CreateArray();
    if (!steps) {
        return NULL;
    }
    for (size_t k = 0; k < motive->occurrence_count; k++) {
        size_t index = motive->development[k];
        const AnalysisMotiveOccurrence *occurrence = &motive->occurrences[index];
        cJSON *step = cJSON_CreateObject();
        if (!(step && common_json_add(step, "occurrence", cJSON_CreateNumber((double)index)) &&
              common_json_add(step, "measure", cJSON_CreateNumber(occurrence->first_measure)) &&
              common_json_add(step, "beat", cJSON_CreateNumber(occurrence->first_beat)) &&
              common_json_add(step, "transformation", index == 0 ? cJSON_CreateNull() : cJSON_CreateString(occurrence->kind)) &&
              common_json_add(step, "from", k == 0 ? cJSON_CreateNull() : cJSON_CreateNumber((double)motive->development[k - 1])))) {
            cJSON_Delete(step);
            cJSON_Delete(steps);
            return NULL;
        }
        if (!common_json_append(steps, step)) {
            cJSON_Delete(steps);
            return NULL;
        }
    }
    return steps;
}

/* One claim per motive family, in the library's order. Its span, note ids and location are the prototype's (the first
   occurrence); note_count is the prototype's length; generic says the pattern is the scale-step interval sequence
   rather than the semitones; uncertainty is the greatest of its occurrences'. occurrences are the claims of every
   occurrence, the prototype first and the rest in the library's order (not always time order); development is the
   family's occurrences in time order as steps. */
cJSON *doc_motives_json(DocBuild *build, const ImportedScore *notes, const ChainResult *report) {
    cJSON *list = cJSON_CreateArray();
    if (!list) {
        return NULL;
    }
    const char *framework = framework_name(standard_form_framework());
    for (size_t m = 0; m < reading_motive_occurrence_count(build->reading); m++) {
        const ReadingMotiveOccurrence *held = reading_motive_occurrence_at(build->reading, m);
        if (held->detail != 0) { /* a family is printed at its prototype, its first occurrence */
            continue;
        }
        const AnalysisMotive *motive = &report->motives[held->motive];
        const AnalysisMotiveOccurrence *prototype = &motive->occurrences[0];
        double uncertainty = 0.0;
        for (size_t o = 0; o < motive->occurrence_count; o++) {
            if (motive->occurrences[o].uncertainty > uncertainty) {
                uncertainty = motive->occurrences[o].uncertainty;
            }
        }
        Rational start = doc_rational(build, prototype->start.num, prototype->start.den);
        Rational end = doc_rational(build, prototype->end.num, prototype->end.den);
        cJSON *claim = doc_claim_json_with(build, framework, notes, prototype->note_ids, prototype->note_id_count, start, end,
                                       "motive", cJSON_CreateNumber(uncertainty));
        cJSON *occurrences = cJSON_CreateArray();
        bool ok = claim && occurrences;
        for (size_t o = 0; ok && o < motive->occurrence_count; o++) {
            ok = common_json_append(occurrences, motive_occurrence_json(build, notes, report, framework, &motive->occurrences[o]));
        }
        bool taken = false; /* common_json_add() takes the occurrences over, and frees them itself when it fails */
        if (!(ok && common_json_add(claim, "index", cJSON_CreateNumber((double)motive->index)) &&
              common_json_add(claim, "note_count", cJSON_CreateNumber(motive->note_count)) &&
              common_json_add(claim, "generic", cJSON_CreateBool(motive->generic)) &&
              (taken = true, common_json_add(claim, "occurrences", occurrences)) &&
              common_json_add(claim, "development", motive_development_json(motive)) &&
              (!motive->has_features || common_json_add(claim, "features", motive_features_json(&motive->features))))) {
            if (!taken) {
                cJSON_Delete(occurrences);
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

/* A pitch and the note it is the pitch of: { "pitch": MIDI note number, "name": "E5", "note_id": ImportedScore
   index }. */
static cJSON *melody_pitch_json(long pitch, const char *name, size_t note) {
    cJSON *object = cJSON_CreateObject();
    if (object && !(common_json_add(object, "pitch", cJSON_CreateNumber((double)pitch)) &&
                    common_json_add(object, "name", cJSON_CreateString(name)) &&
                    common_json_add(object, "note_id", cJSON_CreateNumber((double)note)))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

static cJSON *melody_range_json(const AnalysisMelody *melody) {
    cJSON *object = cJSON_CreateObject();
    if (object && !(common_json_add(object, "lowest", melody_pitch_json(melody->lowest_pitch, melody->lowest_name, melody->lowest_note)) &&
                    common_json_add(object, "highest", melody_pitch_json(melody->highest_pitch, melody->highest_name, melody->highest_note)) &&
                    common_json_add(object, "semitones", cJSON_CreateNumber((double)melody->range_semitones)))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

/* The climax as a claim over its first note: the highest pitch, how many notes sound it, and where the first one
   stands (its place among the notes, its beat strength, whether a leap approaches it, whether it ends the phrase). */
static cJSON *melody_climax_json(DocBuild *build, const ImportedScore *notes, const AnalysisMelody *melody) {
    Rational start;
    Rational end;
    doc_notes_span(build, notes, &melody->climax_note, 1, &start, &end);
    cJSON *claim = doc_claim_json_with(build, melody->framework, notes, &melody->climax_note, 1, start, end, "climax",
                                   cJSON_CreateNumber(melody->uncertainty));
    if (!(claim && common_json_add(claim, "pitch", cJSON_CreateNumber((double)melody->highest_pitch)) &&
          common_json_add(claim, "occurrences", cJSON_CreateNumber(melody->climax_occurrences)) &&
          common_json_add(claim, "unique", cJSON_CreateBool(melody->climax_unique)) &&
          common_json_add(claim, "relative_position", cJSON_CreateNumber(melody->climax_relative_position)) &&
          common_json_add(claim, "strength",
              melody->climax_has_strength ? cJSON_CreateString(melody->climax_strength) : cJSON_CreateNull()) &&
          common_json_add(claim, "by_leap", cJSON_CreateBool(melody->climax_by_leap)) &&
          common_json_add(claim, "final_note", cJSON_CreateBool(melody->climax_final)))) {
        cJSON_Delete(claim);
        return NULL;
    }
    return claim;
}

/* The contour: how many intervals go up, down and stay, how many are steps and leaps, the last note against the first,
   and the direction of each interval in time order. No shape is named. */
static cJSON *melody_contour_json(const AnalysisMelody *melody) {
    cJSON *object = cJSON_CreateObject();
    cJSON *directions = cJSON_CreateArray();
    bool ok = object && directions;
    for (size_t k = 0; ok && k < melody->direction_count; k++) {
        ok = common_json_append(directions, cJSON_CreateString(melody->directions[k]));
    }
    bool taken = false; /* common_json_add() takes the directions over, and frees them itself when it fails */
    if (!(ok && common_json_add(object, "up", cJSON_CreateNumber(melody->up)) &&
          common_json_add(object, "down", cJSON_CreateNumber(melody->down)) &&
          common_json_add(object, "same", cJSON_CreateNumber(melody->same)) &&
          common_json_add(object, "steps", cJSON_CreateNumber(melody->step_count)) &&
          common_json_add(object, "leaps", cJSON_CreateNumber(melody->leap_count)) &&
          common_json_add(object, "net_direction", cJSON_CreateString(melody->net_direction)) &&
          common_json_add(object, "net_semitones", cJSON_CreateNumber((double)melody->net_semitones)) &&
          (taken = true, common_json_add(object, "directions", directions)))) {
        if (!taken) {
            cJSON_Delete(directions);
        }
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

/* One leap as a claim over its two notes: the interval in semitones and in letter names (null when the library has no
   size), whether it was read from the semitones alone, and the library's verdict on its recovery. */
static cJSON *melody_leap_json(DocBuild *build, const ImportedScore *notes, const char *framework, const AnalysisMelodyLeap *leap) {
    size_t ids[2] = {leap->from_note, leap->to_note};
    Rational start;
    Rational end;
    doc_notes_span(build, notes, ids, 2, &start, &end);
    cJSON *claim = doc_claim_json_with(build, framework, notes, ids, 2, start, end, "leap", cJSON_CreateNumber(leap->uncertainty));
    if (!(claim && common_json_add(claim, "semitones", cJSON_CreateNumber((double)leap->semitones)) &&
          common_json_add(claim, "direction", cJSON_CreateString(leap->direction)) &&
          common_json_add(claim, "generic_size", leap->generic_size > 0 ? cJSON_CreateNumber(leap->generic_size) : cJSON_CreateNull()) &&
          common_json_add(claim, "approximate", cJSON_CreateBool(leap->approximate)) &&
          common_json_add(claim, "status", cJSON_CreateString(leap->status)))) {
        cJSON_Delete(claim);
        return NULL;
    }
    return claim;
}

/* The leaps of a phrase: how many the library finds recovered, unrecovered, not required and not judged, and every
   leap as a claim in time order. */
static cJSON *melody_leaps_json(DocBuild *build, const ImportedScore *notes, const AnalysisMelody *melody) {
    static const char *const STATUSES[4] = {"recovered", "unrecovered", "not-required", "not-judged"};
    static const char *const KEYS[4] = {"recovered", "unrecovered", "not_required", "not_judged"};
    cJSON *object = cJSON_CreateObject();
    cJSON *items = cJSON_CreateArray();
    bool ok = object && items;
    for (size_t k = 0; ok && k < 4; k++) {
        size_t count = 0;
        for (size_t i = 0; i < melody->leap_total; i++) {
            count += strcmp(melody->leaps[i].status, STATUSES[k]) == 0 ? 1 : 0;
        }
        ok = common_json_add(object, KEYS[k], cJSON_CreateNumber((double)count));
    }
    for (size_t i = 0; ok && i < melody->leap_total; i++) {
        ok = common_json_append(items, melody_leap_json(build, notes, melody->framework, &melody->leaps[i]));
    }
    bool taken = false; /* common_json_add() takes the items over, and frees them itself when it fails */
    if (!(ok && (taken = true, common_json_add(object, "items", items)))) {
        if (!taken) {
            cJSON_Delete(items);
        }
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

/* The pairs of leaps in a row the library's rule finds fault with, each a claim over its three notes. */
static cJSON *melody_consecutive_leaps_json(DocBuild *build, const ImportedScore *notes, const AnalysisMelody *melody) {
    cJSON *list = cJSON_CreateArray();
    for (size_t i = 0; list && i < melody->consecutive_leap_count; i++) {
        const AnalysisMelodyConsecutiveLeaps *pair = &melody->consecutive_leaps[i];
        size_t ids[3] = {pair->first_note, pair->middle_note, pair->last_note};
        Rational start;
        Rational end;
        doc_notes_span(build, notes, ids, 3, &start, &end);
        cJSON *claim = doc_claim_json_with(build, melody->framework, notes, ids, 3, start, end, "consecutive-leaps",
                                       cJSON_CreateNumber(pair->uncertainty));
        if (!(claim && common_json_add(claim, "same_direction", cJSON_CreateBool(pair->same_direction)) &&
              common_json_add(claim, "three_in_a_row", cJSON_CreateBool(pair->three_in_a_row)) && common_json_append(list, claim))) {
            cJSON_Delete(claim);
            cJSON_Delete(list);
            return NULL;
        }
    }
    return list;
}

/* The non-chord tones of a phrase's melody: the library's counts (chord tones, non-chord tones, unclassified notes, and
   the non-chord tones by type, the types that occur in the library's order) and each non-chord tone as a claim over its
   note, labelled with the library's type. */
static cJSON *melody_non_chord_tones_json(DocBuild *build, const ImportedScore *notes, const AnalysisMelody *melody) {
    cJSON *object = cJSON_CreateObject();
    cJSON *by_type = cJSON_CreateArray();
    cJSON *items = cJSON_CreateArray();
    bool ok = object && by_type && items;
    for (int type = 0; ok && type < ANALYSIS_MELODY_NCT_TYPES; type++) {
        if (melody->nct_by_type[type] > 0) {
            cJSON *entry = cJSON_CreateObject();
            ok = entry && common_json_add(entry, "type", cJSON_CreateString(melody->nct_type_names[type])) &&
                 common_json_add(entry, "count", cJSON_CreateNumber(melody->nct_by_type[type])) && common_json_append(by_type, entry);
            if (!ok) {
                cJSON_Delete(entry);
            }
        }
    }
    for (size_t i = 0; ok && i < melody->nct_total; i++) {
        const AnalysisMelodyNct *nct = &melody->ncts[i];
        Rational start;
        Rational end;
        doc_notes_span(build, notes, &nct->note, 1, &start, &end);
        cJSON *claim = doc_claim_json_with(build, melody->framework, notes, &nct->note, 1, start, end, nct->type,
                                       cJSON_CreateNumber(nct->uncertainty));
        ok = claim && common_json_append(items, claim);
        if (!ok) {
            cJSON_Delete(claim);
        }
    }
    bool by_type_taken = false; /* common_json_add() takes each array over, and frees it itself when it fails */
    bool items_taken = false;
    if (!(ok && common_json_add(object, "chord_tones", cJSON_CreateNumber(melody->chord_tone_count)) &&
          common_json_add(object, "non_chord_tones", cJSON_CreateNumber(melody->non_chord_tone_count)) &&
          common_json_add(object, "unclassified", cJSON_CreateNumber(melody->unclassified_count)) &&
          (by_type_taken = true, common_json_add(object, "by_type", by_type)) &&
          (items_taken = true, common_json_add(object, "items", items)))) {
        if (!by_type_taken) {
            cJSON_Delete(by_type);
        }
        if (!items_taken) {
            cJSON_Delete(items);
        }
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

/* The library's line tendencies as { "tendency": name, "holds": bool }, in its order; empty when the phrase's notes do not
   spell. */
static cJSON *melody_line_tendencies_json(const AnalysisMelody *melody) {
    cJSON *list = cJSON_CreateArray();
    for (int t = 0; list && t < ANALYSIS_MELODY_TENDENCIES && melody->tendency_names[t]; t++) {
        cJSON *entry = cJSON_CreateObject();
        if (!(entry && common_json_add(entry, "tendency", cJSON_CreateString(melody->tendency_names[t])) &&
              common_json_add(entry, "holds", cJSON_CreateBool(melody->tendency_holds[t])) && common_json_append(list, entry))) {
            cJSON_Delete(entry);
            cJSON_Delete(list);
            return NULL;
        }
    }
    return list;
}

/* The seventh degree: the notes the library labels degree 7 that a note follows, judged by its rule LEADING_TONE: how many
   followed it and how many broke it, with each broken one as a claim over the seventh and the note after it. Null when
   the phrase's notes do not spell. */
static cJSON *melody_seventh_degree_json(DocBuild *build, const ImportedScore *notes, const AnalysisMelody *melody) {
    if (!melody->seventh_rule) {
        return cJSON_CreateNull();
    }
    cJSON *object = cJSON_CreateObject();
    cJSON *breaks = cJSON_CreateArray();
    bool ok = object && breaks;
    for (int i = 0; ok && i < melody->seventh_broken; i++) {
        const AnalysisMelodyBreak *broken = &melody->seventh_breaks[i];
        size_t ids[2] = {broken->note, broken->next_note};
        Rational start;
        Rational end;
        doc_notes_span(build, notes, ids, 2, &start, &end);
        cJSON *claim = doc_claim_json_with(build, melody->framework, notes, ids, 2, start, end, melody->seventh_rule,
                                       cJSON_CreateNumber(broken->uncertainty));
        ok = claim && common_json_append(breaks, claim);
        if (!ok) {
            cJSON_Delete(claim);
        }
    }
    bool taken = false; /* common_json_add() takes the breaks over, and frees them itself when it fails */
    if (!(ok && common_json_add(object, "rule", cJSON_CreateString(melody->seventh_rule)) &&
          common_json_add(object, "followed", cJSON_CreateNumber(melody->seventh_followed)) &&
          common_json_add(object, "broken", cJSON_CreateNumber(melody->seventh_broken)) &&
          (taken = true, common_json_add(object, "breaks", breaks)))) {
        if (!taken) {
            cJSON_Delete(breaks);
        }
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

static cJSON *melody_tendencies_json(DocBuild *build, const ImportedScore *notes, const AnalysisMelody *melody) {
    cJSON *object = cJSON_CreateObject();
    if (object && !(common_json_add(object, "line", melody_line_tendencies_json(melody)) &&
                    common_json_add(object, "seventh_degree", melody_seventh_degree_json(build, notes, melody)))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

/* One claim per phrase with melody notes, in phrase order: the soprano notes that start in the phrase. index is the
   phrase's, measures its measures; range, climax, contour, leaps, consecutive leaps, non-chord tones and tendencies are
   the melody adapter's and the library's rules' results over the notes. */
cJSON *doc_melody_json(DocBuild *build, const ImportedScore *notes, const ChainResult *report) {
    cJSON *list = cJSON_CreateArray();
    if (!list) {
        return NULL;
    }
    for (size_t i = 0; i < report->melody_count; i++) {
        const AnalysisMelody *melody = &report->melodies[i];
        Rational start = doc_rational(build, melody->start.num, melody->start.den);
        Rational end = doc_rational(build, melody->end.num, melody->end.den);
        cJSON *claim = doc_claim_json_with(build, melody->framework, notes, melody->note_ids, melody->note_id_count, start, end,
                                       "melody", cJSON_CreateNumber(melody->uncertainty));
        if (!(claim && common_json_add(claim, "index", cJSON_CreateNumber((double)melody->phrase)) &&
              common_json_add(claim, "measures", doc_measures_json(melody->first_measure, melody->last_measure)) &&
              common_json_add(claim, "note_count", cJSON_CreateNumber((double)melody->note_id_count)) &&
              common_json_add(claim, "range", melody_range_json(melody)) && common_json_add(claim, "climax", melody_climax_json(build, notes, melody)) &&
              common_json_add(claim, "contour", melody_contour_json(melody)) &&
              common_json_add(claim, "leaps", melody_leaps_json(build, notes, melody)) &&
              common_json_add(claim, "consecutive_leaps", melody_consecutive_leaps_json(build, notes, melody)) &&
              common_json_add(claim, "non_chord_tones", melody_non_chord_tones_json(build, notes, melody)) &&
              common_json_add(claim, "tendencies", melody_tendencies_json(build, notes, melody)))) {
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
