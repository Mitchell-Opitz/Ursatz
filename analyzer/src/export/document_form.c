/* The form areas of the analysis document: phrases, themes, the form plan and repeats. */

#include "export/analysis_document_internal.h"
#include "theory/vocabulary/framework_names.h"

/* An idea of a phrase: the claim over its soprano notes (label the library's IdeaLabel name) with its place in the
   phrase, its measures and length, and the similarity to the phrase's first idea (null for the first idea). */
static cJSON *idea_json(DocBuild *build, const ImportedScore *notes, const char *framework, const AnalysisIdea *idea) {
    Rational start = doc_rational(build, idea->start.num, idea->start.den);
    Rational end = doc_rational(build, idea->end.num, idea->end.den);
    cJSON *claim = doc_claim_json_with(build, framework, notes, idea->note_ids, idea->note_id_count, start, end, idea->label,
                                   cJSON_CreateNumber(idea->uncertainty));
    if (!(claim && common_json_add(claim, "index", cJSON_CreateNumber(idea->index)) &&
          common_json_add(claim, "measures", doc_measures_json(idea->first_measure, idea->last_measure)) &&
          common_json_add(claim, "length", cJSON_CreateNumber(idea->length_measures)) &&
          common_json_add(claim, "similarity", idea->compared ? cJSON_CreateNumber(idea->similarity) : cJSON_CreateNull()))) {
        cJSON_Delete(claim);
        return NULL;
    }
    return claim;
}

/* A subphrase: the claim over its soprano notes, labelled "subphrase" (the library gives subphrases no name), with its
   index (0 or 1), the ideas it holds (first_idea within the phrase's ideas), measures and length. */
static cJSON *subphrase_json(DocBuild *build, const ImportedScore *notes, const char *framework, const ChainResult *report,
                             const AnalysisPhrase *phrase, const AnalysisSubphrase *subphrase) {
    Rational start = doc_rational(build, subphrase->start.num, subphrase->start.den);
    Rational end = doc_rational(build, subphrase->end.num, subphrase->end.den);
    cJSON *claim = doc_claim_json_with(build, framework, notes, subphrase->note_ids, subphrase->note_id_count, start, end,
                                   "subphrase", cJSON_CreateNumber(subphrase->uncertainty));
    if (!(claim && common_json_add(claim, "index", cJSON_CreateNumber(subphrase->index)) &&
          common_json_add(claim, "first_idea", cJSON_CreateNumber((double)(subphrase->first_idea - phrase->first_idea))) &&
          common_json_add(claim, "idea_count", cJSON_CreateNumber((double)subphrase->idea_count)) &&
          common_json_add(claim, "measures", doc_measures_json(subphrase->first_measure, subphrase->last_measure)) &&
          common_json_add(claim, "length", cJSON_CreateNumber(subphrase->length_measures)))) {
        cJSON_Delete(claim);
        return NULL;
    }
    (void)report;
    return claim;
}

/* The labelled chords of a phrase in order: {"chord": index into "chords", "label": "Tb" | "PD" | "D" | "Te"}. Empty
   when the library labelled none. */
static cJSON *phrase_model_json(const ChainResult *report, const AnalysisPhrase *phrase) {
    cJSON *list = cJSON_CreateArray();
    for (size_t k = 0; list && k < phrase->entry_count; k++) {
        const ChainEntry *entry = &report->entries[phrase->first_entry + k];
        if (!entry->phrase_model) {
            continue;
        }
        cJSON *segment = cJSON_CreateObject();
        if (!(segment && common_json_add(segment, "chord", cJSON_CreateNumber((double)(phrase->first_entry + k))) &&
              common_json_add(segment, "label", cJSON_CreateString(entry->phrase_model)) && common_json_append(list, segment))) {
            cJSON_Delete(segment);
            cJSON_Delete(list);
            return NULL;
        }
    }
    return list;
}

static bool add_ideas_and_subphrases(DocBuild *build, cJSON *claim, const ImportedScore *notes, const char *framework,
                                     const ChainResult *report, const AnalysisPhrase *phrase) {
    cJSON *subphrases = cJSON_CreateArray();
    cJSON *ideas = cJSON_CreateArray();
    bool ok = subphrases && ideas;
    for (size_t k = 0; ok && k < phrase->subphrase_count; k++) {
        ok = common_json_append(subphrases, subphrase_json(build, notes, framework, report, phrase,
                                               &report->subphrases[phrase->first_subphrase + k]));
    }
    for (size_t k = 0; ok && k < phrase->idea_count; k++) {
        ok = common_json_append(ideas, idea_json(build, notes, framework, &report->ideas[phrase->first_idea + k]));
    }
    if (ok && common_json_add(claim, "subphrases", subphrases)) {
        subphrases = NULL;
        if (common_json_add(claim, "ideas", ideas)) {
            return true;
        }
        ideas = NULL;
    }
    cJSON_Delete(subphrases);
    cJSON_Delete(ideas);
    return false;
}

static cJSON *expansion_techniques_json(const AnalysisExpansion *expansion) {
    cJSON *techniques = cJSON_CreateArray();
    for (size_t n = 0; techniques && n < expansion->technique_count; n++) {
        if (!common_json_append(techniques, cJSON_CreateString(expansion->techniques[n]))) {
            cJSON_Delete(techniques);
            return NULL;
        }
    }
    return techniques;
}

/* An expansion or contraction of a phrase: the claim over its soprano notes (label the kind: "internal", "external" or
   "contraction"), with the change ("expansion" or "contraction"), the theme it concerns (null for an external
   expansion of a phrase in no theme), the expected and actual lengths and the measures changed, its measures, and the
   techniques the library finds evidence for. An external one adds external_kind and the two judgements of its length. */
static cJSON *expansion_json(DocBuild *build, const ImportedScore *notes, const char *framework, const AnalysisExpansion *expansion) {
    Rational start = doc_rational(build, expansion->start.num, expansion->start.den);
    Rational end = doc_rational(build, expansion->end.num, expansion->end.den);
    cJSON *claim = doc_claim_json_with(build, framework, notes, expansion->note_ids, expansion->note_id_count, start, end,
                                   expansion->kind, cJSON_CreateNumber(expansion->uncertainty));
    if (!(claim && common_json_add(claim, "index", cJSON_CreateNumber(expansion->index)) &&
          common_json_add(claim, "change", cJSON_CreateString(expansion->change)) &&
          common_json_add(claim, "theme", expansion->has_theme ? cJSON_CreateNumber((double)expansion->theme) : cJSON_CreateNull()) &&
          common_json_add(claim, "expected_length", cJSON_CreateNumber(expansion->expected_length)) &&
          common_json_add(claim, "actual_length", cJSON_CreateNumber(expansion->actual_length)) &&
          common_json_add(claim, "measures_changed", cJSON_CreateNumber(expansion->measures_changed)) &&
          common_json_add(claim, "measures", doc_measures_json(expansion->first_measure, expansion->last_measure)) &&
          common_json_add(claim, "techniques", expansion_techniques_json(expansion)) &&
          (!expansion->external_kind ||
           (common_json_add(claim, "external_kind", cJSON_CreateString(expansion->external_kind)) &&
            common_json_add(claim, "suffix_length_typical", cJSON_CreateBool(expansion->suffix_length_typical)) &&
            common_json_add(claim, "suffix_agrees_with_location", cJSON_CreateBool(expansion->suffix_agrees_with_location)))))) {
        cJSON_Delete(claim);
        return NULL;
    }
    return claim;
}

/* The phrase's expansions and contractions as "expansions", in the library's order; absent when it has none. */
static bool add_expansions(DocBuild *build, cJSON *claim, const ImportedScore *notes, const char *framework, const ChainResult *report,
                           size_t phrase) {
    cJSON *list = NULL;
    for (size_t e = 0; e < report->expansion_count; e++) {
        if (report->expansions[e].phrase != phrase) {
            continue;
        }
        if (!list && !(list = cJSON_CreateArray())) {
            return false;
        }
        if (!common_json_append(list, expansion_json(build, notes, framework, &report->expansions[e]))) {
            cJSON_Delete(list);
            return false;
        }
    }
    return !list || common_json_add(claim, "expansions", list);
}

/* One claim per phrase, in time order, from the library's standard-form analysis. The claim's span is the phrase's
   (an extension after its cadence included) and its note ids are the notes sounding in the chords that start in it.
   measures runs from the phrase's first measure to the measure it ends in; length is its length in measures (a pickup
   not counted); extension_measures the measures the extension adds after it; cadence the index into "cadences" of the
   cadence that ends it (null with none); signal what ended it: "cadence", "discontinuity" or "none" (the end of the
   input). role is where the phrase stands in the piece: "beginning" for the first, "ending" for the last, "middle"
   between (the library names no role for a phrase, so this is position alone). subphrases and ideas are claims inside
   it, in time order. expansions (absent with none) are the claims about its length: an expansion or contraction of its
   ending against its beginning, or an extension after its cadence. phrase_model lists the chords the library labelled for the phrase model, in order. */
cJSON *doc_phrases_json(DocBuild *build, const ImportedScore *notes, const ChainResult *report) {
    cJSON *list = cJSON_CreateArray();
    if (!list) {
        return NULL;
    }
    const char *framework = FRAMEWORK_STANDARD_FORM;
    for (size_t g = 0; g < reading_group_count(build->reading); g++) {
        const ReadingGroup *held = reading_group_at(build->reading, g);
        if (held->kind != READING_GROUP_PHRASE) {
            continue;
        }
        size_t i = held->detail;
        const AnalysisPhrase *phrase = &report->phrases[i];
        Rational start = doc_rational(build, phrase->start.num, phrase->start.den);
        Rational end = doc_rational(build, phrase->end.num, phrase->end.den);
        size_t id_count = 0;
        size_t *ids = phrase->entry_count > 0
                          ? doc_entry_range_note_ids(report, phrase->first_entry,
                                                 phrase->first_entry + phrase->entry_count - 1, &id_count)
                          : NULL;
        cJSON *claim = (ids || phrase->entry_count == 0)
                           ? doc_claim_json_with(build, framework, notes, ids, id_count, start, end, "phrase",
                                             cJSON_CreateNumber(phrase->uncertainty))
                           : NULL;
        free(ids);
        if (!(claim && common_json_add(claim, "index", cJSON_CreateNumber((double)i)) &&
              common_json_add(claim, "measures", doc_measures_json(phrase->first_measure, phrase->end_measure)) &&
              common_json_add(claim, "length", cJSON_CreateNumber(phrase->length_measures)) &&
              common_json_add(claim, "extension_measures", cJSON_CreateNumber(phrase->extension_measures)) &&
              common_json_add(claim, "cadence", phrase->has_cadence ? cJSON_CreateNumber((double)phrase->cadence) : cJSON_CreateNull()) &&
              common_json_add(claim, "signal", cJSON_CreateString(phrase->signal)) &&
              common_json_add(claim, "role", cJSON_CreateString(phrase->role)) &&
              common_json_add(claim, "phrase_model", phrase_model_json(report, phrase)) &&
              add_ideas_and_subphrases(build, claim, notes, framework, report, phrase) &&
              add_expansions(build, claim, notes, framework, report, i))) {
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

/* One part of a theme: the library's name for it (null when the library gives none), its measures and length, and the
   index into "cadences" of the cadence that ends it (null with none: a presentation or the first half of a
   two-phrase theme can end on none). */
static cJSON *theme_part_json(const AnalysisThemePart *part) {
    cJSON *object = cJSON_CreateObject();
    if (!(object && common_json_add(object, "name", doc_string_or_null_json(part->name)) &&
          common_json_add(object, "measures", doc_measures_json(part->first_measure, part->last_measure)) &&
          common_json_add(object, "length", cJSON_CreateNumber(part->length_measures)) &&
          common_json_add(object, "cadence", part->has_cadence ? cJSON_CreateNumber((double)part->cadence) : cJSON_CreateNull()))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

/* The phrases a theme spans: the first one's index and how many. */
static cJSON *theme_phrases_json(const AnalysisTheme *theme) {
    cJSON *object = cJSON_CreateObject();
    if (!(object && common_json_add(object, "first", cJSON_CreateNumber((double)theme->first_phrase)) &&
          common_json_add(object, "count", cJSON_CreateNumber((double)theme->phrase_count)))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

static cJSON *theme_parts_json(const AnalysisTheme *theme) {
    cJSON *parts = cJSON_CreateArray();
    if (!(parts && common_json_append(parts, theme_part_json(&theme->parts[0])) && common_json_append(parts, theme_part_json(&theme->parts[1])))) {
        cJSON_Delete(parts);
        return NULL;
    }
    return parts;
}

/* The continuation traits the library's claim gives, as their names. */
static cJSON *theme_traits_json(const AnalysisTheme *theme) {
    cJSON *traits = cJSON_CreateArray();
    for (size_t n = 0; traits && n < theme->trait_count; n++) {
        if (!common_json_append(traits, cJSON_CreateString(theme->traits[n]))) {
            cJSON_Delete(traits);
            return NULL;
        }
    }
    return traits;
}

/* The two themes that make up a compound period, as indices into "themes". */
static cJSON *theme_halves_json(const AnalysisTheme *theme) {
    cJSON *halves = cJSON_CreateArray();
    if (!(halves && common_json_append(halves, cJSON_CreateNumber((double)theme->halves[0])) &&
          common_json_append(halves, cJSON_CreateNumber((double)theme->halves[1])))) {
        cJSON_Delete(halves);
        return NULL;
    }
    return halves;
}

/* One claim per theme, in the library's order, from its standard-form analysis. label is the library's form name
   ("sentence", "period", "hybrid", "compound-period", "repeated-phrase"); beginning and ending are the library's names
   for the two parts (null for a repeated phrase and a compound period); phrases are the phrases it spans (first index
   and count); measures run from its first measure to the last of its last phrase proper (an extension is not part of
   it); parts are the two parts in order, subphrases of a one-phrase theme or phrases of a two-phrase one.
   commonality is the library's name for how common the pairing of the two parts is (null when it has none);
   part_of_compound is true for a sentence that is a half of a compound period; halves (a compound period only) are the
   indices into "themes" of its two sentences. traits are the library's names for what its claim finds in the
   continuation ("fragmentation"; the claim gives no other); archetypal, cadences_usual and lengths_usual are the
   claim's own judgements of the pairing. */
cJSON *doc_themes_json(DocBuild *build, const ImportedScore *notes, const ChainResult *report) {
    cJSON *list = cJSON_CreateArray();
    if (!list) {
        return NULL;
    }
    const char *framework = FRAMEWORK_STANDARD_FORM;
    for (size_t g = 0; g < reading_group_count(build->reading); g++) {
        const ReadingGroup *held = reading_group_at(build->reading, g);
        if (held->kind != READING_GROUP_THEME) {
            continue;
        }
        const AnalysisTheme *theme = &report->themes[held->detail];
        Rational start = doc_rational(build, theme->start.num, theme->start.den);
        Rational end = doc_rational(build, theme->end.num, theme->end.den);
        cJSON *claim = doc_claim_json_with(build, framework, notes, theme->note_ids, theme->note_id_count, start, end,
                                       theme->form, cJSON_CreateNumber(theme->uncertainty));
        if (!(claim && common_json_add(claim, "index", cJSON_CreateNumber(theme->index)) &&
              common_json_add(claim, "beginning", doc_string_or_null_json(theme->beginning)) &&
              common_json_add(claim, "ending", doc_string_or_null_json(theme->ending)) &&
              common_json_add(claim, "phrases", theme_phrases_json(theme)) &&
              common_json_add(claim, "measures", doc_measures_json(theme->first_measure, theme->last_measure)) &&
              common_json_add(claim, "length", cJSON_CreateNumber(theme->length_measures)) &&
              common_json_add(claim, "parts_are_phrases", cJSON_CreateBool(theme->parts_are_phrases)) &&
              common_json_add(claim, "commonality", doc_string_or_null_json(theme->commonality)) &&
              common_json_add(claim, "part_of_compound", cJSON_CreateBool(theme->part_of_compound)) &&
              common_json_add(claim, "traits", theme_traits_json(theme)) &&
              common_json_add(claim, "archetypal", cJSON_CreateBool(theme->archetypal)) &&
              common_json_add(claim, "cadences_usual", cJSON_CreateBool(theme->cadences_usual)) &&
              common_json_add(claim, "lengths_usual", cJSON_CreateBool(theme->lengths_usual)) &&
              common_json_add(claim, "parts", theme_parts_json(theme)) &&
              (!theme->has_halves || common_json_add(claim, "halves", theme_halves_json(theme))))) {
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

/* The form tree's nodes are named by level: "form", "section", "phrase", "subphrase" and "idea". A phrase, subphrase
   or idea node points at its full claim in "phrases" (phrase, and index within the phrase for the last two) and
   repeats only what reading the outline needs. */

static cJSON *idea_node_json(const AnalysisIdea *idea) {
    cJSON *node = cJSON_CreateObject();
    if (!(node && common_json_add(node, "level", cJSON_CreateString("idea")) &&
          common_json_add(node, "phrase", cJSON_CreateNumber((double)idea->phrase)) &&
          common_json_add(node, "index", cJSON_CreateNumber(idea->index)) && common_json_add(node, "label", cJSON_CreateString(idea->label)) &&
          common_json_add(node, "measures", doc_measures_json(idea->first_measure, idea->last_measure)))) {
        cJSON_Delete(node);
        return NULL;
    }
    return node;
}

/* A subphrase node holds the ideas the subphrase holds. */
static cJSON *subphrase_node_json(const ChainResult *report, const AnalysisSubphrase *subphrase) {
    cJSON *node = cJSON_CreateObject();
    cJSON *ideas = cJSON_CreateArray();
    bool ok = node && ideas;
    for (size_t k = 0; ok && k < subphrase->idea_count; k++) {
        ok = common_json_append(ideas, idea_node_json(&report->ideas[subphrase->first_idea + k]));
    }
    bool taken = false; /* common_json_add() takes the ideas over, and frees them itself when it fails */
    if (!(ok && common_json_add(node, "level", cJSON_CreateString("subphrase")) &&
          common_json_add(node, "phrase", cJSON_CreateNumber((double)subphrase->phrase)) &&
          common_json_add(node, "index", cJSON_CreateNumber(subphrase->index)) &&
          common_json_add(node, "measures", doc_measures_json(subphrase->first_measure, subphrase->last_measure)) &&
          (taken = true, common_json_add(node, "ideas", ideas)))) {
        if (!taken) {
            cJSON_Delete(ideas);
        }
        cJSON_Delete(node);
        return NULL;
    }
    return node;
}

static bool idea_in_subphrase(const ChainResult *report, const AnalysisPhrase *phrase, size_t idea) {
    for (size_t k = 0; k < phrase->subphrase_count; k++) {
        const AnalysisSubphrase *subphrase = &report->subphrases[phrase->first_subphrase + k];
        if (idea >= subphrase->first_idea && idea < subphrase->first_idea + subphrase->idea_count) {
            return true;
        }
    }
    return false;
}

/* A phrase node holds its subphrases, each with its ideas, and the ideas that lie in no subphrase (all of them for a
   phrase of fewer than three ideas, which has no subphrases), so each idea appears once in the tree. */
static cJSON *phrase_node_json(const ChainResult *report, size_t index) {
    const AnalysisPhrase *phrase = &report->phrases[index];
    cJSON *node = cJSON_CreateObject();
    cJSON *subphrases = cJSON_CreateArray();
    cJSON *ideas = cJSON_CreateArray();
    bool ok = node && subphrases && ideas;
    for (size_t k = 0; ok && k < phrase->subphrase_count; k++) {
        ok = common_json_append(subphrases, subphrase_node_json(report, &report->subphrases[phrase->first_subphrase + k]));
    }
    for (size_t k = 0; ok && k < phrase->idea_count; k++) {
        if (!idea_in_subphrase(report, phrase, phrase->first_idea + k)) {
            ok = common_json_append(ideas, idea_node_json(&report->ideas[phrase->first_idea + k]));
        }
    }
    bool taken_subphrases = false;
    bool taken_ideas = false;
    if (!(ok && common_json_add(node, "level", cJSON_CreateString("phrase")) &&
          common_json_add(node, "phrase", cJSON_CreateNumber((double)index)) &&
          common_json_add(node, "measures", doc_measures_json(phrase->first_measure, phrase->end_measure)) &&
          common_json_add(node, "length", cJSON_CreateNumber(phrase->length_measures)) &&
          common_json_add(node, "cadence", phrase->has_cadence ? cJSON_CreateNumber((double)phrase->cadence) : cJSON_CreateNull()) &&
          (taken_subphrases = true, common_json_add(node, "subphrases", subphrases)) &&
          (taken_ideas = true, common_json_add(node, "ideas", ideas)))) {
        if (!taken_subphrases) {
            cJSON_Delete(subphrases);
        }
        if (!taken_ideas) {
            cJSON_Delete(ideas);
        }
        cJSON_Delete(node);
        return NULL;
    }
    return node;
}

static cJSON *section_phrases_json(const ChainResult *report, const AnalysisSection *section) {
    cJSON *phrases = cJSON_CreateArray();
    for (size_t k = 0; phrases && k < section->phrase_count; k++) {
        if (!common_json_append(phrases, phrase_node_json(report, section->first_phrase + k))) {
            cJSON_Delete(phrases);
            return NULL;
        }
    }
    return phrases;
}

static bool add_section_fields(cJSON *claim, const ChainResult *report, const AnalysisSection *section,
                               size_t index) {
    return common_json_add(claim, "level", cJSON_CreateString("section")) && common_json_add(claim, "index", cJSON_CreateNumber((double)index)) &&
           common_json_add(claim, "measures", doc_measures_json(section->first_measure, section->last_measure)) &&
           common_json_add(claim, "key", section->in_region ? cJSON_CreateString(report->key_regions[section->region].label)
                                                : cJSON_CreateNull()) &&
           common_json_add(claim, "region", section->in_region ? cJSON_CreateNumber((double)section->region) : cJSON_CreateNull()) &&
           common_json_add(claim, "closed", cJSON_CreateBool(section->closed)) &&
           common_json_add(claim, "cadence", section->has_cadence ? cJSON_CreateNumber((double)section->cadence) : cJSON_CreateNull()) &&
           common_json_add(claim, "returns_from",
               section->has_return ? cJSON_CreateNumber((double)section->returns_from) : cJSON_CreateNull()) &&
           common_json_add(claim, "return",
               section->has_return ? cJSON_CreateNumber((double)section->return_index) : cJSON_CreateNull()) &&
           common_json_add(claim, "phrases", section_phrases_json(report, section));
}

/* One claim per section, labelled with its letter: the span runs from its first phrase's start to its last phrase's
   end, the notes are those sounding in its chords, the uncertainty the greatest of its phrases'. key is the key
   region the section opens in, closed whether one of the library's section-closing cadences ended it (false for the
   phrases after the last one), cadence the cadence that ends its last phrase, returns_from the earlier section whose
   material its opening theme returns (null for a section with a new letter) and return the index into the form's
   "returns" of that return. */
static cJSON *section_json(DocBuild *build, const ImportedScore *notes, const ChainResult *report, size_t index) {
    const AnalysisSection *section = &report->sections[index];
    Rational start = doc_rational(build, section->start.num, section->start.den);
    Rational end = doc_rational(build, section->end.num, section->end.den);
    size_t id_count = 0;
    size_t *ids = section->entry_count > 0 ? doc_entry_range_note_ids(report, section->first_entry,
                                                                  section->first_entry + section->entry_count - 1,
                                                                  &id_count)
                                           : NULL;
    cJSON *claim = (ids || section->entry_count == 0)
                       ? doc_claim_json_with(build, FRAMEWORK_STANDARD_FORM, notes, ids, id_count, start, end,
                                         section->label, cJSON_CreateNumber(section->uncertainty))
                       : NULL;
    free(ids);
    if (!(claim && add_section_fields(claim, report, section, index))) {
        cJSON_Delete(claim);
        return NULL;
    }
    return claim;
}

static cJSON *return_similarity_json(const AnalysisThematicReturn *item) {
    cJSON *object = cJSON_CreateObject();
    if (!(object && common_json_add(object, "value", cJSON_CreateNumber(item->similarity)) &&
          common_json_add(object, "contour", cJSON_CreateNumber(item->contour_score)) &&
          common_json_add(object, "rhythm", cJSON_CreateNumber(item->rhythm_score)) &&
          common_json_add(object, "window", cJSON_CreateNumber((double)item->window_length)) &&
          common_json_add(object, "earlier_offset", cJSON_CreateNumber((double)item->earlier_offset)) &&
          common_json_add(object, "later_offset", cJSON_CreateNumber((double)item->later_offset)))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

/* One claim per thematic return, by later theme, then earlier theme: the later theme recalls the earlier one. The
   claim is over the later theme (its span, notes and location); earlier_theme and later_theme are indices into
   "themes", earlier_section and later_section into the form's "sections", earlier and later the measures of the
   two themes, and similarity the kernel's scores for the best stretch pair (value is the lower of contour and
   rhythm; window the notes compared, the offsets where the stretches begin inside each theme). The uncertainty is
   1 minus the similarity. */
static cJSON *return_json(DocBuild *build, const ImportedScore *notes, const ChainResult *report,
                          const AnalysisThematicReturn *item) {
    const AnalysisTheme *earlier = &report->themes[item->earlier_theme];
    const AnalysisTheme *later = &report->themes[item->later_theme];
    Rational start = doc_rational(build, later->start.num, later->start.den);
    Rational end = doc_rational(build, later->end.num, later->end.den);
    cJSON *claim = doc_claim_json_with(build, FRAMEWORK_STANDARD_FORM, notes, later->note_ids,
                                   later->note_id_count, start, end, "thematic-return",
                                   cJSON_CreateNumber(1.0 - item->similarity));
    if (!(claim && common_json_add(claim, "earlier_theme", cJSON_CreateNumber((double)item->earlier_theme)) &&
          common_json_add(claim, "later_theme", cJSON_CreateNumber((double)item->later_theme)) &&
          common_json_add(claim, "earlier_section", cJSON_CreateNumber((double)item->earlier_section)) &&
          common_json_add(claim, "later_section", cJSON_CreateNumber((double)item->later_section)) &&
          common_json_add(claim, "earlier", doc_measures_json(earlier->first_measure, earlier->last_measure)) &&
          common_json_add(claim, "later", doc_measures_json(later->first_measure, later->last_measure)) &&
          common_json_add(claim, "similarity", return_similarity_json(item)))) {
        cJSON_Delete(claim);
        return NULL;
    }
    return claim;
}

/* The form: a tree of sections, each holding its phrases, subphrases and ideas, and the thematic returns between
   its themes. null when the library found no phrases to group. The root has no span of its own; its measures run
   from the first section's first measure to the last section's last. */
cJSON *doc_form_json(DocBuild *build, const ImportedScore *notes, const ChainResult *report) {
    if (report->section_count == 0) {
        return cJSON_CreateNull();
    }
    cJSON *form = cJSON_CreateObject();
    cJSON *sections = cJSON_CreateArray();
    cJSON *returns = cJSON_CreateArray();
    bool ok = form && sections && returns;
    for (size_t g = 0; ok && g < reading_group_count(build->reading); g++) {
        const ReadingGroup *held = reading_group_at(build->reading, g);
        if (held->kind == READING_GROUP_SECTION) {
            ok = common_json_append(sections, section_json(build, notes, report, held->detail));
        }
    }
    for (size_t r = 0; ok && r < report->thematic_return_count; r++) {
        ok = common_json_append(returns, return_json(build, notes, report, &report->thematic_returns[r]));
    }
    bool taken_sections = false;
    bool taken_returns = false;
    if (!(ok && common_json_add(form, "level", cJSON_CreateString("form")) &&
          common_json_add(form, "measures",
              doc_measures_json(report->sections[0].first_measure,
                            report->sections[report->section_count - 1].last_measure)) &&
          (taken_sections = true, common_json_add(form, "sections", sections)) &&
          (taken_returns = true, common_json_add(form, "returns", returns)))) {
        if (!taken_sections) {
            cJSON_Delete(sections);
        }
        if (!taken_returns) {
            cJSON_Delete(returns);
        }
        cJSON_Delete(form);
        return NULL;
    }
    return form;
}

/* One claim per repeat or return of the library, in its order: the claim is over the later span (its measures and its
   soprano notes), kind and label are "repeat" or "return", earlier and later the measures of the two runs, and
   transposition the semitones from the earlier run to the later (0 for a repeat). The uncertainty is 0: the library
   finds repeats by exact match. */
static cJSON *repeat_json(DocBuild *build, const ImportedScore *notes, const AnalysisRepeat *repeat) {
    const char *kind = repeat->is_return ? "return" : "repeat";
    Rational start = doc_rational(build, repeat->start.num, repeat->start.den);
    Rational end = doc_rational(build, repeat->end.num, repeat->end.den);
    cJSON *claim = doc_claim_json_with(build, FRAMEWORK_STANDARD_FORM, notes, repeat->note_ids,
                                   repeat->note_id_count, start, end, kind, cJSON_CreateNumber(0.0));
    if (!(claim && common_json_add(claim, "kind", cJSON_CreateString(kind)) &&
          common_json_add(claim, "earlier", doc_measures_json(repeat->earlier_first, repeat->earlier_last)) &&
          common_json_add(claim, "later", doc_measures_json(repeat->later_first, repeat->later_last)) &&
          common_json_add(claim, "transposition", cJSON_CreateNumber((double)repeat->transposition)))) {
        cJSON_Delete(claim);
        return NULL;
    }
    return claim;
}

/* The repeats and returns as "repeats", in the library's order; an empty array when there are none. */
cJSON *doc_repeats_json(DocBuild *build, const ImportedScore *notes, const ChainResult *report) {
    cJSON *list = cJSON_CreateArray();
    for (size_t i = 0; list && i < report->repeat_count; i++) {
        if (!common_json_append(list, repeat_json(build, notes, &report->repeats[i]))) {
            cJSON_Delete(list);
            return NULL;
        }
    }
    return list;
}
