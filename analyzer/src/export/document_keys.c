/* The keys areas of the analysis document: key regions, the home key, key changes (tonicizations, modulations, pivot chords) and
   the notes on the analysis (a notated key signature the detected keys do not fit). */

#include "export/analysis_document_internal.h"

/* How the home key is chosen: the library offers no determination of its own, so it is the key with the
   greatest total duration over all its regions, a tie going to the key that appears first. */
static const char HOME_KEY_RULE[] = "longest_total_duration";

/* The imported notes that start in [start, end), as indices; *count may be 0, and the result is then still a
   valid pointer. NULL on allocation failure. */
static size_t *notes_starting_in(DocBuild *build, const ImportedScore *notes, Rational start, Rational end, size_t *count) {
    size_t *ids = malloc(sizeof(size_t) * (notes->count > 0 ? notes->count : 1));
    if (!ids) {
        return NULL;
    }
    *count = 0;
    for (size_t i = 0; i < notes->count; i++) {
        Rational onset = doc_note_onset(notes->events[i]);
        if (!doc_time_before(onset, start) && doc_time_before(onset, end)) {
            ids[(*count)++] = i;
        }
    }
    return ids;
}

/* A claim over one key region's span: the notes that start in it, its label, and the library's uncertainty
   for the region (in [0, 1], higher is LESS certain, unlike a chord's confidence). */
static cJSON *region_span_claim(DocBuild *build, const ImportedScore *notes, const AnalysisKeyRegion *region, const char *label,
                                cJSON *uncertainty) {
    size_t id_count = 0;
    size_t *ids = notes_starting_in(build, notes, doc_region_start(build, region), doc_region_end(build, region), &id_count);
    if (!ids) {
        cJSON_Delete(uncertainty);
        return NULL;
    }
    cJSON *claim = doc_claim_json_with(build, doc_library_framework_name(), notes, ids, id_count, doc_region_start(build, region),
                                   doc_region_end(build, region), label, uncertainty);
    free(ids);
    return claim;
}

static const char *region_source_name(AnalysisRegionSource source) {
    return source == ANALYSIS_REGION_SOURCE_KEY_SIGNATURE ? "key_signature" : "profile";
}

/* One key region: the shared claim (its span runs barline to barline) and the key's fields. */
static cJSON *key_region_json(DocBuild *build, const ImportedScore *notes, const AnalysisKeyRegion *region, size_t index) {
    cJSON *claim = region_span_claim(build, notes, region, region->label, cJSON_CreateNumber(region->uncertainty));
    if (!(claim && json_add(claim, "index", cJSON_CreateNumber((double)index)) &&
          json_add(claim, "tonic", cJSON_CreateString(pitch_class_name(region->tonic_pitch_class, region->prefer_flats))) &&
          json_add(claim, "material", cJSON_CreateString(region->material->id)) &&
          json_add(claim, "measures", doc_measures_json(region->first_measure, region->last_measure)) &&
          json_add(claim, "source", cJSON_CreateString(region_source_name(region->source))))) {
        cJSON_Delete(claim);
        return NULL;
    }
    return claim;
}

/* The indices of every region in the home key, in time order. */
static cJSON *home_region_indices_json(const ChainResult *report) {
    const AnalysisKeyRegion *home = &report->key_regions[report->home_region];
    cJSON *indices = cJSON_CreateArray();
    for (size_t i = 0; indices && i < report->key_region_count; i++) {
        const AnalysisKeyRegion *region = &report->key_regions[i];
        if (region->tonic_pitch_class == home->tonic_pitch_class && region->material == home->material &&
            !json_append(indices, cJSON_CreateNumber((double)i))) {
            cJSON_Delete(indices);
            return NULL;
        }
    }
    return indices;
}

static cJSON *home_key_json(DocBuild *build, const ChainResult *report) {
    const AnalysisKeyRegion *home = &report->key_regions[report->home_region];
    cJSON *object = cJSON_CreateObject();
    if (object &&
        !(json_add(object, "rule", cJSON_CreateString(HOME_KEY_RULE)) &&
          json_add(object, "label", cJSON_CreateString(home->label)) &&
          json_add(object, "region_indices", home_region_indices_json(report)) &&
          json_add(object, "duration",
                   doc_rational_json(doc_rational(build, report->home_duration.num, report->home_duration.den))))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

/* The keys section: the key regions in time order, the home key derived from them, and the indices of the
   opening and closing regions. With no key found the regions are empty and the rest null. */
cJSON *doc_keys_json(DocBuild *build, const ImportedScore *notes, const ChainResult *report) {
    cJSON *keys = cJSON_CreateObject();
    cJSON *regions = cJSON_CreateArray();
    if (!keys || !regions) {
        cJSON_Delete(keys);
        cJSON_Delete(regions);
        return NULL;
    }
    for (size_t r = 0; r < reading_region_count(build->reading); r++) {
        size_t i = reading_region_at(build->reading, r)->detail;
        if (!json_append(regions, key_region_json(build, notes, &report->key_regions[i], i))) {
            cJSON_Delete(keys);
            cJSON_Delete(regions);
            return NULL;
        }
    }
    bool found = report->key_region_count > 0;
    if (!(json_add(keys, "regions", regions) && json_add(keys, "home_key", found ? home_key_json(build, report) : cJSON_CreateNull()) &&
          json_add(keys, "opening_region", found ? cJSON_CreateNumber(0) : cJSON_CreateNull()) &&
          json_add(keys, "closing_region",
              found ? cJSON_CreateNumber((double)(report->key_region_count - 1)) : cJSON_CreateNull()))) {
        cJSON_Delete(keys);
        return NULL;
    }
    return keys;
}

static const char *extent_name(AnalysisKeyChangeExtent extent) {
    switch (extent) {
        case ANALYSIS_KEY_CHANGE_EXTENDED_TONICIZATION:
            return "extended-tonicization";
        case ANALYSIS_KEY_CHANGE_WEAK_MODULATION:
            return "weak-modulation";
        case ANALYSIS_KEY_CHANGE_MODULATION:
            return "modulation";
        default:
            return "tonicization";
    }
}

/* A change the library leaves as a modulation or weak modulation: a lasting new key. */
static bool change_is_modulation(const AnalysisKeyChange *change) {
    return !doc_change_is_tonicization(change);
}

/* "<from> to <to>" for the labels of two regions. */
static char *join_labels(const char *prefix, const char *middle, const char *label) {
    size_t size = strlen(prefix) + strlen(middle) + strlen(label) + 1;
    char *text = malloc(size);
    if (text) {
        snprintf(text, size, "%s%s%s", prefix, middle, label);
    }
    return text;
}

/* Fields every key change shares: its extent, the two regions and their keys. */
static bool add_change_fields(cJSON *claim, const ChainResult *report, const AnalysisKeyChange *change,
                              const char *to_name) {
    const AnalysisKeyRegion *from = &report->key_regions[change->from_region];
    const AnalysisKeyRegion *to = &report->key_regions[change->to_region];
    return json_add(claim, "extent", cJSON_CreateString(extent_name(change->extent))) &&
           json_add(claim, "cadence_confirmed", cJSON_CreateBool(change->cadence_confirmed)) &&
           json_add(claim, "from_region", cJSON_CreateNumber((double)change->from_region)) &&
           json_add(claim, to_name, cJSON_CreateNumber((double)change->to_region)) &&
           json_add(claim, "from_key", cJSON_CreateString(from->label)) &&
           json_add(claim, "to_key", cJSON_CreateString(to->label));
}

/* One claim per tonicization: a brief excursion to another key that returns to the one before it. Its span is
   the excursion (the region the library puts the new key in), its target the key of that region. The change
   back out of the excursion is not listed again. */
cJSON *doc_tonicizations_json(DocBuild *build, const ImportedScore *notes, const ChainResult *report) {
    cJSON *list = cJSON_CreateArray();
    if (!list) {
        return NULL;
    }
    for (size_t i = 0; i < report->key_change_count; i++) {
        const AnalysisKeyChange *change = &report->key_changes[i];
        if (!doc_change_is_tonicization(change) || change->is_return) {
            continue;
        }
        const AnalysisKeyRegion *target = &report->key_regions[change->to_region];
        char *label = join_labels("tonicization of ", "", target->label);
        cJSON *claim = label ? region_span_claim(build, notes, target, label, cJSON_CreateNull()) : NULL;
        free(label);
        if (!(claim && add_change_fields(claim, report, change, "target_region"))) {
            cJSON_Delete(claim);
            cJSON_Delete(list);
            return NULL;
        }
        if (!json_append(list, claim)) {
            cJSON_Delete(list);
            return NULL;
        }
    }
    return list;
}

/* Whether a change's pivot chord was identified, so it appears in pivot_chords. */
static bool change_has_pivot_reading(const AnalysisKeyChange *change) {
    return change->has_pivot && change->pivot.label != NULL;
}

/* One claim per modulation (and weak modulation): a lasting new key. Its span is the passage in the new key,
   so location.start is where the new key arrives. type is "pivot" when the library found a pivot chord and
   "direct" when it did not; pivot_chord_index points into pivot_chords. is_return marks the change back to the
   key before an excursion. */
cJSON *doc_modulations_json(DocBuild *build, const ImportedScore *notes, const ChainResult *report) {
    cJSON *list = cJSON_CreateArray();
    if (!list) {
        return NULL;
    }
    size_t pivot_index = 0;
    for (size_t i = 0; i < report->key_change_count; i++) {
        const AnalysisKeyChange *change = &report->key_changes[i];
        if (!change_is_modulation(change)) {
            continue;
        }
        const AnalysisKeyRegion *from = &report->key_regions[change->from_region];
        const AnalysisKeyRegion *to = &report->key_regions[change->to_region];
        char *label = join_labels(from->label, " to ", to->label);
        cJSON *claim = label ? region_span_claim(build, notes, to, label, cJSON_CreateNull()) : NULL;
        free(label);
        bool has_reading = change_has_pivot_reading(change);
        if (!(claim && add_change_fields(claim, report, change, "to_region") &&
              json_add(claim, "type", cJSON_CreateString(change->has_pivot ? "pivot" : "direct")) &&
              json_add(claim, "is_return", cJSON_CreateBool(change->is_return)) &&
              json_add(claim, "pivot_chord_index",
                  has_reading ? cJSON_CreateNumber((double)pivot_index) : cJSON_CreateNull()))) {
            cJSON_Delete(claim);
            cJSON_Delete(list);
            return NULL;
        }
        pivot_index += has_reading ? 1 : 0;
        if (!json_append(list, claim)) {
            cJSON_Delete(list);
            return NULL;
        }
    }
    return list;
}

/* One claim per pivot chord: the chord that is diatonic in both the old and the new key at a modulation,
   identified without a Roman numeral (root, quality, bass, inversion), read in the old key. The claim is the
   chord's segment. */
cJSON *doc_pivot_chords_json(DocBuild *build, const ImportedScore *notes, const ChainResult *report) {
    cJSON *list = cJSON_CreateArray();
    if (!list) {
        return NULL;
    }
    size_t modulation_index = 0;
    for (size_t i = 0; i < report->key_change_count; i++) {
        const AnalysisKeyChange *change = &report->key_changes[i];
        if (!change_is_modulation(change)) {
            continue;
        }
        size_t this_modulation = modulation_index++;
        if (!change_has_pivot_reading(change) || change->pivot_entry >= report->entry_count) {
            continue;
        }
        const ChainEntry *entry = &report->entries[change->pivot_entry];
        const AnalysisChordReading *pivot = &change->pivot;
        char *label = join_labels(pivot->root, " ", pivot->quality);
        cJSON *claim =
            label ? doc_claim_json_at(build, doc_library_framework_name(), notes, entry->note_ids, entry->note_id_count,
                                  doc_rational(build, entry->time.num, entry->time.den),
                                  doc_rational(build, entry->end.num, entry->end.den), label, pivot->confidence)
                  : NULL;
        free(label);
        if (!(claim && json_add(claim, "modulation_index", cJSON_CreateNumber((double)this_modulation)) &&
              json_add(claim, "chord_index", cJSON_CreateNumber((double)change->pivot_entry)) &&
              json_add(claim, "root", cJSON_CreateString(pivot->root)) &&
              json_add(claim, "quality", cJSON_CreateString(pivot->quality)) &&
              json_add(claim, "bass", cJSON_CreateString(pivot->bass)) &&
              json_add(claim, "inversion",
                  pivot->inversion >= 0 ? cJSON_CreateNumber(pivot->inversion) : cJSON_CreateNull()) &&
              json_add(claim, "from_key", cJSON_CreateString(report->key_regions[change->from_region].label)) &&
              json_add(claim, "to_key", cJSON_CreateString(report->key_regions[change->to_region].label)))) {
            cJSON_Delete(claim);
            cJSON_Delete(list);
            return NULL;
        }
        if (!json_append(list, claim)) {
            cJSON_Delete(list);
            return NULL;
        }
    }
    return list;
}

/* Whether a key is the major key of a notated signature or its relative minor. */
static bool key_fits_signature(const AnalysisKeyRegion *region, int sharps_flats) {
    int major = chain_key_signature_major_tonic(sharps_flats);
    CommonPracticeKeyMode mode;
    if (!common_practice_key_mode_of(region->material, &mode)) {
        return false;
    }
    return mode == COMMON_PRACTICE_KEY_MAJOR ? region->tonic_pitch_class == major
                                             : region->tonic_pitch_class == (major + 9) % 12;
}

/* The region in effect at time: the one whose span holds it, the first for a time before the piece's first
   region; NULL for a time at or after the end of the last. */
static const AnalysisKeyRegion *region_at(DocBuild *build, const ChainResult *report, Rational time, size_t *index) {
    for (size_t i = 0; i < report->key_region_count; i++) {
        if (doc_time_before(time, doc_region_end(build, &report->key_regions[i]))) {
            *index = i;
            return &report->key_regions[i];
        }
    }
    return NULL;
}

/* One disagreement between a notated signature and the detected keys. signature_index is -1 for a note about
   the home key, which fits no signature at all. */
static cJSON *disagreement_json(const DocBuild *build, const char *scope, long signature_index, const MidiKeySignatureEvent *event,
                                size_t region_index, const AnalysisKeyRegion *region) {
    char notated[96] = "";
    char message[256];
    if (event) {
        int major = chain_key_signature_major_tonic(event->sharps_flats);
        char *major_label = chain_key_label(major, build->materials.major);
        char *minor_label = chain_key_label((major + 9) % 12, build->materials.natural_minor);
        if (major_label && minor_label) {
            snprintf(notated, sizeof(notated), "%s or %s", major_label, minor_label);
        }
        free(major_label);
        free(minor_label);
        snprintf(message, sizeof(message),
                 "The notated key signature allows %s, but the key detected there is %s. Both are reported; "
                 "neither is preferred.",
                 notated, region->label);
    } else {
        snprintf(message, sizeof(message),
                 "The home key %s is not the major key or relative minor of any notated key signature. Both are "
                 "reported; neither is preferred.",
                 region->label);
    }
    cJSON *note = cJSON_CreateObject();
    if (note && !(json_add(note, "kind", cJSON_CreateString("key_signature_disagreement")) &&
                  json_add(note, "scope", cJSON_CreateString(scope)) &&
                  json_add(note, "signature_index",
                      signature_index >= 0 ? cJSON_CreateNumber((double)signature_index) : cJSON_CreateNull()) &&
                  json_add(note, "region_index", cJSON_CreateNumber((double)region_index)) &&
                  json_add(note, "notated", event ? cJSON_CreateString(notated) : cJSON_CreateNull()) &&
                  json_add(note, "detected", cJSON_CreateString(region->label)) &&
                  json_add(note, "message", cJSON_CreateString(message)))) {
        cJSON_Delete(note);
        return NULL;
    }
    return note;
}

/* Notes on the analysis. A notated key signature that the detected keys do not fit is surfaced here, never
   resolved silently: where a signature starts, the region in effect there should be its major key or relative
   minor (the library restricts it to those, so a mismatch means the restriction did not hold); and the home
   key should be the major key or relative minor of at least one signature. Inside a signature any key may
   come and go, so the other regions are not compared. */
cJSON *doc_notes_json(DocBuild *build, const ImportedScore *notes, const ChainResult *report) {
    cJSON *list = cJSON_CreateArray();
    if (!list) {
        return NULL;
    }
    size_t signature_count = notes->key_signature_events.count;
    if (report->key_region_count == 0 || signature_count == 0) {
        return list;
    }
    bool home_fits_any = false;
    const AnalysisKeyRegion *home = &report->key_regions[report->home_region];
    for (size_t i = 0; i < signature_count; i++) {
        const MidiKeySignatureEvent *event = &notes->key_signature_events.events[i];
        home_fits_any = home_fits_any || key_fits_signature(home, event->sharps_flats);
        if (i > 0 && event->sharps_flats == notes->key_signature_events.events[i - 1].sharps_flats) {
            continue; /* the same signature repeated is not a new one (the library does not treat it as a boundary) */
        }
        size_t region_index = 0;
        const AnalysisKeyRegion *region = region_at(build, report, event->position.value, &region_index);
        if (region && !key_fits_signature(region, event->sharps_flats) &&
            !json_append(list, disagreement_json(build, "region_at_signature", (long)i, event, region_index, region))) {
            cJSON_Delete(list);
            return NULL;
        }
    }
    if (!home_fits_any && !json_append(list, disagreement_json(build, "home_key", -1, NULL, report->home_region, home))) {
        cJSON_Delete(list);
        return NULL;
    }
    return list;
}
