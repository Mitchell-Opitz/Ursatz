#include "reading/reading_build.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "model/time/meter_map.h"
#include "model/time/time_signature.h"

/* The evidence of the claim being built: performer note ids, each once. */
typedef struct {
    char (*ids)[REALIZATION_ID_MAX];
    size_t count;
    size_t capacity;
} Evidence;

typedef struct {
    const AnalysisChain *chain;
    const ChainResult *result;
    const ImportedScore *score;
    const char (*note_ids)[REALIZATION_ID_MAX];
    size_t note_count;
    Reading *reading;
    unsigned char *seen;   /* note_count flags: a note is in the evidence being built */
    size_t *touched;       /* the notes whose flag is set, to clear them for the next claim */
    size_t touched_count;
    size_t touched_capacity;
    Evidence evidence;
    size_t line_number;
    RealizationResult status;
} Builder;

/* Starts the evidence of the next claim. */
static void evidence_begin(Builder *b) {
    for (size_t i = 0; i < b->touched_count; i++) {
        b->seen[b->touched[i]] = 0;
    }
    b->touched_count = 0;
    b->evidence.count = 0;
}

static bool touch(Builder *b, size_t note) {
    if (b->touched_count == b->touched_capacity) {
        size_t capacity = b->touched_capacity ? b->touched_capacity * 2 : 64;
        size_t *grown = realloc(b->touched, capacity * sizeof(size_t));
        if (!grown) {
            return false;
        }
        b->touched = grown;
        b->touched_capacity = capacity;
    }
    b->touched[b->touched_count++] = note;
    b->seen[note] = 1;
    return true;
}

/* Adds the performer ids of the chain notes `notes` (ImportedScore indices), a note once. False for an index with no id or on memory. */
static bool evidence_notes(Builder *b, const size_t *notes, size_t count) {
    for (size_t i = 0; i < count; i++) {
        size_t note = notes[i];
        if (note >= b->note_count) {
            b->status = REALIZATION_ERR_BAD_VALUE;
            return false;
        }
        if (b->seen[note]) {
            continue;
        }
        if (b->evidence.count == b->evidence.capacity) {
            size_t capacity = b->evidence.capacity ? b->evidence.capacity * 2 : 32;
            char(*grown)[REALIZATION_ID_MAX] = realloc(b->evidence.ids, capacity * REALIZATION_ID_MAX);
            if (!grown) {
                b->status = REALIZATION_ERR_ALLOC;
                return false;
            }
            b->evidence.ids = grown;
            b->evidence.capacity = capacity;
        }
        if (!touch(b, note)) {
            b->status = REALIZATION_ERR_ALLOC;
            return false;
        }
        memcpy(b->evidence.ids[b->evidence.count++], b->note_ids[note], REALIZATION_ID_MAX);
    }
    return true;
}

static void set_text(char *out, size_t size, const char *text) {
    snprintf(out, size, "%s", text ? text : "");
}

static void claim_head(ReadingClaim *claim, const char *kind, size_t number, Rational start, Rational end, size_t detail, const Builder *b) {
    snprintf(claim->id, sizeof(claim->id), "%s-%zu", kind, number);
    claim->start = start;
    claim->end = end;
    claim->detail = detail;
    claim->evidence.ids = b->evidence.ids;
    claim->evidence.count = b->evidence.count;
}

static Rational score_end(const Builder *b) {
    return b->score ? b->score->meter.end_time.value : (Rational){0, 1};
}

static bool fail(Builder *b, RealizationResult status) {
    if (b->status == REALIZATION_OK) {
        b->status = status;
    }
    return false;
}

static bool add(Builder *b, RealizationResult result) {
    return result == REALIZATION_OK || fail(b, result);
}

/* ------------------------------------------------------------------------------------------------ regions */

static bool build_regions(Builder *b) {
    const ChainResult *result = b->result;
    for (size_t r = 0; r < result->key_region_count; r++) {
        const AnalysisKeyRegion *region = &result->key_regions[r];
        evidence_begin(b);
        for (size_t e = 0; e < result->entry_count; e++) {
            const ChainEntry *entry = &result->entries[e];
            if (entry->in_region && entry->region == r && !evidence_notes(b, entry->note_ids, entry->note_id_count)) {
                return false;
            }
        }
        ReadingRegion claim;
        memset(&claim, 0, sizeof(claim));
        claim_head((ReadingClaim *)&claim, "region", r + 1, region->start, region->end, r, b);
        claim.center = region->tonic_pitch_class;
        claim.material = region->material;
        claim.strength = REGION_STRENGTH_ESTABLISHED;
        set_text(claim.label, sizeof(claim.label), region->label);
        if (!add(b, reading_add_region(b->reading, &claim))) {
            return false;
        }
    }
    return true;
}

/* ------------------------------------------------------------------------------------------------ chords */

static bool chord_exists(const ChainEntry *entry) {
    return entry->chord_label && strcmp(entry->chord_label, "no match") != 0;
}

static bool build_chords(Builder *b) {
    const ChainResult *result = b->result;
    size_t number = 0;
    for (size_t e = 0; e < result->entry_count; e++) {
        const ChainEntry *entry = &result->entries[e];
        if (!chord_exists(entry)) {
            continue;
        }
        evidence_begin(b);
        if (!evidence_notes(b, entry->note_ids, entry->note_id_count)) {
            return false;
        }
        ReadingChord claim;
        memset(&claim, 0, sizeof(claim));
        claim_head((ReadingClaim *)&claim, "chord", ++number, entry->time, entry->end, e, b);
        claim.confidence.present = entry->chord_confidence.present;
        claim.confidence.value = entry->chord_confidence.value;
        claim.has_reading = analysis_chain_chord_reading(b->chain, e, &claim.reading) == ANALYSIS_CHAIN_OK;
        if (entry->in_region && entry->roman.labelled) {
            set_text(claim.numeral, sizeof(claim.numeral), entry->roman.numeral);
        }
        set_text(claim.function, sizeof(claim.function), entry->function);
        set_text(claim.label, sizeof(claim.label), entry->chord_label);
        if (!add(b, reading_add_chord(b->reading, &claim))) {
            return false;
        }
    }
    return true;
}

/* ------------------------------------------------------------------------------------------------ cadences */

static bool build_cadences(Builder *b) {
    const ChainResult *result = b->result;
    for (size_t c = 0; c < result->cadence_count; c++) {
        const AnalysisCadence *cadence = &result->cadences[c];
        if (cadence->arrival_first >= result->entry_count || cadence->arrival_last >= result->entry_count) {
            return fail(b, REALIZATION_ERR_BAD_VALUE);
        }
        evidence_begin(b);
        for (size_t e = cadence->arrival_first; e <= cadence->arrival_last; e++) {
            if (!evidence_notes(b, result->entries[e].note_ids, result->entries[e].note_id_count)) {
                return false;
            }
        }
        ReadingCadence claim;
        memset(&claim, 0, sizeof(claim));
        Rational start = result->entries[cadence->arrival_first].time;
        Rational end = cadence->arrival_last + 1 < result->entry_count ? result->entries[cadence->arrival_last + 1].time : score_end(b);
        if (rational_cmp(end, start) < 0) {
            end = start;
        }
        claim_head((ReadingClaim *)&claim, "cadence", c + 1, start, end, c, b);
        claim.arrival = start;
        claim.type = CADENCE_TYPE_UNKNOWN;
        cadence_type_from_name(cadence->type, &claim.type);
        if (cadence->has_alternative && cadence->alternative.type) {
            CadenceType alternative;
            if (cadence_type_from_name(cadence->alternative.type, &alternative)) {
                claim.alternatives[claim.alternative_count++] = alternative;
            }
        }
        if (!add(b, reading_add_cadence(b->reading, &claim))) {
            return false;
        }
    }
    return true;
}

/* ------------------------------------------------------------------------------------------------ groups */

static bool has_trait(const AnalysisTheme *theme, const char *name) {
    for (size_t i = 0; i < theme->trait_count; i++) {
        if (theme->traits[i] && strcmp(theme->traits[i], name) == 0) {
            return true;
        }
    }
    return false;
}

static bool in_span(Rational at, Rational start, Rational end) {
    return rational_cmp(at, start) >= 0 && rational_cmp(at, end) < 0;
}

/* The index of the section whose span holds `at`; the last section when none does (the sections tile the phrases). */
static size_t section_holding(const ChainResult *result, Rational at) {
    for (size_t s = 0; s < result->section_count; s++) {
        if (in_span(at, result->sections[s].start, result->sections[s].end)) {
            return s;
        }
    }
    return result->section_count > 0 ? result->section_count - 1 : READING_NO_PARENT;
}

static bool add_group(Builder *b, size_t *number, ReadingGroup *claim, ReadingGroupKind kind, size_t parent, Rational start, Rational end, size_t detail) {
    claim_head((ReadingClaim *)claim, "group", ++*number, start, end, detail, b);
    claim->kind = kind;
    claim->parent = parent;
    return add(b, reading_add_group(b->reading, claim));
}

static bool build_groups(Builder *b) {
    const ChainResult *result = b->result;
    size_t number = 0;
    size_t *phrase_parent = result->phrase_count ? calloc(result->phrase_count, sizeof(size_t)) : NULL;
    if (result->phrase_count && !phrase_parent) {
        return fail(b, REALIZATION_ERR_ALLOC);
    }
    bool ok = true;
    for (size_t s = 0; ok && s < result->section_count; s++) {
        const AnalysisSection *section = &result->sections[s];
        evidence_begin(b);
        ReadingGroup claim;
        memset(&claim, 0, sizeof(claim));
        set_text(claim.label, sizeof(claim.label), section->label);
        claim.first_measure = section->first_measure;
        claim.last_measure = section->last_measure;
        claim.phrase_count = section->phrase_count;
        ok = add_group(b, &number, &claim, READING_GROUP_SECTION, READING_NO_PARENT, section->start, section->end, s);
    }
    size_t sections = result->section_count;
    for (size_t t = 0; ok && t < result->theme_count; t++) {
        const AnalysisTheme *theme = &result->themes[t];
        evidence_begin(b);
        ok = evidence_notes(b, theme->note_ids, theme->note_id_count);
        if (!ok) {
            break;
        }
        ReadingGroup claim;
        memset(&claim, 0, sizeof(claim));
        snprintf(claim.label, sizeof(claim.label), "theme-%d", theme->index + 1);
        set_text(claim.form, sizeof(claim.form), theme->form);
        set_text(claim.beginning, sizeof(claim.beginning), theme->beginning);
        set_text(claim.ending, sizeof(claim.ending), theme->ending);
        set_text(claim.commonality, sizeof(claim.commonality), theme->commonality);
        claim.first_measure = theme->first_measure;
        claim.last_measure = theme->last_measure;
        claim.phrase_count = theme->phrase_count;
        for (size_t p = 0; p < 2; p++) {
            if (theme->parts[p].has_cadence && theme->parts[p].cadence < result->cadence_count) {
                CadenceType type;
                if (cadence_type_from_name(result->cadences[theme->parts[p].cadence].type, &type)) {
                    claim.part_cadences[claim.part_cadence_count++] = type;
                }
            }
        }
        claim.archetypal = theme->archetypal;
        claim.fragmentation = has_trait(theme, "fragmentation");
        size_t parent = section_holding(result, theme->start);
        ok = add_group(b, &number, &claim, READING_GROUP_THEME, parent, theme->start, theme->end, t);
        for (size_t p = theme->first_phrase; ok && p < theme->first_phrase + theme->phrase_count && p < result->phrase_count; p++) {
            phrase_parent[p] = sections + t;
        }
    }
    /* A phrase that no theme holds sits under its section; the others under their theme. */
    for (size_t p = 0; ok && p < result->phrase_count; p++) {
        bool in_theme = false;
        for (size_t t = 0; t < result->theme_count; t++) {
            if (p >= result->themes[t].first_phrase && p < result->themes[t].first_phrase + result->themes[t].phrase_count) {
                in_theme = true;
            }
        }
        if (!in_theme) {
            phrase_parent[p] = section_holding(result, result->phrases[p].start);
        }
    }
    for (size_t p = 0; ok && p < result->phrase_count; p++) {
        const AnalysisPhrase *phrase = &result->phrases[p];
        evidence_begin(b);
        ReadingGroup claim;
        memset(&claim, 0, sizeof(claim));
        set_text(claim.label, sizeof(claim.label), phrase->role);
        claim.first_measure = phrase->first_measure;
        claim.last_measure = phrase->last_measure;
        claim.phrase_count = 1;
        if (phrase->has_cadence && phrase->cadence < result->cadence_count) {
            CadenceType type;
            if (cadence_type_from_name(result->cadences[phrase->cadence].type, &type)) {
                claim.part_cadences[claim.part_cadence_count++] = type;
            }
        }
        ok = add_group(b, &number, &claim, READING_GROUP_PHRASE, phrase_parent[p], phrase->start, phrase->end, p);
    }
    for (size_t i = 0; ok && i < result->subphrase_count; i++) {
        const AnalysisSubphrase *subphrase = &result->subphrases[i];
        evidence_begin(b);
        ok = evidence_notes(b, subphrase->note_ids, subphrase->note_id_count);
        if (!ok) {
            break;
        }
        ReadingGroup claim;
        memset(&claim, 0, sizeof(claim));
        snprintf(claim.label, sizeof(claim.label), "subphrase-%d", subphrase->index + 1);
        claim.first_measure = subphrase->first_measure;
        claim.last_measure = subphrase->last_measure;
        claim.phrase_count = 1;
        size_t parent = subphrase->phrase < result->phrase_count ? phrase_parent[subphrase->phrase] : READING_NO_PARENT;
        ok = add_group(b, &number, &claim, READING_GROUP_SUBPHRASE, parent, subphrase->start, subphrase->end, i);
    }
    for (size_t i = 0; ok && i < result->idea_count; i++) {
        const AnalysisIdea *idea = &result->ideas[i];
        evidence_begin(b);
        ok = evidence_notes(b, idea->note_ids, idea->note_id_count);
        if (!ok) {
            break;
        }
        ReadingGroup claim;
        memset(&claim, 0, sizeof(claim));
        set_text(claim.label, sizeof(claim.label), idea->label);
        claim.first_measure = idea->first_measure;
        claim.last_measure = idea->last_measure;
        claim.phrase_count = 1;
        size_t parent = idea->phrase < result->phrase_count ? phrase_parent[idea->phrase] : READING_NO_PARENT;
        ok = add_group(b, &number, &claim, READING_GROUP_IDEA, parent, idea->start, idea->end, i);
    }
    free(phrase_parent);
    return ok;
}

/* ------------------------------------------------------------------------------------------------ motives */

static bool build_motive_occurrences(Builder *b) {
    const ChainResult *result = b->result;
    size_t number = 0;
    for (size_t m = 0; m < result->motive_count; m++) {
        const AnalysisMotive *motive = &result->motives[m];
        for (size_t o = 0; o < motive->occurrence_count; o++) {
            const AnalysisMotiveOccurrence *occurrence = &motive->occurrences[o];
            evidence_begin(b);
            if (!evidence_notes(b, occurrence->note_ids, occurrence->note_id_count)) {
                return false;
            }
            ReadingMotiveOccurrence claim;
            memset(&claim, 0, sizeof(claim));
            claim_head((ReadingClaim *)&claim, "motive", ++number, occurrence->start, occurrence->end, o, b);
            claim.motive = m;
            set_text(claim.kind, sizeof(claim.kind), occurrence->kind);
            if (!add(b, reading_add_motive_occurrence(b->reading, &claim))) {
                return false;
            }
        }
    }
    return true;
}

/* ------------------------------------------------------------------------------------------------ meters */

static bool build_meters(Builder *b) {
    const MeterMap *map = b->score ? b->score->meter.meter_map : NULL;
    size_t count = map ? meter_map_region_count(map) : 0;
    Rational end_of_score = score_end(b);
    for (size_t i = 0; i < count; i++) {
        const MeterMapRegion *region = meter_map_region(map, i);
        const TimeSignature *signature = meter_map_region_time_signature(region);
        Rational start = meter_map_region_start(region).value;
        MusicalTime region_end;
        Rational end = meter_map_region_end(region, &region_end) ? region_end.value : end_of_score;
        if (rational_cmp(end, start) < 0) {
            end = start;
        }
        evidence_begin(b);
        ReadingMeter claim;
        memset(&claim, 0, sizeof(claim));
        claim_head((ReadingClaim *)&claim, "meter", i + 1, start, end, READING_NO_DETAIL, b);
        snprintf(claim.time_signature, sizeof(claim.time_signature), "%u/%u", time_signature_top_number(signature), time_signature_bottom_number(signature));
        if (!add(b, reading_add_meter(b->reading, &claim))) {
            return false;
        }
    }
    return true;
}

/* ------------------------------------------------------------------------------------------------ lines */

static bool build_outer_line(Builder *b, bool soprano) {
    const ChainResult *result = b->result;
    evidence_begin(b);
    bool any = false;
    Rational start = {0, 1};
    Rational end = {0, 1};
    for (size_t i = 0; i < result->outer_line_count; i++) {
        const AnalysisOuterLineEntry *entry = &result->outer_lines[i];
        const AnalysisOuterNote *note = soprano ? &entry->soprano : &entry->bass;
        if (note->status != ANALYSIS_OUTER_PRESENT || note->held) {
            continue;
        }
        if (!any) {
            start = entry->time;
            any = true;
        }
        end = entry->end;
        if (!evidence_notes(b, &note->note_id, 1)) {
            return false;
        }
    }
    if (!any) {
        return true;
    }
    ReadingLine claim;
    memset(&claim, 0, sizeof(claim));
    claim_head((ReadingClaim *)&claim, "line", ++b->line_number, start, end, soprano ? 0 : 1, b);
    claim.role = soprano ? LINE_ROLE_MELODY : LINE_ROLE_BASS;
    return add(b, reading_add_line(b->reading, &claim));
}

/* ------------------------------------------------------------------------------------------------ voice faults */

static bool build_voice_faults(Builder *b) {
    ChainVoiceFault *faults = NULL;
    size_t count = 0;
    if (!chain_result_voice_faults(b->chain, &faults, &count)) {
        return true; /* a chain that ran on no score has none */
    }
    bool ok = true;
    for (size_t i = 0; ok && i < count; i++) {
        evidence_begin(b);
        ok = evidence_notes(b, faults[i].note_ids, faults[i].note_id_count);
        if (!ok) {
            break;
        }
        /* The span the fault's notes cover. */
        Rational start = {0, 1};
        Rational end = {0, 1};
        bool first = true;
        for (size_t n = 0; b->score && n < faults[i].note_id_count; n++) {
            size_t note = faults[i].note_ids[n];
            if (note >= b->score->count) {
                continue;
            }
            Rational onset = temporal_anchor_position(note_event_temporal_anchor(b->score->events[note]))->value;
            Rational stop = onset;
            Rational length = note_event_duration(b->score->events[note])->value;
            rational_add(onset, length, &stop);
            if (first || rational_cmp(onset, start) < 0) {
                start = onset;
            }
            if (first || rational_cmp(stop, end) > 0) {
                end = stop;
            }
            first = false;
        }
        ReadingVoiceFault claim;
        memset(&claim, 0, sizeof(claim));
        claim_head((ReadingClaim *)&claim, "fault", i + 1, start, end, i, b);
        set_text(claim.rule, sizeof(claim.rule), faults[i].rule);
        claim.hard = faults[i].hard;
        claim.measure = faults[i].measure;
        ok = add(b, reading_add_voice_fault(b->reading, &claim));
    }
    chain_voice_faults_free(faults, count);
    return ok;
}

RealizationResult reading_build(const AnalysisChain *chain, const ReadingSource *source, const ReadingTarget *target,
                                const char (*note_ids)[REALIZATION_ID_MAX], size_t note_count, Reading **out) {
    if (out) {
        *out = NULL;
    }
    if (!chain || !source || !target || !out || (note_count > 0 && !note_ids)) {
        return REALIZATION_ERR_NULL;
    }
    Builder builder;
    memset(&builder, 0, sizeof(builder));
    builder.chain = chain;
    builder.result = analysis_chain_result(chain);
    builder.score = analysis_chain_score(chain);
    builder.note_ids = note_ids;
    builder.note_count = note_count;
    builder.status = reading_create(source, target, &builder.reading);
    if (builder.status != REALIZATION_OK) {
        return builder.status;
    }
    builder.seen = calloc(note_count ? note_count : 1, 1);
    if (!builder.seen) {
        reading_destroy(builder.reading);
        return REALIZATION_ERR_ALLOC;
    }
    bool ok = build_regions(&builder) && build_chords(&builder) && build_cadences(&builder) && build_groups(&builder) &&
              build_motive_occurrences(&builder) && build_meters(&builder) && build_outer_line(&builder, true) &&
              build_outer_line(&builder, false) && build_voice_faults(&builder);
    free(builder.seen);
    free(builder.touched);
    free(builder.evidence.ids);
    if (!ok) {
        reading_destroy(builder.reading);
        return builder.status != REALIZATION_OK ? builder.status : REALIZATION_ERR_BAD_VALUE;
    }
    reading_set_detail(builder.reading, chain);
    *out = builder.reading;
    return REALIZATION_OK;
}
