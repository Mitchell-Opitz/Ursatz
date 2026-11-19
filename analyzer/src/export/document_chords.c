/* The chord areas of the analysis document: chords, non-chord tones and the chord vocabulary. */

#include "export/analysis_document_internal.h"

/* The runner-up candidate, or null when the library gave none. */
static cJSON *runner_up_json(const AnalysisChordReading *reading) {
    if (!reading->label) {
        return cJSON_CreateNull();
    }
    cJSON *object = cJSON_CreateObject();
    if (object && !(common_json_add(object, "label", cJSON_CreateString(reading->label)) &&
                    common_json_add(object, "root", cJSON_CreateString(reading->root)) &&
                    common_json_add(object, "quality", cJSON_CreateString(reading->quality)) &&
                    common_json_add(object, "score", cJSON_CreateNumber(reading->score)) &&
                    common_json_add(object, "uncertainty", doc_uncertainty_json(reading->confidence)))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

static const char *chord_context_name(AnalysisChordContext context) {
    switch (context) {
        case ANALYSIS_CHORD_CONTEXT_PREVIOUS:
            return "previous";
        case ANALYSIS_CHORD_CONTEXT_NEXT:
            return "next";
        case ANALYSIS_CHORD_CONTEXT_ARPEGGIO:
            return "arpeggio";
        default:
            return "none";
    }
}

/* The chord fields beyond the shared claim: root, quality, bass, inversion, duration, score, runner-up and the
   context the reading came from. */
static bool add_chord_fields(DocBuild *build, cJSON *claim, const ImportedScore *notes, const ChainEntry *entry, Rational start,
                             Rational end) {
    const AnalysisChordReading *chord = &entry->chord;
    bool matched = chord->label != NULL;
    cJSON *duration = cJSON_CreateObject();
    if (duration && !(common_json_add(duration, "whole_notes", doc_rational_json(doc_sub(build, end, start))) &&
                      common_json_add(duration, "beats", doc_span_beats_json(build, notes->meter.meter_map, start, end)))) {
        cJSON_Delete(duration);
        duration = NULL;
    }
    return common_json_add(claim, "root", doc_optional_string_json(chord->root)) &&
           common_json_add(claim, "quality", doc_optional_string_json(chord->quality)) &&
           common_json_add(claim, "bass", doc_optional_string_json(chord->bass)) &&
           common_json_add(claim, "inversion", matched && chord->inversion >= 0 ? cJSON_CreateNumber(chord->inversion)
                                                                    : cJSON_CreateNull()) &&
           common_json_add(claim, "duration", duration) &&
           common_json_add(claim, "score", matched ? cJSON_CreateNumber(chord->score) : cJSON_CreateNull()) &&
           common_json_add(claim, "runner_up", runner_up_json(&entry->runner_up)) &&
           common_json_add(claim, "context", matched ? cJSON_CreateString(chord_context_name(entry->chord_context))
                                         : cJSON_CreateNull());
}

/* One outer voice of a chord: its status, the note and pitch name when a note sounds as the line, its scale
   degree in the local key (null in no key region), and the degree's uncertainty. */
static cJSON *outer_voice_summary_json(const ChainResult *report, const ChainEntry *entry,
                                       const AnalysisOuterNote *note) {
    bool present = note->status == ANALYSIS_OUTER_PRESENT;
    cJSON *object = cJSON_CreateObject();
    if (object &&
        !(common_json_add(object, "status", cJSON_CreateString(doc_outer_status_name(note->status))) &&
          common_json_add(object, "note_id", present ? cJSON_CreateNumber((double)note->note_id) : cJSON_CreateNull()) &&
          common_json_add(object, "pitch", doc_optional_string_json(note->pitch_name)) &&
          common_json_add(object, "degree", doc_optional_string_json(note->degree)) &&
          common_json_add(object, "uncertainty", doc_outer_uncertainty_json(report, entry, note)))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

/* The chord's "outer" field: the soprano and the bass sounding in its segment. (The chord's own "bass" field is
   the pitch class the chord is read over.) */
static cJSON *chord_outer_json(const ChainResult *report, const ChainEntry *entry) {
    cJSON *outer = cJSON_CreateObject();
    if (outer && !(common_json_add(outer, "soprano", outer_voice_summary_json(report, entry, &entry->soprano)) &&
                   common_json_add(outer, "bass", outer_voice_summary_json(report, entry, &entry->bass)))) {
        cJSON_Delete(outer);
        return NULL;
    }
    return outer;
}

/* Whether a chord's span lies in, or runs straight into, the span of a region: the tonicization region the
   chord belongs to, or the one it leads to. */
static bool span_touches(Rational chord_start, Rational chord_end, Rational start, Rational end) {
    return (doc_time_before(chord_start, end) && doc_time_before(start, chord_end)) || rational_equals(chord_end, start);
}

/* The chord's applied reading and its cross-reference to the tonicization claims, or null for a chord that is
   not applied. The tonicization claims are numbered as in "tonicizations". Among the claims whose span touches the
   chord's (see span_touches), one whose key has the tonic the applied target names is the agreeing one; if
   only claims with another tonic touch it, the two disagree; if none touches, there is no tonicization.
   Neither side is changed to fit the other. */
static cJSON *applied_json(DocBuild *build, const ChainResult *report, const ChainEntry *entry) {
    const AnalysisRomanNumeral *roman = &entry->roman;
    if (!roman->applied) {
        return cJSON_CreateNull();
    }
    bool in_region = entry->in_region && entry->region < report->key_region_count;
    int target_tonic = -1;
    if (in_region) {
        const AnalysisKeyRegion *local = &report->key_regions[entry->region];
        int semitones = standard_tonality_material_degree_semitone(local->material, roman->target_degree);
        target_tonic = semitones < 0 ? -1 : (local->tonic_pitch_class + semitones) % 12;
    }
    Rational chord_start = doc_rational(build, entry->time.num, entry->time.den);
    Rational chord_end = doc_rational(build, entry->end.num, entry->end.den);

    bool touching = false;
    size_t touching_index = 0;
    size_t touching_region = 0;
    bool agrees = false;
    size_t tonicization_index = 0;
    for (size_t i = 0; i < report->key_change_count && !agrees; i++) {
        const AnalysisKeyChange *change = &report->key_changes[i];
        if (!doc_change_is_tonicization(change) || change->is_return) {
            continue;
        }
        const AnalysisKeyRegion *target = &report->key_regions[change->to_region];
        if (span_touches(chord_start, chord_end, doc_region_start(build, target), doc_region_end(build, target))) {
            if (!touching || target->tonic_pitch_class == target_tonic) {
                touching = true;
                touching_index = tonicization_index;
                touching_region = change->to_region;
                agrees = target->tonic_pitch_class == target_tonic;
            }
        }
        tonicization_index++;
    }

    const char *agreement = agrees ? "agrees" : (touching ? "target_differs" : "no_tonicization");
    cJSON *object = cJSON_CreateObject();
    if (object &&
        !(common_json_add(object, "numeral", cJSON_CreateString(roman->applied_numeral)) &&
          common_json_add(object, "target_numeral", cJSON_CreateString(roman->target_numeral)) &&
          common_json_add(object, "target_degree", cJSON_CreateNumber(roman->target_degree)) &&
          common_json_add(object, "target_tonic", target_tonic >= 0 ? cJSON_CreateString(pitch_class_name(
                                          target_tonic, report->key_regions[entry->region].prefer_flats))
                                                         : cJSON_CreateNull()) &&
          common_json_add(object, "kind", cJSON_CreateString(roman->applied_kind)) &&
          common_json_add(object, "resolves_to", roman->resolution_entry == SIZE_MAX
                                          ? cJSON_CreateNull()
                                          : cJSON_CreateNumber((double)roman->resolution_entry)) &&
          common_json_add(object, "deceptive", cJSON_CreateBool(roman->deceptive)) &&
          common_json_add(object, "tonicization_agreement", cJSON_CreateString(agreement)) &&
          common_json_add(object, "tonicization_index", touching ? cJSON_CreateNumber((double)touching_index) : cJSON_CreateNull()) &&
          common_json_add(object, "target_region", touching ? cJSON_CreateNumber((double)touching_region) : cJSON_CreateNull()) &&
          common_json_add(object, "tonicization_key", touching ? cJSON_CreateString(report->key_regions[touching_region].label)
                                                   : cJSON_CreateNull()))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

/* The chord's Roman numeral: the library's label, the key region it is relative to (the region the segment
   starts in, which is also the key the chord was read in), its scale degree and uncertainty; or null label with
   the reason there is none. */
static cJSON *roman_numeral_json(DocBuild *build, const ChainResult *report, const ChainEntry *entry) {
    const AnalysisRomanNumeral *roman = &entry->roman;
    bool in_region = entry->in_region && entry->region < report->key_region_count;
    cJSON *object = cJSON_CreateObject();
    if (object &&
        !(common_json_add(object, "label", doc_optional_string_json(roman->numeral)) &&
          common_json_add(object, "base_numeral", doc_optional_string_json(roman->base_numeral)) &&
          common_json_add(object, "key_region", in_region ? cJSON_CreateNumber((double)entry->region) : cJSON_CreateNull()) &&
          common_json_add(object, "key", in_region ? cJSON_CreateString(report->key_regions[entry->region].label)
                                       : cJSON_CreateNull()) &&
          common_json_add(object, "degree", roman->labelled ? cJSON_CreateNumber(roman->degree) : cJSON_CreateNull()) &&
          common_json_add(object, "uncertainty", roman->labelled ? cJSON_CreateNumber(roman->uncertainty) : cJSON_CreateNull()) &&
          common_json_add(object, "unlabelled_reason", doc_optional_string_json(roman->reason)) &&
          common_json_add(object, "applied", applied_json(build, report, entry)))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

/* The chord's harmonic function, the library's (standard-harmony), with the target degree of an applied chord (null
   otherwise). Chords the library gives no function have none. */
static bool add_function(cJSON *claim, const ChainEntry *entry) {
    if (entry->six_four) {
        cJSON *six_four = cJSON_CreateObject();
        if (!(six_four && common_json_add(six_four, "type", cJSON_CreateString(entry->six_four)) &&
              common_json_add(claim, "six_four", six_four))) {
            cJSON_Delete(six_four);
            return false;
        }
    }
    if (!entry->function) {
        return true;
    }
    cJSON *object = cJSON_CreateObject();
    if (!(object && common_json_add(object, "name", cJSON_CreateString(entry->function)) &&
          common_json_add(object, "applied_to",
              entry->function_applied ? cJSON_CreateNumber(entry->function_applied_to) : cJSON_CreateNull()) &&
          common_json_add(object, "predominant", doc_optional_string_json(entry->predominant_strength)) &&
          common_json_add(object, "framework", cJSON_CreateString(framework_name(standard_harmony_framework()))) &&
          common_json_add(object, "uncertainty", cJSON_CreateNumber(entry->function_uncertainty)))) {
        cJSON_Delete(object);
        return false;
    }
    return common_json_add(claim, "function", object);
}

/* One chord claim per report entry, in time order. */
cJSON *doc_chords_json(DocBuild *build, const ImportedScore *notes, const ChainResult *report) {
    cJSON *chords = cJSON_CreateArray();
    if (!chords) {
        return NULL;
    }
    const char *framework = doc_library_framework_name();
    for (size_t i = 0; i < report->entry_count; i++) {
        const ChainEntry *entry = &report->entries[i];
        Rational start = doc_rational(build, entry->time.num, entry->time.den);
        Rational end = doc_rational(build, entry->end.num, entry->end.den);
        cJSON *claim = doc_claim_json_at(build, framework, notes, entry->note_ids, entry->note_id_count, start, end,
                                     entry->chord_label, entry->chord_confidence);
        if (!(claim && common_json_add(claim, "key_label", cJSON_CreateString(entry->key_label)) &&
              add_chord_fields(build, claim, notes, entry, start, end) &&
              common_json_add(claim, "outer", chord_outer_json(report, entry)) &&
              common_json_add(claim, "roman_numeral", roman_numeral_json(build, report, entry)) && add_function(claim, entry) &&
              (!entry->phrase_model || common_json_add(claim, "phrase_model", cJSON_CreateString(entry->phrase_model))))) {
            cJSON_Delete(claim);
            cJSON_Delete(chords);
            return NULL;
        }
        if (!common_json_append(chords, claim)) {
            cJSON_Delete(chords);
            return NULL;
        }
    }
    return chords;
}

/* One claim per non-chord tone, in time order. Each is located where the note sounds against the chord:
   that chord's segment. */
cJSON *doc_non_chord_tones_json(DocBuild *build, const ImportedScore *notes, const ChainResult *report) {
    cJSON *tones = cJSON_CreateArray();
    if (!tones) {
        return NULL;
    }
    const char *framework = doc_library_framework_name();
    for (size_t i = 0; i < report->non_chord_tone_count; i++) {
        const AnalysisNonChordTone *tone = &report->non_chord_tones[i];
        const ChainEntry *chord = &report->entries[tone->chord_index];
        AnalysisConfidence none = {false, {0, 1}};
        cJSON *claim = doc_claim_json_at(build, 
            framework, notes, &tone->note_id, 1, doc_rational(build, chord->time.num, chord->time.den),
            doc_rational(build, chord->end.num, chord->end.den), tone->type ? tone->type : "unclassified", none);
        if (!(claim && common_json_add(claim, "type", doc_optional_string_json(tone->type)) &&
              common_json_add(claim, "classified", cJSON_CreateBool(tone->type != NULL)) &&
              common_json_add(claim, "chord_index", cJSON_CreateNumber((double)tone->chord_index)) &&
              common_json_add(claim, "chord_label", cJSON_CreateString(chord->chord_label)))) {
            cJSON_Delete(claim);
            cJSON_Delete(tones);
            return NULL;
        }
        if (!common_json_append(tones, claim)) {
            cJSON_Delete(tones);
            return NULL;
        }
    }
    return tones;
}

/* The groups a labelled chord falls in. Applied is the library's applied reading. The library's Roman numeral
   adapter does not yet name borrowed (modal mixture) chords, so no chord is placed in the mixture group today. */
typedef enum { GROUP_DIATONIC = 0, GROUP_APPLIED, GROUP_MIXTURE, GROUP_COUNT } VocabularyGroup;

static const char *const GROUP_NAMES[GROUP_COUNT] = {"diatonic", "applied", "mixture"};

static const char *const LEVEL_NAMES[2] = {"numeral", "base_numeral"};

#define SCOPE_OVERALL (-2L)

/* One labelled chord, read back from its chord claim. */
typedef struct {
    size_t index;
    long region; /* -1 when the chord lies in no key region */
    int group;
    const char *label;
    const char *base;
    Rational whole;
    bool has_beats;
    Rational beats;
    bool starts_run; /* not a repeat of the chord before it */
    const cJSON *position; /* location.start */
} VocabularyChord;

typedef struct {
    const char *name;
    size_t count;
    size_t runs;
    Rational whole;
    Rational beats;
    bool beats_known;
    size_t first; /* index of its first chord claim */
    const cJSON *position;
} VocabularyTally;

static Rational rational_read(DocBuild *build, const cJSON *object) {
    long long num = 0;
    long long den = 1;
    if (!common_json_rational_read(object, COMMON_RATIONAL_OBJECT, &num, &den)) {
        num = 0;
        den = 1;
    }
    return doc_rational(build, num, (unsigned long long)den);
}

/* The labelled chords of the chords section, in time order. *count is how many. NULL on allocation failure. */
static VocabularyChord *vocabulary_chords(DocBuild *build, const cJSON *chords, size_t *count) {
    size_t total = (size_t)cJSON_GetArraySize(chords);
    VocabularyChord *records = calloc(total > 0 ? total : 1, sizeof(VocabularyChord));
    if (!records) {
        return NULL;
    }
    *count = 0;
    const VocabularyChord *previous = NULL;
    size_t index = 0;
    const cJSON *chord;
    cJSON_ArrayForEach(chord, chords) {
        const cJSON *roman = cJSON_GetObjectItemCaseSensitive(chord, "roman_numeral");
        const char *label = common_json_string_or_null(roman, "label");
        const char *base = common_json_string_or_null(roman, "base_numeral");
        if (label && base) {
            const cJSON *region = cJSON_GetObjectItemCaseSensitive(roman, "key_region");
            const cJSON *duration = cJSON_GetObjectItemCaseSensitive(chord, "duration");
            const cJSON *beats = cJSON_GetObjectItemCaseSensitive(duration, "beats");
            VocabularyChord *record = &records[(*count)++];
            record->index = index;
            record->region = cJSON_IsNumber(region) ? (long)region->valuedouble : -1;
            record->group = cJSON_IsObject(cJSON_GetObjectItemCaseSensitive(roman, "applied")) ? GROUP_APPLIED
                                                                                                 : GROUP_DIATONIC;
            record->label = label;
            record->base = base;
            record->whole = rational_read(build, cJSON_GetObjectItemCaseSensitive(duration, "whole_notes"));
            record->has_beats = cJSON_IsObject(beats);
            record->beats = record->has_beats ? rational_read(build, beats) : doc_rational(build, 0, 1);
            record->position = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(chord, "location"),
                                                                "start");
            record->starts_run = !(previous && previous->index + 1 == index && previous->region == record->region &&
                                   strcmp(previous->label, label) == 0);
            previous = record;
        } else {
            previous = NULL;
        }
        index++;
    }
    return records;
}

/* Most chords first, then the longest, then the earliest. */
static int compare_tallies(const void *a, const void *b) {
    const VocabularyTally *left = a;
    const VocabularyTally *right = b;
    if (left->count != right->count) {
        return left->count > right->count ? -1 : 1;
    }
    if (doc_time_before(right->whole, left->whole)) {
        return -1;
    }
    if (doc_time_before(left->whole, right->whole)) {
        return 1;
    }
    return left->first < right->first ? -1 : (left->first > right->first ? 1 : 0);
}

static cJSON *vocabulary_duration_json(const VocabularyTally *tally) {
    cJSON *duration = cJSON_CreateObject();
    if (duration && !(common_json_add(duration, "whole_notes", doc_rational_json(tally->whole)) &&
                      common_json_add(duration, "beats", tally->beats_known ? doc_rational_json(tally->beats) : cJSON_CreateNull()))) {
        cJSON_Delete(duration);
        return NULL;
    }
    return duration;
}

static cJSON *vocabulary_first_json(const VocabularyTally *tally) {
    cJSON *first = cJSON_CreateObject();
    if (first && !(common_json_add(first, "chord_index", cJSON_CreateNumber((double)tally->first)) &&
                   common_json_add(first, "position", cJSON_Duplicate(tally->position, true)))) {
        cJSON_Delete(first);
        return NULL;
    }
    return first;
}

static cJSON *vocabulary_entry_json(const ChainResult *report, long scope, int group, int level,
                                    const VocabularyTally *tally, Rational total) {
    bool in_region = scope >= 0 && (size_t)scope < report->key_region_count;
    double share = total.num > 0 ? ((double)tally->whole.num / (double)tally->whole.den) /
                                       ((double)total.num / (double)total.den)
                                 : 0.0;
    cJSON *object = cJSON_CreateObject();
    if (object &&
        !(common_json_add(object, "scope", cJSON_CreateString(scope == SCOPE_OVERALL ? "overall" : "region")) &&
          common_json_add(object, "key_region", in_region ? cJSON_CreateNumber((double)scope) : cJSON_CreateNull()) &&
          common_json_add(object, "key", in_region ? cJSON_CreateString(report->key_regions[scope].label) : cJSON_CreateNull()) &&
          common_json_add(object, "group", cJSON_CreateString(GROUP_NAMES[group])) &&
          common_json_add(object, "level", cJSON_CreateString(LEVEL_NAMES[level])) &&
          common_json_add(object, "numeral", cJSON_CreateString(tally->name)) &&
          common_json_add(object, "count", cJSON_CreateNumber((double)tally->count)) &&
          common_json_add(object, "runs", cJSON_CreateNumber((double)tally->runs)) &&
          common_json_add(object, "duration", vocabulary_duration_json(tally)) &&
          common_json_add(object, "share", cJSON_CreateNumber(share)) && common_json_add(object, "first", vocabulary_first_json(tally)))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

/* The entries of one scope, level and group, sorted, appended to list. False on allocation failure. */
static bool append_tallies(DocBuild *build, cJSON *list, const ChainResult *report, const VocabularyChord *records, size_t count,
                           long scope, int group, int level, Rational total) {
    VocabularyTally *tallies = calloc(count > 0 ? count : 1, sizeof(VocabularyTally));
    if (!tallies) {
        return false;
    }
    size_t distinct = 0;
    for (size_t i = 0; i < count; i++) {
        const VocabularyChord *record = &records[i];
        if (record->group != group || (scope != SCOPE_OVERALL && record->region != scope)) {
            continue;
        }
        const char *name = level == 0 ? record->label : record->base;
        size_t t = 0;
        while (t < distinct && strcmp(tallies[t].name, name) != 0) {
            t++;
        }
        VocabularyTally *tally = &tallies[t];
        if (t == distinct) {
            *tally = (VocabularyTally){
                name, 0, 0, doc_rational(build, 0, 1), doc_rational(build, 0, 1), true, record->index, record->position};
            distinct++;
        }
        tally->count++;
        tally->runs += record->starts_run ? 1 : 0;
        tally->whole = doc_add(build, tally->whole, record->whole);
        tally->beats_known = tally->beats_known && record->has_beats;
        if (tally->beats_known) {
            tally->beats = doc_add(build, tally->beats, record->beats);
        }
    }
    qsort(tallies, distinct, sizeof(VocabularyTally), compare_tallies);
    bool ok = true;
    for (size_t t = 0; ok && t < distinct; t++) {
        cJSON *entry = vocabulary_entry_json(report, scope, group, level, &tallies[t], total);
        ok = entry && common_json_append(list, entry);
    }
    free(tallies);
    return ok;
}

/* The chord vocabulary: for the whole piece and for each key region, each distinct Roman numeral of the labelled
   chords in three groups (diatonic, applied, mixture), once as the library names it (inversion figure included)
   and once by base numeral (the same numeral in root position). count is the number of chord claims with the
   numeral; runs counts a stretch of adjacent claims with the identical numeral in one key region once. duration is
   their total length, share its fraction of the scope's total labelled chord duration, and first the earliest.
   Entries are sorted by count, then duration, then first occurrence. Chords without a numeral are not counted. */
cJSON *doc_chord_vocabulary_json(DocBuild *build, const ChainResult *report, const cJSON *chords) {
    cJSON *list = cJSON_CreateArray();
    size_t count = 0;
    VocabularyChord *records = list ? vocabulary_chords(build, chords, &count) : NULL;
    if (!records) {
        cJSON_Delete(list);
        return NULL;
    }
    bool ok = true;
    for (long scope = SCOPE_OVERALL; ok && scope < (long)report->key_region_count; scope = scope == SCOPE_OVERALL ? 0 : scope + 1) {
        Rational total = doc_rational(build, 0, 1);
        for (size_t i = 0; i < count; i++) {
            if (scope == SCOPE_OVERALL || records[i].region == scope) {
                total = doc_add(build, total, records[i].whole);
            }
        }
        for (int level = 0; ok && level < 2; level++) {
            for (int group = 0; ok && group < GROUP_COUNT; group++) {
                ok = append_tallies(build, list, report, records, count, scope, group, level, total);
            }
        }
    }
    free(records);
    if (!ok) {
        cJSON_Delete(list);
        return NULL;
    }
    return list;
}
