#include <ursatz_analyzer/input_pipeline.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <ursatz_analyzer/midi_pipeline.h>
#include <ursatz_analyzer/musicxml_pipeline.h>

bool input_format_parse(const char *name, InputFormat *out) {
    if (strcmp(name, "midi") == 0) {
        *out = INPUT_FORMAT_MIDI;
        return true;
    }
    if (strcmp(name, "musicxml") == 0) {
        *out = INPUT_FORMAT_MUSICXML;
        return true;
    }
    return false;
}

const char *input_format_name(InputFormat format) {
    switch (format) {
        case INPUT_FORMAT_MIDI:
            return "midi";
        case INPUT_FORMAT_MUSICXML:
            return "musicxml";
        default:
            return "auto";
    }
}

static bool extension_equals(const char *path, const char *extension) {
    const char *dot = strrchr(path, '.');
    const char *slash = strrchr(path, '/');
    if (!dot || (slash && dot < slash)) {
        return false;
    }
    dot++;
    while (*dot && *extension) {
        if (tolower((unsigned char)*dot) != *extension) {
            return false;
        }
        dot++;
        extension++;
    }
    return *dot == '\0' && *extension == '\0';
}

static InputFormat detect_by_extension(const char *path) {
    if (extension_equals(path, "mid") || extension_equals(path, "midi")) {
        return INPUT_FORMAT_MIDI;
    }
    if (extension_equals(path, "musicxml") || extension_equals(path, "xml") || extension_equals(path, "mxl")) {
        return INPUT_FORMAT_MUSICXML;
    }
    return INPUT_FORMAT_AUTO;
}

static InputFormat detect_by_content(const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        return INPUT_FORMAT_AUTO;
    }
    unsigned char head[64];
    size_t count = fread(head, 1, sizeof head, file);
    fclose(file);

    if (count >= 4 && memcmp(head, "MThd", 4) == 0) {
        return INPUT_FORMAT_MIDI;
    }
    if (count >= 4 && memcmp(head, "PK\x03\x04", 4) == 0) {
        return INPUT_FORMAT_MUSICXML;
    }
    size_t i = 0;
    if (count >= 3 && memcmp(head, "\xEF\xBB\xBF", 3) == 0) {
        i = 3;
    }
    while (i < count && isspace(head[i])) {
        i++;
    }
    if (i < count && head[i] == '<') {
        return INPUT_FORMAT_MUSICXML;
    }
    return INPUT_FORMAT_AUTO;
}

InputFormat input_format_detect(const char *path) {
    InputFormat format = detect_by_extension(path);
    return format != INPUT_FORMAT_AUTO ? format : detect_by_content(path);
}

InputPipelineResult input_pipeline_import_file(const char *path, InputFormat format, ImportedNotes *out) {
    if (format == INPUT_FORMAT_AUTO) {
        format = input_format_detect(path);
        if (format == INPUT_FORMAT_AUTO) {
            FILE *file = fopen(path, "rb");
            if (!file) {
                fprintf(stderr, "Error: cannot read file '%s'\n", path);
                return INPUT_PIPELINE_ERR_CANNOT_READ_FILE;
            }
            fclose(file);
            fprintf(stderr,
                    "Error: cannot tell the input format of '%s'; use --input-format midi|musicxml\n", path);
            return INPUT_PIPELINE_ERR_UNKNOWN_FORMAT;
        }
    }

    switch (format) {
        case INPUT_FORMAT_MIDI: {
            MidiPipelineResult result = midi_pipeline_import_file(path, out);
            if (result == MIDI_PIPELINE_OK) {
                return INPUT_PIPELINE_OK;
            }
            return result == MIDI_PIPELINE_ERR_CANNOT_READ_FILE ? INPUT_PIPELINE_ERR_CANNOT_READ_FILE
                                                                : INPUT_PIPELINE_ERR_IMPORT_FAILED;
        }
        default: {
            MusicXmlPipelineResult result = musicxml_pipeline_import_file(path, out);
            if (result == MUSICXML_PIPELINE_OK) {
                return INPUT_PIPELINE_OK;
            }
            return result == MUSICXML_PIPELINE_ERR_CANNOT_READ_FILE ? INPUT_PIPELINE_ERR_CANNOT_READ_FILE
                                                                    : INPUT_PIPELINE_ERR_IMPORT_FAILED;
        }
    }
}

static InputFormat detect_by_bytes(const unsigned char *head, size_t count) {
    if (count >= 4 && memcmp(head, "MThd", 4) == 0) {
        return INPUT_FORMAT_MIDI;
    }
    if (count >= 4 && memcmp(head, "PK\x03\x04", 4) == 0) {
        return INPUT_FORMAT_MUSICXML;
    }
    size_t i = 0;
    if (count >= 3 && memcmp(head, "\xEF\xBB\xBF", 3) == 0) {
        i = 3;
    }
    while (i < count && isspace(head[i])) {
        i++;
    }
    return i < count && head[i] == '<' ? INPUT_FORMAT_MUSICXML : INPUT_FORMAT_AUTO;
}

InputPipelineResult input_pipeline_import_memory(const char *bytes, size_t size, InputFormat format, ImportedNotes *out) {
    if (!bytes || size == 0 || !out) {
        fprintf(stderr, "Error: no input bytes to import\n");
        return INPUT_PIPELINE_ERR_IMPORT_FAILED;
    }
    if (format == INPUT_FORMAT_AUTO) {
        format = detect_by_bytes((const unsigned char *)bytes, size < 64 ? size : 64);
        if (format == INPUT_FORMAT_AUTO) {
            fprintf(stderr, "Error: cannot tell the input format of the bytes; pass midi or musicxml\n");
            return INPUT_PIPELINE_ERR_UNKNOWN_FORMAT;
        }
    }

    const char *directory = getenv("TMPDIR");
    if (!directory || directory[0] == '\0') {
        directory = "/tmp";
    }
    const char *suffix = format == INPUT_FORMAT_MIDI ? ".mid" : ".musicxml";
    size_t length = strlen(directory) + strlen("/ursatz-import-XXXXXX") + strlen(suffix) + 1;
    char *path = malloc(length);
    if (!path) {
        fprintf(stderr, "Error: out of memory\n");
        return INPUT_PIPELINE_ERR_IMPORT_FAILED;
    }
    snprintf(path, length, "%s/ursatz-import-XXXXXX%s", directory, suffix);
    int fd = mkstemps(path, (int)strlen(suffix));
    if (fd < 0) {
        fprintf(stderr, "Error: cannot create a temporary file in '%s'\n", directory);
        free(path);
        return INPUT_PIPELINE_ERR_CANNOT_READ_FILE;
    }
    FILE *file = fdopen(fd, "wb");
    bool written = file && fwrite(bytes, 1, size, file) == size;
    if (file) {
        written = fclose(file) == 0 && written;
    } else {
        close(fd);
    }
    InputPipelineResult result = INPUT_PIPELINE_ERR_CANNOT_READ_FILE;
    if (written) {
        result = input_pipeline_import_file(path, format, out);
    } else {
        fprintf(stderr, "Error: cannot write the temporary file '%s'\n", path);
    }
    unlink(path);
    free(path);
    return result;
}
