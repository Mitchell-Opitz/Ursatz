#include "motive_input.h"

#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/motive.h>

#include "cli_options.h"

/* "0", "1", "1/2": beats of the beat unit, as whole notes. */
static bool parse_pickup(const char *text, int beat_unit, PlanTime *out) {
    char *end = NULL;
    long numerator = strtol(text, &end, 10);
    long denominator = 1;
    if (end == text || numerator < 0) {
        return false;
    }
    if (*end == '/') {
        const char *rest = end + 1;
        denominator = strtol(rest, &end, 10);
        if (end == rest || denominator < 1) {
            return false;
        }
    }
    return *end == '\0' && plan_time_make(numerator, (unsigned long)(denominator * beat_unit), out) == PLAN_OK;
}

int motive_input_load(const MotiveInput *options, Motive *out) {
    MotiveOptions motive_options;
    memset(&motive_options, 0, sizeof(motive_options));
    if (options->meter_text) {
        if (!motive_parse_meter(options->meter_text, &motive_options.meter)) {
            return cli_fail(CLI_EXIT_USAGE, "meter '%s' is not a meter; write beats over a beat unit, as in 4/4 or 6/8",
                            options->meter_text);
        }
        motive_options.has_meter = true;
    }
    if (options->key_text) {
        if (!motive_parse_key(options->key_text, &motive_options.key)) {
            return cli_fail(CLI_EXIT_USAGE,
                            "unknown key '%s'; write a tonic letter A-G with an optional # or b, then major or minor, "
                            "for example \"D minor\"",
                            options->key_text);
        }
        motive_options.has_key = true;
    }
    MotiveFormat format = MOTIVE_FORMAT_AUTO;
    if (options->format_text && !motive_format_parse(options->format_text, &format)) {
        return cli_fail(CLI_EXIT_USAGE,
                        "--format must be auto, midi, musicxml, notes, degrees, rhythm, contour or json, not '%s'",
                        options->format_text);
    }
    if (options->form_text) {
        MotiveForm form;
        if (!motive_form_parse(options->form_text, &form)) {
            return cli_fail(CLI_EXIT_USAGE, "--form must be full, rhythm or contour, not '%s'", options->form_text);
        }
        MotiveFormat wanted = form == MOTIVE_FORM_RHYTHM ? MOTIVE_FORMAT_RHYTHM : MOTIVE_FORMAT_CONTOUR;
        if (form != MOTIVE_FORM_FULL && format != MOTIVE_FORMAT_AUTO && format != wanted) {
            return cli_fail(CLI_EXIT_USAGE, "--form %s reads a %s, which --format %s does not", options->form_text,
                            options->form_text, options->format_text);
        }
        if (form == MOTIVE_FORM_FULL && (format == MOTIVE_FORMAT_RHYTHM || format == MOTIVE_FORMAT_CONTOUR)) {
            return cli_fail(CLI_EXIT_USAGE, "--form full does not read a %s; use --form %s or another --format",
                            options->format_text, options->format_text);
        }
        if (form != MOTIVE_FORM_FULL) {
            format = wanted;
        }
    }

    MotiveError error;
    MotiveResult result = motive_load_file(options->path, format, &motive_options, out, &error);
    if (result == MOTIVE_OK && (options->pickup_text || motive_extras_given(&options->extras))) {
        /* The pickup and the chord onsets are in beats of the motive's meter, which a file may declare, so they are
           read after the first load. */
        if (options->pickup_text) {
            PlanTime pickup;
            if (!parse_pickup(options->pickup_text, out->meter.beat_unit, &pickup)) {
                return cli_fail(CLI_EXIT_USAGE, "pickup '%s' is not a number of beats; write 0, 1 or 1/2",
                                options->pickup_text);
            }
            motive_options.has_pickup = true;
            motive_options.pickup = pickup;
        }
        int status = motive_extras_options(&options->extras, &out->meter, &motive_options);
        if (status != 0) {
            return status;
        }
        result = motive_load_file(options->path, format, &motive_options, out, &error);
    }
    if (result != MOTIVE_OK) {
        return cli_fail(CLI_EXIT_UNSATISFIABLE, "cannot load '%s' (%s)%s%s: %s", options->path,
                        motive_result_name(result), error.location[0] ? " at " : "", error.location, error.message);
    }
    return 0;
}
