#include "remote.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <civetweb.h>

#define ANSWER_TIMEOUT_MS 60000

char *cli_percent_encode(const char *text) {
    size_t length = text ? strlen(text) : 0;
    char *out = malloc(length * 3 + 1);
    size_t used = 0;
    for (size_t i = 0; out && i < length; i++) {
        unsigned char c = (unsigned char)text[i];
        if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '-' || c == '_' || c == '.' || c == '~') {
            out[used++] = (char)c;
        } else {
            used += (size_t)snprintf(out + used, 4, "%%%02X", c);
        }
    }
    if (out) {
        out[used] = '\0';
    }
    return out;
}

/* The value of the response header name (case-insensitive), NULL when there is none. */
static const char *response_header(const struct mg_response_info *info, const char *name) {
    for (int i = 0; i < info->num_headers; i++) {
        if (info->http_headers[i].name && strcasecmp(info->http_headers[i].name, name) == 0) {
            return info->http_headers[i].value;
        }
    }
    return NULL;
}

/* The file name of a Content-Disposition value (attachment; filename="x.mid"), "" when there is none. */
static void disposition_filename(const char *value, char *out, size_t size) {
    out[0] = '\0';
    const char *at = value ? strstr(value, "filename=") : NULL;
    if (!at) {
        return;
    }
    at += 9;
    if (*at == '"') {
        at++;
    }
    size_t length = strcspn(at, "\";");
    snprintf(out, size, "%.*s", (int)length, at);
}

/* Reads the whole body into a malloc'ed buffer (NUL-terminated). */
static char *read_all(struct mg_connection *conn, size_t *size) {
    size_t capacity = 8192;
    size_t used = 0;
    char *data = malloc(capacity + 1);
    while (data) {
        if (used == capacity) {
            capacity *= 2;
            char *grown = realloc(data, capacity + 1);
            if (!grown) {
                free(data);
                return NULL;
            }
            data = grown;
        }
        int got = mg_read(conn, data + used, capacity - used);
        if (got <= 0) {
            break;
        }
        used += (size_t)got;
    }
    if (data) {
        data[used] = '\0';
        *size = used;
    }
    return data;
}

UrsatzStatus cli_remote_request(int port, const char *method, const char *path, const char *content_type, const char *extra_headers,
                                const uint8_t *body, size_t size, WorkshopReply *reply, UrsatzError *error) {
    static bool initialized = false;
    memset(reply, 0, sizeof *reply);
    if (!initialized) {
        mg_init_library(0);
        initialized = true;
    }
    char message[256] = "";
    struct mg_connection *conn = mg_connect_client("127.0.0.1", port, 0, message, sizeof message);
    if (!conn) {
        return ursatz_error_set(error, URSATZ_ERR_IO, "", "cannot reach the Ursatz process on port %d: %s", port, message);
    }
    mg_printf(conn, "%s %s HTTP/1.1\r\nHost: 127.0.0.1:%d\r\nContent-Type: %s\r\nContent-Length: %zu\r\nConnection: close\r\n%s\r\n", method, path, port,
              content_type ? content_type : "application/json", size, extra_headers ? extra_headers : "");
    if (size > 0) {
        mg_write(conn, body, size);
    }
    if (mg_get_response(conn, message, sizeof message, ANSWER_TIMEOUT_MS) < 0) {
        mg_close_connection(conn);
        return ursatz_error_set(error, URSATZ_ERR_IO, "", "cannot reach the Ursatz process on port %d: %s", port, message);
    }
    const struct mg_response_info *info = mg_get_response_info(conn);
    const char *header_type = info ? response_header(info, "Content-Type") : NULL;
    char type_text[64] = "";
    if (header_type) {
        snprintf(type_text, sizeof type_text, "%s", header_type);
    }
    const char *type = header_type ? type_text : NULL;
    char filename[128];
    disposition_filename(info ? response_header(info, "Content-Disposition") : NULL, filename, sizeof filename);
    size_t length = 0;
    char *answer = read_all(conn, &length);
    mg_close_connection(conn);
    if (!answer) {
        return ursatz_error_set(error, URSATZ_ERR_ALLOC, "", "out of memory");
    }
    if (type && strncmp(type, "application/json", 16) != 0) {
        reply->bytes = (uint8_t *)answer;
        reply->size = length;
        snprintf(reply->content_type, sizeof reply->content_type, "%s", type);
        snprintf(reply->filename, sizeof reply->filename, "%s", filename);
        return URSATZ_OK;
    }
    cJSON *document = length > 0 ? cJSON_Parse(answer) : cJSON_CreateObject();
    free(answer);
    if (!cJSON_IsObject(document)) {
        cJSON_Delete(document);
        return ursatz_error_set(error, URSATZ_ERR_INTERNAL, "", "the Ursatz process on port %d answered something that is not JSON", port);
    }
    const cJSON *ok = cJSON_GetObjectItemCaseSensitive(document, "ok");
    if (cJSON_IsFalse(ok)) {
        const cJSON *text = cJSON_GetObjectItemCaseSensitive(document, "error");
        const cJSON *code = cJSON_GetObjectItemCaseSensitive(document, "code");
        const cJSON *where = cJSON_GetObjectItemCaseSensitive(document, "path");
        UrsatzStatus status = URSATZ_ERR_INTERNAL;
        if (!cJSON_IsString(code) || !ursatz_status_parse(code->valuestring, &status)) {
            status = URSATZ_ERR_INTERNAL;
        }
        ursatz_error_set(error, status, cJSON_IsString(where) ? where->valuestring : "", "%s", cJSON_IsString(text) ? text->valuestring : "the Ursatz process refused the call");
        cJSON_DeleteItemFromObjectCaseSensitive(document, "ok");
        cJSON_DeleteItemFromObjectCaseSensitive(document, "error");
        cJSON_DeleteItemFromObjectCaseSensitive(document, "code");
        cJSON_DeleteItemFromObjectCaseSensitive(document, "path");
        reply->json = document;
        return status;
    }
    cJSON_DeleteItemFromObjectCaseSensitive(document, "ok");
    reply->json = document;
    return URSATZ_OK;
}

UrsatzStatus cli_remote_request_status(int port, WorkshopReply *reply, UrsatzError *error) {
    return cli_remote_request(port, "GET", "/api/workshop/status", NULL, "", NULL, 0, reply, error);
}
