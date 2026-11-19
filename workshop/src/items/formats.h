#ifndef WORKSHOP_ITEMS_FORMATS_H
#define WORKSHOP_ITEMS_FORMATS_H

/*
 * The formats of the files the workshop owns (R-642, <ursatz_workshop/workshop_api.h> names them; schemas/<name>.v1.schema.json
 * describe them) and the one reader and writer of them. Every file carries "format" and "format_version"; a reader refuses another
 * format, a version it does not know or a file that lacks a member its format requires with URSATZ_ERR_FORMAT naming the file.
 * Services keep the shapes they always held in memory: reading strips the two members and writing adds them.
 */

#include <stdbool.h>
#include <stddef.h>

#include <cJSON.h>

#include <common/status.h>
#include <ursatz_workshop/workshop_api.h>

/* The version this build writes (and the newest it reads) of format; 0 for a name that is not a workshop format. */
int workshop_format_version(const char *format);

/* True when name is one of the workshop's format names. */
bool workshop_format_known(const char *name);

/* Adds "format" and "format_version" to object as its first members (replacing any it has). false on allocation failure. */
bool workshop_format_stamp(cJSON *object, const char *format);

/* Removes "format" and "format_version" from object. */
void workshop_format_strip(cJSON *object);

/*
 * Checks object against format as the file at path: an object whose "format" is format, whose "format_version" is a whole number from
 * 1 to the version this build reads, and that holds every member the format requires with its type. The members of the file other than
 * these are not looked at. URSATZ_ERR_FORMAT names path; format may be NULL to accept any workshop format (the three body formats
 * are told apart by their own member).
 */
UrsatzStatus workshop_format_check(const cJSON *object, const char *format, const char *path, UrsatzError *error);

/* Reads path, checks it as format (workshop_format_check) and, with strip, removes the two members. NOT_FOUND when the file is absent. */
UrsatzStatus workshop_format_read(const char *path, const char *format, bool strip, cJSON **out, UrsatzError *error);

/* Stamps object (in place, then unstamped again) and writes it to path atomically, after checking it as format. */
UrsatzStatus workshop_format_write(const char *path, const char *format, cJSON *object, UrsatzError *error);

/*
 * The format of the file at a path relative to the data folder (layout.h), or NULL when the layout names no workshop format for it (a
 * composed version's MIDI, the analysis document, request.json...). A body's format is the one of its item's type: for a body.json
 * this returns "" and the caller reads the file's own "format" member.
 */
const char *workshop_format_of_path(const char *relative_path);

#endif
