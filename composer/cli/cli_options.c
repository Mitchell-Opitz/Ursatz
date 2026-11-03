#include "cli_options.h"

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <standard-texture/texture_rules.h>

#include "api/api_grow.h"
#include "api/api_internal.h"

#include <ursatz_composer/harmony_phrase.h>
#include <ursatz_composer/plan_json.h>
#include "util/plan_util.h"
#include "util/util.h"

const char *cli_accompaniment_name(PlanAccompanimentPattern pattern) {
    return api_accompaniment_name(pattern);
}

int cli_read_accompaniment(const char *pattern_text, const char *texture_text, const char *meter_text, const PlanMeter *meter,
                           PlanAccompaniment *out) {
    ComposerError error;
    memset(&error, 0, sizeof error);
    ComposerStatus status = api_read_accompaniment(pattern_text, texture_text, meter_text, meter, out, &error);
    return status == COMPOSER_OK ? 0 : cli_fail_api(status, &error, NULL);
}

void cli_print_json_string(const char *text) {
    putchar('"');
    for (; *text; text++) {
        if (*text == '"' || *text == '\\') {
            putchar('\\');
        }
        if ((unsigned char)*text >= 0x20) {
            putchar(*text);
        }
    }
    putchar('"');
}

int cli_fail(int status, const char *format, ...) {
    va_list args;
    va_start(args, format);
    fprintf(stderr, "Error: ");
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
    va_end(args);
    return status;
}

int cli_fail_api(ComposerStatus status, const ComposerError *error, const char *input_name) {
    static const char *const OPTIONS[] = {"format", "form", "harmony", "feature", "role", "tone", "tempo",
                                          "count", "seed", "distance", "function", "depth", "mode", "suggest",
                                          "into", "hybrid", "bars", "cadence", "first_cadence", "accompaniment", "texture",
                                          "voicing", "style_weight", "blend_weight", "blend_weights", "blend", "key", "candidates", "style", "at", "cells", "like", "like_progression", "style_without"};
    char message[sizeof error->message + 256];
    const char *source = error->message;
    size_t used = 0;
    /* A message about a member starts with its name. */
    size_t lead = 0;
    bool about_form = strncmp(source, "form ", 5) == 0;
    for (size_t i = 0; i < sizeof OPTIONS / sizeof OPTIONS[0]; i++) {
        size_t length = strlen(OPTIONS[i]);
        if (strncmp(source, OPTIONS[i], length) == 0 && (source[length] == ' ' || source[length] == ':')) {
            used += (size_t)snprintf(message + used, sizeof message - used, "--");
            for (size_t c = 0; c < length; c++) {
                message[used++] = source[c] == '_' ? '-' : source[c];
            }
            source += length;
            lead = length;
            break;
        }
    }
    while (*source && used + 8 < sizeof message) {
        if (input_name && strncmp(source, "the input", 9) == 0) {
            used += (size_t)snprintf(message + used, sizeof message - used, "'%s'", input_name);
            source += 9;
        } else if (about_form && source > error->message + lead && (strncmp(source, "form", 4) == 0 || strncmp(source, "format", 6) == 0) &&
                   source[-1] == ' ') {
            size_t length = strncmp(source, "format", 6) == 0 ? 6 : 4;
            if (source[length] == ' ' || source[length] == '\0') {
                used += (size_t)snprintf(message + used, sizeof message - used, "--%.*s", (int)length, source);
                source += length;
            } else {
                message[used++] = *source++;
            }
        } else {
            message[used++] = *source++;
        }
    }
    message[used] = '\0';
    /* Phrases about members that name several options, or a request the command line has no word for. */
    static const char *const PHRASES[][2] = {
        {"each_op takes no chain, count, distance or functions", "--each-op takes no --chain, --count, --distance or --function"},
        {"a distance takes random chains, not a chain", "--distance takes random chains, not --chain"},
        {"composer_api_variant_operations lists them", "--list-ops shows them"},
        {"style_without needs style", "--style-without needs --style"},
        {"needs style", "needs --style"},
        {"needs blend", "needs --blend"},
        {"needs hybrid <beginning>", "needs --hybrid <beginning>"},
        {"applies only to into hybrid", "applies only to --into hybrid"},
        {"use into ", "use --into "},
        {"regenerate it with lanes harmony", "regenerate it with --lane harmony"},
        {"and texture chorale write the inner voices themselves; they cannot be combined with accompaniment or another "
         "texture",
         "and --texture chorale write the inner voices themselves; they cannot be combined with --accompaniment or another "
         "--texture"},
        {"leave out mode", "leave out --mode"},
        {"no size of count ", "no size of --count "},
        {" with count ", " with --count "},
        {"(or count ", "(or --count "},
        {"with mode \"minor\"", "with --mode minor"},
        {"needs a style: give style or", "needs a style: give --style or"},
        {"needs an at:", "needs an --at:"},
        {"needs corpus <dir>", "needs --from-corpus <dir>"},
        {"goes with a progression", "goes with --given-progression"},
        {"plan it with a given melody", "plan it with --given-melody"},
        {"corpus needs like <file> or a like_progression", "--from-corpus needs --like <file> or --like-progression"},
        {" and motive both give", " and --motive both give"},
        {"melody, voicing keeps the grown one", "melody, --voicing keeps the grown one"},
        {"accompaniment motive needs an accompaniment motive", "accompaniment motive needs --accompaniment-motive <file>"},
        {"the accompaniment motive is the accompaniment pattern; it cannot be combined with accompaniment ",
         "--accompaniment-motive is the accompaniment pattern; it cannot be combined with --accompaniment "},
        {"an accompaniment motive cannot be combined with voicing or texture chorale",
         "--accompaniment-motive cannot be combined with --voicing or --texture chorale"},
    };
    for (size_t i = 0; i < sizeof PHRASES / sizeof PHRASES[0]; i++) {
        char *at = strstr(message, PHRASES[i][0]);
        if (at) {
            char tail[sizeof message];
            snprintf(tail, sizeof tail, "%s", at + strlen(PHRASES[i][0]));
            snprintf(at, sizeof message - (size_t)(at - message), "%s%s", PHRASES[i][1], tail);
        }
    }
    return cli_fail(status == COMPOSER_ERR_UNSATISFIABLE ? CLI_EXIT_UNSATISFIABLE : CLI_EXIT_USAGE, "%s", message);
}

int cli_read_export_options(const char *tempo_text, bool plain_midi, PlanExportOptions *out) {
    *out = plan_export_options_default();
    if (plain_midi) {
        out->track_names = false;
        out->section_markers = false;
    }
    if (!tempo_text) {
        return 0;
    }
    ComposerRatio tempo;
    if (!request_parse_ratio(tempo_text, &tempo) || tempo.denominator == 0 || tempo.numerator < 20UL * tempo.denominator ||
        tempo.numerator > 400UL * tempo.denominator) {
        return cli_fail(CLI_EXIT_USAGE,
                        "--tempo must be from 20 to 400 quarter notes per minute, a whole number or a fraction like "
                        "195/2, not '%s'",
                        tempo_text);
    }
    out->tempo_numerator = tempo.numerator;
    out->tempo_denominator = tempo.denominator;
    return 0;
}

static const char *const STYLE_CATEGORIES[] = {
    STYLE_CATEGORY_CHORD_NUMERAL,     STYLE_CATEGORY_CHORD_PROGRESSION_2, STYLE_CATEGORY_CHORD_PROGRESSION_3,
    STYLE_CATEGORY_CADENCE_TYPE,      STYLE_CATEGORY_HARMONIC_RHYTHM,     STYLE_CATEGORY_DURATION,
    STYLE_CATEGORY_METER,             STYLE_CATEGORY_SYNCOPATION,         STYLE_CATEGORY_MELODIC_INTERVAL,
    STYLE_CATEGORY_CONTOUR,           STYLE_CATEGORY_PHRASE_RANGE,        STYLE_CATEGORY_MOTIVE_DEGREES,
    STYLE_CATEGORY_MOTIVE_INTERVALS,  STYLE_CATEGORY_MOTIVE_RHYTHM,       STYLE_CATEGORY_MOTIVE_NCT_FIGURE,
    STYLE_CATEGORY_THEME_FORM,        STYLE_CATEGORY_PHRASE_LENGTH,       STYLE_CATEGORY_SECTION_LETTERS,
    STYLE_CATEGORY_MODE,              STYLE_CATEGORY_TEMPO,               STYLE_CATEGORY_NOTE_DENSITY};

int cli_style_without_add(CliStyleWithout *list, const char *name) {
    bool known = false;
    for (size_t i = 0; i < sizeof STYLE_CATEGORIES / sizeof *STYLE_CATEGORIES; i++) {
        known = known || strcmp(name, STYLE_CATEGORIES[i]) == 0;
    }
    if (!known) {
        char names[640];
        size_t used = 0;
        names[0] = '\0';
        for (size_t i = 0; i < sizeof STYLE_CATEGORIES / sizeof *STYLE_CATEGORIES && used < sizeof names; i++) {
            int written = snprintf(names + used, sizeof names - used, "%s%s", i ? ", " : "", STYLE_CATEGORIES[i]);
            used += written > 0 ? (size_t)written : 0;
        }
        return cli_fail(CLI_EXIT_USAGE, "--style-without: '%s' is not a style category (the categories are %s)", name,
                        names);
    }
    for (size_t i = 0; i < list->count; i++) {
        if (strcmp(list->names[i], name) == 0) {
            return 0;
        }
    }
    if (list->count >= CLI_STYLE_WITHOUT_MAX) {
        return cli_fail(CLI_EXIT_USAGE, "--style-without is given at most %d times", CLI_STYLE_WITHOUT_MAX);
    }
    list->names[list->count++] = name;
    return 0;
}

int cli_load_style(const char *path, const char *const *without, size_t without_count, ComposerStyle **out) {
    ComposerError error;
    memset(&error, 0, sizeof error);
    ComposerStatus status = api_load_style(path, without, without_count, "/style", "/style_without", out, &error);
    return status == COMPOSER_OK ? 0 : cli_fail_api(status, &error, NULL);
}

int cli_read_blend(bool has_style, const char *blend_path, const char *all_text, const char *list_text,
                   ComposerStyleWeights *out) {
    ComposerError error;
    memset(&error, 0, sizeof error);
    ComposerStatus status = api_read_blend(has_style, blend_path, all_text, list_text, out, &error);
    return status == COMPOSER_OK ? 0 : cli_fail_api(status, &error, NULL);
}

int cli_load_blend(const char *style_path, const char *blend_path, const ComposerStyleWeights *weights,
                   const char *const *without, size_t without_count, ComposerStyle **out) {
    ComposerError error;
    memset(&error, 0, sizeof error);
    ComposerStatus status = api_load_blend(style_path, blend_path, weights, without, without_count, out, &error);
    return status == COMPOSER_OK ? 0 : cli_fail_api(status, &error, NULL);
}

void cli_print_json(const cJSON *document) {
    char *printed = cJSON_Print(document);
    if (!printed) {
        return;
    }
    /* cJSON indents with tabs and puts one after each colon; the commands write 2 spaces and one space. */
    for (const char *c = printed; *c; c++) {
        if (*c == '\n') {
            putchar('\n');
            size_t tabs = 0;
            while (c[1] == '\t') {
                tabs++;
                c++;
            }
            printf("%*s", (int)(tabs * 2), "");
        } else {
            putchar(*c == '\t' ? ' ' : *c);
        }
    }
    putchar('\n');
    cJSON_free(printed);
}

const char *cli_key_name(const AnalysisKey *key, char *out, size_t size) {
    util_key_name(analysis_key_tonic_pitch_class(key), key->mode == KEY_STRUCTURE_MODE_MINOR, out, size);
    return out;
}

const char *cli_cadence_name(CadenceType cadence) {
    return plan_cadence_name(cadence);
}

unsigned long cli_default_seed(void) {
    return api_default_seed();
}

bool cli_alternative_path(const char *path, size_t k, bool several, char *out, size_t size) {
    if (!several) {
        return snprintf(out, size, "%s", path) < (int)size;
    }
    const char *dot = strrchr(path, '.');
    const char *slash = strrchr(path, '/');
    if (!dot || (slash && dot < slash)) {
        return snprintf(out, size, "%s-%zu", path, k) < (int)size;
    }
    return snprintf(out, size, "%.*s-%zu%s", (int)(dot - path), path, k, dot) < (int)size;
}

int cli_write_explanation(const char *final_path, const char *write_path, const Explanation *explanation) {
    size_t length = strlen(final_path);
    bool json = length >= 5 && request_equals_ignore_case(final_path + length - 5, ".json");
    char *text = json ? explain_to_json(explanation) : explain_to_text(explanation);
    if (!text) {
        return -1;
    }
    FILE *file = fopen(write_path, "wb");
    bool ok = file && fputs(text, file) >= 0;
    free(text);
    if (file && fclose(file) != 0) {
        ok = false;
    }
    return ok ? 0 : -1;
}

/* --- seeding from a corpus ------------------------------------------------------- */

static int seed_failure(CorpusSeedResult result, const char *corpus_dir, const char *what) {
    switch (result) {
    case CORPUS_SEED_ERR_NO_MATCH:
        return cli_fail(CLI_EXIT_UNSATISFIABLE, "no passage of the corpus '%s' is similar to %s (a score of 1/2 or more)",
                        corpus_dir, what);
    case CORPUS_SEED_ERR_QUERY:
        return cli_fail(CLI_EXIT_USAGE, "cannot read %s as a passage to look for", what);
    case CORPUS_SEED_ERR_CORPUS:
        return cli_fail(CLI_EXIT_UNSATISFIABLE,
                        "cannot read the corpus '%s' (the output directory of ursatz-analyzer --corpus)", corpus_dir);
    default:
        return cli_fail(CLI_EXIT_UNSATISFIABLE, "%s", corpus_seed_result_name(result));
    }
}

static void seed_warn_stale(CorpusSeedResult result, const CorpusSeed *seed, const char *corpus_dir) {
    if (result == CORPUS_SEED_OK && seed->stale_documents > 0) {
        fprintf(stderr, "Warning: %zu stale documents in '%s' (analyzed by another version; run the corpus again)\n",
                seed->stale_documents, corpus_dir);
    }
}

int cli_seed_motive(const char *corpus_dir, const char *like_path, const PlanMeter *meter, const AnalysisKey *key,
                    unsigned long seed, CorpusSeed *out) {
    CorpusSeedResult result = corpus_seed_motive(corpus_dir, like_path, meter, key, seed, out);
    seed_warn_stale(result, out, corpus_dir);
    return result == CORPUS_SEED_OK ? 0 : seed_failure(result, corpus_dir, like_path);
}

int cli_seed_progression(const char *corpus_dir, const char *numerals_text, KeyStructureMode mode, CorpusSeed *out) {
    CorpusSeedResult result = corpus_seed_progression(corpus_dir, numerals_text, mode, out);
    seed_warn_stale(result, out, corpus_dir);
    return result == CORPUS_SEED_OK ? 0 : seed_failure(result, corpus_dir, numerals_text);
}

void cli_seed_numerals_text(const CorpusSeed *seed, char *out, size_t size) {
    size_t used = 0;
    out[0] = '\0';
    for (size_t i = 0; i < seed->numeral_count && used + 1 < size; i++) {
        int written = snprintf(out + used, size - used, i ? " %s" : "%s", seed->numerals[i]);
        if (written < 0 || (size_t)written >= size - used) {
            break;
        }
        used += (size_t)written;
    }
}

void cli_print_seed_line(const char *what, const CorpusSeed *seed) {
    char span[96];
    corpus_seed_describe(seed, span, sizeof(span));
    printf("%s from %s %s, score %lu/%lu\n", what, seed->piece, span, seed->score_numerator, seed->score_denominator);
}

int cli_seed_source(const CorpusSeed *seed, PlanSource *out) {
    memset(out, 0, sizeof(*out));
    if (snprintf(out->piece, PLAN_SOURCE_PIECE_MAX, "%s", seed->piece) >= PLAN_SOURCE_PIECE_MAX) {
        return cli_fail(CLI_EXIT_UNSATISFIABLE, "the source piece's path is too long to record");
    }
    out->start_measure = seed->start_measure;
    out->start_beat = seed->start_beat;
    out->end_measure = seed->end_measure;
    out->end_beat = seed->end_beat;
    out->score_numerator = seed->score_numerator;
    out->score_denominator = seed->score_denominator;
    return 0;
}

NodeId cli_find_node(const PiecePlan *plan, const char *name) {
    return api_find_node(plan, name);
}

bool cli_write_bytes(const char *path, const void *data, size_t size) {
    FILE *file = fopen(path, "wb");
    bool wrote = file && fwrite(data, 1, size, file) == size;
    bool closed = file && fclose(file) == 0;
    if (!wrote || !closed) {
        remove(path);
    }
    return wrote && closed;
}

int cli_write_plan_object(const cJSON *plan, const char *temporary, const char *final_path) {
    char *text = cJSON_PrintUnformatted(plan);
    PiecePlan *loaded = NULL;
    PlanJsonError error;
    memset(&error, 0, sizeof error);
    bool ok = text && plan_from_json(text, &loaded, &error) == PLAN_OK;
    cJSON_free(text);
    if (ok && plan_write_json_file(loaded, temporary, &error) != PLAN_OK) {
        ok = false;
    }
    plan_destroy(loaded);
    return ok ? 0 : cli_fail(CLI_EXIT_WRITE, "cannot write '%s': %s", final_path, error.message[0] ? error.message : "the plan");
}

int cli_export_plan(const cJSON *plan, double qpm, bool plain_midi, bool midi, bool musicxml, const cJSON *loop,
                    ComposerResponse *out) {
    cJSON *request = cJSON_CreateObject();
    bool ok = request && cJSON_AddItemToObject(request, "plan", cJSON_Duplicate(plan, true)) &&
              cJSON_AddBoolToObject(request, "midi", midi) && cJSON_AddBoolToObject(request, "musicxml", musicxml) &&
              cJSON_AddBoolToObject(request, "plain_midi", plain_midi);
    if (ok && loop) {
        ok = cJSON_AddItemToObject(request, "loop", cJSON_Duplicate(loop, true));
    }
    if (ok && qpm > 0) {
        /* A tempo is a whole number or half-steps like 195/2: the document wrote it with %g. */
        char text[48] = "";
        for (long den = 1; den <= 16 && !text[0]; den++) {
            double scaled = qpm * (double)den;
            if (scaled - (double)(long)(scaled + 0.5) < 1e-6 && (double)(long)(scaled + 0.5) - scaled < 1e-6) {
                snprintf(text, sizeof text, den == 1 ? "%ld" : "%ld/%ld", (long)(scaled + 0.5), den);
            }
        }
        ok = text[0] && cJSON_AddStringToObject(request, "tempo", text);
    }
    char *printed = ok ? cJSON_PrintUnformatted(request) : NULL;
    cJSON_Delete(request);
    if (!printed) {
        return cli_fail(CLI_EXIT_WRITE, "cannot render the plan");
    }
    ComposerStatus status = composer_api_export(printed, NULL, out);
    cJSON_free(printed);
    return status == COMPOSER_OK ? 0 : cli_fail(CLI_EXIT_WRITE, "cannot render the plan");
}

void cli_discard_pending(CliPendingFile *pending, size_t count) {
    for (size_t i = 0; i < count; i++) {
        remove(pending[i].temporary);
    }
}

char *cli_motive_text(const Motive *motive) {
    return motive->form == MOTIVE_SKETCH_FORM_RHYTHM    ? motive_to_rhythm_text(motive)
           : motive->form == MOTIVE_SKETCH_FORM_CONTOUR ? motive_to_contour_text(motive)
                                                 : motive_to_notes_text(motive);
}
