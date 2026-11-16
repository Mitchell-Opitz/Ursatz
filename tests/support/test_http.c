#include "test_http.h"

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
