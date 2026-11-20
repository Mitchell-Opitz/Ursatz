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

/*
 * The frontend sends the version it read with every write to an item that has versions (R-646). The route tests play the frontend: a write
 * of that kind that does not give base_version gets the item's current version, read with a GET first (in the JSON body, or the query of a
 * DELETE). A test of the refusal sends base_version itself, so it is left as given.
 */
static bool writes_with_base_version(const char *method, const char *path, char *id, size_t id_size) {
    static const char kPrefix[] = "/api/materials/items/";
    if (strncmp(path, kPrefix, sizeof(kPrefix) - 1) != 0 || strstr(path, "base_version") != NULL) {
        return false;
    }
    const char *start = path + sizeof(kPrefix) - 1;
    size_t length = strcspn(start, "/?");
    if (length == 0 || length >= id_size || memchr(start, '-', length) == NULL) {
        return false;
    }
    memcpy(id, start, length);
    id[length] = '\0';
    const char *rest = start + length;
    if (strcmp(method, "PATCH") == 0 || strcmp(method, "DELETE") == 0) {
        return *rest == '\0' || *rest == '?';
    }
    if (strcmp(method, "POST") != 0) {
        return false;
    }
    static const char *const kTails[] = {"/versions", "/compose", "/reanalyze", "/reroll", "/flag", "/promote", "/link-form"};
    for (size_t i = 0; i < sizeof(kTails) / sizeof(kTails[0]); i++) {
        if (strcmp(rest, kTails[i]) == 0) {
            return true;
        }
    }
    size_t rest_length = strlen(rest);
    return (strncmp(rest, "/uses/", 6) == 0 && rest_length > 7 && strcmp(rest + rest_length - 7, "/update") == 0) ||
           (strncmp(rest, "/versions/", 10) == 0 && ((rest_length > 11 && strcmp(rest + rest_length - 11, "/regenerate") == 0) ||
                                                      (rest_length > 7 && strcmp(rest + rest_length - 7, "/choose") == 0)));
}

/* The current version of item id, -1 when it has none (a request for an item that is not there goes through as it was). */
static long current_version_of(const char *port, const char *id) {
    char path[320];
    snprintf(path, sizeof path, "/api/materials/items/%s", id);
    char *response = exchange_with(port, "GET", path, "", "", 0);
    cJSON *answer = response ? cJSON_Parse(test_http_body(response)) : NULL;
    const cJSON *item = answer ? cJSON_GetObjectItemCaseSensitive(answer, "item") : NULL;
    const cJSON *current = item ? cJSON_GetObjectItemCaseSensitive(item, "current_version") : NULL;
    long version = cJSON_IsNumber(current) ? (long)current->valuedouble : -1;
    cJSON_Delete(answer);
    free(response);
    return version;
}

char *test_http_exchange(const char *port, const char *method, const char *path, const char *content_type,
                         const char *body, size_t body_size) {
    char id[128];
    char with_path[1024];
    char *with_body = NULL;
    if (writes_with_base_version(method, path, id, sizeof id)) {
        long version = current_version_of(port, id);
        if (version >= 0) {
            if (strcmp(method, "DELETE") == 0) {
                snprintf(with_path, sizeof with_path, "%s%cbase_version=%ld", path, strchr(path, '?') ? '&' : '?', version);
                path = with_path;
            } else {
                cJSON *object = body_size > 0 ? cJSON_Parse(body) : cJSON_CreateObject();
                if (cJSON_IsObject(object) && !cJSON_HasObjectItem(object, "base_version")) {
                    cJSON_AddNumberToObject(object, "base_version", (double)version);
                    with_body = cJSON_PrintUnformatted(object);
                    body = with_body;
                    body_size = strlen(with_body);
                }
                cJSON_Delete(object);
            }
        }
    }
    char headers[256];
    snprintf(headers, sizeof headers, "Content-Type: %s\r\n", content_type ? content_type : "application/json");
    char *response = exchange_with(port, method, path, headers, body, body_size);
    free(with_body);
    return response;
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

char *test_http_upload(const char *port, const char *file_path, const char *encoded_name, char **response_out) {
    size_t size = 0;
    char *data = test_read_file(file_path, &size);
    if (!data) {
        return NULL;
    }
    char headers[512];
    snprintf(headers, sizeof headers, "Content-Type: application/json\r\nX-Filename: %s\r\n", encoded_name);
    char *response = exchange_with(port, "POST", "/api/analyzer/pieces", headers, data, size);
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
