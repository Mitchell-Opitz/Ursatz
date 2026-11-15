#include "cli_options.h"

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>



bool cli_read_file(const char *path, char **text, size_t *size) {
    *text = NULL;
    FILE *file = fopen(path, "rb");
    if (!file) {
        return false;
    }
    long length = -1;
    if (fseek(file, 0, SEEK_END) == 0) {
        length = ftell(file);
        if (length < 0 || fseek(file, 0, SEEK_SET) != 0) {
            length = -1;
        }
    }
    char *buffer = length >= 0 ? malloc((size_t)length + 1) : NULL;
    bool read = buffer && fread(buffer, 1, (size_t)length, file) == (size_t)length;
    fclose(file);
    if (!read) {
        free(buffer);
        return false;
    }
    buffer[length] = '\0';
    *text = buffer;
    if (size) {
        *size = (size_t)length;
    }
    return true;
}

bool cli_whole_number(const char *text, unsigned long maximum, unsigned long *out) {
    if (!text || !isdigit((unsigned char)text[0])) {
        return false;
    }
    char *end = NULL;
    errno = 0;
    unsigned long value = strtoul(text, &end, 10);
    if (errno != 0 || *end != '\0' || value > maximum) {
        return false;
    }
    *out = value;
    return true;
}

bool cli_equals_ignore_case(const char *a, const char *b) {
    if (!a || !b) {
        return false;
    }
    for (; *a && *b; a++, b++) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) {
            return false;
        }
    }
    return *a == *b;
}

long cli_limit(const char *name) {
    ComposerResponse response;
    long value = 0;
    if (composer_api_vocabulary(&response) == COMPOSER_OK) {
        cJSON *document = cJSON_Parse(response.json);
        const cJSON *item = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(document, "limits"), name);
        value = cJSON_IsNumber(item) ? (long)item->valuedouble : 0;
        cJSON_Delete(document);
    }
    composer_response_destroy(&response);
    return value;
}

bool cli_list_full(size_t count, const char *limit_name) {
    return count >= CLI_LIST_CAPACITY || (long)count >= cli_limit(limit_name);
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
                                          "voicing", "style_weight", "blend_weight", "blend_weights", "blend", "key", "candidates", "style", "at", "cells", "like", "like_progression", "style_without",
                                          "embellish", "schema", "sequence", "species", "op", "by", "degree", "axis", "interval", "amount",
                                          "position", "length", "min", "drop", "ratio", "tonal", "keep_contour", "keep_rhythm"};
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
        {"and degree both say", "and --degree both say"},
        {"needs a key; give key", "needs a key; give --key"},
        {"and sequence write their own bass, so they cannot be used with bass false",
         "and --sequence write their own bass, so they cannot be used with --no-bass"},
        {"midi and musicxml write the bass line (harmony makes no sound yet), so they cannot be used with bass false",
         "--midi and --musicxml write the bass line (harmony makes no sound yet), so they cannot be used with --no-bass"},
        {"(list_schemas lists them)", "(see --list-schemas)"},
        {"give a tonic alone with mode", "give a tonic alone with --mode"},
        {"harmony, features and role describe a motive input", "--harmony, --feature and --role describe a motive file"},
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

int cli_style_without_add(CliStyleWithout *list, const char *name) {
    if (list->count >= CLI_LIST_CAPACITY) {
        return cli_fail(CLI_EXIT_USAGE, "--style-without is given at most %d times", CLI_LIST_CAPACITY);
    }
    list->names[list->count++] = name;
    return 0;
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
    /* The API gives the plan as the library writes plan files. */
    cJSON *request = cJSON_CreateObject();
    bool ok = request && cJSON_AddItemToObject(request, "plan", cJSON_Duplicate(plan, true)) &&
              cJSON_AddBoolToObject(request, "plan_file", true);
    char *printed = ok ? cJSON_PrintUnformatted(request) : NULL;
    cJSON_Delete(request);
    ComposerResponse response;
    memset(&response, 0, sizeof response);
    ComposerStatus status = printed ? composer_api_export(printed, NULL, &response) : COMPOSER_ERR_ALLOC;
    cJSON_free(printed);
    ok = status == COMPOSER_OK && response.plan_json &&
         cli_write_bytes(temporary, response.plan_json, strlen(response.plan_json));
    const char *why = response.error.message[0] ? response.error.message : "the plan";
    int result = ok ? 0 : cli_fail(CLI_EXIT_WRITE, "cannot write '%s': %s", final_path, why);
    composer_response_destroy(&response);
    return result;
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
