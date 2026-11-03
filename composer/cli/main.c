#include <stdio.h>
#include <string.h>

#include "counterpoint.h"
#include "develop.h"
#include "fill.h"
#include "grow.h"
#include "motive.h"
#include "plan.h"
#include "progression.h"
#include "random_motive.h"
#include "recognize.h"
#include "regenerate.h"
#include "piece.h"
#include "theme.h"
#include "variations.h"
#include "variants_cmd.h"
#include "cli_options.h"

#include <ursatz_composer/composer.h>
#include <ursatz_composer/export.h>
#include <ursatz_composer/plan_json.h>
#include <ursatz_composer/render.h>
#include "util/plan_util.h"

static void print_usage(FILE *out) {
    fprintf(out, "Usage: ursatz-composer [--version] [--help]\n");
    fprintf(out, "       ursatz-composer export <plan.json> [--midi <out.mid>] [--musicxml <out.xml>] [--tempo <bpm>] [--plain-midi]\n");
    fprintf(out, "%s\n", PROGRESSION_USAGE_LINE);
    fprintf(out, "%s\n", MOTIVE_USAGE_LINE);
    fprintf(out, "%s\n", DEVELOP_USAGE_LINE);
    fprintf(out, "%s\n", GROW_USAGE_LINE);
    fprintf(out, "%s\n", THEME_USAGE_LINE);
    fprintf(out, "%s\n", PIECE_USAGE_LINE);
    fprintf(out, "%s\n", COUNTERPOINT_USAGE_LINE);
    fprintf(out, "%s\n", PLAN_USAGE_LINE);
    fprintf(out, "%s\n", FILL_USAGE_LINE);
    fprintf(out, "%s\n", LOCK_USAGE_LINE);
    fprintf(out, "%s\n", REGENERATE_USAGE_LINE);
    fprintf(out, "%s\n", VARIATIONS_USAGE_LINE);
    fprintf(out, "%s\n", VARIANTS_USAGE_LINE);
    fprintf(out, "%s\n", RECOGNIZE_USAGE_LINE);
    fprintf(out, "%s\n", RANDOM_MOTIVE_USAGE_LINE);
    fprintf(out, "%s\n", HARMONIZE_USAGE_LINE);
}

static void report_skipped(const RenderedPlan *rendered) {
    for (size_t i = 0; i < rendered->skipped_count; i++) {
        const RenderSkip *skip = &rendered->skipped[i];
        fprintf(stderr, "Skipped lane span %zu (%s): %s\n", skip->span, plan_lane_name(skip->lane),
                skip->reason == RENDER_SKIP_NOT_REALIZED ? "not Given or Realized"
                : skip->reason == RENDER_SKIP_MATERIAL ? "source material"
                : skip->reason == RENDER_SKIP_LAYER    ? "not in the layer"
                : skip->reason == RENDER_SKIP_SEAM     ? "seam"
                                                         : "lane is not rendered yet");
    }
}

static int run_export(int argc, char **argv) {
    const char *plan_path = NULL;
    const char *midi_path = NULL;
    const char *musicxml_path = NULL;
    const char *tempo_text = NULL;
    bool plain_midi = false;
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--tempo") == 0 && i + 1 < argc && !tempo_text) {
            tempo_text = argv[++i];
        } else if (strcmp(argv[i], "--plain-midi") == 0) {
            plain_midi = true;
        } else if (strcmp(argv[i], "--midi") == 0 && i + 1 < argc && !midi_path) {
            midi_path = argv[++i];
        } else if (strcmp(argv[i], "--musicxml") == 0 && i + 1 < argc && !musicxml_path) {
            musicxml_path = argv[++i];
        } else if (argv[i][0] != '-' && !plan_path) {
            plan_path = argv[i];
        } else {
            fprintf(stderr, "Error: unexpected argument '%s'\n", argv[i]);
            print_usage(stderr);
            return 1;
        }
    }
    if (!plan_path || (!midi_path && !musicxml_path)) {
        fprintf(stderr, "Error: export needs a plan file and at least one of --midi or --musicxml\n");
        print_usage(stderr);
        return 1;
    }

    PlanExportOptions export_options;
    if (cli_read_export_options(tempo_text, plain_midi, &export_options) != 0) {
        return 1;
    }

    PiecePlan *plan = NULL;
    PlanJsonError load_error;
    if (plan_read_json_file(plan_path, &plan, &load_error) != PLAN_OK) {
        fprintf(stderr, "Error: cannot read plan '%s': %s%s%s\n", plan_path, load_error.message,
                load_error.path[0] ? " at " : "", load_error.path);
        return 1;
    }

    RenderedPlan rendered;
    if (plan_export_render(plan, &rendered, NULL) != PLAN_EXPORT_OK) {
        fprintf(stderr, "Error: cannot render plan '%s'\n", plan_path);
        plan_destroy(plan);
        return 1;
    }
    report_skipped(&rendered);

    PlanExportError export_error;
    int status = 0;
    if (midi_path && rendered_plan_export_midi(&rendered, &export_options, midi_path, &export_error) != PLAN_EXPORT_OK) {
        fprintf(stderr, "Error: MIDI export failed: %s\n", export_error.message);
        status = 1;
    } else if (midi_path) {
        printf("MIDI written to %s\n", midi_path);
    }
    if (musicxml_path && rendered_plan_export_musicxml(&rendered, &export_options, musicxml_path, &export_error) != PLAN_EXPORT_OK) {
        fprintf(stderr, "Error: MusicXML export failed: %s\n", export_error.message);
        status = 1;
    } else if (musicxml_path) {
        printf("MusicXML written to %s\n", musicxml_path);
    }

    rendered_plan_destroy(&rendered);
    plan_destroy(plan);
    return status;
}

int main(int argc, char **argv) {
    if (argc >= 2 && strcmp(argv[1], "export") == 0) {
        return run_export(argc, argv);
    }
    if (argc >= 2 && strcmp(argv[1], "progression") == 0) {
        return run_progression(argc, argv);
    }
    if (argc >= 2 && strcmp(argv[1], "motive") == 0) {
        return run_motive(argc, argv);
    }
    if (argc >= 2 && strcmp(argv[1], "develop") == 0) {
        return run_develop(argc, argv);
    }
    if (argc >= 2 && strcmp(argv[1], "grow") == 0) {
        return run_grow(argc, argv);
    }
    if (argc >= 2 && strcmp(argv[1], "theme") == 0) {
        return run_theme(argc, argv);
    }
    if (argc >= 2 && strcmp(argv[1], "piece") == 0) {
        return run_piece(argc, argv);
    }
    if (argc >= 2 && strcmp(argv[1], "counterpoint") == 0) {
        return run_counterpoint(argc, argv);
    }
    if (argc >= 2 && strcmp(argv[1], "plan") == 0) {
        return run_plan(argc, argv);
    }
    if (argc >= 2 && strcmp(argv[1], "fill") == 0) {
        return run_fill(argc, argv);
    }
    if (argc >= 2 && strcmp(argv[1], "lock") == 0) {
        return run_lock(argc, argv);
    }
    if (argc >= 2 && strcmp(argv[1], "regenerate") == 0) {
        return run_regenerate(argc, argv);
    }
    if (argc >= 2 && strcmp(argv[1], "variations") == 0) {
        return run_variations(argc, argv);
    }
    if (argc >= 2 && strcmp(argv[1], "variants") == 0) {
        return run_variants(argc, argv);
    }
    if (argc >= 2 && strcmp(argv[1], "recognize") == 0) {
        return run_recognize(argc, argv);
    }
    if (argc >= 2 && strcmp(argv[1], "random-motive") == 0) {
        return run_random_motive(argc, argv);
    }
    if (argc >= 2 && strcmp(argv[1], "harmonize") == 0) {
        return run_harmonize(argc, argv);
    }
    if (argc == 2 && strcmp(argv[1], "--version") == 0) {
        printf("ursatz-composer %s\n", composer_version());
        return 0;
    }
    if (argc == 2 && strcmp(argv[1], "--help") == 0) {
        print_usage(stdout);
        return 0;
    }
    print_usage(stderr);
    return argc == 1 ? 0 : 1;
}
