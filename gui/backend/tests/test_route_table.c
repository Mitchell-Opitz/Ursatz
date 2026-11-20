#include <stdio.h>
#include <string.h>

#include "routes/route_table.h"
#include "ursatz_check.h"

/* The one parser of the route table (R-647): a pattern's literal segments must equal the path's, a placeholder takes one segment, the
 * counts must agree, and nothing but a clean path matches. */

static int matches(const char *pattern, const char *path, UrsatzGuiRouteArgs *args) {
  memset(args, 0, sizeof(*args));
  return ursatz_gui_route_match(pattern, path, args);
}

int main(void) {
  UrsatzGuiRouteArgs args;
  check(matches("/api/jobs/<id>", "/api/jobs/job-1", &args) && args.count == 1 && strcmp(args.values[0], "job-1") == 0,
        "a placeholder takes a segment");
  check(matches("/api/materials/items/<id>/versions/<n>/files/<name>", "/api/materials/items/thm-1/versions/2/files/theme.mid", &args) &&
            args.count == 3 && strcmp(args.values[0], "thm-1") == 0 && strcmp(args.values[1], "2") == 0 && strcmp(args.values[2], "theme.mid") == 0,
        "placeholders are given in order");
  check(matches("/api/subjects/groups", "/api/subjects/groups", &args) && args.count == 0, "a pattern of literals needs no arguments");
  check(!matches("/api/subjects/groups", "/api/subjects/other", &args), "a literal that differs does not match");
  check(!matches("/api/jobs/<id>", "/api/jobs", &args) && !matches("/api/jobs/<id>", "/api/jobs/a/b", &args), "the segment counts must agree");
  check(!matches("/api/jobs/<id>", "/api/jobs/", &args) && !matches("/api/jobs/<id>", "/api/jobs//x", &args), "an empty segment matches nothing");
  check(!matches("/api/jobs/<id>", "api/jobs/x", &args), "a path that does not start with a slash matches nothing");
  char long_segment[400];
  memset(long_segment, 'a', sizeof(long_segment) - 1);
  long_segment[sizeof(long_segment) - 1] = '\0';
  char path[512];
  snprintf(path, sizeof(path), "/api/jobs/%s", long_segment);
  check(!matches("/api/jobs/<id>", path, &args), "a segment of 256 bytes or more matches nothing");
  return test_finish("route_table");
}
