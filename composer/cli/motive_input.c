#include "motive_input.h"

#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/motive.h>

#include "cli_options.h"

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
            if (!cli_parse_pickup(options->pickup_text, out->meter.beat_unit, &pickup)) {
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

bool motive_input_take_basic(BasicInput *basic, int argc, char **argv, int *index, int *status) {
    const char *argument = argv[*index];
    const char **slot = strcmp(argument, "--basic") == 0           ? &basic->path
                        : strcmp(argument, "--basic-format") == 0  ? &basic->format_text
                        : strcmp(argument, "--basic-pickup") == 0  ? &basic->pickup_text
                        : strcmp(argument, "--basic-harmony") == 0 ? &basic->harmony_text
                                                                   : NULL;
    if (!slot) {
        return false;
    }
    *status = 0;
    if (*index + 1 >= argc) {
        *status = cli_fail(CLI_EXIT_USAGE, "%s needs a value", argument);
    } else if (*slot) {
        *status = cli_fail(CLI_EXIT_USAGE, "%s given twice", argument);
    } else {
        *slot = argv[++*index];
    }
    return true;
}

bool motive_input_basic_given(const BasicInput *basic) {
    return basic->path || basic->format_text || basic->pickup_text || basic->harmony_text;
}

int motive_input_load_basic(const BasicInput *basic, const MotiveInput *source, Motive *out) {
    if (!basic->path) {
        return cli_fail(CLI_EXIT_USAGE, "--basic-format, --basic-pickup and --basic-harmony need --basic <file>");
    }
    MotiveInput input;
    memset(&input, 0, sizeof(input));
    input.path = basic->path;
    input.meter_text = source->meter_text;
    input.key_text = source->key_text;
    input.form_text = source->form_text;
    input.format_text = basic->format_text;
    input.pickup_text = basic->pickup_text;
    input.extras.harmony_text = basic->harmony_text;
    input.extras.role_text = source->extras.role_text;
    return motive_input_load(&input, out);
}

void motive_input_print_basic_help(FILE *out) {
    fprintf(out, "  --basic <file>       the basic motive the family of forms comes from, read in the source's form, meter, key\n");
    fprintf(out, "                       and role; remoteness is measured from it. Default: the source is the basic motive\n");
    fprintf(out, "  --basic-format <f>, --basic-pickup <b>, --basic-harmony \"<text>\"   as for the basic motive's own file\n");
}
