#include "pipeline/form_plan_pipeline.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "analysis/form_structure_detection.h"
#include "analysis/thematic_return_detection.h"
#include "containers/section.h"
#include "core/value.h"
#include "events/note_event.h"
#include "identity/entity_id.h"
#include "pitch/pitch_identity.h"
#include "relationship/relationship.h"
#include "semantics/cadence_type.h"
#include "semantics/contour.h"
#include "semantics/rhythmic_pattern.h"
#include "structure/phrase_occurrence.h"
#include "structure/structure.h"
#include "time/duration.h"

#define THEME_ID_PREFIX "theme-"

/* ---- Sections ---- */

static void fill_section(AnalysisSection *section, const AnalysisReport *report, size_t first_phrase,
                         size_t phrase_count, bool closed) {
    const AnalysisPhrase *first = &report->phrases[first_phrase];
    const AnalysisPhrase *last = &report->phrases[first_phrase + phrase_count - 1];
    section->first_phrase = first_phrase;
    section->phrase_count = phrase_count;
    section->start_numerator = first->start_numerator;
    section->start_denominator = first->start_denominator;
    section->end_numerator = last->end_numerator;
    section->end_denominator = last->end_denominator;
    section->first_entry = first->first_entry;
    section->entry_count = last->first_entry + last->entry_count - first->first_entry;
    section->first_measure = first->first_measure;
    section->last_measure = last->last_measure;
    section->closed = closed;
    section->has_cadence = last->has_cadence;
    section->cadence = last->has_cadence ? last->cadence : 0;
    if (section->entry_count > 0 && report->entries[section->first_entry].in_region) {
        section->in_region = true;
        section->region = report->entries[section->first_entry].region;
    }
    for (size_t p = first_phrase; p < first_phrase + phrase_count; p++) {
        if (report->phrases[p].uncertainty > section->uncertainty) {
            section->uncertainty = report->phrases[p].uncertainty;
        }
    }
}

/* The relationships "precedes" the kernel builds link its sections in order; they must agree with the sections. */
static bool relationships_agree(const FormStructureDetectionEntry *entries, size_t entry_count, Section **sections,
                                size_t section_count) {
    Relationship **relationships = NULL;
    size_t relationship_count = 0;
    if (form_structure_detection_build_relationships(entries, entry_count, &relationships, &relationship_count) !=
        FORM_STRUCTURE_DETECTION_REL_OK) {
        return false;
    }
    bool agree = relationship_count == (section_count > 0 ? section_count - 1 : 0);
    for (size_t k = 0; agree && k < relationship_count; k++) {
        agree = strcmp(entity_id_value(relationship_source_id(relationships[k])),
                       entity_id_value(section_id(sections[k]))) == 0 &&
                strcmp(entity_id_value(relationship_target_id(relationships[k])),
                       entity_id_value(section_id(sections[k + 1]))) == 0;
    }
    form_structure_detection_destroy_relationships(relationships, relationship_count);
    return agree;
}

static bool build_sections(const FormStage *stage, AnalysisReport *report) {
    size_t phrase_count = report->phrase_count;
    FormStructureDetectionEntry *entries = calloc(phrase_count, sizeof(*entries));
    if (!entries) {
        return false;
    }
    for (size_t i = 0; i < phrase_count; i++) {
        int type = stage->phrases.items[i].claim.cadence_type;
        entries[i].phrase_occurrence = stage->phrases.items[i].occurrence;
        entries[i].cadence_type = type >= 0 ? (CadenceType)type : CADENCE_TYPE_UNKNOWN;
    }
    Section **found = NULL;
    size_t found_count = 0;
    bool ok = form_structure_detection_build_sections(entries, phrase_count, &found, &found_count) ==
                  FORM_STRUCTURE_DETECTION_OK &&
              relationships_agree(entries, phrase_count, found, found_count);
    size_t covered = 0;
    size_t total = 0;
    if (ok) {
        for (size_t k = 0; ok && k < found_count; k++) {
            size_t count = section_phrase_id_count(found[k]);
            ok = count > 0 && covered + count <= phrase_count &&
                 strcmp(entity_id_value(section_phrase_id_at(found[k], 0)),
                        entity_id_value(phrase_occurrence_phrase_id(stage->phrases.items[covered].occurrence))) == 0;
            covered += count;
        }
        total = found_count + (covered < phrase_count ? 1 : 0);
        report->sections = ok ? calloc(total, sizeof(AnalysisSection)) : NULL;
        ok = ok && report->sections != NULL;
    }
    if (ok) {
        report->section_count = total;
        size_t next = 0;
        for (size_t k = 0; k < found_count; k++) {
            size_t count = section_phrase_id_count(found[k]);
            fill_section(&report->sections[k], report, next, count, true);
            next += count;
        }
        if (next < phrase_count) {
            fill_section(&report->sections[found_count], report, next, phrase_count - next, false);
        }
    }
    form_structure_detection_destroy_sections(found, found_count);
    free(entries);
    return ok;
}

/* ---- Thematic returns ---- */

/* One theme as a window for the kernel's detection: the contour, the rhythm and the ids of its soprano notes. */
typedef struct {
    EntityID *id;
    EntityID **event_ids; /* owned, with the ids */
    size_t event_count;
    Contour *contour;
    RhythmicPattern *rhythm;
    MotifWindow window;
    bool usable;
} ThemeWindow;

static void theme_window_destroy(ThemeWindow *window) {
    if (window->id) {
        entity_id_destroy(window->id);
    }
    for (size_t i = 0; i < window->event_count; i++) {
        entity_id_destroy(window->event_ids[i]);
    }
    free(window->event_ids);
    if (window->contour) {
        contour_destroy(window->contour);
    }
    if (window->rhythm) {
        rhythmic_pattern_destroy(window->rhythm);
    }
}

/* The position in the form input of the soprano note with this id, searching on from `hint`, or note_count. */
static size_t find_note(const StandardFormInput *input, const char *id, size_t hint) {
    for (size_t k = 0; k < input->note_count; k++) {
        size_t n = (hint + k) % input->note_count;
        if (strcmp(input->notes[n].note_id, id) == 0) {
            return n;
        }
    }
    return input->note_count;
}

static void free_pitch_ids(PitchIdentity **ids, size_t count) {
    for (size_t i = 0; i < count; i++) {
        if (ids[i]) {
            pitch_identity_destroy(ids[i]);
        }
    }
    free(ids);
}

static void free_durations(Duration **durations, size_t count) {
    for (size_t i = 0; i < count; i++) {
        if (durations[i]) {
            duration_destroy(durations[i]);
        }
    }
    free(durations);
}

/* The contour and rhythm of the notes of input->notes at `positions` (count >= 2). On false nothing is kept. */
static bool make_contour_and_rhythm(const StandardFormInput *input, const size_t *positions, size_t count,
                                    Contour **contour, RhythmicPattern **rhythm) {
    PitchIdentity **pitch_ids = calloc(count, sizeof(PitchIdentity *));
    ContourDirection *directions = calloc(count - 1, sizeof(ContourDirection));
    Duration **durations = calloc(count, sizeof(Duration *));
    bool ok = pitch_ids && directions && durations;
    for (size_t i = 0; ok && i < count; i++) {
        const StandardFormNote *note = &input->notes[positions[i]];
        char token[48];
        int length = snprintf(token, sizeof(token), "p%ld-%zu", note->semitone, i);
        const Duration *duration = note_event_duration(note->event);
        ok = pitch_identity_create(token, (size_t)length, &pitch_ids[i]) == PITCH_IDENTITY_OK &&
             duration_create(duration_numerator(duration), duration_denominator(duration), &durations[i]) ==
                 DURATION_OK;
        if (ok && i > 0) {
            long previous = input->notes[positions[i - 1]].semitone;
            directions[i - 1] = note->semitone > previous ? CONTOUR_DIRECTION_UP
                                                          : (note->semitone < previous ? CONTOUR_DIRECTION_DOWN
                                                                                       : CONTOUR_DIRECTION_SAME);
        }
    }
    if (ok) {
        /* On success the contour and the pattern own the ids and the durations and copy the arrays. */
        ok = contour_create(pitch_ids, count, directions, count - 1, contour) == CONTOUR_OK;
        if (ok) {
            ok = rhythmic_pattern_create(durations, count, rhythm) == RHYTHMIC_PATTERN_OK;
            if (!ok) {
                contour_destroy(*contour);
                *contour = NULL;
                free(pitch_ids);
                free(directions);
                free_durations(durations, count);
                return false;
            }
            free(pitch_ids);
            free(directions);
            free(durations);
            return true;
        }
    }
    free_pitch_ids(pitch_ids, count);
    free(directions);
    free_durations(durations, count);
    return false;
}

/* The window of theme t, from the ids in its shell. A theme with fewer than two notes is not usable; false only on
   allocation or library failure. */
static bool make_theme_window(const FormStage *stage, size_t t, ThemeWindow *window) {
    const Structure *shell = stage->themes.items[t].shell;
    size_t count = shell ? structure_event_id_count(shell) : 0;
    if (count < 2) {
        return true;
    }
    size_t *positions = malloc(sizeof(size_t) * count);
    window->event_ids = calloc(count, sizeof(EntityID *));
    if (!positions || !window->event_ids) {
        free(positions);
        return false;
    }
    char name[48];
    snprintf(name, sizeof(name), THEME_ID_PREFIX "%zu", t);
    bool ok = entity_id_create(name, &window->id) == ENTITY_ID_OK;
    size_t hint = 0;
    for (size_t i = 0; ok && i < count; i++) {
        const char *value = entity_id_value(structure_event_id_at(shell, i));
        positions[i] = find_note(&stage->built.input, value, hint);
        ok = positions[i] < stage->built.input.note_count &&
             entity_id_create(value, &window->event_ids[i]) == ENTITY_ID_OK;
        window->event_count = ok ? i + 1 : i;
        hint = positions[i] + 1;
    }
    ok = ok && make_contour_and_rhythm(&stage->built.input, positions, count, &window->contour, &window->rhythm);
    free(positions);
    if (ok) {
        window->window.contour = window->contour;
        window->window.rhythm = window->rhythm;
        window->window.event_ids = (const EntityID *const *)window->event_ids;
        window->window.event_id_count = count;
        window->usable = true;
    }
    return ok;
}

static size_t section_of_phrase(const AnalysisReport *report, size_t phrase) {
    for (size_t s = 0; s < report->section_count; s++) {
        if (phrase >= report->sections[s].first_phrase &&
            phrase < report->sections[s].first_phrase + report->sections[s].phrase_count) {
            return s;
        }
    }
    return report->section_count;
}

static bool theme_index_of(const EntityID *id, size_t theme_count, size_t *index) {
    const char *value = entity_id_value(id);
    size_t prefix = strlen(THEME_ID_PREFIX);
    if (strncmp(value, THEME_ID_PREFIX, prefix) != 0) {
        return false;
    }
    char *end = NULL;
    unsigned long parsed = strtoul(value + prefix, &end, 10);
    *index = parsed;
    return end && *end == '\0' && parsed < theme_count;
}

static int compare_returns(const void *a, const void *b) {
    const AnalysisThematicReturn *x = a;
    const AnalysisThematicReturn *y = b;
    if (x->later_theme != y->later_theme) {
        return x->later_theme < y->later_theme ? -1 : 1;
    }
    return x->earlier_theme < y->earlier_theme ? -1 : (x->earlier_theme > y->earlier_theme ? 1 : 0);
}

/* Reads the returns of the kernel's relationships into the report, dropping those between themes that share a
   phrase. */
static bool collect_returns(Relationship **relationships, size_t relationship_count, AnalysisReport *report) {
    AnalysisThematicReturn *returns = calloc(relationship_count > 0 ? relationship_count : 1, sizeof(*returns));
    if (!returns) {
        return false;
    }
    size_t count = 0;
    for (size_t r = 0; r < relationship_count; r++) {
        size_t later = 0;
        size_t earlier = 0;
        const Value *qualifiers = relationship_qualifiers(relationships[r]);
        size_t size = 0;
        const void *content = qualifiers && strcmp(value_type(qualifiers), THEMATIC_RETURN_SIMILARITY_VALUE_TYPE) == 0
                                  ? value_content(qualifiers, &size)
                                  : NULL;
        if (!theme_index_of(relationship_source_id(relationships[r]), report->theme_count, &later) ||
            !theme_index_of(relationship_target_id(relationships[r]), report->theme_count, &earlier) ||
            !content || size != sizeof(ThematicReturnSimilarity)) {
            free(returns);
            return false;
        }
        const AnalysisTheme *a = &report->themes[earlier];
        const AnalysisTheme *b = &report->themes[later];
        if (a->first_phrase + a->phrase_count > b->first_phrase) {
            continue;
        }
        ThematicReturnSimilarity similarity;
        memcpy(&similarity, content, sizeof(similarity));
        AnalysisThematicReturn *item = &returns[count++];
        item->earlier_theme = earlier;
        item->later_theme = later;
        item->earlier_section = section_of_phrase(report, a->first_phrase);
        item->later_section = section_of_phrase(report, b->first_phrase);
        item->similarity = similarity.similarity;
        item->contour_score = similarity.contour_score;
        item->rhythm_score = similarity.rhythm_score;
        item->earlier_offset = similarity.earlier_offset;
        item->later_offset = similarity.later_offset;
        item->window_length = similarity.window_length;
    }
    if (count == 0) {
        free(returns);
        return true;
    }
    qsort(returns, count, sizeof(*returns), compare_returns);
    report->thematic_returns = returns;
    report->thematic_return_count = count;
    return true;
}

static bool find_returns(const FormStage *stage, AnalysisReport *report) {
    size_t theme_count = report->theme_count;
    if (theme_count < 2 || stage->themes.count != theme_count) {
        return true;
    }
    ThemeWindow *windows = calloc(theme_count, sizeof(ThemeWindow));
    ThematicReturnDetectionEntry *entries = calloc(theme_count, sizeof(*entries));
    if (!windows || !entries) {
        free(windows);
        free(entries);
        return false;
    }
    bool ok = true;
    size_t entry_count = 0;
    for (size_t t = 0; ok && t < theme_count; t++) {
        ok = make_theme_window(stage, t, &windows[t]);
        if (ok && windows[t].usable) {
            entries[entry_count].theme_occurrence_id = windows[t].id;
            entries[entry_count].window = &windows[t].window;
            entry_count++;
        }
    }
    Relationship **relationships = NULL;
    size_t relationship_count = 0;
    if (ok) {
        ok = thematic_return_detection_build_relationships_configured(entries, entry_count, NULL, &relationships,
                                                                      &relationship_count) ==
             THEMATIC_RETURN_DETECTION_REL_OK;
    }
    if (ok) {
        ok = collect_returns(relationships, relationship_count, report);
        thematic_return_detection_destroy_relationships(relationships, relationship_count);
    }
    for (size_t t = 0; t < theme_count; t++) {
        theme_window_destroy(&windows[t]);
    }
    free(windows);
    free(entries);
    return ok;
}

/* ---- Letters ---- */

/* The letter of family f: A to Z, then AA, AB, and so on. */
static void family_letters(size_t family, char *out) {
    if (family < 26) {
        out[0] = (char)('A' + family);
        out[1] = '\0';
    } else {
        out[0] = (char)('A' + (family / 26 - 1) % 26);
        out[1] = (char)('A' + family % 26);
        out[2] = '\0';
    }
}

/* A section that has primes follows its letter with one apostrophe each, up to three, then "'" and the count. */
static void set_label(AnalysisSection *section, size_t family, size_t primes) {
    char letters[4];
    family_letters(family, letters);
    if (primes <= 3) {
        snprintf(section->label, sizeof(section->label), "%s%.*s", letters, (int)primes, "'''");
    } else {
        snprintf(section->label, sizeof(section->label), "%s'%u", letters, (unsigned)(primes % 1000));
    }
}

/* The return that sets section s's letter: among the returns of a theme that opens the section (the first phrase of
   the section) from an earlier section, the most similar, then the earliest section, then the first. */
static bool best_return(const AnalysisReport *report, size_t s, size_t *best) {
    bool found = false;
    for (size_t r = 0; r < report->thematic_return_count; r++) {
        const AnalysisThematicReturn *item = &report->thematic_returns[r];
        if (item->later_section != s || item->earlier_section >= s ||
            report->themes[item->later_theme].first_phrase != report->sections[s].first_phrase) {
            continue;
        }
        if (!found || item->similarity > report->thematic_returns[*best].similarity ||
            (item->similarity == report->thematic_returns[*best].similarity &&
             item->earlier_section < report->thematic_returns[*best].earlier_section)) {
            *best = r;
            found = true;
        }
    }
    return found;
}

static bool assign_letters(AnalysisReport *report) {
    size_t count = report->section_count;
    size_t *family_of = calloc(count, sizeof(size_t));
    size_t *members = calloc(count, sizeof(size_t));
    if (!family_of || !members) {
        free(family_of);
        free(members);
        return false;
    }
    size_t families = 0;
    for (size_t s = 0; s < count; s++) {
        AnalysisSection *section = &report->sections[s];
        size_t r = 0;
        if (best_return(report, s, &r)) {
            section->has_return = true;
            section->returns_from = report->thematic_returns[r].earlier_section;
            section->return_index = r;
            family_of[s] = family_of[section->returns_from];
        } else {
            family_of[s] = families++;
        }
        set_label(section, family_of[s], members[family_of[s]]++);
    }
    free(family_of);
    free(members);
    return true;
}

bool form_plan_stage_collect(const FormStage *stage, AnalysisReport *report) {
    if (report->phrase_count == 0 || stage->phrases.count != report->phrase_count) {
        return true;
    }
    return build_sections(stage, report) && find_returns(stage, report) && assign_letters(report);
}
