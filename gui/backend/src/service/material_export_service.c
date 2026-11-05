#include "material_export_service.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>
#include <ursatz_analyzer/text_export.h>

#include "composer_client.h"
#include "material_service.h"
#include "motif_service.h"
#include "util/json_util.h"

/* ---- the build: what the export owns until the Analyzer has written the text -------------------------------------- */

typedef struct {
  AnalyzerTextExportFact *items;
  size_t count;
  size_t capacity;
} FactList;

typedef struct {
  AnalyzerTextExportSegment *items;
  size_t count;
  size_t capacity;
} SegmentList;

typedef struct {
  const char *label;
  const char *musicxml;
  FactList facts;
  SegmentList segments;
} Draft;

typedef struct {
  /* Strings and JSON documents the facts, labels and MusicXML point into. */
  char **strings;
  size_t string_count;
  size_t string_capacity;
  cJSON **documents;
  size_t document_count;
  size_t document_capacity;
  Draft *drafts;
  size_t draft_count;
  size_t draft_capacity;
  FactList material_facts;
  /* The first failure; later ones are ignored. */
  UrsatzGuiMaterialExportResult result;
  char message[URSATZ_GUI_MATERIAL_EXPORT_MESSAGE_SIZE];
} Build;

static bool failed(const Build *build) {
  return build->result != URSATZ_GUI_MATERIAL_EXPORT_OK;
}

static bool fail(Build *build, UrsatzGuiMaterialExportResult result, const char *message) {
  if (!failed(build)) {
    build->result = result;
    snprintf(build->message, sizeof(build->message), "%s", message);
  }
  return false;
}

static bool fail_alloc(Build *build) {
  return fail(build, URSATZ_GUI_MATERIAL_EXPORT_ALLOC, "");
}

/* Doubles a pointer array's capacity when it is full. */
static bool grow(void **array, size_t *capacity, size_t count, size_t element_size) {
  if (count < *capacity) {
    return true;
  }
  size_t next = *capacity == 0 ? 8 : *capacity * 2;
  void *bigger = realloc(*array, next * element_size);
  if (!bigger) {
    return false;
  }
  *array = bigger;
  *capacity = next;
  return true;
}

/* Takes ownership of text (a malloc'd string). NULL (an allocation that failed) fails the build. */
static const char *keep_string(Build *build, char *text) {
  if (!text || !grow((void **)&build->strings, &build->string_capacity, build->string_count, sizeof(char *))) {
    free(text);
    fail_alloc(build);
    return NULL;
  }
  build->strings[build->string_count++] = text;
  return text;
}

/* Takes ownership of document. NULL fails the build. */
static cJSON *keep_document(Build *build, cJSON *document) {
  if (!document || !grow((void **)&build->documents, &build->document_capacity, build->document_count, sizeof(cJSON *))) {
    cJSON_Delete(document);
    fail_alloc(build);
    return NULL;
  }
  build->documents[build->document_count++] = document;
  return document;
}

static const char *format(Build *build, const char *pattern, ...) {
  va_list args;
  va_start(args, pattern);
  va_list again;
  va_copy(again, args);
  int length = vsnprintf(NULL, 0, pattern, args);
  va_end(args);
  char *text = length >= 0 ? malloc((size_t)length + 1) : NULL;
  if (text) {
    vsnprintf(text, (size_t)length + 1, pattern, again);
  }
  va_end(again);
  return keep_string(build, text);
}

static void add_fact(Build *build, FactList *list, const char *name, const char *value) {
  if (!name || !value || failed(build)) {
    return;
  }
  if (!grow((void **)&list->items, &list->capacity, list->count, sizeof(*list->items))) {
    fail_alloc(build);
    return;
  }
  list->items[list->count].name = name;
  list->items[list->count].value = value;
  list->count++;
}

/* The fact name = the text of member `key` of object, when it is a non-empty string. */
static void add_text_member(Build *build, FactList *list, const char *name, const cJSON *object, const char *key) {
  const cJSON *value = cJSON_GetObjectItemCaseSensitive(object, key);
  if (cJSON_IsString(value) && value->valuestring[0] != '\0') {
    add_fact(build, list, name, value->valuestring);
  }
}

/* A string or a number of a store member as text; NULL for anything else or an empty string. */
static const char *scalar_text(Build *build, const cJSON *value) {
  if (cJSON_IsString(value)) {
    return value->valuestring[0] != '\0' ? value->valuestring : NULL;
  }
  if (cJSON_IsNumber(value)) {
    return format(build, "%.15g", value->valuedouble);
  }
  return NULL;
}

/* A growing string. */
typedef struct {
  char *data;
  size_t length;
  size_t capacity;
} Text;

static bool text_append(Text *text, const char *piece, const char *separator) {
  size_t piece_length = strlen(piece);
  size_t separator_length = text->length > 0 ? strlen(separator) : 0;
  size_t needed = text->length + separator_length + piece_length + 1;
  if (needed > text->capacity) {
    size_t next = text->capacity == 0 ? 64 : text->capacity;
    while (next < needed) {
      next *= 2;
    }
    char *bigger = realloc(text->data, next);
    if (!bigger) {
      return false;
    }
    text->data = bigger;
    text->capacity = next;
  }
  memcpy(text->data + text->length, separator, separator_length);
  memcpy(text->data + text->length + separator_length, piece, piece_length + 1);
  text->length += separator_length + piece_length;
  return true;
}

static const char *text_keep(Build *build, Text *text) {
  if (!text->data) {
    return "";
  }
  return keep_string(build, text->data);
}

/* The strings of an array joined with ", "; for an array of objects, their member `key` instead. Empty when there are
 * none. */
static const char *join_list(Build *build, const cJSON *array, const char *key) {
  Text text = {0};
  const cJSON *entry = NULL;
  cJSON_ArrayForEach(entry, array) {
    const cJSON *value = key ? cJSON_GetObjectItemCaseSensitive(entry, key) : entry;
    if (cJSON_IsString(value) && !text_append(&text, value->valuestring, ", ")) {
      free(text.data);
      fail_alloc(build);
      return "";
    }
  }
  return text_keep(build, &text);
}

/* ---- sections ------------------------------------------------------------------------------------------------------ */

static Draft *add_draft(Build *build, const char *label, const char *musicxml) {
  if (failed(build) || !label) {
    return NULL;
  }
  if (!grow((void **)&build->drafts, &build->draft_capacity, build->draft_count, sizeof(Draft))) {
    fail_alloc(build);
    return NULL;
  }
  Draft *draft = &build->drafts[build->draft_count++];
  memset(draft, 0, sizeof(*draft));
  draft->label = label;
  draft->musicxml = musicxml;
  return draft;
}

static void add_segments(Build *build, Draft *draft, const cJSON *features) {
  const cJSON *feature = NULL;
  cJSON_ArrayForEach(feature, features) {
    const cJSON *label = cJSON_GetObjectItemCaseSensitive(feature, "label");
    const cJSON *first = cJSON_GetObjectItemCaseSensitive(feature, "first");
    const cJSON *last = cJSON_GetObjectItemCaseSensitive(feature, "last");
    if (!cJSON_IsString(label) || !cJSON_IsNumber(first) || !cJSON_IsNumber(last) || first->valuedouble < 0 ||
        last->valuedouble < first->valuedouble) {
      continue;
    }
    if (!grow((void **)&draft->segments.items, &draft->segments.capacity, draft->segments.count,
              sizeof(*draft->segments.items))) {
      fail_alloc(build);
      return;
    }
    AnalyzerTextExportSegment *segment = &draft->segments.items[draft->segments.count++];
    segment->label = label->valuestring;
    segment->first = (size_t)first->valuedouble;
    segment->last = (size_t)last->valuedouble;
  }
}

/* "<note> <type>" for each tone label of the stored analysis, joined with ", " (the role when the label has no type). */
static const char *tones_text(Build *build, const cJSON *tones) {
  Text text = {0};
  const cJSON *tone = NULL;
  cJSON_ArrayForEach(tone, tones) {
    const cJSON *note = cJSON_GetObjectItemCaseSensitive(tone, "note");
    const cJSON *type = cJSON_GetObjectItemCaseSensitive(tone, "type");
    const cJSON *role = cJSON_GetObjectItemCaseSensitive(tone, "role");
    const cJSON *name = cJSON_IsString(type) ? type : role;
    if (!cJSON_IsNumber(note) || !cJSON_IsString(name)) {
      continue;
    }
    const char *piece = format(build, "%d %s", note->valueint, name->valuestring);
    if (!piece || !text_append(&text, piece, ", ")) {
      free(text.data);
      fail_alloc(build);
      return "";
    }
  }
  return text_keep(build, &text);
}

static void add_motif_facts(Build *build, FactList *facts, const cJSON *motif) {
  add_text_member(build, facts, "role", motif, "role");
  add_text_member(build, facts, "form", motif, "form");
  add_text_member(build, facts, "meter", motif, "meter");
  add_text_member(build, facts, "key", motif, "key");
  add_text_member(build, facts, "pickup", motif, "pickup");
  add_text_member(build, facts, "notes", motif, "notes");
  const cJSON *analysis = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(motif, "analysis"), "analysis");
  const cJSON *base = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(analysis, "base_harmony"), "text");
  if (cJSON_IsString(base) && base->valuestring[0] != '\0') {
    add_fact(build, facts, "base harmony", base->valuestring);
  }
  const cJSON *tones = cJSON_GetObjectItemCaseSensitive(analysis, "tones");
  if (cJSON_IsArray(tones) && cJSON_GetArraySize(tones) > 0) {
    add_fact(build, facts, "tones", tones_text(build, tones));
  }
}

static void add_derived_facts(Build *build, FactList *facts, const cJSON *derived) {
  add_text_member(build, facts, "kind", derived, "kind");
  add_fact(build, facts, "kept", join_list(build, cJSON_GetObjectItemCaseSensitive(derived, "kept"), NULL));
  add_fact(build, facts, "changed", join_list(build, cJSON_GetObjectItemCaseSensitive(derived, "changed"), NULL));
  add_fact(build, facts, "functions", join_list(build, cJSON_GetObjectItemCaseSensitive(derived, "functions"), NULL));
  add_text_member(build, facts, "flag", derived, "flag");
}

/* The Composer's MusicXML for a motif body member, as the Composer renders its request (no render_key: the motif's own
 * key). NULL with the build failed when the Composer refuses or cannot render it. */
static const char *render_motif(Build *build, const cJSON *motif) {
  cJSON *request = ursatz_gui_motif_request(motif);
  if (!request) {
    fail(build, URSATZ_GUI_MATERIAL_EXPORT_FAILED, "the motif cannot be rendered");
    return NULL;
  }
  if (!cJSON_AddTrueToObject(request, "musicxml")) {
    cJSON_Delete(request);
    fail_alloc(build);
    return NULL;
  }
  UrsatzGuiComposerReply reply;
  bool called = ursatz_gui_motif_motive_render(request, &reply);
  cJSON_Delete(request);
  if (!called) {
    fail_alloc(build);
    return NULL;
  }
  const char *xml = NULL;
  if (ursatz_gui_composer_reply_refused(&reply)) {
    fail(build, URSATZ_GUI_MATERIAL_EXPORT_REFUSED, reply.message[0] ? reply.message : "composer could not render the motif");
  } else if (reply.status != COMPOSER_OK || !reply.musicxml) {
    fail(build, URSATZ_GUI_MATERIAL_EXPORT_FAILED, "composer could not render the motif");
  } else {
    xml = keep_string(build, strdup(reply.musicxml));
  }
  ursatz_gui_composer_reply_destroy(&reply);
  return xml;
}

/* A section for a motif body member: its render, its segments and, when given, the facts of the motif and of a
 * variant's derived record. */
static void add_motif_section(Build *build, const char *label, const cJSON *motif, bool motif_facts, const cJSON *derived) {
  if (failed(build) || !label) {
    return;
  }
  const char *xml = render_motif(build, motif);
  Draft *draft = xml ? add_draft(build, label, xml) : NULL;
  if (!draft) {
    return;
  }
  add_segments(build, draft, cJSON_GetObjectItemCaseSensitive(motif, "features"));
  if (motif_facts) {
    add_motif_facts(build, &draft->facts, motif);
  }
  if (derived) {
    add_derived_facts(build, &draft->facts, derived);
  }
}

/* The bytes of one of a version's files as a NUL-terminated string; *xml stays NULL when the version has no such file
 * (it was not composed). */
static void read_version_xml(Build *build, const char *id, int version, const char *name, const char **xml) {
  *xml = NULL;
  unsigned char *data = NULL;
  size_t size = 0;
  UrsatzGuiMaterialResult result = ursatz_gui_material_version_file(id, version, name, &data, &size);
  if (result == URSATZ_GUI_MATERIAL_ERR_NOT_FOUND) {
    return;
  }
  if (result != URSATZ_GUI_MATERIAL_OK) {
    fail(build, URSATZ_GUI_MATERIAL_EXPORT_FAILED, "a version file cannot be read");
    return;
  }
  char *copy = malloc(size + 1);
  if (copy) {
    memcpy(copy, data, size);
    copy[size] = '\0';
  }
  free(data);
  *xml = keep_string(build, copy);
}

/* ---- the three kinds ----------------------------------------------------------------------------------------------- */

static void build_motif(Build *build, const char *id, const cJSON *body) {
  const cJSON *motif = cJSON_GetObjectItemCaseSensitive(body, "motif");
  if (!cJSON_IsObject(motif)) {
    fail(build, URSATZ_GUI_MATERIAL_EXPORT_FAILED, "the item has no motif");
    return;
  }
  add_motif_section(build, "motif", motif, true, NULL);
  cJSON *derived_list = NULL;
  if (failed(build)) {
    return;
  }
  UrsatzGuiMaterialResult listed = ursatz_gui_material_derived_motifs(&derived_list);
  if (listed != URSATZ_GUI_MATERIAL_OK || !keep_document(build, derived_list)) {
    if (!failed(build)) {
      fail(build, URSATZ_GUI_MATERIAL_EXPORT_FAILED, "the variants cannot be listed");
    }
    return;
  }
  int number = 0;
  const cJSON *entry = NULL;
  cJSON_ArrayForEach(entry, derived_list) {
    const cJSON *derived = cJSON_GetObjectItemCaseSensitive(entry, "derived");
    if (strcmp(ursatz_gui_json_string(derived, "source_item"), id) != 0 ||
        cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(derived, "standalone"))) {
      continue;
    }
    cJSON *variant_body = NULL;
    if (ursatz_gui_material_item_get(ursatz_gui_json_string(entry, "id"), NULL, &variant_body) !=
            URSATZ_GUI_MATERIAL_OK ||
        !keep_document(build, variant_body)) {
      if (!failed(build)) {
        fail(build, URSATZ_GUI_MATERIAL_EXPORT_FAILED, "a variant cannot be read");
      }
      return;
    }
    number++;
    const char *label = format(build, "variant %d: %s", number,
                               join_list(build, cJSON_GetObjectItemCaseSensitive(derived, "operations"), "op"));
    add_motif_section(build, label, cJSON_GetObjectItemCaseSensitive(variant_body, "motif"), false, derived);
  }
}

/* The body copy kept in the theme's uses for a motif version: the slot is rendered from what the theme was made with. */
static const cJSON *used_motif(const cJSON *body, const char *item_id, int version) {
  const cJSON *use = NULL;
  cJSON_ArrayForEach(use, cJSON_GetObjectItemCaseSensitive(body, "uses")) {
    const cJSON *number = cJSON_GetObjectItemCaseSensitive(use, "version");
    if (strcmp(ursatz_gui_json_string(use, "item_id"), item_id) == 0 && cJSON_IsNumber(number) &&
        number->valueint == version) {
      return cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(use, "copy"), "motif");
    }
  }
  return NULL;
}

static void build_theme(Build *build, const char *id, int version, const cJSON *body) {
  const cJSON *theme = cJSON_GetObjectItemCaseSensitive(body, "theme");
  if (!cJSON_IsObject(theme)) {
    fail(build, URSATZ_GUI_MATERIAL_EXPORT_FAILED, "the item has no theme");
    return;
  }
  const cJSON *form = cJSON_GetObjectItemCaseSensitive(theme, "form");
  const cJSON *member = NULL;
  if (cJSON_IsObject(form)) {
    cJSON_ArrayForEach(member, form) {
      add_fact(build, &build->material_facts, member->string, scalar_text(build, member));
    }
  }
  const cJSON *chord = NULL;
  cJSON_ArrayForEach(chord, cJSON_GetObjectItemCaseSensitive(theme, "chords")) {
    const cJSON *bar = cJSON_GetObjectItemCaseSensitive(chord, "bar");
    const cJSON *text = cJSON_GetObjectItemCaseSensitive(chord, "text");
    if (cJSON_IsNumber(bar) && cJSON_IsString(text)) {
      add_fact(build, &build->material_facts, format(build, "bar %d", bar->valueint), text->valuestring);
    }
  }
  /* The slots, each with the motif's name: one fact now, one section after the theme's. */
  const cJSON *slots = cJSON_GetObjectItemCaseSensitive(theme, "slots");
  const cJSON *slot = NULL;
  cJSON_ArrayForEach(slot, slots) {
    const cJSON *name = cJSON_GetObjectItemCaseSensitive(slot, "slot");
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(slot, "item_id");
    const cJSON *number = cJSON_GetObjectItemCaseSensitive(slot, "version");
    if (!cJSON_IsString(name) || !cJSON_IsString(item) || !cJSON_IsNumber(number)) {
      continue;
    }
    cJSON *meta = NULL;
    const char *motif_name = item->valuestring;
    if (ursatz_gui_material_item_get(item->valuestring, &meta, NULL) == URSATZ_GUI_MATERIAL_OK && keep_document(build, meta)) {
      motif_name = ursatz_gui_json_string(meta, "name");
    }
    add_fact(build, &build->material_facts, format(build, "slot %s", name->valuestring),
             format(build, "%s v%d", motif_name, number->valueint));
  }
  const char *xml = NULL;
  read_version_xml(build, id, version, "theme.musicxml", &xml);
  add_draft(build, "theme", xml);
  cJSON_ArrayForEach(slot, slots) {
    const cJSON *name = cJSON_GetObjectItemCaseSensitive(slot, "slot");
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(slot, "item_id");
    const cJSON *number = cJSON_GetObjectItemCaseSensitive(slot, "version");
    if (failed(build)) {
      return;
    }
    if (!cJSON_IsString(name) || !cJSON_IsString(item) || !cJSON_IsNumber(number)) {
      continue;
    }
    const cJSON *motif = used_motif(body, item->valuestring, number->valueint);
    cJSON *slot_body = NULL;
    if (!motif) {
      if (ursatz_gui_material_item_get_version(item->valuestring, number->valueint, &slot_body) !=
              URSATZ_GUI_MATERIAL_OK ||
          !keep_document(build, slot_body)) {
        if (!failed(build)) {
          fail(build, URSATZ_GUI_MATERIAL_EXPORT_FAILED, "a slot motif cannot be read");
        }
        return;
      }
      motif = cJSON_GetObjectItemCaseSensitive(slot_body, "motif");
    }
    cJSON *meta = NULL;
    const char *motif_name = item->valuestring;
    if (ursatz_gui_material_item_get(item->valuestring, &meta, NULL) == URSATZ_GUI_MATERIAL_OK && keep_document(build, meta)) {
      motif_name = ursatz_gui_json_string(meta, "name");
    }
    add_motif_section(build, format(build, "slot %s: %s v%d", name->valuestring, motif_name, number->valueint), motif,
                      false, NULL);
  }
}

static void build_piece(Build *build, const char *id, int version, const cJSON *body) {
  const cJSON *piece = cJSON_GetObjectItemCaseSensitive(body, "piece");
  if (!cJSON_IsObject(piece)) {
    fail(build, URSATZ_GUI_MATERIAL_EXPORT_FAILED, "the item has no piece");
    return;
  }
  const cJSON *structure = cJSON_GetObjectItemCaseSensitive(piece, "structure");
  if (cJSON_IsObject(structure)) {
    add_text_member(build, &build->material_facts, "key", structure, "key");
    add_text_member(build, &build->material_facts, "mode", structure, "mode");
    add_text_member(build, &build->material_facts, "meter", structure, "meter");
    const cJSON *section = NULL;
    cJSON_ArrayForEach(section, cJSON_GetObjectItemCaseSensitive(structure, "sections")) {
      const cJSON *letter = cJSON_GetObjectItemCaseSensitive(section, "letter");
      const cJSON *part = cJSON_GetObjectItemCaseSensitive(section, "part");
      const cJSON *beats = cJSON_GetObjectItemCaseSensitive(section, "beats");
      const cJSON *fill = cJSON_GetObjectItemCaseSensitive(section, "fill");
      const cJSON *cadence = cJSON_GetObjectItemCaseSensitive(section, "cadence");
      if (!cJSON_IsString(letter) || !cJSON_IsString(part) || !cJSON_IsNumber(beats)) {
        continue;
      }
      const cJSON *kind = cJSON_GetObjectItemCaseSensitive(fill, "kind");
      add_fact(build, &build->material_facts, format(build, "%s %s", letter->valuestring, part->valuestring),
               format(build, "%.15g beats, fill %s, cadence %s", beats->valuedouble,
                      cJSON_IsString(kind) ? kind->valuestring : "empty",
                      cJSON_IsString(cadence) && cadence->valuestring[0] != '\0' ? cadence->valuestring : "none"));
    }
  }
  static const char *const files[] = {"loop.musicxml", "ending.musicxml"};
  static const char *const labels[] = {"loop", "ending"};
  for (size_t i = 0; i < 2 && !failed(build); i++) {
    const char *xml = NULL;
    read_version_xml(build, id, version, files[i], &xml);
    add_draft(build, labels[i], xml);
  }
}

/* ---- the export ---------------------------------------------------------------------------------------------------- */

static void release(Build *build) {
  for (size_t i = 0; i < build->string_count; i++) {
    free(build->strings[i]);
  }
  for (size_t i = 0; i < build->document_count; i++) {
    cJSON_Delete(build->documents[i]);
  }
  for (size_t i = 0; i < build->draft_count; i++) {
    free(build->drafts[i].facts.items);
    free(build->drafts[i].segments.items);
  }
  free(build->strings);
  free(build->documents);
  free(build->drafts);
  free(build->material_facts.items);
}

static UrsatzGuiMaterialExportResult write_text(Build *build, const char *kind, const char *id, const char *name,
                                                int version, char **text) {
  AnalyzerTextExportSection *sections = calloc(build->draft_count > 0 ? build->draft_count : 1, sizeof(*sections));
  if (!sections) {
    return URSATZ_GUI_MATERIAL_EXPORT_ALLOC;
  }
  for (size_t i = 0; i < build->draft_count; i++) {
    const Draft *draft = &build->drafts[i];
    sections[i].label = draft->label;
    sections[i].musicxml = draft->musicxml;
    sections[i].facts = draft->facts.items;
    sections[i].fact_count = draft->facts.count;
    sections[i].segments = draft->segments.items;
    sections[i].segment_count = draft->segments.count;
  }
  AnalyzerTextExportMaterial material = {
      .kind = kind,
      .id = id,
      .name = name,
      .version = version,
      .facts = build->material_facts.items,
      .fact_count = build->material_facts.count,
      .sections = sections,
      .section_count = build->draft_count,
  };
  AnalyzerStatus status = analyzer_text_export_material(&material, text);
  free(sections);
  if (status == ANALYZER_OK) {
    return URSATZ_GUI_MATERIAL_EXPORT_OK;
  }
  if (status == ANALYZER_ERR_ALLOC) {
    return URSATZ_GUI_MATERIAL_EXPORT_ALLOC;
  }
  snprintf(build->message, sizeof(build->message), "%s",
           status == ANALYZER_ERR_ARGUMENT ? "ANALYZER_ERR_ARGUMENT" : "ANALYZER_ERR_LIBRARY");
  fprintf(stderr, "ursatz_gui_material_export: analyzer_text_export_material failed for item '%s' with AnalyzerStatus=%d\n", id,
          (int)status);
  return URSATZ_GUI_MATERIAL_EXPORT_FAILED;
}

UrsatzGuiMaterialExportResult ursatz_gui_material_export(const char *item_id, int version, char **text,
                                                          char message[URSATZ_GUI_MATERIAL_EXPORT_MESSAGE_SIZE]) {
  *text = NULL;
  message[0] = '\0';
  if (!item_id || version < 0) {
    return URSATZ_GUI_MATERIAL_EXPORT_NOT_FOUND;
  }
  Build build;
  memset(&build, 0, sizeof(build));
  cJSON *meta = NULL;
  cJSON *body = NULL;
  UrsatzGuiMaterialResult found = ursatz_gui_material_item_get(item_id, &meta, &body);
  if (found != URSATZ_GUI_MATERIAL_OK) {
    return found == URSATZ_GUI_MATERIAL_ERR_ALLOC ? URSATZ_GUI_MATERIAL_EXPORT_ALLOC
           : found == URSATZ_GUI_MATERIAL_ERR_NOT_FOUND || found == URSATZ_GUI_MATERIAL_ERR_INVALID_ID
               ? URSATZ_GUI_MATERIAL_EXPORT_NOT_FOUND
               : URSATZ_GUI_MATERIAL_EXPORT_FAILED;
  }
  if (!keep_document(&build, meta)) {
    cJSON_Delete(body);
    release(&build);
    return URSATZ_GUI_MATERIAL_EXPORT_ALLOC;
  }
  if (!keep_document(&build, body)) {
    release(&build);
    return URSATZ_GUI_MATERIAL_EXPORT_ALLOC;
  }
  const cJSON *current = cJSON_GetObjectItemCaseSensitive(meta, "current_version");
  if (version == 0 || (cJSON_IsNumber(current) && version == current->valueint)) {
    version = cJSON_IsNumber(current) ? current->valueint : 1;
  } else {
    cJSON *older = NULL;
    UrsatzGuiMaterialResult read = ursatz_gui_material_item_get_version(item_id, version, &older);
    if (read != URSATZ_GUI_MATERIAL_OK) {
      release(&build);
      return read == URSATZ_GUI_MATERIAL_ERR_NOT_FOUND ? URSATZ_GUI_MATERIAL_EXPORT_NOT_FOUND
             : read == URSATZ_GUI_MATERIAL_ERR_ALLOC   ? URSATZ_GUI_MATERIAL_EXPORT_ALLOC
                                                       : URSATZ_GUI_MATERIAL_EXPORT_FAILED;
    }
    body = keep_document(&build, older);
    if (!body) {
      release(&build);
      return URSATZ_GUI_MATERIAL_EXPORT_ALLOC;
    }
  }
  const char *type = ursatz_gui_json_string(meta, "type");
  if (strcmp(type, "motif") == 0) {
    build_motif(&build, item_id, body);
  } else if (strcmp(type, "theme") == 0) {
    build_theme(&build, item_id, version, body);
  } else if (strcmp(type, "piece") == 0) {
    build_piece(&build, item_id, version, body);
  } else {
    fail(&build, URSATZ_GUI_MATERIAL_EXPORT_FAILED, "the item is not a motif, theme or piece");
  }
  UrsatzGuiMaterialExportResult result = build.result;
  if (!failed(&build)) {
    result = write_text(&build, type, item_id, ursatz_gui_json_string(meta, "name"), version, text);
  }
  snprintf(message, URSATZ_GUI_MATERIAL_EXPORT_MESSAGE_SIZE, "%s", build.message);
  release(&build);
  return result;
}
