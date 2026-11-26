#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "theory/knowledge/knowledge_internal.h"

/* K7 formal function (kind function, subtype formal-function) and K8 theme type (kind theme-type). */

static const char *const CORES[] = {"initiating", "medial", "concluding", "before-the-beginning", "after-the-end", "connective"};
static const char *const LEVELS[] = {"idea", "subphrase", "phrase"};
static const char *const PACES[] = {"any", "faster", "slower"};
static const char *const CADENCES[] = {"none", "weak", "strong", "any"};
static const char *const MOTIVES[] = {"keep", "free"};
static const char *const SLOTS[] = {"basic", "contrasting", "continuation", "cadential"};
static const char *const HALVES[] = {"beginning", "ending"};
static const char *const IDEA_CADENCES[] = {"none", "first", "final"};

static const char *const EXPECT_JOBS[] = {"motif", "variant", "contrast", "running", "long", "syncopated", "liquidation", "cadence"};
static const char *const EXPECT_SOUNDS[] = {"ninth_downbeat", "sus4_dominant", "appoggiatura", "major_seventh", "leading_tone"};

#define COUNT_OF(array) (sizeof(array) / sizeof((array)[0]))

/* "3" or "1/2": a non-negative amount with a positive denominator. */
static bool read_amount(const char *text, Rational *out) {
    char *end = NULL;
    long long num = strtoll(text, &end, 10);
    long long den = 1;
    if (end == text || num < 0) {
        return false;
    }
    if (*end == '/') {
        const char *tail = end + 1;
        den = strtoll(tail, &end, 10);
        if (end == tail || den < 1) {
            return false;
        }
    }
    return *end == '\0' && rational_make(num, den, out) == RATIONAL_OK;
}

static KnowledgeStatus read_contrast(Slot *slot, const cJSON *content, const char *path, char *message, size_t message_size) {
    const cJSON *contrast = knowledge_member(content, "contrast");
    if (contrast == NULL) {
        return KNOWLEDGE_OK;
    }
    const char *id = slot->entry.id;
    const char *against = cJSON_IsObject(contrast) ? knowledge_string_of(knowledge_member(contrast, "against")) : NULL;
    const cJSON *aspects = cJSON_IsObject(contrast) ? knowledge_member(contrast, "aspects") : NULL;
    int count = cJSON_IsArray(aspects) ? cJSON_GetArraySize(aspects) : -1;
    if (against == NULL) {
        knowledge_fail(message, message_size, path, id, "content.contrast.against", "missing");
        return KNOWLEDGE_ERR_FIELD;
    }
    if (count < 1 || count > FORMAL_ASPECT_MAX) {
        knowledge_fail(message, message_size, path, id, "content.contrast.aspects", "not 1 to 4 aspects");
        return KNOWLEDGE_ERR_FIELD;
    }
    slot->against_target = knowledge_own(slot, against);
    if (slot->against_target == NULL) {
        return KNOWLEDGE_ERR_ALLOC;
    }
    static const char *const ASPECTS[] = {"rhythm", "contour", "register", "density"};
    const cJSON *item;
    cJSON_ArrayForEach(item, aspects) {
        const char *name = cJSON_IsObject(item) ? knowledge_string_of(knowledge_member(item, "aspect")) : NULL;
        const char *amount = cJSON_IsObject(item) ? knowledge_string_of(knowledge_member(item, "amount")) : NULL;
        FormalAspect *out = &slot->function.aspects[slot->function.aspect_count];
        if (knowledge_word_index(name, ASPECTS, COUNT_OF(ASPECTS)) < 0 || amount == NULL || !read_amount(amount, &out->amount)) {
            knowledge_fail(message, message_size, path, id, "content.contrast.aspects",
                           "an aspect is not rhythm, contour, register or density with an amount such as \"1/2\"");
            return KNOWLEDGE_ERR_FIELD;
        }
        for (size_t i = 0; i < slot->function.aspect_count; i++) {
            if (strcmp(slot->function.aspects[i].aspect, name) == 0) {
                knowledge_fail(message, message_size, path, id, "content.contrast.aspects", "two aspects have the same name");
                return KNOWLEDGE_ERR_FIELD;
            }
        }
        out->aspect = knowledge_own(slot, name);
        if (out->aspect == NULL) {
            return KNOWLEDGE_ERR_ALLOC;
        }
        slot->function.aspect_count++;
    }
    return KNOWLEDGE_OK;
}

KnowledgeStatus knowledge_form_read_function(Slot *slot, const cJSON *entry, const char *subtype, const char *path, char *message,
                                             size_t message_size) {
    const char *id = slot->entry.id;
    if (subtype == NULL || strcmp(subtype, "formal-function") != 0) {
        knowledge_fail(message, message_size, path, id, "subtype", subtype == NULL ? "missing" : "not formal-function");
        return KNOWLEDGE_ERR_FIELD;
    }
    const cJSON *content = knowledge_member(entry, "content");
    if (!cJSON_IsObject(content)) {
        knowledge_fail(message, message_size, path, id, "content", "not an object");
        return KNOWLEDGE_ERR_FIELD;
    }
    FormalFunction *function = &slot->function;
    const char *code = knowledge_string_of(knowledge_member(content, "code"));
    if (code == NULL || !knowledge_lowercase_words(code, '-')) {
        knowledge_fail(message, message_size, path, id, "content.code", code == NULL ? "missing" : "not lowercase words joined by hyphens");
        return KNOWLEDGE_ERR_FIELD;
    }
    function->code = knowledge_own(slot, code);
    if (function->code == NULL) {
        return KNOWLEDGE_ERR_ALLOC;
    }
    int index = 0;
    KnowledgeStatus status = knowledge_read_word(slot, content, "core", CORES, COUNT_OF(CORES),
                                                 "not initiating, medial, concluding, before-the-beginning, after-the-end or connective",
                                                 &index, path, message, message_size);
    function->core = (FormalCore)index;
    if (status == KNOWLEDGE_OK) {
        status = knowledge_read_word(slot, content, "level", LEVELS, COUNT_OF(LEVELS), "not idea, subphrase or phrase", &index, path,
                                     message, message_size);
        function->level = (FormalLevel)index;
    }
    if (status == KNOWLEDGE_OK) {
        status = knowledge_read_word(slot, content, "harmonic_rhythm", PACES, 2, "not any or faster", &index, path, message, message_size);
        function->harmonic_rhythm = (FormalPace)index;
    }
    if (status == KNOWLEDGE_OK) {
        status = knowledge_read_word(slot, content, "rhythmic_activity", PACES, 3, "not any, faster or slower", &index, path, message,
                                     message_size);
        function->rhythmic_activity = (FormalPace)index;
    }
    if (status == KNOWLEDGE_OK) {
        status = knowledge_read_word(slot, content, "motive_harmony", MOTIVES, 2, "not keep or free", &index, path, message, message_size);
        function->keeps_motive_harmony = index == 0;
    }
    function->cadence = FORMAL_CADENCE_ANY;
    if (status == KNOWLEDGE_OK && knowledge_member(content, "cadence") != NULL) {
        status = knowledge_read_word(slot, content, "cadence", CADENCES, COUNT_OF(CADENCES), "not none, weak, strong or any", &index,
                                     path, message, message_size);
        function->cadence = (FormalCadence)index;
    }
    if (status != KNOWLEDGE_OK) {
        return status;
    }
    const cJSON *harmony = knowledge_member(content, "harmony");
    int count = cJSON_IsArray(harmony) ? cJSON_GetArraySize(harmony) : -1;
    if (count < 1 || count > FORMAL_HARMONY_MAX) {
        knowledge_fail(message, message_size, path, id, "content.harmony", harmony == NULL ? "missing" : "not 1 to 4 entry ids");
        return KNOWLEDGE_ERR_FIELD;
    }
    const cJSON *item;
    cJSON_ArrayForEach(item, harmony) {
        const char *target = knowledge_string_of(item);
        if (target == NULL) {
            knowledge_fail(message, message_size, path, id, "content.harmony", "not 1 to 4 entry ids");
            return KNOWLEDGE_ERR_FIELD;
        }
        slot->harmony_targets[function->harmony_count] = knowledge_own(slot, target);
        if (slot->harmony_targets[function->harmony_count] == NULL) {
            return KNOWLEDGE_ERR_ALLOC;
        }
        function->harmony_count++;
    }
    return read_contrast(slot, content, path, message, message_size);
}

/* An idea's refusal: "idea '<name>': <problem>". */
static KnowledgeStatus expect_fail(char *message, size_t message_size, const char *path, const char *id, const char *name, const char *field,
                                   const char *problem) {
    char text[200];
    snprintf(text, sizeof text, "idea '%s': %s", name, problem);
    knowledge_fail(message, message_size, path, id, field, text);
    return KNOWLEDGE_ERR_FIELD;
}

/* A list of words from a fixed set as a bit mask; false for an unknown word, a repeat or a count outside 1 to max. */
static bool read_word_bits(const cJSON *list, const char *const *words, size_t word_count, int max, unsigned *out) {
    int count = cJSON_IsArray(list) ? cJSON_GetArraySize(list) : -1;
    if (count < 1 || count > max) {
        return false;
    }
    unsigned bits = 0;
    const cJSON *item;
    cJSON_ArrayForEach(item, list) {
        int at = knowledge_word_index(knowledge_string_of(item), words, word_count);
        if (at < 0 || (bits & (1u << at)) != 0) {
            return false;
        }
        bits |= 1u << at;
    }
    *out = bits;
    return true;
}

static KnowledgeStatus read_expect_harmony(Slot *slot, const cJSON *list, ThemeTypeIdea *idea, const char *path, char *message, size_t message_size) {
    const char *id = slot->entry.id;
    ThemeTypeExpect *expect = &idea->expect;
    int count = cJSON_IsArray(list) ? cJSON_GetArraySize(list) : -1;
    if (count < 1 || count > THEME_EXPECT_HARMONY_MAX) {
        return expect_fail(message, message_size, path, id, idea->name, "content.ideas", "expect.harmony is not 1 to 6 alternatives");
    }
    const cJSON *item;
    cJSON_ArrayForEach(item, list) {
        ThemeHarmonyTemplate *template = &expect->harmony[expect->harmony_count];
        const char *type = cJSON_IsObject(item) ? knowledge_string_of(knowledge_member(item, "type")) : NULL;
        const cJSON *restate = cJSON_IsObject(item) ? knowledge_member(item, "restate") : NULL;
        const cJSON *chords = cJSON_IsObject(item) ? knowledge_member(item, "chords") : NULL;
        bool restating = restate != NULL && cJSON_IsTrue(restate);
        if (type == NULL || restating == (chords != NULL) || (restate != NULL && !restating)) {
            return expect_fail(message, message_size, path, id, idea->name, "content.ideas",
                               "an expect.harmony alternative needs a type and either chords or restate true");
        }
        template->type_id = knowledge_own(slot, type);
        if (template->type_id == NULL) {
            return KNOWLEDGE_ERR_ALLOC;
        }
        template->restate = restating;
        if (!restating) {
            int chord_count = cJSON_IsArray(chords) ? cJSON_GetArraySize(chords) : -1;
            if (chord_count < 1 || chord_count > THEME_EXPECT_CHORD_MAX) {
                return expect_fail(message, message_size, path, id, idea->name, "content.ideas", "expect.harmony chords are not 1 to 8 texts");
            }
            if (chord_count != idea->units) {
                return expect_fail(message, message_size, path, id, idea->name, "content.ideas",
                                   "an expect.harmony chords list does not have one text per unit");
            }
            const cJSON *chord;
            cJSON_ArrayForEach(chord, chords) {
                const char *text = knowledge_string_of(chord);
                if (text == NULL || text[0] == '\0') {
                    return expect_fail(message, message_size, path, id, idea->name, "content.ideas", "an expect.harmony chord is not a text");
                }
                template->chords[template->chord_count] = knowledge_own(slot, text);
                if (template->chords[template->chord_count] == NULL) {
                    return KNOWLEDGE_ERR_ALLOC;
                }
                template->chord_count++;
            }
        }
        expect->harmony_count++;
    }
    return KNOWLEDGE_OK;
}

/* The optional expect object of an idea: jobs and chords per unit, the climax units and the sounds a unit may take (K8, R-749). */
static KnowledgeStatus read_expect(Slot *slot, const cJSON *item, ThemeTypeIdea *idea, const char *path, char *message, size_t message_size) {
    const cJSON *object = knowledge_member(item, "expect");
    if (object == NULL) {
        return KNOWLEDGE_OK;
    }
    const char *id = slot->entry.id;
    const char *name = idea->name;
    if (!cJSON_IsObject(object)) {
        return expect_fail(message, message_size, path, id, name, "content.ideas", "expect is not an object");
    }
    ThemeTypeExpect *expect = &idea->expect;
    expect->present = true;
    const cJSON *jobs = knowledge_member(object, "jobs");
    if (jobs != NULL) {
        int count = cJSON_IsArray(jobs) ? cJSON_GetArraySize(jobs) : -1;
        if (count != idea->units) {
            return expect_fail(message, message_size, path, id, name, "content.ideas", "expect.jobs does not have one list per unit");
        }
        const cJSON *unit;
        cJSON_ArrayForEach(unit, jobs) {
            if (!read_word_bits(unit, EXPECT_JOBS, COUNT_OF(EXPECT_JOBS), 5, &expect->jobs[expect->job_count])) {
                return expect_fail(message, message_size, path, id, name, "content.ideas", "expect.jobs holds an unknown or repeated word");
            }
            expect->job_count++;
        }
    }
    const cJSON *harmony = knowledge_member(object, "harmony");
    if (harmony != NULL) {
        KnowledgeStatus status = read_expect_harmony(slot, harmony, idea, path, message, message_size);
        if (status != KNOWLEDGE_OK) {
            return status;
        }
    }
    const cJSON *climax = knowledge_member(object, "climax");
    if (climax != NULL) {
        const cJSON *unit;
        if (!cJSON_IsArray(climax)) {
            return expect_fail(message, message_size, path, id, name, "content.ideas", "expect.climax is not a list of units");
        }
        cJSON_ArrayForEach(unit, climax) {
            if (!knowledge_in_range(unit, 0, 7) || unit->valueint >= idea->units || (expect->climax & (1u << unit->valueint)) != 0) {
                return expect_fail(message, message_size, path, id, name, "content.ideas", "expect.climax holds a unit outside the idea or a repeat");
            }
            expect->climax |= 1u << unit->valueint;
        }
    }
    const cJSON *sounds = knowledge_member(object, "sounds");
    if (sounds != NULL) {
        const cJSON *place;
        if (!cJSON_IsArray(sounds)) {
            return expect_fail(message, message_size, path, id, name, "content.ideas", "expect.sounds is not a list");
        }
        cJSON_ArrayForEach(place, sounds) {
            const cJSON *unit = cJSON_IsObject(place) ? knowledge_member(place, "unit") : NULL;
            unsigned bits = 0;
            if (!knowledge_in_range(unit, 0, 7) || unit->valueint >= idea->units ||
                !read_word_bits(knowledge_member(place, "kinds"), EXPECT_SOUNDS, COUNT_OF(EXPECT_SOUNDS), 5, &bits)) {
                return expect_fail(message, message_size, path, id, name, "content.ideas",
                                   "an expect.sounds place needs a unit of the idea and one to five distinct sound kinds");
            }
            expect->sounds[unit->valueint] |= bits;
        }
    }
    return KNOWLEDGE_OK;
}

KnowledgeStatus knowledge_form_read_theme_type(Slot *slot, const cJSON *entry, const char *path, char *message, size_t message_size) {
    const char *id = slot->entry.id;
    const cJSON *content = knowledge_member(entry, "content");
    if (!cJSON_IsObject(content)) {
        knowledge_fail(message, message_size, path, id, "content", "not an object");
        return KNOWLEDGE_ERR_FIELD;
    }
    ThemeType *type = &slot->theme_type;
    const char *code = knowledge_string_of(knowledge_member(content, "code"));
    if (code == NULL || !knowledge_lowercase_words(code, '-')) {
        knowledge_fail(message, message_size, path, id, "content.code", code == NULL ? "missing" : "not lowercase words joined by hyphens");
        return KNOWLEDGE_ERR_FIELD;
    }
    type->code = knowledge_own(slot, code);
    if (type->code == NULL) {
        return KNOWLEDGE_ERR_ALLOC;
    }
    const char *beginning = NULL;
    const char *ending = NULL;
    KnowledgeStatus status = knowledge_read_text(slot, content, "beginning", &beginning, path, message, message_size);
    if (status == KNOWLEDGE_OK) {
        status = knowledge_read_text(slot, content, "ending", &ending, path, message, message_size);
    }
    if (status != KNOWLEDGE_OK) {
        return status;
    }
    slot->beginning_target = beginning;
    slot->ending_target = ending;
    const cJSON *ideas = knowledge_member(content, "ideas");
    int count = cJSON_IsArray(ideas) ? cJSON_GetArraySize(ideas) : -1;
    if (count < 2 || count > THEME_TYPE_IDEA_MAX) {
        knowledge_fail(message, message_size, path, id, "content.ideas", ideas == NULL ? "missing" : "not 2 to 6 ideas");
        return KNOWLEDGE_ERR_FIELD;
    }
    const cJSON *item;
    cJSON_ArrayForEach(item, ideas) {
        ThemeTypeIdea *idea = &type->ideas[type->idea_count];
        const char *name = cJSON_IsObject(item) ? knowledge_string_of(knowledge_member(item, "name")) : NULL;
        const char *function = cJSON_IsObject(item) ? knowledge_string_of(knowledge_member(item, "function")) : NULL;
        int slot_at = cJSON_IsObject(item) ? knowledge_word_index(knowledge_string_of(knowledge_member(item, "slot")), SLOTS, COUNT_OF(SLOTS)) : -1;
        int half = cJSON_IsObject(item) ? knowledge_word_index(knowledge_string_of(knowledge_member(item, "half")), HALVES, 2) : -1;
        int cadence = cJSON_IsObject(item) ? knowledge_word_index(knowledge_string_of(knowledge_member(item, "cadence")), IDEA_CADENCES, 3) : -1;
        if (name == NULL || function == NULL || slot_at < 0 || half < 0 || cadence < 0 ||
            !knowledge_in_range(knowledge_member(item, "units"), 1, 8)) {
            knowledge_fail(message, message_size, path, id, "content.ideas",
                           "an idea needs a name, a function id, a slot, a half, units from 1 to 8 and a cadence");
            return KNOWLEDGE_ERR_FIELD;
        }
        idea->name = knowledge_own(slot, name);
        slot->idea_targets[type->idea_count] = knowledge_own(slot, function);
        if (idea->name == NULL || slot->idea_targets[type->idea_count] == NULL) {
            return KNOWLEDGE_ERR_ALLOC;
        }
        idea->slot = SLOTS[slot_at];
        idea->in_beginning = half == 0;
        idea->units = knowledge_member(item, "units")->valueint;
        idea->cadence = (ThemeIdeaCadence)cadence;
        KnowledgeStatus expect_status = read_expect(slot, item, idea, path, message, message_size);
        if (expect_status != KNOWLEDGE_OK) {
            return expect_status;
        }
        type->idea_count++;
    }
    return KNOWLEDGE_OK;
}

static const FormalFunction *function_named(const KnowledgeSnapshot *snapshot, const char *target) {
    const Slot *found = knowledge_find_slot(snapshot, target);
    return found != NULL && found->has_function ? &found->function : NULL;
}

KnowledgeStatus knowledge_form_link(const KnowledgeSnapshot *snapshot, Slot *slot, char *message, size_t message_size) {
    const char *id = slot->entry.id;
    for (size_t i = 0; slot->has_function && i < slot->function.harmony_count; i++) {
        const Slot *found = knowledge_find_slot(snapshot, slot->harmony_targets[i]);
        if (found == NULL || !found->has_vocabulary || found->vocabulary.subtype != VOCABULARY_PROGRESSION_TYPE) {
            knowledge_fail(message, message_size, slot->path, id, "content.harmony", "an id is not a progression-type vocabulary entry");
            return KNOWLEDGE_ERR_FIELD;
        }
        slot->function.harmony[i] = &found->vocabulary;
    }
    if (slot->has_function && slot->against_target != NULL) {
        slot->function.contrast_against = function_named(snapshot, slot->against_target);
        if (slot->function.contrast_against == NULL) {
            knowledge_fail(message, message_size, slot->path, id, "content.contrast.against", "is not a formal-function entry");
            return KNOWLEDGE_ERR_FIELD;
        }
    }
    if (!slot->has_theme_type) {
        return KNOWLEDGE_OK;
    }
    ThemeType *type = &slot->theme_type;
    type->beginning = function_named(snapshot, slot->beginning_target);
    if (type->beginning == NULL || type->beginning->level == FORMAL_LEVEL_IDEA) {
        knowledge_fail(message, message_size, slot->path, id, "content.beginning", "is not a formal function of subphrase or phrase level");
        return KNOWLEDGE_ERR_FIELD;
    }
    type->ending = function_named(snapshot, slot->ending_target);
    if (type->ending == NULL || type->ending->level == FORMAL_LEVEL_IDEA) {
        knowledge_fail(message, message_size, slot->path, id, "content.ending", "is not a formal function of subphrase or phrase level");
        return KNOWLEDGE_ERR_FIELD;
    }
    for (size_t i = 0; i < type->idea_count; i++) {
        type->ideas[i].function = function_named(snapshot, slot->idea_targets[i]);
        if (type->ideas[i].function == NULL || type->ideas[i].function->level != FORMAL_LEVEL_IDEA) {
            knowledge_fail(message, message_size, slot->path, id, "content.ideas", "an idea's function is not a formal function of idea level");
            return KNOWLEDGE_ERR_FIELD;
        }
        ThemeTypeIdea *idea = &type->ideas[i];
        for (size_t k = 0; k < idea->expect.harmony_count; k++) {
            ThemeHarmonyTemplate *template = &idea->expect.harmony[k];
            const Slot *found = knowledge_find_slot(snapshot, template->type_id);
            if (found == NULL || !found->has_vocabulary || found->vocabulary.subtype != VOCABULARY_PROGRESSION_TYPE) {
                knowledge_fail(message, message_size, slot->path, id, "content.ideas", "an expect harmony type is not a progression-type vocabulary entry");
                return KNOWLEDGE_ERR_FIELD;
            }
            /* The function's own link may not have run yet: compare its target ids. */
            const Slot *function_slot = knowledge_find_slot(snapshot, slot->idea_targets[i]);
            bool listed = false;
            for (size_t h = 0; function_slot != NULL && h < function_slot->function.harmony_count; h++) {
                listed = listed || strcmp(function_slot->harmony_targets[h], template->type_id) == 0;
            }
            if (!listed) {
                char text[200];
                snprintf(text, sizeof text, "%s idea '%s': harmony type %s is not one of %s's", id, idea->name, template->type_id, idea->function->id);
                knowledge_fail(message, message_size, slot->path, id, "content.ideas", text);
                return KNOWLEDGE_ERR_FIELD;
            }
            template->type = &found->vocabulary;
        }
    }
    return KNOWLEDGE_OK;
}
