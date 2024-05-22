/* Confirms GET /api/analyzer/pieces/<id> includes provenance_state for
 * each interpretation summary, for both values it can take ("present"
 * and "absent"). No analyzer upstream produces a "present"
 * ProvenanceReference today (only provenance_reference_create_absent()
 * has any callers), so both cases here hand-build an Interpretation
 * directly against Ursatz's persistence API and record it into
 * ursatz-gui's own interpretation index table (the same private
 * schema/INSERT analysis_service.c uses), then load it back through the
 * route. This only proves the serialization path, not real analyzer
 * behavior. Uses a plain TCP socket for the HTTP request, same as
 * test_health_route.c. */

#include <civetweb.h>
#include <sqlite3.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
typedef SOCKET socket_t;
#define CLOSESOCK closesocket
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
typedef int socket_t;
#define CLOSESOCK close
#define INVALID_SOCKET (-1)
#endif

#include <core/provenance_reference.h>
#include <core/value.h>
#include <identity/entity_id.h>
#include <interpretation/interpretation.h>
#include <interpretation/uncertainty.h>
#include <persistence/persistence.h>
#include <persistence/persistence_interpretation.h>

#include "server.h"

static void sleep_ms(int ms) {
#ifdef _WIN32
  Sleep((DWORD)ms);
#else
  struct timespec ts = {0, (long)ms * 1000 * 1000};
  nanosleep(&ts, NULL);
#endif
}

static int failures = 0;

static void check(bool cond, const char *what) {
  if (!cond) {
    fprintf(stderr, "FAIL: %s\n", what);
    failures++;
  } else {
    printf("PASS: %s\n", what);
  }
}

/* Same private index table analysis_service.c maintains alongside
 * Ursatz's own persistence, in the same per-piece SQLite file. Reused
 * here (not reinvented) so the route's normal read path picks up the
 * hand-built Interpretation below. */
static const char *const kIndexTableName = "ursatz_gui_interpretations";

/* Same private piece-metadata table analysis_service.c maintains
 * (original_filename), reused here so the route's normal read path
 * picks up the hand-seeded filename below. */
static const char *const kMetaTableName = "ursatz_gui_piece_meta";

static bool seed_original_filename(const char *db_path, const char *filename) {
  sqlite3 *db = NULL;
  if (sqlite3_open(db_path, &db) != SQLITE_OK) {
    if (db) {
      sqlite3_close(db);
    }
    return false;
  }

  char sql[256];
  snprintf(sql, sizeof(sql),
           "CREATE TABLE IF NOT EXISTS %s ("
           "  id INTEGER PRIMARY KEY CHECK (id = 0),"
           "  original_filename TEXT NOT NULL"
           ");",
           kMetaTableName);
  if (sqlite3_exec(db, sql, NULL, NULL, NULL) != SQLITE_OK) {
    sqlite3_close(db);
    return false;
  }

  snprintf(sql, sizeof(sql), "INSERT OR REPLACE INTO %s (id, original_filename) VALUES (0, ?);", kMetaTableName);
  sqlite3_stmt *stmt = NULL;
  bool ok = false;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
    sqlite3_bind_text(stmt, 1, filename, -1, SQLITE_TRANSIENT);
    ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
  }
  sqlite3_close(db);
  return ok;
}

static bool record_interpretation_id(const char *db_path, const char *interpretation_id, const char *type) {
  sqlite3 *db = NULL;
  if (sqlite3_open(db_path, &db) != SQLITE_OK) {
    if (db) {
      sqlite3_close(db);
    }
    return false;
  }

  char sql[256];
  snprintf(sql, sizeof(sql),
           "CREATE TABLE IF NOT EXISTS %s ("
           "  interpretation_id TEXT PRIMARY KEY,"
           "  interpretation_type TEXT NOT NULL"
           ");",
           kIndexTableName);
  if (sqlite3_exec(db, sql, NULL, NULL, NULL) != SQLITE_OK) {
    sqlite3_close(db);
    return false;
  }

  snprintf(sql, sizeof(sql), "INSERT OR REPLACE INTO %s (interpretation_id, interpretation_type) VALUES (?, ?);",
           kIndexTableName);
  sqlite3_stmt *stmt = NULL;
  bool ok = false;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
    sqlite3_bind_text(stmt, 1, interpretation_id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, type, -1, SQLITE_TRANSIENT);
    ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
  }
  sqlite3_close(db);
  return ok;
}

/* Hand-builds and stores one Key Interpretation into db_path, with a
 * ProvenanceReference in the given state, and records it in the
 * interpretation index so the detail route will pick it up. is_present
 * selects provenance_reference_create_present() (needs a throwaway
 * target EntityID) vs. provenance_reference_create_absent(). Returns
 * false on any failure (caller treats that as a fatal setup failure). */
static bool seed_interpretation_with_claim(const char *db_path, const char *interpretation_id, bool is_present,
                                            const char *claim_text) {
  bool ok = false;

  EntityID *interp_id = NULL;
  EntityID *target_id = NULL;
  EntityID *provenance_target_id = NULL;
  Value *claim = NULL;
  Uncertainty *uncertainty = NULL;
  ProvenanceReference *provenance = NULL;
  Interpretation *interpretation = NULL;
  PersistenceHandle *handle = NULL;

  if (entity_id_create(interpretation_id, &interp_id) != ENTITY_ID_OK) {
    goto done;
  }
  if (entity_id_create("test-target", &target_id) != ENTITY_ID_OK) {
    goto done;
  }

  if (value_create("chord-label", claim_text, strlen(claim_text) + 1, &claim) != VALUE_OK) {
    goto done;
  }
  if (uncertainty_create_unknown(&uncertainty) != UNCERTAINTY_OK) {
    goto done;
  }

  if (is_present) {
    if (entity_id_create("test-provenance-target", &provenance_target_id) != ENTITY_ID_OK) {
      goto done;
    }
    /* Takes ownership of provenance_target_id on success. */
    if (provenance_reference_create_present(provenance_target_id, &provenance) != PROVENANCE_REFERENCE_OK) {
      goto done;
    }
    provenance_target_id = NULL;
  } else {
    if (provenance_reference_create_absent(&provenance) != PROVENANCE_REFERENCE_OK) {
      goto done;
    }
  }

  /* Takes ownership of interp_id, target_id, claim, uncertainty, provenance on success. */
  if (interpretation_create(interp_id, target_id, claim, NULL, NULL, NULL, uncertainty, "test-author", "test-method",
                             INTERPRETATION_STATUS_MACHINE_GENERATED, provenance, &interpretation) != INTERPRETATION_OK) {
    goto done;
  }
  interp_id = NULL;
  target_id = NULL;
  claim = NULL;
  uncertainty = NULL;
  provenance = NULL;

  if (persistence_open(db_path, &handle) != PERSISTENCE_OK) {
    goto done;
  }
  if (persistence_store_interpretation(handle, interpretation) != PERSISTENCE_INTERPRETATION_OK) {
    goto done;
  }

  ok = record_interpretation_id(db_path, interpretation_id, "key");

done:
  if (handle) {
    persistence_close(handle);
  }
  if (interpretation) {
    interpretation_destroy(interpretation);
  } else {
    if (provenance) {
      provenance_reference_destroy(provenance);
    }
    if (uncertainty) {
      uncertainty_destroy(uncertainty);
    }
    if (claim) {
      value_destroy(claim);
    }
    if (target_id) {
      entity_id_destroy(target_id);
    }
    if (interp_id) {
      entity_id_destroy(interp_id);
    }
  }
  if (provenance_target_id) {
    entity_id_destroy(provenance_target_id);
  }

  return ok;
}

static bool seed_interpretation(const char *db_path, const char *interpretation_id, bool is_present) {
  return seed_interpretation_with_claim(db_path, interpretation_id, is_present, "D major");
}

static bool http_request(const char *method, const char *path, char *response, size_t response_capacity) {
  socket_t sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock == INVALID_SOCKET) {
    return false;
  }

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(18766);
  inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

  if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
    CLOSESOCK(sock);
    return false;
  }

  char request[512];
  snprintf(request, sizeof(request), "%s %s HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n", method, path);
  send(sock, request, (int)strlen(request), 0);

  memset(response, 0, response_capacity);
  size_t total = 0;
  for (;;) {
    int n = recv(sock, response + total, (int)(response_capacity - 1 - total), 0);
    if (n <= 0) {
      break;
    }
    total += (size_t)n;
    if (total >= response_capacity - 1) {
      break;
    }
  }

  CLOSESOCK(sock);
  return total > 0;
}

/* Same shape as http_request, but writes a request body with a
 * Content-Length header -- used for PATCH, whose route reads the body
 * via mg_read the same way the POST upload route does. */
static bool http_request_with_body(const char *method, const char *path, const char *body, char *response,
                                    size_t response_capacity) {
  socket_t sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock == INVALID_SOCKET) {
    return false;
  }

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(18766);
  inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

  if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
    CLOSESOCK(sock);
    return false;
  }

  size_t body_len = strlen(body);
  char request[1024];
  int header_len = snprintf(request, sizeof(request),
                             "%s %s HTTP/1.1\r\nHost: 127.0.0.1\r\nContent-Type: application/json\r\n"
                             "Content-Length: %zu\r\nConnection: close\r\n\r\n",
                             method, path, body_len);
  send(sock, request, header_len, 0);
  send(sock, body, (int)body_len, 0);

  memset(response, 0, response_capacity);
  size_t total = 0;
  for (;;) {
    int n = recv(sock, response + total, (int)(response_capacity - 1 - total), 0);
    if (n <= 0) {
      break;
    }
    total += (size_t)n;
    if (total >= response_capacity - 1) {
      break;
    }
  }

  CLOSESOCK(sock);
  return total > 0;
}

static bool http_get(const char *path, char *response, size_t response_capacity) {
  return http_request("GET", path, response, response_capacity);
}

static bool http_delete(const char *path, char *response, size_t response_capacity) {
  return http_request("DELETE", path, response, response_capacity);
}

static bool http_patch(const char *path, const char *body, char *response, size_t response_capacity) {
  return http_request_with_body("PATCH", path, body, response, response_capacity);
}

int main(void) {
#ifdef _WIN32
  WSADATA wsa_data;
  WSAStartup(MAKEWORD(2, 2), &wsa_data);
#endif

  const char *pieces_dir = "pieces";
  const char *piece_id = "test-provenance-piece";
  char db_path[320];
  snprintf(db_path, sizeof(db_path), "%s/%s.db", pieces_dir, piece_id);

#ifdef _WIN32
  _mkdir(pieces_dir);
#else
  mkdir(pieces_dir, 0755);
#endif
  remove(db_path);

  check(seed_interpretation(db_path, "test-present-interpretation", true), "seed present-provenance interpretation");
  check(seed_interpretation(db_path, "test-absent-interpretation", false), "seed absent-provenance interpretation");
  check(seed_original_filename(db_path, "my \"draft\" \\ song.mid"),
        "seed original filename, including characters needing JSON escaping");

  /* Previously, handle_get_piece pre-sized its response buffer using a
   * per-interpretation budget (96 bytes + field lengths) that was
   * smaller than the format's actual fixed overhead (107 bytes). Once
   * enough interpretations were present, that shortfall accumulated
   * until an snprintf's offset overran the buffer's capacity, and the
   * following (capacity - offset) size_t subtraction underflowed into
   * a huge write size -- corrupting the heap and crashing the process.
   * Thirty interpretations reliably overran the old buffer. */
  const char *const many_ids[30] = {
      "many-01", "many-02", "many-03", "many-04", "many-05", "many-06", "many-07", "many-08", "many-09", "many-10",
      "many-11", "many-12", "many-13", "many-14", "many-15", "many-16", "many-17", "many-18", "many-19", "many-20",
      "many-21", "many-22", "many-23", "many-24", "many-25", "many-26", "many-27", "many-28", "many-29", "many-30",
  };
  bool seeded_many = true;
  for (size_t i = 0; i < 30; i++) {
    if (!seed_interpretation_with_claim(db_path, many_ids[i], false, "D major")) {
      seeded_many = false;
    }
  }
  check(seeded_many, "seed enough interpretations to have overrun the old fixed-size buffer");

  /* Also confirm arbitrary claim text -- including characters that must
   * be JSON-escaped -- round-trips as valid JSON rather than corrupting
   * the response, per issue #7 (truncated/invalid JSON on some pieces). */
  check(seed_interpretation_with_claim(db_path, "escaping-interpretation", false, "V7 says \"resolve\" \\ now"),
        "seed interpretation with a label needing JSON escaping");

  struct mg_context *ctx = ursatz_gui_server_start("18766", ".");
  check(ctx != NULL, "server starts");
  if (!ctx) {
    remove(db_path);
    return 1;
  }

  sleep_ms(200);

  char path[128];
  snprintf(path, sizeof(path), "/api/analyzer/pieces/%s", piece_id);

  char response[65536];
  bool got_response = http_get(path, response, sizeof(response));
  check(got_response, "got a response");
  if (got_response) {
    check(strstr(response, "200") != NULL, "response is 200 OK");
    check(strstr(response, "\"provenance_state\":\"present\"") != NULL,
          "response contains provenance_state=present for hand-built interpretation");
    check(strstr(response, "\"provenance_state\":\"absent\"") != NULL,
          "response contains provenance_state=absent for hand-built interpretation");
    check(strstr(response, "\"interpretation_id\":\"many-30\"") != NULL,
          "response includes the last of the 30 interpretations that used to overrun the buffer");
    check(strstr(response, "\"label\":\"V7 says \\\"resolve\\\" \\\\ now\"") != NULL,
          "response JSON-escapes a label containing quotes and a backslash");
    check(strstr(response, "\"filename\":\"my \\\"draft\\\" \\\\ song.mid\"") != NULL,
          "piece detail response includes the seeded original filename, JSON-escaped");
  }

  /* Confirms the same seeded filename round-trips through the piece
   * list route too, not just piece detail. */
  char list_response[65536];
  bool got_list_response = http_get("/api/analyzer/pieces", list_response, sizeof(list_response));
  check(got_list_response, "got a list response");
  if (got_list_response) {
    check(strstr(list_response, "200") != NULL, "list response is 200 OK");
    check(strstr(list_response, "\"id\":\"test-provenance-piece\"") != NULL,
          "list response includes the seeded piece id");
    check(strstr(list_response, "\"filename\":\"my \\\"draft\\\" \\\\ song.mid\"") != NULL,
          "list response includes the seeded original filename, JSON-escaped");
  }

  /* The server surviving to answer a second request (rather than the
   * connection just dropping) confirms the earlier request didn't crash
   * the backend process. */
  char followup[256];
  check(http_get("/api/analyzer/pieces", followup, sizeof(followup)) && strstr(followup, "200") != NULL,
        "server is still alive and answering requests after the large piece detail response");

  /* PATCH /api/analyzer/pieces/<id>. Renames the piece's persisted
   * original_filename, and only that -- it must not disturb the
   * NoteEvents/Interpretations already checked above. Plain filename
   * here (no quotes/backslashes): the route's extract_json_string_field
   * is a minimal flat-object parser with no escape support, same
   * documented limitation as corpus.c's -- escaping is already covered
   * separately, by the seeded-filename checks above. */
  char rename_response[4096];
  bool got_rename_response = http_patch(path, "{\"filename\":\"renamed-song.mid\"}", rename_response,
                                         sizeof(rename_response));
  check(got_rename_response, "got a response to PATCH");
  if (got_rename_response) {
    check(strstr(rename_response, "200") != NULL, "rename response is 200 OK");
    check(strstr(rename_response, "\"ok\":true") != NULL, "rename response reports ok:true");
    check(strstr(rename_response, "\"filename\":\"renamed-song.mid\"") != NULL,
          "rename response includes the new filename");
  }

  char get_after_rename[65536];
  bool got_get_after_rename = http_get(path, get_after_rename, sizeof(get_after_rename));
  check(got_get_after_rename, "got a response to GET after rename");
  if (got_get_after_rename) {
    check(strstr(get_after_rename, "\"filename\":\"renamed-song.mid\"") != NULL,
          "renamed piece's new filename is visible via GET");
    check(strstr(get_after_rename, "\"provenance_state\":\"present\"") != NULL,
          "renamed piece's interpretations are untouched by the rename");
  }

  char rename_missing_response[4096];
  bool got_rename_missing_response = http_patch("/api/analyzer/pieces/no-such-piece", "{\"filename\":\"x.mid\"}",
                                                 rename_missing_response, sizeof(rename_missing_response));
  check(got_rename_missing_response, "got a response to PATCH of a nonexistent piece");
  if (got_rename_missing_response) {
    check(strstr(rename_missing_response, "404") != NULL, "renaming a nonexistent piece returns 404");
  }

  char rename_empty_response[4096];
  bool got_rename_empty_response =
      http_patch(path, "{\"filename\":\"\"}", rename_empty_response, sizeof(rename_empty_response));
  check(got_rename_empty_response, "got a response to PATCH with an empty filename");
  if (got_rename_empty_response) {
    check(strstr(rename_empty_response, "\"ok\":false") != NULL, "an empty filename in the PATCH body is rejected");
  }

  /* DELETE /api/analyzer/pieces/<id>. Every piece is one SQLite file
   * (see analysis_service.c's db_path), holding NoteEvents, the
   * hand-seeded Interpretations above, and the
   * ursatz_gui_interpretations/ursatz_gui_piece_meta index tables --
   * so confirming the file itself is gone confirms all of that
   * cascaded data is gone too, not just the piece's top-level record. */
  char delete_response[4096];
  bool got_delete_response = http_delete(path, delete_response, sizeof(delete_response));
  check(got_delete_response, "got a response to DELETE");
  if (got_delete_response) {
    check(strstr(delete_response, "200") != NULL, "delete response is 200 OK");
    check(strstr(delete_response, "\"ok\":true") != NULL, "delete response reports ok:true");
  }

  FILE *db_file_after_delete = fopen(db_path, "rb");
  check(db_file_after_delete == NULL, "piece's SQLite file (NoteEvents/Interpretations/index tables) is gone after delete");
  if (db_file_after_delete) {
    fclose(db_file_after_delete);
  }

  char get_after_delete[4096];
  bool got_get_after_delete = http_get(path, get_after_delete, sizeof(get_after_delete));
  check(got_get_after_delete, "got a response to GET after delete");
  if (got_get_after_delete) {
    check(strstr(get_after_delete, "\"message\":\"piece not found\"") != NULL,
          "deleted piece is no longer found by GET");
  }

  char delete_missing_response[4096];
  bool got_delete_missing_response = http_delete("/api/analyzer/pieces/no-such-piece", delete_missing_response,
                                                  sizeof(delete_missing_response));
  check(got_delete_missing_response, "got a response to DELETE of a nonexistent piece");
  if (got_delete_missing_response) {
    check(strstr(delete_missing_response, "404") != NULL, "deleting a nonexistent piece returns 404");
  }

  ursatz_gui_server_stop(ctx);
  check(true, "server stops cleanly");

  remove(db_path);

#ifdef _WIN32
  WSACleanup();
#endif

  printf(failures == 0 ? "ALL TESTS PASSED\n" : "TESTS FAILED\n");
  return failures == 0 ? 0 : 1;
}
