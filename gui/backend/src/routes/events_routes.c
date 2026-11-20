#include "events_routes.h"

#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <civetweb.h>

#include "../server.h"
#include "route_util.h"

/*
 * GET /api/events (R-648): the workshop's events as server-sent events. The answer is text/event-stream with no length and no "Connection: close"; each event
 * is "event: <name>\ndata: <json>\n\n", a ": keep-alive" comment goes out every 15 seconds, and the stream ends when the client closes it (a write fails)
 * or the server stops. At most eight streams are open; a ninth is refused with 503. The request thread of a stream is its own for as long as it is open.
 */

#define STREAMS_MAX 8
#define KEEP_ALIVE_SECONDS 15

static atomic_int g_streams = 0;
static atomic_bool g_stopping = false;

void ursatz_gui_events_stop(void) {
  atomic_store(&g_stopping, true);
}

void ursatz_gui_events_start(void) {
  atomic_store(&g_stopping, false);
}

static int write_text(struct mg_connection *conn, const char *text) {
  size_t length = strlen(text);
  return mg_write(conn, text, length) == (int)length;
}

static int stream_events(struct mg_connection *conn, const UrsatzGuiRouteArgs *args) {
  (void)args;
  if (atomic_fetch_add(&g_streams, 1) >= STREAMS_MAX) {
    atomic_fetch_sub(&g_streams, 1);
    return ursatz_gui_send_failure(conn, 503, "too many event streams are open");
  }
  WorkshopEvents *events = workshop_events_subscribe(ursatz_gui_workshop());
  if (!events) {
    atomic_fetch_sub(&g_streams, 1);
    return ursatz_gui_send_failure(conn, 503, "the workshop is not running");
  }
  mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nCache-Control: no-cache\r\n\r\n");
  bool alive = write_text(conn, ": connected\n\n");
  time_t last_write = time(NULL);
  while (alive && !atomic_load(&g_stopping)) {
    char *name = NULL;
    char *json = NULL;
    if (workshop_events_next(events, 1000, &name, &json)) {
      size_t size = strlen(name) + strlen(json) + 24;
      char *frame = malloc(size);
      if (frame) {
        snprintf(frame, size, "event: %s\ndata: %s\n\n", name, json);
        alive = write_text(conn, frame);
        last_write = time(NULL);
        free(frame);
      }
      free(name);
      free(json);
    } else if (time(NULL) - last_write >= KEEP_ALIVE_SECONDS) {
      alive = write_text(conn, ": keep-alive\n\n");
      last_write = time(NULL);
    } else if (atomic_load(&g_stopping)) {
      break;
    }
  }
  workshop_events_unsubscribe(events);
  atomic_fetch_sub(&g_streams, 1);
  return 200;
}

static const UrsatzGuiRoute kRoutes[] = {
    {"GET", "/api/events", stream_events},
};

const UrsatzGuiRoute *ursatz_gui_events_routes(size_t *count) {
  *count = sizeof(kRoutes) / sizeof(kRoutes[0]);
  return kRoutes;
}
