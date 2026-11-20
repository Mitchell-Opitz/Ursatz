#include "journal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <common/files.h>
#include <common/json.h>

#include <ursatz_workshop/workshop_api.h>

static const char kHeader[] = "{\"format\":\"" WORKSHOP_FORMAT_JOURNAL "\",\"format_version\":1}\n";

static UrsatzStatus bad_journal(UrsatzError *error, const char *path, const char *why) {
    return ursatz_error_set(error, URSATZ_ERR_FORMAT, path, "%s is not a journal: %s", path, why);
}

UrsatzStatus workshop_journal_read(const char *path, WorkshopJournal *out, UrsatzError *error) {
    memset(out, 0, sizeof(*out));
    if (!common_path_exists(path)) {
        return URSATZ_OK;
    }
    char *data = NULL;
    size_t size = 0;
    UrsatzStatus read = common_file_read(path, &data, &size, error);
    if (read != URSATZ_OK) {
        return read;
    }
    size_t start = 0;
    size_t capacity = 0;
    bool first = true;
    UrsatzStatus status = URSATZ_OK;
    while (start < size) {
        char *newline = memchr(data + start, '\n', size - start);
        if (!newline) {
            out->torn = true;
            break;
        }
        size_t length = (size_t)(newline - (data + start));
        char *line = data + start;
        line[length] = '\0';
        cJSON *value = cJSON_Parse(line);
        if (first) {
            first = false;
            bool header = value && strcmp(common_json_string(value, "format"), WORKSHOP_FORMAT_JOURNAL) == 0;
            long long version = 0;
            header = header && common_json_long(value, "format_version", &version);
            if (!header || version != 1) {
                cJSON_Delete(value);
                status = bad_journal(error, path, header ? "its format_version is not 1" : "its first line is not the journal header");
                break;
            }
            cJSON_Delete(value);
        } else {
            long long seq = 0;
            if (!cJSON_IsObject(value) || !common_json_long(value, "seq", &seq) || seq != out->last_seq + 1) {
                cJSON_Delete(value);
                status = bad_journal(error, path, "a line is not the next record");
                break;
            }
            if (out->count == capacity) {
                capacity = capacity ? capacity * 2 : 16;
                cJSON **grown = realloc(out->records, capacity * sizeof(*grown));
                if (!grown) {
                    cJSON_Delete(value);
                    status = ursatz_error_set(error, URSATZ_ERR_ALLOC, path, "out of memory");
                    break;
                }
                out->records = grown;
            }
            out->records[out->count++] = value;
            out->last_seq = seq;
        }
        start += length + 1;
        out->good_bytes = start;
    }
    free(data);
    if (status != URSATZ_OK) {
        workshop_journal_free(out);
    }
    return status;
}

void workshop_journal_free(WorkshopJournal *journal) {
    for (size_t i = 0; i < journal->count; i++) {
        cJSON_Delete(journal->records[i]);
    }
    free(journal->records);
    memset(journal, 0, sizeof(*journal));
}

UrsatzStatus workshop_journal_append(const char *path, const cJSON *record, void (*between)(void), UrsatzError *error) {
    if (!common_path_exists(path)) {
        char folder[512];
        snprintf(folder, sizeof(folder), "%s", path);
        char *slash = strrchr(folder, '/');
        if (slash) {
            *slash = '\0';
            UrsatzStatus made = common_dir_make(folder, error);
            if (made != URSATZ_OK) {
                return made;
            }
        }
        UrsatzStatus header = common_file_append_sync(path, kHeader, sizeof(kHeader) - 1, error);
        if (header != URSATZ_OK) {
            return header;
        }
    }
    char *text = cJSON_PrintUnformatted(record);
    if (!text) {
        return ursatz_error_set(error, URSATZ_ERR_ALLOC, path, "out of memory");
    }
    UrsatzStatus status = common_file_append_sync(path, text, strlen(text), error);
    free(text);
    if (status != URSATZ_OK) {
        return status;
    }
    if (between) {
        between();
    }
    return common_file_append_sync(path, "\n", 1, error);
}

UrsatzStatus workshop_journal_truncate(const char *path, size_t good_bytes, UrsatzError *error) {
    if (truncate(path, (off_t)good_bytes) != 0) {
        return ursatz_error_set(error, URSATZ_ERR_IO, path, "cannot cut the torn line off %s", path);
    }
    return URSATZ_OK;
}
