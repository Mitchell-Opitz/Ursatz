#include "composer_client.h"

#include <stdlib.h>
#include <string.h>

void ursatz_gui_composer_reply_take(ComposerStatus status, ComposerResponse *response, UrsatzGuiComposerReply *reply) {
  memset(reply, 0, sizeof(*reply));
  reply->status = status;
  if (!response) {
    return;
  }
  reply->json = response->json;
  reply->plan_json = response->plan_json;
  reply->midi = response->midi;
  reply->midi_size = response->midi_size;
  reply->musicxml = response->musicxml;
  response->json = NULL;
  response->plan_json = NULL;
  response->midi = NULL;
  response->midi_size = 0;
  response->musicxml = NULL;
  memcpy(reply->message, response->error.message, sizeof(reply->message));
  memcpy(reply->path, response->error.path, sizeof(reply->path));
  reply->message[sizeof(reply->message) - 1] = '\0';
  reply->path[sizeof(reply->path) - 1] = '\0';
  composer_response_destroy(response);
}

void ursatz_gui_composer_reply_destroy(UrsatzGuiComposerReply *reply) {
  if (!reply) {
    return;
  }
  free(reply->json);
  free(reply->plan_json);
  free(reply->midi);
  free(reply->musicxml);
  memset(reply, 0, sizeof(*reply));
}
