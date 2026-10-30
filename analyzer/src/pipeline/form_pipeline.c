#include "pipeline/form_pipeline.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <limits.h>

#include "standard-form/continuation_trait.h"
#include "standard-form/expansion_placement.h"
#include "standard-form/external_expansion_kind.h"
#include "standard-form/internal_expansion_technique.h"
#include "standard-form/phrase_length_change.h"
#include "standard-form/hybrid_rules.h"
#include "standard-form/idea_label.h"
#include "standard-form/phrase_beginning.h"
#include "standard-form/phrase_ending.h"
#include "standard-form/phrase_level_form.h"
#include "pitch/pitch_numeric_value.h"
#include "time/meter_map.h"
#include "time/musical_time.h"

#define CADENCE_ID_PREFIX "chain-"

bool form_stage_read(FormStage *stage, const ImportedNotes *notes, const ChainInput *input,
                     const AnalysisChain *library) {
    memset(stage, 0, sizeof(*stage));
    const StandardFormPhrases *phrases = analysis_chain_phrases(library);
    const StandardFormSegmentation *ideas = analysis_chain_ideas(library);
    const StandardFormThemes *themes = analysis_chain_themes(library);
    const StandardFormExpansions *expansions = analysis_chain_expansions(library);
    const ChainSegments *segments = analysis_chain_segments(library);
    const OuterVoiceLines *lines = analysis_chain_outer_voices(library);
    const HarmonyCadences *cadences = analysis_chain_cadences(library);
    if (!notes->meter.meter_map || !phrases || !ideas || !themes || !expansions || !segments || !lines || !cadences ||
        phrases->count == 0) {
        return true; /* nothing to read phrases from */
    }
    /* The chain keeps the form input to itself; its builder gives it again, for the notes the results index, the cadence
       ids and the meter. */
    ChainInputStatus built =
        chain_build_form_input(&input->set, segments, lines, 1, INT_MAX, cadences, &stage->built);
    if (built != CHAIN_INPUT_OK) {
        chain_form_input_destroy(&stage->built);
        memset(&stage->built, 0, sizeof(stage->built));
        return built != CHAIN_INPUT_ERR_ALLOC;
    }
    stage->have_input = true;
    size_t count = stage->built.input.note_count;
    stage->note_indices = malloc(sizeof(size_t) * (count > 0 ? count : 1));
    if (!stage->note_indices) {
        return false;
    }
    for (size_t n = 0; n < count; n++) {
        size_t index = chain_input_import_index(input, stage->built.input.notes[n].note_id);
        if (index == (size_t)-1) {
            return false;
        }
        stage->note_indices[stage->note_index_count++] = index;
    }
    stage->phrases = *phrases;
    stage->segmentation = *ideas;
    stage->themes = *themes;
    stage->expansions = *expansions;
    return true;
}

long form_stage_phrase_cadence(const FormStage *stage, size_t phrase) {
    if (phrase >= stage->phrases.count) {
        return -1;
    }
    int index = stage->phrases.items[phrase].claim.cadence_index;
    if (index < 0 || (size_t)index >= stage->built.input.cadence_count) {
        return -1;
    }
    /* The input sorts its cadences by arrival; the id names the harmony stage's cadence. */
    const char *id = stage->built.input.cadences[index].id;
    return strncmp(id, CADENCE_ID_PREFIX, strlen(CADENCE_ID_PREFIX)) == 0 ? atol(id + strlen(CADENCE_ID_PREFIX)) : -1;
}

static const char *signal_name(int signal) {
    switch (signal) {
        case STANDARD_FORM_SIGNAL_CADENCE:
            return "cadence";
        case STANDARD_FORM_SIGNAL_DISCONTINUITY:
            return "discontinuity";
        default:
            return "none";
    }
}

/* a/b < c/d for positive denominators. */
static bool rational_less(long a, unsigned long b, long c, unsigned long d) {
    return (long long)a * (long long)d < (long long)c * (long long)b;
}

/* The span from the start of measure `first` to the end of measure `last`. False if the meter does not place them. */
static bool measures_span(const MeterMap *map, int first, int last, long *start_numerator,
                          unsigned long *start_denominator, long *end_numerator, unsigned long *end_denominator) {
    TimeSpan *from = NULL;
    TimeSpan *to = NULL;
    bool ok = meter_map_measure_span(map, first, &from) == METER_MAP_OK &&
              meter_map_measure_span(map, last, &to) == METER_MAP_OK;
    if (ok) {
        *start_numerator = musical_time_numerator(time_span_start(from));
        *start_denominator = musical_time_denominator(time_span_start(from));
        *end_numerator = musical_time_numerator(time_span_end(to));
        *end_denominator = musical_time_denominator(time_span_end(to));
    }
    time_span_destroy(from);
    time_span_destroy(to);
    return ok;
}

/* Clips [start, end) to the phrase's span: an idea ends with its last measure, a phrase with its cadence's arrival chord
   (snapped to the beat), and the pickup lies before the first barline. */
static void clip_to_phrase(const AnalysisPhrase *phrase, long *start_numerator, unsigned long *start_denominator,
                           long *end_numerator, unsigned long *end_denominator) {
    if (rational_less(*start_numerator, *start_denominator, phrase->start_numerator, phrase->start_denominator)) {
        *start_numerator = phrase->start_numerator;
        *start_denominator = phrase->start_denominator;
    }
    if (rational_less(phrase->end_numerator, phrase->end_denominator, *end_numerator, *end_denominator)) {
        *end_numerator = phrase->end_numerator;
        *end_denominator = phrase->end_denominator;
    }
}

/* The ImportedNotes indices of the input's soprano notes [first, end), owned. NULL with *count 0 when empty. */
static size_t *note_ids_of(const FormStage *stage, int first, int end, size_t *count) {
    *count = 0;
    if (first < 0 || end <= first || (size_t)end > stage->note_index_count) {
        return NULL;
    }
    size_t *ids = malloc(sizeof(size_t) * (size_t)(end - first));
    if (!ids) {
        return NULL;
    }
    memcpy(ids, stage->note_indices + first, sizeof(size_t) * (size_t)(end - first));
    *count = (size_t)(end - first);
    return ids;
}

static bool collect_ideas(const FormStage *stage, AnalysisReport *report) {
    size_t count = stage->segmentation.idea_count;
    if (count == 0) {
        return true;
    }
    report->ideas = calloc(count, sizeof(AnalysisIdea));
    if (!report->ideas) {
        return false;
    }
    report->idea_count = count;
    for (size_t i = 0; i < count; i++) {
        const StandardFormIdea *found = &stage->segmentation.ideas[i];
        const StandardFormIdeaClaim *claim = &found->claim;
        AnalysisIdea *idea = &report->ideas[i];
        idea->phrase = (size_t)claim->phrase;
        idea->index = claim->index;
        idea->first_measure = claim->first_measure;
        idea->last_measure = claim->last_measure;
        idea->length_measures = claim->length_measures;
        idea->label = idea_label_name((IdeaLabel)claim->label);
        idea->compared = claim->compared != 0;
        idea->similarity = claim->similarity;
        idea->uncertainty = found->uncertainty;
        if (claim->phrase < 0 || (size_t)claim->phrase >= report->phrase_count) {
            return false;
        }
        if (!idea->label || !measures_span(stage->built.input.map, claim->first_measure, claim->last_measure,
                                           &idea->start_numerator, &idea->start_denominator, &idea->end_numerator,
                                           &idea->end_denominator)) {
            return false;
        }
        idea->note_ids = note_ids_of(stage, claim->note_first, claim->note_end, &idea->note_id_count);
        if (claim->note_end > claim->note_first && !idea->note_ids) {
            return false;
        }
        AnalysisPhrase *phrase = &report->phrases[idea->phrase];
        clip_to_phrase(phrase, &idea->start_numerator, &idea->start_denominator, &idea->end_numerator,
                       &idea->end_denominator);
        if (phrase->idea_count == 0) {
            phrase->first_idea = i;
        }
        phrase->idea_count++;
    }
    return true;
}

static bool collect_subphrases(const FormStage *stage, AnalysisReport *report) {
    size_t count = stage->segmentation.subphrase_count;
    if (count == 0) {
        return true;
    }
    report->subphrases = calloc(count, sizeof(AnalysisSubphrase));
    if (!report->subphrases) {
        return false;
    }
    report->subphrase_count = count;
    for (size_t i = 0; i < count; i++) {
        const StandardFormSubphrase *found = &stage->segmentation.subphrases[i];
        const StandardFormSubphraseClaim *claim = &found->claim;
        AnalysisSubphrase *subphrase = &report->subphrases[i];
        subphrase->phrase = (size_t)claim->phrase;
        subphrase->index = claim->index;
        subphrase->idea_count = (size_t)claim->idea_count;
        subphrase->first_measure = claim->first_measure;
        subphrase->last_measure = claim->last_measure;
        subphrase->length_measures = claim->length_measures;
        subphrase->uncertainty = found->uncertainty;
        AnalysisPhrase *phrase = &report->phrases[subphrase->phrase];
        subphrase->first_idea = phrase->first_idea + (size_t)claim->first_idea;
        if (!measures_span(stage->built.input.map, claim->first_measure, claim->last_measure, &subphrase->start_numerator,
                           &subphrase->start_denominator, &subphrase->end_numerator, &subphrase->end_denominator)) {
            return false;
        }
        subphrase->note_ids = note_ids_of(stage, claim->note_first, claim->note_end, &subphrase->note_id_count);
        if (claim->note_end > claim->note_first && !subphrase->note_ids) {
            return false;
        }
        clip_to_phrase(phrase, &subphrase->start_numerator, &subphrase->start_denominator, &subphrase->end_numerator,
                       &subphrase->end_denominator);
        if (phrase->subphrase_count == 0) {
            phrase->first_subphrase = i;
        }
        phrase->subphrase_count++;
    }
    return true;
}

/* The ImportedNotes indices of the input's soprano notes that start in [start, end), owned. NULL with *count 0 when
   none. */
static size_t *note_ids_between(const FormStage *stage, long start_numerator, unsigned long start_denominator,
                                long end_numerator, unsigned long end_denominator, size_t *count) {
    *count = 0;
    size_t *ids = malloc(sizeof(size_t) * (stage->note_index_count > 0 ? stage->note_index_count : 1));
    if (!ids) {
        return NULL;
    }
    for (size_t n = 0; n < stage->note_index_count && n < stage->built.input.note_count; n++) {
        const MusicalTime *start = time_span_start(stage->built.input.notes[n].span);
        long numerator = musical_time_numerator(start);
        unsigned long denominator = musical_time_denominator(start);
        if (!rational_less(numerator, denominator, start_numerator, start_denominator) &&
            rational_less(numerator, denominator, end_numerator, end_denominator)) {
            ids[(*count)++] = stage->note_indices[n];
        }
    }
    if (*count == 0) {
        free(ids);
        return NULL;
    }
    return ids;
}

static void fill_part(AnalysisThemePart *part, const char *name, int first, int last, int length,
                      const AnalysisPhrase *phrase) {
    part->name = name;
    part->first_measure = first;
    part->last_measure = last;
    part->length_measures = length;
    part->has_cadence = phrase && phrase->has_cadence;
    part->cadence = part->has_cadence ? phrase->cadence : 0;
}

/* A compound period's halves are the two sentences the library marks part_of_compound over its two phrases. */
static void find_compound_halves(const FormStage *stage, AnalysisReport *report, size_t compound) {
    AnalysisTheme *theme = &report->themes[compound];
    size_t found = 0;
    for (size_t j = 0; j < report->theme_count && found < 2; j++) {
        const StandardFormThemeClaim *claim = &stage->themes.items[j].claim;
        if (j != compound && claim->part_of_compound && claim->phrase_count == 1 &&
            (size_t)claim->first_phrase >= theme->first_phrase &&
            (size_t)claim->first_phrase < theme->first_phrase + theme->phrase_count) {
            theme->halves[found++] = j;
        }
    }
    theme->has_halves = found == 2;
}

static bool collect_themes(const FormStage *stage, AnalysisReport *report) {
    size_t count = stage->themes.count;
    if (count == 0) {
        return true;
    }
    report->themes = calloc(count, sizeof(AnalysisTheme));
    if (!report->themes) {
        return false;
    }
    report->theme_count = count;
    for (size_t i = 0; i < count; i++) {
        const StandardFormTheme *found = &stage->themes.items[i];
        const StandardFormThemeClaim *claim = &found->claim;
        AnalysisTheme *theme = &report->themes[i];
        if (claim->first_phrase < 0 || claim->phrase_count < 1 || claim->phrase_count > 2 ||
            (size_t)(claim->first_phrase + claim->phrase_count) > report->phrase_count) {
            return false;
        }
        theme->index = claim->index;
        theme->form = phrase_level_form_name((PhraseLevelForm)claim->form);
        theme->beginning = claim->beginning >= 0 ? phrase_beginning_name((PhraseBeginning)claim->beginning) : NULL;
        theme->ending = claim->ending >= 0 ? phrase_ending_name((PhraseEnding)claim->ending) : NULL;
        theme->first_phrase = (size_t)claim->first_phrase;
        theme->phrase_count = (size_t)claim->phrase_count;
        theme->first_measure = claim->first_measure;
        theme->last_measure = claim->last_measure;
        theme->length_measures = claim->length_measures;
        theme->parts_are_phrases = claim->parts_are_phrases != 0;
        theme->commonality = claim->commonality >= 0
                                 ? standard_form_commonality_name((StandardFormCommonality)claim->commonality)
                                 : NULL;
        theme->part_of_compound = claim->part_of_compound != 0;
        theme->archetypal = claim->archetypal != 0;
        theme->cadences_usual = claim->cadences_usual != 0;
        theme->lengths_usual = claim->lengths_usual != 0;
        /* The claim names one trait of a continuation: fragmentation. */
        if (claim->has_fragmentation) {
            theme->traits[theme->trait_count++] = continuation_trait_name(CONTINUATION_TRAIT_FRAGMENTATION);
        }
        theme->uncertainty = found->uncertainty;
        /* The parts of a two-phrase theme are its phrases, each with its own cadence. The beginning of a one-phrase
           theme is a subphrase with no cadence; its ending is the phrase's end, with the phrase's cadence. */
        const AnalysisPhrase *first_phrase = &report->phrases[theme->first_phrase];
        const AnalysisPhrase *second_phrase = &report->phrases[theme->first_phrase + theme->phrase_count - 1];
        fill_part(&theme->parts[0], theme->beginning, claim->first_part_first_measure, claim->first_part_last_measure,
                  claim->first_part_length, theme->phrase_count == 2 ? first_phrase : NULL);
        fill_part(&theme->parts[1], theme->ending, claim->second_part_first_measure, claim->second_part_last_measure,
                  claim->second_part_length, second_phrase);
        if (!theme->form || !measures_span(stage->built.input.map, claim->first_measure, claim->last_measure,
                                           &theme->start_numerator, &theme->start_denominator, &theme->end_numerator,
                                           &theme->end_denominator)) {
            return false;
        }
        /* A theme lies inside its phrases: the pickup lies before the first barline and the extension after the last
           phrase's cadence is not part of it. */
        if (rational_less(theme->start_numerator, theme->start_denominator, first_phrase->start_numerator,
                          first_phrase->start_denominator)) {
            theme->start_numerator = first_phrase->start_numerator;
            theme->start_denominator = first_phrase->start_denominator;
        }
        if (rational_less(second_phrase->end_numerator, second_phrase->end_denominator, theme->end_numerator,
                          theme->end_denominator)) {
            theme->end_numerator = second_phrase->end_numerator;
            theme->end_denominator = second_phrase->end_denominator;
        }
        theme->note_ids = note_ids_between(stage, theme->start_numerator, theme->start_denominator,
                                           theme->end_numerator, theme->end_denominator, &theme->note_id_count);
    }
    for (size_t i = 0; i < count; i++) {
        if (stage->themes.items[i].claim.form == PHRASE_LEVEL_FORM_COMPOUND_PERIOD) {
            find_compound_halves(stage, report, i);
        }
    }
    return true;
}

static void fill_techniques(AnalysisExpansion *expansion, int mask) {
    for (int bit = 0; bit < INTERNAL_EXPANSION_TECHNIQUE_COUNT; bit++) {
        if ((mask & (1 << bit)) && expansion->technique_count < ANALYSIS_EXPANSION_MAX_TECHNIQUES) {
            expansion->techniques[expansion->technique_count++] =
                internal_expansion_technique_name((InternalExpansionTechnique)bit);
        }
    }
}

static bool collect_expansions(const FormStage *stage, AnalysisReport *report) {
    size_t count = stage->expansions.count;
    if (count == 0) {
        return true;
    }
    report->expansions = calloc(count, sizeof(AnalysisExpansion));
    if (!report->expansions) {
        return false;
    }
    report->expansion_count = count;
    for (size_t i = 0; i < count; i++) {
        const StandardFormExpansion *found = &stage->expansions.items[i];
        const StandardFormExpansionClaim *claim = &found->claim;
        AnalysisExpansion *expansion = &report->expansions[i];
        if (claim->phrase < 0 || (size_t)claim->phrase >= report->phrase_count ||
            (claim->theme >= 0 && (size_t)claim->theme >= report->theme_count)) {
            return false;
        }
        expansion->index = claim->index;
        expansion->change = phrase_length_change_name((PhraseLengthChange)claim->change);
        expansion->kind = claim->kind == STANDARD_FORM_EXPANSION_CONTRACTION
                              ? expansion->change
                              : expansion_placement_name((ExpansionPlacement)claim->placement);
        expansion->has_theme = claim->theme >= 0;
        expansion->theme = expansion->has_theme ? (size_t)claim->theme : 0;
        expansion->phrase = (size_t)claim->phrase;
        expansion->expected_length = claim->expected_length;
        expansion->actual_length = claim->actual_length;
        expansion->measures_changed = claim->measures_changed;
        expansion->first_measure = claim->first_measure;
        expansion->last_measure = claim->last_measure;
        expansion->uncertainty = found->uncertainty;
        fill_techniques(expansion, claim->techniques);
        if (claim->kind == STANDARD_FORM_EXPANSION_EXTERNAL) {
            expansion->external_kind = claim->external_kind >= 0
                                           ? external_expansion_kind_name((ExternalExpansionKind)claim->external_kind)
                                           : NULL;
            expansion->suffix_length_typical = claim->suffix_length_typical != 0;
            expansion->suffix_agrees_with_location = claim->suffix_agrees_with_location != 0;
        }
        if (!expansion->change || !expansion->kind ||
            !measures_span(stage->built.input.map, claim->first_measure, claim->last_measure, &expansion->start_numerator,
                           &expansion->start_denominator, &expansion->end_numerator, &expansion->end_denominator)) {
            return false;
        }
        clip_to_phrase(&report->phrases[expansion->phrase], &expansion->start_numerator,
                       &expansion->start_denominator, &expansion->end_numerator, &expansion->end_denominator);
        expansion->note_ids = note_ids_between(stage, expansion->start_numerator, expansion->start_denominator,
                                               expansion->end_numerator, expansion->end_denominator,
                                               &expansion->note_id_count);
    }
    return true;
}

bool form_stage_collect(const FormStage *stage, AnalysisReport *report) {
    size_t count = stage->phrases.count;
    if (count == 0) {
        return true;
    }
    report->phrases = calloc(count, sizeof(AnalysisPhrase));
    if (!report->phrases) {
        return false;
    }
    report->phrase_count = count;
    size_t next_entry = 0;
    for (size_t i = 0; i < count; i++) {
        const StandardFormPhrase *found = &stage->phrases.items[i];
        const StandardFormPhraseClaim *claim = &found->claim;
        AnalysisPhrase *phrase = &report->phrases[i];
        const MusicalTime *start = time_span_start(found->span);
        const MusicalTime *end = time_span_end(found->span);
        phrase->start_numerator = musical_time_numerator(start);
        phrase->start_denominator = musical_time_denominator(start);
        phrase->end_numerator = musical_time_numerator(end);
        phrase->end_denominator = musical_time_denominator(end);
        /* The chords starting inside the span: the entries are in time order and the phrases tile. */
        while (next_entry < report->entry_count &&
               rational_less(report->entries[next_entry].time_numerator, report->entries[next_entry].time_denominator,
                             phrase->start_numerator, phrase->start_denominator)) {
            next_entry++;
        }
        phrase->first_entry = next_entry;
        while (next_entry < report->entry_count &&
               rational_less(report->entries[next_entry].time_numerator, report->entries[next_entry].time_denominator,
                             phrase->end_numerator, phrase->end_denominator)) {
            next_entry++;
        }
        phrase->entry_count = next_entry - phrase->first_entry;
        phrase->first_measure = claim->first_measure;
        phrase->end_measure = claim->end_measure;
        phrase->last_measure = claim->last_measure;
        phrase->length_measures = claim->length_measures;
        phrase->extension_measures = claim->extension_measures;
        long cadence = form_stage_phrase_cadence(stage, i);
        phrase->has_cadence = cadence >= 0;
        phrase->cadence = cadence >= 0 ? (size_t)cadence : 0;
        phrase->signal = signal_name(claim->signal);
        phrase->uncertainty = found->uncertainty;
        phrase->role = i == 0 ? "beginning" : (i + 1 == count ? "ending" : "middle");
    }
    return collect_ideas(stage, report) && collect_subphrases(stage, report) && collect_themes(stage, report) &&
           collect_expansions(stage, report);
}

void form_stage_destroy(FormStage *stage) {
    if (!stage) {
        return;
    }
    free(stage->note_indices);
    if (stage->have_input) {
        chain_form_input_destroy(&stage->built);
    }
    memset(stage, 0, sizeof(*stage));
}
