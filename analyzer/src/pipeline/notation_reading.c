#include <ursatz_analyzer/notation_reading.h>

#include <ctype.h>
#include <limits.h>
#include <stdlib.h>

/* One unsigned decimal number from *text, advancing past it; false when there is none or it overflows. */
static bool parse_number(const char **text, unsigned long *out) {
    const char *p = *text;
    if (!isdigit((unsigned char)*p)) {
        return false;
    }
    unsigned long value = 0;
    for (; isdigit((unsigned char)*p); p++) {
        unsigned digit = (unsigned)(*p - '0');
        if (value > (ULONG_MAX - digit) / 10) {
            return false;
        }
        value = value * 10 + digit;
    }
    *text = p;
    *out = value;
    return true;
}

/* "n" or "n/d", the whole text; the denominator is 1 for "n". */
static bool parse_fraction(const char *text, unsigned long *numerator, unsigned long *denominator) {
    if (!text || !parse_number(&text, numerator)) {
        return false;
    }
    *denominator = 1;
    if (*text == '/') {
        text++;
        if (!parse_number(&text, denominator)) {
            return false;
        }
    }
    return *text == '\0' && *denominator != 0;
}

static bool is_power_of_two(unsigned long value) {
    return value != 0 && (value & (value - 1)) == 0;
}

bool notation_reading_parse_meter(const char *text, NotationReading *r) {
    unsigned long beats;
    unsigned long beat_type;
    if (!text || !parse_number(&text, &beats) || *text++ != '/' || !parse_number(&text, &beat_type) || *text != '\0') {
        return false;
    }
    if (beats < 1 || beats > 64 || beat_type > 16 || !is_power_of_two(beat_type)) {
        return false;
    }
    r->has_meter = true;
    r->beats = (int)beats;
    r->beat_type = (int)beat_type;
    return true;
}

bool notation_reading_parse_scale(const char *text, NotationReading *r) {
    unsigned long numerator;
    unsigned long denominator;
    if (!parse_fraction(text, &numerator, &denominator)) {
        return false;
    }
    bool allowed = (denominator == 1 && (numerator == 1 || numerator == 2 || numerator == 4)) ||
                   (numerator == 1 && (denominator == 2 || denominator == 4));
    if (!allowed) {
        return false;
    }
    r->scale_numerator = (long)numerator;
    r->scale_denominator = denominator;
    return true;
}

bool notation_reading_parse_pickup(const char *text, NotationReading *r) {
    unsigned long numerator;
    unsigned long denominator;
    if (!parse_fraction(text, &numerator, &denominator) || numerator == 0 || numerator > LONG_MAX) {
        return false;
    }
    r->has_pickup = true;
    r->pickup_numerator = (long)numerator;
    r->pickup_denominator = denominator;
    return true;
}

NotationReading notation_reading_none(void) {
    NotationReading none = {.scale_numerator = 1, .scale_denominator = 1, .pickup_denominator = 1};
    return none;
}
