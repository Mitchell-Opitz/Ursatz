#include "motive_input.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/motive.h>

#include "api/api_internal.h"
#include "cli_options.h"

int motive_input_read_file(const char *path, unsigned char **bytes, size_t *size, char *error, size_t error_size) {
    *bytes = NULL;
    *size = 0;
    const char *why = NULL;
    char limit[64];
    MotiveResult result = MOTIVE_ERR_IO;
    FILE *file = fopen(path, "rb");
    if (!file) {
        why = "cannot open the file";
    } else {
        fseek(file, 0, SEEK_END);
        long length = ftell(file);
        fseek(file, 0, SEEK_SET);
        if (length == 0) {
            result = MOTIVE_ERR_EMPTY;
            why = "the file is empty";
        } else if (length < 0 || length > MOTIVE_INPUT_FILE_LIMIT) {
            snprintf(limit, sizeof limit, "the file is empty or larger than %ld bytes", MOTIVE_INPUT_FILE_LIMIT);
            why = limit;
        } else {
            unsigned char *data = malloc((size_t)length);
            if (data && fread(data, 1, (size_t)length, file) == (size_t)length) {
                *bytes = data;
                *size = (size_t)length;
            } else {
                free(data);
                result = data ? MOTIVE_ERR_IO : MOTIVE_ERR_ALLOC;
                why = data ? "cannot read the file" : "out of memory";
            }
        }
        fclose(file);
    }
    if (why) {
        snprintf(error, error_size, "cannot load '%s' (%s) at %s: %s", path, motive_result_name(result), path, why);
    }
    return why ? CLI_EXIT_UNSATISFIABLE : 0;
}

/* Whether the bytes are read by the MIDI entry: asked for as midi, or detected, and not given a form the file cannot have. */
static bool reads_as_midi(const MotiveInput *options, const unsigned char *bytes, size_t size) {
    if (options->form_text && strcmp(options->form_text, "full") != 0) {
        return false;
    }
    if (!options->format_text || strcmp(options->format_text, "auto") == 0) {
        return bytes && motive_detect_format(bytes, size) == MOTIVE_FORMAT_MIDI;
    }
    return bytes && strcmp(options->format_text, "midi") == 0;
}

cJSON *motive_input_request(const MotiveInput *options, const unsigned char *bytes, size_t size, bool *midi) {
    cJSON *request = cJSON_CreateObject();
    bool ok = request != NULL;
    if (ok && options->meter_text) {
        ok = cJSON_AddStringToObject(request, "meter", options->meter_text) != NULL;
    }
    if (ok && options->key_text) {
        ok = cJSON_AddStringToObject(request, "key", options->key_text) != NULL;
    }
    if (ok && options->pickup_text) {
        ok = cJSON_AddStringToObject(request, "pickup", options->pickup_text) != NULL;
    }
    ok = ok && motive_extras_add_members(&options->extras, request);
    *midi = ok && reads_as_midi(options, bytes, size);
    if (ok && !*midi) {
        cJSON *input = cJSON_AddObjectToObject(request, "input");
        char *text = bytes ? malloc(size + 1) : NULL;
        if (text) {
            memcpy(text, bytes, size);
            text[size] = '\0';
        }
        ok = input && (!bytes || text) && cJSON_AddStringToObject(input, "text", text ? text : "") != NULL;
        free(text);
        if (ok && options->format_text) {
            ok = cJSON_AddStringToObject(input, "format", options->format_text) != NULL;
        }
        if (ok && options->form_text) {
            ok = cJSON_AddStringToObject(input, "form", options->form_text) != NULL;
        }
    }
    if (!ok) {
        cJSON_Delete(request);
        return NULL;
    }
    return request;
}

bool motive_input_take_basic(BasicInput *basic, int argc, char **argv, int *index, int *status) {
    const char *argument = argv[*index];
    const char **slot = strcmp(argument, "--basic") == 0           ? &basic->path
                        : strcmp(argument, "--basic-format") == 0  ? &basic->format_text
                        : strcmp(argument, "--basic-pickup") == 0  ? &basic->pickup_text
                        : strcmp(argument, "--basic-harmony") == 0 ? &basic->harmony_text
                                                                   : NULL;
    if (!slot) {
        return false;
    }
    *status = 0;
    if (*index + 1 >= argc) {
        *status = cli_fail(CLI_EXIT_USAGE, "%s needs a value", argument);
    } else if (*slot) {
        *status = cli_fail(CLI_EXIT_USAGE, "%s given twice", argument);
    } else {
        *slot = argv[++*index];
    }
    return true;
}

bool motive_input_basic_given(const BasicInput *basic) {
    return basic->path || basic->format_text || basic->pickup_text || basic->harmony_text;
}

int motive_input_basic_input(const BasicInput *basic, const MotiveInput *source, MotiveInput *out) {
    if (!basic->path) {
        return cli_fail(CLI_EXIT_USAGE, "--basic-format, --basic-pickup and --basic-harmony need --basic <file>");
    }
    memset(out, 0, sizeof(*out));
    out->path = basic->path;
    out->meter_text = source->meter_text;
    out->key_text = source->key_text;
    out->form_text = source->form_text;
    out->format_text = basic->format_text;
    out->pickup_text = basic->pickup_text;
    out->extras.harmony_text = basic->harmony_text;
    out->extras.role_text = source->extras.role_text;
    return 0;
}

int motive_input_text_request(const MotiveInput *input, cJSON **out, MotiveFile *file) {
    unsigned char *bytes = NULL;
    size_t size = 0;
    char read_error[512];
    int read_status = motive_input_read_file(input->path, &bytes, &size, read_error, sizeof read_error);
    if (read_status != 0 && file->read_status == 0) {
        file->read_status = read_status;
        snprintf(file->read_error, sizeof file->read_error, "%s", read_error);
    }
    bool midi = false;
    cJSON *request = motive_input_request(input, bytes, size, &midi);
    if (!request) {
        free(bytes);
        return cli_fail(CLI_EXIT_USAGE, "out of memory");
    }
    if (midi) {
        /* Read once with the input's options; the motive that came out is the input, which the entry reads as text. */
        char *text = cJSON_PrintUnformatted(request);
        ComposerResponse response;
        memset(&response, 0, sizeof response);
        ComposerStatus status = text ? composer_api_motive_from_midi(bytes, size, text, NULL, &response) : COMPOSER_ERR_ALLOC;
        free(text);
        free(bytes);
        cJSON_Delete(request);
        if (status != COMPOSER_OK) {
            int failed = cli_fail_api(status, &response.error, input->path);
            composer_response_destroy(&response);
            return failed;
        }
        cJSON *document = cJSON_Parse(response.json);
        const cJSON *motive = document ? cJSON_GetObjectItemCaseSensitive(document, "motive") : NULL;
        char *motive_text = motive ? cJSON_PrintUnformatted(motive) : NULL;
        request = cJSON_CreateObject();
        cJSON *part = request ? cJSON_AddObjectToObject(request, "input") : NULL;
        bool ok = part && motive_text && cJSON_AddStringToObject(part, "format", "json") &&
                  cJSON_AddStringToObject(part, "text", motive_text) &&
                  (!input->extras.role_text || cJSON_AddStringToObject(request, "role", input->extras.role_text));
        free(motive_text);
        cJSON_Delete(document);
        composer_response_destroy(&response);
        if (!ok) {
            cJSON_Delete(request);
            return cli_fail(CLI_EXIT_USAGE, "out of memory");
        }
    } else {
        free(bytes);
    }
    *out = request;
    return 0;
}

int motive_input_fail(const MotiveFile *file, ComposerStatus status, const ComposerError *error,
                      const MotiveInputNames *names) {
    /* A bad option is reported before a file that cannot be read, as it always was. */
    if (file->read_status != 0 && status != COMPOSER_ERR_REQUEST) {
        return cli_fail(file->read_status, "%s", file->read_error);
    }
    const char *name = strncmp(error->path, "/accompaniment_motive", 21) == 0 && names->accompaniment ? names->accompaniment
                       : strncmp(error->path, "/basic", 6) == 0 ? names->basic
                       : strncmp(error->path, "/target", 7) == 0 ? names->target
                                                                 : names->source;
    return cli_fail_api(status, error, name);
}

void motive_input_print_basic_help(FILE *out) {
    fprintf(out, "  --basic <file>       the basic motive the family of forms comes from, read in the source's form, meter, key\n");
    fprintf(out, "                       and role; remoteness is measured from it. Default: the source is the basic motive\n");
    fprintf(out, "  --basic-format <f>, --basic-pickup <b>, --basic-harmony \"<text>\"   as for the basic motive's own file\n");
}
