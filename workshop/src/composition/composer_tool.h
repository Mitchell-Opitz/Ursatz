#ifndef URSATZ_WORKSHOP_COMPOSITION_COMPOSER_TOOL_H
#define URSATZ_WORKSHOP_COMPOSITION_COMPOSER_TOOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <cJSON.h>

#include <common/status.h>
#include <ursatz_composer/composer_api.h>

/*
 * The tool "composer" (R-656): one Composer API entry called with the workshop's knowledge, no job and no data folder (the coordinator's
 * tools.c reads the params and fills the reply). The entries are export, motive, motive_from_midi (midi is the MIDI file), motive_render,
 * variant_operations, variants, recognize, random_motive, theme, piece, piece_variations, plan, fill, lock, regenerate, variations, harmonize,
 * progression, counterpoint, develop, grow, vocabulary, versions and validate (kind names the request's kind). request is required except for
 * versions, vocabulary and variant_operations. out is the Composer's response, the caller frees it with composer_response_destroy; the
 * vocabulary's json is a copy of the cached text. A refusal answers the Composer's status and error unchanged (its path points into the
 * request); an unknown entry is URSATZ_ERR_REQUEST, path "/params/entry".
 */
UrsatzStatus workshop_composer_tool(const char *entry, const cJSON *request, const char *kind, const uint8_t *midi, size_t midi_size,
                                    ComposerResponse *out, UrsatzError *error);

#endif
