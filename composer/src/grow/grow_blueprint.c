#include <ursatz_composer/grow_types.h>

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>

#include <ursatz_composer/given.h>
#include <ursatz_composer/motive.h>
#include <ursatz_composer/theme_shape.h>

#include "grow/grow_internal.h"
#include "util/util.h"

/*
 * Theme blueprints (R-743, docs/blueprints.md): the `blueprint` member of a theme request read and checked bar by bar, then built
 * into the theme's plan with the rest of the request.
 */

/* ------------------------------------------------------------ reading */

static UrsatzStatus fail(UrsatzError *error, const char *path, const char *format, ...) __attribute__((format(printf, 3, 4)));

static UrsatzStatus fail(UrsatzError *error, const char *path, const char *format, ...) {
    if (error) {
        va_list args;
        va_start(args, format);
        vsnprintf(error->message, sizeof error->message, format, args);
        va_end(args);
        snprintf(error->path, sizeof error->path, "%s", path);
    }
    return URSATZ_ERR_REQUEST;
}

/* The words of the jobs, in GrowJob order from GROW_JOB_MOTIF. */
static const char *const JOB_WORDS[] = {"motif", "variant", "contrast", "running", "long", "syncopated", "liquidation", "cadence"};

/* A time in beats of the meter's beat unit, as a chord onset writes it ("3/2", "2"). */
static void beats_text(Rational time, const PlanMeter *meter, char *out, size_t size) {
    Rational beats = {0, 1};
    if (rational_mul_int(time, meter->beat_unit, &beats) != RATIONAL_OK) {
        beats = time;
    }
    if (beats.den == 1) {
        snprintf(out, size, "%lld", (long long)beats.num);
    } else {
        snprintf(out, size, "%lld/%lld", (long long)beats.num, (long long)beats.den);
    }
}

/* The meter as text, "4/4". */
static void meter_text(const PlanMeter *meter, char *out, size_t size) {
    snprintf(out, size, "%d/%d", (int)meter->beats_per_bar, (int)meter->beat_unit);
}

/*
 * One bar of notes (rhythm false) or of a rhythm (true) into line: the text read with the note-list reader, which takes a closing
 * rest because a note follows it (a whole note, dropped), and which must fill the bar exactly.
 */
static UrsatzStatus read_line(const char *text, bool rhythm, const KnowledgeSnapshot *knowledge, const PlanMeter *meter,
                              Rational bar_length, const char *path, const char *what, size_t bar, GrowBlueprintLine *line,
                              UrsatzError *error) {
    size_t length = strlen(text);
    while (length > 0 && isspace((unsigned char)text[length - 1])) {
        length--;
    }
    if (length == 0) {
        return fail(error, path, "bar %zu %s: the text is empty", bar, what);
    }
    if (text[length - 1] == '~') {
        return fail(error, path, "bar %zu %s: a bar ends on a note or a rest, not on a tie", bar, what);
    }
    char *closed = malloc(length + 8);
    if (!closed) {
        return fail(error, path, "out of memory");
    }
    snprintf(closed, length + 8, "%.*s %s", (int)length, text, rhythm ? "x:w" : "C4:w");
    MotiveOptions options;
    memset(&options, 0, sizeof(options));
    options.has_meter = true;
    options.meter = *meter;
    options.knowledge = knowledge;
    Motive motive;
    MotiveError detail;
    MotiveResult read = rhythm ? motive_from_rhythm_text(closed, &options, &motive, &detail)
                               : motive_from_notes_text(closed, &options, &motive, &detail);
    free(closed);
    if (read != MOTIVE_OK) {
        return fail(error, path, "bar %zu %s: %s%s%s", bar, what, detail.message, detail.location[0] ? " at " : "",
                    detail.location);
    }
    const MotiveSketchNote *end = &motive.notes[motive.note_count - 1];
    if (rational_cmp(end->onset, bar_length) != 0) {
        char have[32];
        char want[32];
        beats_text(end->onset, meter, have, sizeof(have));
        beats_text(bar_length, meter, want, sizeof(want));
        return fail(error, path, "bar %zu %s: the notes fill %s beats; a bar has %s", bar, what, have, want);
    }
    line->count = motive.note_count - 1;
    for (size_t i = 0; i < line->count; i++) {
        line->notes[i] = plan_note(motive_pitch(&motive, i), motive.notes[i].onset, motive.notes[i].duration);
    }
    return URSATZ_OK;
}

/* A pitch written as a note name ("Ab5"); false when it is not one. */
static bool read_pitch(const char *text, const KnowledgeSnapshot *knowledge, int *out) {
    char note[24];
    if (!text || strlen(text) == 0 || strlen(text) > 8 || strchr(text, ':') || strchr(text, ' ') || text[0] == 'r') {
        return false;
    }
    snprintf(note, sizeof(note), "%s:q", text);
    MotiveOptions options;
    memset(&options, 0, sizeof(options));
    options.has_meter = true;
    options.meter = (PlanMeter){4, 4, 0};
    options.knowledge = knowledge;
    Motive motive;
    if (motive_from_notes_text(note, &options, &motive, NULL) != MOTIVE_OK || motive.note_count != 1) {
        return false;
    }
    *out = motive_pitch(&motive, 0);
    return true;
}

/* An onset in beats of the beat unit from the bar's start: a whole number, or a fraction text "n" or "n/d". */
static bool read_at(const cJSON *item, const PlanMeter *meter, Rational bar_length, Rational *out) {
    long long num = 0;
    long long den = 1;
    if (cJSON_IsNumber(item)) {
        if (item->valuedouble < 0 || item->valuedouble > 64 || (double)(long long)item->valuedouble != item->valuedouble) {
            return false;
        }
        num = (long long)item->valuedouble;
    } else if (cJSON_IsString(item)) {
        const char *at = item->valuestring;
        char *end = NULL;
        if (!isdigit((unsigned char)*at)) {
            return false;
        }
        num = strtoll(at, &end, 10);
        if (*end == '/') {
            const char *below = end + 1;
            if (!isdigit((unsigned char)*below)) {
                return false;
            }
            den = strtoll(below, &end, 10);
        }
        if (*end != '\0' || den <= 0 || num > 4096 || den > 4096) {
            return false;
        }
    } else {
        return false;
    }
    Rational beats;
    return rational_make(num, den, &beats) == RATIONAL_OK &&
           rational_make(beats.num, beats.den * meter->beat_unit, out) == RATIONAL_OK && rational_cmp(*out, bar_length) < 0;
}

/* The members a bar may have. */
static bool known_member(const char *name) {
    static const char *const MEMBERS[] = {"chords", "melody", "rhythm", "job", "anchors", "events", "bass", "inner"};
    for (size_t i = 0; i < sizeof(MEMBERS) / sizeof(MEMBERS[0]); i++) {
        if (strcmp(name, MEMBERS[i]) == 0) {
            return true;
        }
    }
    return false;
}

/* A string member of a bar, NULL when it is absent or null; false (with the refusal) when it is something else. */
static bool bar_string(const cJSON *bar_item, const char *name, size_t bar, const char **out,
                       UrsatzError *error) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(bar_item, name);
    *out = NULL;
    if (!item || cJSON_IsNull(item)) {
        return true;
    }
    if (!cJSON_IsString(item)) {
        char path[96];
        snprintf(path, sizeof(path), "/blueprint/bars/%zu/%s", bar, name);
        fail(error, path, "bar %zu: \"%s\" must be a string", bar, name);
        return false;
    }
    *out = item->valuestring;
    return true;
}

/* The job a bar names: a job word, or the id of a K11 rhythm entry of the meter. */
static UrsatzStatus read_job(const char *text, const KnowledgeSnapshot *knowledge, const PlanMeter *meter, const char *path,
                             size_t bar, GrowBlueprintBar *out, UrsatzError *error) {
    for (size_t j = 0; j < sizeof(JOB_WORDS) / sizeof(JOB_WORDS[0]); j++) {
        if (strcmp(text, JOB_WORDS[j]) == 0) {
            out->job = (GrowJob)(GROW_JOB_MOTIF + j);
            return URSATZ_OK;
        }
    }
    const PatternEntry *entry = strchr(text, '.') && knowledge ? knowledge_snapshot_pattern(knowledge, text) : NULL;
    if (!entry || !entry->is_rhythm) {
        return fail(error, path,
                    "bar %zu: '%s' is not a rhythm job (motif, variant, contrast, running, long, syncopated, liquidation, cadence) "
                    "or the id of a K11 rhythm",
                    bar, text);
    }
    char want[16];
    meter_text(meter, want, sizeof(want));
    if (!entry->rhythm.meter || strcmp(entry->rhythm.meter, want) != 0) {
        return fail(error, path, "bar %zu: the rhythm %s is in %s, not %s", bar, text, entry->rhythm.meter ? entry->rhythm.meter : "?",
                    want);
    }
    out->job = GROW_JOB_ENTRY;
    out->job_entry = entry;
    return URSATZ_OK;
}

/* The anchors (events false) or the events (true) of a bar. */
static UrsatzStatus read_marks(const cJSON *list, bool events, const KnowledgeSnapshot *knowledge, const PlanMeter *meter,
                               Rational bar_length, size_t bar, GrowBlueprintBar *out, UrsatzError *error) {
    const char *name = events ? "events" : "anchors";
    size_t most = events ? GROW_BLUEPRINT_EVENTS : GROW_BLUEPRINT_ANCHORS;
    char path[128];
    snprintf(path, sizeof(path), "/blueprint/bars/%zu/%s", bar, name);
    if (!list || cJSON_IsNull(list)) {
        return URSATZ_OK;
    }
    if (!cJSON_IsArray(list) || (size_t)cJSON_GetArraySize(list) > most) {
        return fail(error, path, "bar %zu: \"%s\" must be a list of up to %zu {\"at\", \"%s\"}", bar, name, most,
                    events ? "type" : "pitch");
    }
    size_t index = 0;
    const cJSON *item = NULL;
    cJSON_ArrayForEach(item, list) {
        char item_path[160];
        snprintf(item_path, sizeof(item_path), "%s/%zu", path, index++);
        const cJSON *at = cJSON_IsObject(item) ? cJSON_GetObjectItemCaseSensitive(item, "at") : NULL;
        const cJSON *what = cJSON_IsObject(item) ? cJSON_GetObjectItemCaseSensitive(item, events ? "type" : "pitch") : NULL;
        if (!at || !cJSON_IsString(what) || (cJSON_IsObject(item) && cJSON_GetArraySize(item) != 2)) {
            return fail(error, item_path, "bar %zu: each of \"%s\" is {\"at\": \"<beats>\", \"%s\": ...}", bar, name,
                        events ? "type" : "pitch");
        }
        Rational offset;
        if (!read_at(at, meter, bar_length, &offset)) {
            char at_path[176];
            snprintf(at_path, sizeof(at_path), "%s/at", item_path);
            return fail(error, at_path, "bar %zu: \"at\" is an onset in beats inside the bar (\"0\", \"3/2\")", bar);
        }
        if (events) {
            PlanEventType type;
            if (!plan_event_type_named(what->valuestring, &type)) {
                char type_path[176];
                snprintf(type_path, sizeof(type_path), "%s/type", item_path);
                return fail(error, type_path,
                            "bar %zu: '%s' is not an event: appoggiatura, suspension, passing, neighbor, escape, anticipation or "
                            "retardation",
                            bar, what->valuestring);
            }
            out->events[out->event_count++] = (PlanEvent){offset, type};
        } else {
            int midi;
            if (!read_pitch(what->valuestring, knowledge, &midi)) {
                char pitch_path[176];
                snprintf(pitch_path, sizeof(pitch_path), "%s/pitch", item_path);
                return fail(error, pitch_path, "bar %zu: '%s' is not a pitch (a note name such as Ab5)", bar, what->valuestring);
            }
            out->anchors[out->anchor_count++] = (PlanAnchor){offset, midi};
        }
    }
    return URSATZ_OK;
}

/* Whether the notes fill the bar with no rest: the first at 0, each where the one before ends, the last at the bar's end. */
static bool line_tiles(const GrowBlueprintLine *line, Rational bar_length) {
    Rational end = {0, 1};
    for (size_t i = 0; i < line->count; i++) {
        if (rational_cmp(line->notes[i].offset, end) != 0 || rational_add(end, line->notes[i].duration, &end) != RATIONAL_OK) {
            return false;
        }
    }
    return line->count > 0 && rational_cmp(end, bar_length) == 0;
}

/* One bar of the list. */
static UrsatzStatus read_bar(const cJSON *item, const KnowledgeSnapshot *knowledge, const PlanMeter *meter, Rational bar_length,
                             size_t bar, GrowBlueprintBar *out, UrsatzError *error) {
    char base[48];
    char path[96];
    snprintf(base, sizeof(base), "/blueprint/bars/%zu", bar);
    if (!cJSON_IsObject(item)) {
        return fail(error, base, "bar %zu must be an object: {\"chords\", \"melody\" | \"rhythm\" | \"job\", ...}", bar);
    }
    for (const cJSON *member = item->child; member; member = member->next) {
        if (!known_member(member->string)) {
            snprintf(path, sizeof(path), "%s/%.40s", base, member->string);
            return fail(error, path,
                        "bar %zu: unknown member '%s'; a bar has chords, melody, rhythm, job, anchors, events, bass and inner", bar,
                        member->string);
        }
    }
    const char *chords;
    const char *melody;
    const char *rhythm;
    const char *job;
    const char *bass;
    if (!bar_string(item, "chords", bar, &chords, error) || !bar_string(item, "melody", bar, &melody, error) ||
        !bar_string(item, "rhythm", bar, &rhythm, error) || !bar_string(item, "job", bar, &job, error) ||
        !bar_string(item, "bass", bar, &bass, error)) {
        return URSATZ_ERR_REQUEST;
    }
    if (chords) {
        if (strlen(chords) >= sizeof(out->chords)) {
            snprintf(path, sizeof(path), "%s/chords", base);
            return fail(error, path, "bar %zu: the chord text is too long", bar);
        }
        snprintf(out->chords, sizeof(out->chords), "%s", chords);
    }
    if (melody && (rhythm || job)) {
        snprintf(path, sizeof(path), "/blueprint/bars/%zu/%s", bar, rhythm ? "rhythm" : "job");
        return fail(error, path, "bar %zu gives a melody and a %s: give one of melody, rhythm and job", bar, rhythm ? "rhythm" : "job");
    }
    if (rhythm && job) {
        snprintf(path, sizeof(path), "%s/job", base);
        return fail(error, path, "bar %zu gives a rhythm and a job: give one of melody, rhythm and job", bar);
    }
    UrsatzStatus status = URSATZ_OK;
    if (melody) {
        snprintf(path, sizeof(path), "%s/melody", base);
        status = read_line(melody, false, knowledge, meter, bar_length, path, "melody", bar, &out->melody, error);
        if (status == URSATZ_OK && out->melody.count == 0) {
            status = fail(error, path, "bar %zu melody: give a note at least (leave the melody out to have it composed)", bar);
        }
        out->has_melody = true;
    }
    if (status == URSATZ_OK && rhythm) {
        snprintf(path, sizeof(path), "%s/rhythm", base);
        status = read_line(rhythm, true, knowledge, meter, bar_length, path, "rhythm", bar, &out->rhythm, error);
        if (status == URSATZ_OK && out->rhythm.count == 0) {
            status = fail(error, path, "bar %zu rhythm: give a note at least", bar);
        }
        out->has_rhythm = true;
    }
    if (status == URSATZ_OK && job) {
        snprintf(path, sizeof(path), "%s/job", base);
        status = read_job(job, knowledge, meter, path, bar, out, error);
    }
    if (status == URSATZ_OK) {
        status = read_marks(cJSON_GetObjectItemCaseSensitive(item, "anchors"), false, knowledge, meter, bar_length, bar, out, error);
    }
    if (status == URSATZ_OK) {
        status = read_marks(cJSON_GetObjectItemCaseSensitive(item, "events"), true, knowledge, meter, bar_length, bar, out, error);
    }
    if (status == URSATZ_OK && out->has_melody && (out->anchor_count > 0 || out->event_count > 0)) {
        snprintf(path, sizeof(path), "/blueprint/bars/%zu/%s", bar, out->anchor_count > 0 ? "anchors" : "events");
        status = fail(error, path, "bar %zu gives its melody: anchors and events go on a bar whose melody is composed", bar);
    }
    if (status == URSATZ_OK && bass) {
        snprintf(path, sizeof(path), "%s/bass", base);
        status = read_line(bass, false, knowledge, meter, bar_length, path, "bass", bar, &out->bass, error);
        if (status == URSATZ_OK && out->bass.count == 0) {
            status = fail(error, path, "bar %zu bass: give a note at least", bar);
        }
        if (status == URSATZ_OK && !out->has_melody && !line_tiles(&out->bass, bar_length)) {
            status = fail(error, path, "bar %zu's bass rests and its melody is composed: a bass rests only under a given melody", bar);
        }
        out->has_bass = true;
    }
    const cJSON *inner = cJSON_GetObjectItemCaseSensitive(item, "inner");
    if (status == URSATZ_OK && inner && !cJSON_IsNull(inner)) {
        snprintf(path, sizeof(path), "%s/inner", base);
        if (!cJSON_IsArray(inner) || cJSON_GetArraySize(inner) < 1 || cJSON_GetArraySize(inner) > GROW_BLUEPRINT_INNER) {
            return fail(error, path, "bar %zu: \"inner\" must be a list of 1 to %d note lists, one a voice", bar, GROW_BLUEPRINT_INNER);
        }
        const cJSON *voice = NULL;
        cJSON_ArrayForEach(voice, inner) {
            char voice_path[112];
            char what[24];
            snprintf(voice_path, sizeof(voice_path), "%s/%zu", path, out->inner_count);
            snprintf(what, sizeof(what), "inner voice %zu", out->inner_count + 1);
            if (!cJSON_IsString(voice)) {
                return fail(error, voice_path, "bar %zu: each inner voice is a note list", bar);
            }
            status = read_line(voice->valuestring, false, knowledge, meter, bar_length, voice_path, what, bar,
                               &out->inner[out->inner_count], error);
            if (status != URSATZ_OK) {
                return status;
            }
            out->inner_count++;
        }
    }
    return status;
}

UrsatzStatus grow_blueprint_read(const cJSON *json, const KnowledgeSnapshot *knowledge, const PlanMeter *meter, size_t bars,
                                 GrowBlueprint *out, UrsatzError *error) {
    memset(out, 0, sizeof(*out));
    Rational bar_length;
    if (!cJSON_IsObject(json)) {
        return fail(error, "/blueprint", "\"blueprint\" must be an object: {\"bars\": [...], \"climax\", \"accompaniment\"}");
    }
    if (plan_meter_bar_length(meter, &bar_length) != PLAN_OK || bar_length.num <= 0) {
        return fail(error, "/meter", "the meter has no bar length");
    }
    for (const cJSON *member = json->child; member; member = member->next) {
        if (strcmp(member->string, "bars") != 0 && strcmp(member->string, "climax") != 0 &&
            strcmp(member->string, "accompaniment") != 0) {
            char path[96];
            snprintf(path, sizeof(path), "/blueprint/%.60s", member->string);
            return fail(error, path, "unknown member '%s'; a blueprint has bars, climax and accompaniment", member->string);
        }
    }
    const cJSON *list = cJSON_GetObjectItemCaseSensitive(json, "bars");
    if (!cJSON_IsArray(list) || cJSON_GetArraySize(list) < 1 || cJSON_GetArraySize(list) > GROW_BLUEPRINT_BARS) {
        return fail(error, "/blueprint/bars", "a blueprint needs \"bars\": a list of 1 to %d bar objects", GROW_BLUEPRINT_BARS);
    }
    if ((size_t)cJSON_GetArraySize(list) != bars) {
        return fail(error, "/blueprint/bars", "the blueprint has %d bars; the theme has %zu", cJSON_GetArraySize(list), bars);
    }
    const cJSON *item = NULL;
    cJSON_ArrayForEach(item, list) {
        UrsatzStatus status = read_bar(item, knowledge, meter, bar_length, out->bar_count, &out->bars[out->bar_count], error);
        if (status != URSATZ_OK) {
            return status;
        }
        out->bar_count++;
    }
    /* Bass and inner voices are given in every bar or in none, as many inner voices in each. */
    for (size_t bar = 1; bar < out->bar_count; bar++) {
        const GrowBlueprintBar *first = &out->bars[0];
        const GrowBlueprintBar *this = &out->bars[bar];
        char path[64];
        if (this->has_bass != first->has_bass) {
            size_t without = this->has_bass ? 0 : bar;
            snprintf(path, sizeof(path), "/blueprint/bars/%zu", without);
            return fail(error, path, "a blueprint gives a bass in every bar or in none: bar %zu has none", without);
        }
        if (this->inner_count != first->inner_count) {
            snprintf(path, sizeof(path), "/blueprint/bars/%zu/inner", bar);
            return fail(error, path, "bar %zu has %zu inner voices and bar 0 has %zu: every bar gives as many", bar, this->inner_count,
                        first->inner_count);
        }
    }
    const cJSON *climax = cJSON_GetObjectItemCaseSensitive(json, "climax");
    if (climax && !cJSON_IsNull(climax)) {
        const cJSON *bar = cJSON_IsObject(climax) ? cJSON_GetObjectItemCaseSensitive(climax, "bar") : NULL;
        const cJSON *at = cJSON_IsObject(climax) ? cJSON_GetObjectItemCaseSensitive(climax, "at") : NULL;
        const cJSON *pitch = cJSON_IsObject(climax) ? cJSON_GetObjectItemCaseSensitive(climax, "pitch") : NULL;
        if (!cJSON_IsNumber(bar) || !at || !cJSON_IsString(pitch) || cJSON_GetArraySize(climax) != 3) {
            return fail(error, "/blueprint/climax", "the climax is {\"bar\": <from 0>, \"at\": \"<beats>\", \"pitch\": \"Ab5\"}");
        }
        if (bar->valuedouble < 0 || bar->valuedouble >= (double)out->bar_count ||
            (double)(long)bar->valuedouble != bar->valuedouble) {
            return fail(error, "/blueprint/climax/bar", "the climax's bar is one of the theme's, 0 to %zu", out->bar_count - 1);
        }
        out->climax_bar = (size_t)bar->valuedouble;
        if (!read_at(at, meter, bar_length, &out->climax_at)) {
            return fail(error, "/blueprint/climax/at", "the climax's \"at\" is an onset in beats inside the bar (\"0\", \"3/2\")");
        }
        if (!read_pitch(pitch->valuestring, knowledge, &out->climax_midi)) {
            return fail(error, "/blueprint/climax/pitch", "'%s' is not a pitch (a note name such as Ab5)", pitch->valuestring);
        }
        out->has_climax = true;
    }
    const cJSON *accompaniment = cJSON_GetObjectItemCaseSensitive(json, "accompaniment");
    out->has_accompaniment = accompaniment && !cJSON_IsNull(accompaniment);
    if (out->has_accompaniment && out->bars[0].inner_count > 0) {
        return fail(error, "/blueprint/bars/0/inner",
                    "the accompaniment composes the inner voices: give the inner voices or blueprint.accompaniment, not both");
    }
    return URSATZ_OK;
}

/* ------------------------------------------------------------ the ideas */

/* The bars an idea lies over: from `first`, `count` of them. False when it has no length. */
static bool idea_bars(Rational start, Rational length, Rational bar, size_t *first, size_t *count) {
    Rational end;
    Rational at;
    long long from;
    long long to;
    if (length.num <= 0 || rational_add(start, length, &end) != RATIONAL_OK || rational_div(start, bar, &at) != RATIONAL_OK ||
        rational_floor(at, &from) != RATIONAL_OK || rational_div(end, bar, &at) != RATIONAL_OK || rational_ceil(at, &to) != RATIONAL_OK ||
        to <= from || from < 0) {
        return false;
    }
    *first = (size_t)from;
    *count = (size_t)(to - from);
    return true;
}

static bool gives_rhythm(const GrowBlueprintBar *bar) {
    return bar->has_rhythm || bar->job != GROW_JOB_NONE;
}

/* "bars 2 and 3" or "bars 4 to 7". */
static void bars_text(size_t first, size_t count, char *out, size_t size) {
    if (count == 2) {
        snprintf(out, size, "bars %zu and %zu", first, first + 1);
    } else {
        snprintf(out, size, "bars %zu to %zu", first, first + count - 1);
    }
}

/* An idea's bars give a melody in all or none, and a rhythm or a job in all or none. */
static UrsatzStatus check_idea(const GrowBlueprint *blueprint, size_t first, size_t count, UrsatzError *error) {
    char which[48];
    char path[64];
    bars_text(first, count, which, sizeof(which));
    for (size_t bar = first + 1; bar < first + count && bar < blueprint->bar_count; bar++) {
        const GrowBlueprintBar *head = &blueprint->bars[first];
        const GrowBlueprintBar *this = &blueprint->bars[bar];
        snprintf(path, sizeof(path), "/blueprint/bars/%zu", bar);
        if (this->has_melody != head->has_melody) {
            return fail(error, path, "%s are one idea: give a melody in %s", which, count == 2 ? "both or neither" : "all or none");
        }
        if (gives_rhythm(this) != gives_rhythm(head)) {
            return fail(error, path, "%s are one idea: give a rhythm or a job in %s", which,
                        count == 2 ? "both or neither" : "all or none");
        }
    }
    return URSATZ_OK;
}

UrsatzStatus grow_blueprint_check_ideas(const GrowBlueprint *blueprint, const SectionPairing *pairing, const PlanMeter *meter,
                                        size_t bars, const SectionConstruction *construction, bool basic_slot, UrsatzError *error) {
    GrowThemeRequest request;
    GrowLayout layout;
    memset(&request, 0, sizeof(request));
    request.pairing = *pairing;
    request.meter = *meter;
    request.bars = bars;
    if (construction) {
        request.has_construction = true;
        request.construction = *construction;
    }
    if (plan_meter_bar_length(meter, &request.bar) != PLAN_OK || !grow_layout_of(&request, &layout)) {
        return fail(error, "/blueprint/bars", "the ideas of this theme cannot be laid out over its bars");
    }
    char path[64];
    for (size_t place = 0; place < GROW_PLACE_COUNT; place++) {
        size_t first;
        size_t count;
        if (!idea_bars(layout.start[place], layout.length[place], request.bar, &first, &count)) {
            continue;
        }
        UrsatzStatus status = check_idea(blueprint, first, count, error);
        if (status != URSATZ_OK) {
            return status;
        }
        if (place != GROW_PLACE_BASIC) {
            continue;
        }
        for (size_t bar = first; bar < first + count && bar < blueprint->bar_count; bar++) {
            const GrowBlueprintBar *this = &blueprint->bars[bar];
            if (basic_slot && (this->has_melody || gives_rhythm(this))) {
                snprintf(path, sizeof(path), "/blueprint/bars/%zu/%s", bar,
                         this->has_melody ? "melody" : this->has_rhythm ? "rhythm" : "job");
                return fail(error, path, "slots.basic gives the basic idea: its bar %zu gives no melody, rhythm or job", bar);
            }
            if (this->job == GROW_JOB_MOTIF || this->job == GROW_JOB_VARIANT || this->job == GROW_JOB_CONTRAST) {
                snprintf(path, sizeof(path), "/blueprint/bars/%zu/job", bar);
                return fail(error, path, "bar %zu: the job %s is made from the basic idea's rhythm, so the basic idea cannot take it", bar,
                            JOB_WORDS[this->job - GROW_JOB_MOTIF]);
            }
        }
        if (!basic_slot && !blueprint->bars[first].has_melody && !gives_rhythm(&blueprint->bars[first])) {
            snprintf(path, sizeof(path), "/blueprint/bars/%zu", first);
            return fail(error, path, "the basic idea needs a melody, a rhythm or a job in its bars, or slots.basic");
        }
    }
    /* A repeat, an evaded cadence's idea or an addition has no idea motive of its own: a melody or nothing. */
    for (size_t k = 0; k < layout.extension_count; k++) {
        size_t first;
        size_t count;
        if (layout.extensions[k].kind == SECTION_EXTENSION_STRETCH ||
            !idea_bars(layout.extensions[k].start, layout.extensions[k].length, request.bar, &first, &count)) {
            continue;
        }
        UrsatzStatus status = check_idea(blueprint, first, count, error);
        if (status != URSATZ_OK) {
            return status;
        }
        for (size_t bar = first; bar < first + count && bar < blueprint->bar_count; bar++) {
            if (gives_rhythm(&blueprint->bars[bar])) {
                snprintf(path, sizeof(path), "/blueprint/bars/%zu/%s", bar, blueprint->bars[bar].has_rhythm ? "rhythm" : "job");
                return fail(error, path, "bar %zu is in an extension of the construction: give a melody there or leave it open", bar);
            }
        }
    }
    if (blueprint->bar_count > 0 && blueprint->bars[0].job == GROW_JOB_LIQUIDATION) {
        return fail(error, "/blueprint/bars/0/job", "bar 0: liquidation takes the rhythm of the bar before, and the first bar has none");
    }
    return URSATZ_OK;
}

/* ------------------------------------------------------------ building the plan */

/* Records a refusal of the build: its result, the request path of its field and its message. */
static GrowResult refuse(GrowRefusal *refusal, GrowResult result, const char *path, const char *format, ...)
    __attribute__((format(printf, 4, 5)));

static GrowResult refuse(GrowRefusal *refusal, GrowResult result, const char *path, const char *format, ...) {
    if (refusal) {
        va_list args;
        refusal->result = result;
        snprintf(refusal->path, sizeof(refusal->path), "%s", path);
        va_start(args, format);
        vsnprintf(refusal->message, sizeof(refusal->message), format, args);
        va_end(args);
    }
    return result;
}

GrowResult grow_blueprint_give_voices(Plan *plan, NodeId theme, const GrowBlueprint *blueprint, GrowRefusal *refusal) {
    for (size_t bar = 0; bar < blueprint->bar_count; bar++) {
        const GrowBlueprintBar *this = &blueprint->bars[bar];
        char path[64];
        if (this->has_bass) {
            GivenResult given = this->has_melody ? plan_give_bass_with_rests(plan, theme, bar, this->bass.notes, this->bass.count)
                                                 : plan_give_bass(plan, theme, bar, this->bass.notes, this->bass.count);
            if (given != GIVEN_OK) {
                snprintf(path, sizeof(path), "/blueprint/bars/%zu/bass", bar);
                return given == GIVEN_ERR_PLAN ? GROW_ERR_ALLOC
                                               : refuse(refusal, GROW_ERR_BAD_REQUEST, path, "bar %zu bass: %s", bar,
                                                        given_result_name(given));
            }
        }
        for (size_t voice = 0; voice < this->inner_count; voice++) {
            const GrowBlueprintLine *line = &this->inner[voice];
            if (line->count == 0) {
                continue;
            }
            GivenResult given = plan_give_inner_voice(plan, theme, bar, (int)voice + 1, line->notes, line->count);
            if (given != GIVEN_OK) {
                snprintf(path, sizeof(path), "/blueprint/bars/%zu/inner/%zu", bar, voice);
                return given == GIVEN_ERR_PLAN ? GROW_ERR_ALLOC
                                               : refuse(refusal, GROW_ERR_BAD_REQUEST, path, "bar %zu inner voice %zu: %s", bar,
                                                        voice + 1, given_result_name(given));
            }
        }
    }
    return GROW_OK;
}
