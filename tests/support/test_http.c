#include "test_http.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include "ursatz_test.h"

void test_sleep_ms(int ms) {
    struct timespec ts = {ms / 1000, (long)(ms % 1000) * 1000 * 1000};
    nanosleep(&ts, NULL);
}

static char *exchange_with(const char *port, const char *method, const char *path, const char *headers,
                           const char *body, size_t body_size) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        return NULL;
    }
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short)atoi(port));
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    if (connect(sock, (struct sockaddr *)&addr, sizeof addr) != 0) {
        close(sock);
        return NULL;
    }

    char header[1024];
    int header_size = snprintf(header, sizeof header,
                               "%s %s HTTP/1.1\r\nHost: 127.0.0.1\r\n%sContent-Length: %zu\r\nConnection: close\r\n\r\n",
                               method, path, headers, body_size);
    send(sock, header, (size_t)header_size, 0);
    if (body_size > 0) {
        send(sock, body, body_size, 0);
    }

    size_t capacity = 65536;
    size_t total = 0;
    char *response = malloc(capacity);
    while (response) {
        if (total + 1 >= capacity) {
            capacity *= 2;
            char *grown = realloc(response, capacity);
            if (!grown) {
                free(response);
                response = NULL;
                break;
            }
            response = grown;
        }
        ssize_t n = recv(sock, response + total, capacity - 1 - total, 0);
        if (n <= 0) {
            break;
        }
        total += (size_t)n;
    }
    close(sock);
    if (response) {
        response[total] = '\0';
    }
    return response;
}

char *test_http_exchange(const char *port, const char *method, const char *path, const char *content_type,
                         const char *body, size_t body_size) {
    char headers[256];
    snprintf(headers, sizeof headers, "Content-Type: %s\r\n", content_type ? content_type : "application/json");
    return exchange_with(port, method, path, headers, body, body_size);
}

char *test_http_exchange_headers(const char *port, const char *method, const char *path, const char *headers, const char *body, size_t body_size) {
    return exchange_with(port, method, path, headers ? headers : "", body, body_size);
}

int test_http_status(const char *response) {
    int status = 0;
    if (response && sscanf(response, "HTTP/1.1 %d", &status) == 1) {
        return status;
    }
    return 0;
}

const char *test_http_body(const char *response) {
    const char *split = response ? strstr(response, "\r\n\r\n") : NULL;
    return split ? split + 4 : "";
}

cJSON *test_http_json(const char *port, const char *method, const char *path, const char *json_body, int *status) {
    char *response = test_http_exchange(port, method, path, NULL, json_body ? json_body : "",
                                        json_body ? strlen(json_body) : 0);
    *status = test_http_status(response);
    cJSON *json = response ? cJSON_Parse(test_http_body(response)) : NULL;
    free(response);
    return json;
}

/* A response made of the status and body of a finished job, as the route answered before the job. */
static char *job_answer(int status, const cJSON *result) {
    char *body = result ? cJSON_PrintUnformatted(result) : NULL;
    const char *text = body ? body : "{}";
    size_t size = strlen(text) + 64;
    char *response = malloc(size);
    if (response) {
        snprintf(response, size, "HTTP/1.1 %d Job\r\nContent-Type: application/json\r\n\r\n%s", status, text);
    }
    free(body);
    return response;
}

char *test_http_finish(const char *port, char *response) {
    if (!response || test_http_status(response) != 202) {
        return response;
    }
    cJSON *accepted = cJSON_Parse(test_http_body(response));
    const cJSON *job = accepted ? cJSON_GetObjectItemCaseSensitive(accepted, "job") : NULL;
    const cJSON *id = job ? cJSON_GetObjectItemCaseSensitive(job, "id") : NULL;
    if (!cJSON_IsString(id)) {
        cJSON_Delete(accepted);
        return response;
    }
    char path[128];
    snprintf(path, sizeof path, "/api/jobs/%s", id->valuestring);
    cJSON_Delete(accepted);
    for (int attempt = 0; attempt < 6000; attempt++) {
        int status = 0;
        cJSON *polled = test_http_json(port, "GET", path, NULL, &status);
        const cJSON *current = polled ? cJSON_GetObjectItemCaseSensitive(polled, "job") : NULL;
        const cJSON *state = current ? cJSON_GetObjectItemCaseSensitive(current, "state") : NULL;
        if (cJSON_IsString(state) && strcmp(state->valuestring, "queued") != 0 && strcmp(state->valuestring, "running") != 0) {
            const cJSON *http_status = cJSON_GetObjectItemCaseSensitive(current, "http_status");
            int answer_status = cJSON_IsNumber(http_status) ? (int)http_status->valuedouble : 499;
            char *answer = job_answer(answer_status, cJSON_GetObjectItemCaseSensitive(current, "result"));
            cJSON_Delete(polled);
            if (answer) {
                free(response);
                return answer;
            }
            return response;
        }
        cJSON_Delete(polled);
        test_sleep_ms(20);
    }
    return response;
}

cJSON *test_http_json_finish(const char *port, const char *method, const char *path, const char *json_body, int *status) {
    char *response = test_http_exchange(port, method, path, NULL, json_body ? json_body : "", json_body ? strlen(json_body) : 0);
    response = test_http_finish(port, response);
    *status = test_http_status(response);
    cJSON *json = response ? cJSON_Parse(test_http_body(response)) : NULL;
    free(response);
    return json;
}

/* A percent-encoded copy of text (every byte but a letter, a digit, "-", "_" and "."); the caller frees it. */
static char *percent_encode(const char *text) {
    size_t length = strlen(text);
    char *out = malloc(length * 3 + 1);
    size_t used = 0;
    for (size_t i = 0; out && i < length; i++) {
        unsigned char c = (unsigned char)text[i];
        if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '-' || c == '_' || c == '.') {
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

/* A percent-decoded copy of text; the caller frees it. */
static char *percent_decode(const char *text) {
    size_t length = strlen(text);
    char *out = malloc(length + 1);
    size_t used = 0;
    for (size_t i = 0; out && i < length; i++) {
        unsigned int value = 0;
        if (text[i] == '%' && i + 2 < length && sscanf(text + i + 1, "%2x", &value) == 1) {
            out[used++] = (char)value;
            i += 2;
        } else {
            out[used++] = text[i];
        }
    }
    if (out) {
        out[used] = '\0';
    }
    return out;
}

char *test_http_command_file(const char *port, const char *command_json, const char *encoded_name, const char *data, size_t size) {
    char *encoded_command = percent_encode(command_json);
    if (!encoded_command) {
        return NULL;
    }
    size_t capacity = strlen(encoded_command) + strlen(encoded_name) + 256;
    char *headers = malloc(capacity);
    char *response = NULL;
    if (headers) {
        snprintf(headers, capacity, "Content-Type: application/octet-stream\r\nX-Ursatz-Command: %s\r\nX-Filename: %s\r\n", encoded_command, encoded_name);
        response = exchange_with(port, "POST", "/api/workshop/command", headers, data, size);
    }
    free(headers);
    free(encoded_command);
    return response;
}

char *test_http_upload(const char *port, const char *file_path, const char *encoded_name, char **response_out) {
    size_t size = 0;
    char *data = test_read_file(file_path, &size);
    if (!data) {
        return NULL;
    }
    char *name = percent_decode(encoded_name);
    cJSON *command = cJSON_CreateObject();
    cJSON *params = command ? cJSON_AddObjectToObject(command, "params") : NULL;
    char *command_json = NULL;
    if (params && name && cJSON_AddStringToObject(command, "command", "library.import") && cJSON_AddStringToObject(command, "actor", "michelle") &&
        cJSON_AddStringToObject(params, "filename", name)) {
        command_json = cJSON_PrintUnformatted(command);
    }
    cJSON_Delete(command);
    free(name);
    char *response = command_json ? test_http_command_file(port, command_json, encoded_name, data, size) : NULL;
    free(command_json);
    free(data);
    response = test_http_finish(port, response); /* the import is a job: its answer is what the route answered before jobs */
    char *id = NULL;
    cJSON *json = response ? cJSON_Parse(test_http_body(response)) : NULL;
    const cJSON *id_item = json ? cJSON_GetObjectItemCaseSensitive(json, "id") : NULL;
    if (cJSON_IsString(id_item)) {
        id = strdup(id_item->valuestring);
    }
    cJSON_Delete(json);
    if (response_out) {
        *response_out = response;
    } else {
        free(response);
    }
    return id;
}
