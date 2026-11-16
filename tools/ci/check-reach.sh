#!/usr/bin/env bash
# Reach scan (Law 25 as amended, R-529): lists every function in Ursatz's own C code that no application reaches.
# Roots are the mains of the three applications: ursatz-analyzer, ursatz-composer and ursatz-gui-backend.
#
# usage: bash tools/ci/check-reach.sh [<scan build dir>]     (default: build-reach at the repository root)
#
# The scan builds its own tree from scratch (tests off, -ffunction-sections -fdata-sections: without the second, a switch
# jump table in a file's shared .rodata keeps its function alive), relinks each application from every object of
# the targets it links with --gc-sections, and takes as reached every function section some application keeps.
# It writes <dir>/reach/unreached.txt, one "<source path> <function>" line per unreached function.
# With tools/ci/reach-allow.txt present it exits 1 when an unreached function is not listed there, or a listed one
# is reached or gone. Without that file it only reports.
set -euo pipefail

cd "$(git rev-parse --show-toplevel)"
root=$(pwd)
dir=${1:-build-reach}
allow="$root/tools/ci/reach-allow.txt"

# Every run builds from scratch: objects and archives of deleted files would stay in a reused tree and count.
if [ -e "$dir" ] && [ ! -f "$dir/.reach-scan" ] && [ -n "$(ls -A "$dir" 2>/dev/null)" ]; then
  echo "check-reach: $dir is not empty and not a scan dir; give a new or empty dir" >&2
  exit 2
fi
rm -rf "$dir"
mkdir -p "$dir"
dir=$(cd "$dir" && pwd)
: > "$dir/.reach-scan"

cmake -S "$root" -B "$dir" -DURSATZ_TESTS=OFF "-DCMAKE_C_FLAGS=-ffunction-sections -fdata-sections" > /dev/null
cmake --build "$dir" -j > "$dir/reach-build.log" 2>&1 || {
  tail -50 "$dir/reach-build.log" >&2
  echo "check-reach: the scan build failed (log: $dir/reach-build.log)" >&2
  exit 1
}

out="$dir/reach"
rm -rf "$out"
mkdir -p "$out"

# Objects of one archive target: <build dir of the archive>/CMakeFiles/<target>.dir/**/*.c.o
objects_of() {
  local a=$1 name
  name=$(basename "$a" .a)
  name=${name#lib}
  find "$(dirname "$a")/CMakeFiles/$name.dir" -name '*.c.o' | sort
}

# Source path (repository relative) of an object under the scan build: <B>/CMakeFiles/<t>.dir/<rel>.o -> <B rel>/<rel>
source_of() {
  awk -v dir="$dir/" '{
    o = $1; f = $2
    sub(dir, "", o)
    i = index(o, "/CMakeFiles/"); b = substr(o, 1, i - 1); r = substr(o, i + 12)
    sub(/^[^\/]+\.dir\//, "", r); sub(/\.o$/, "", r); gsub(/__\//, "../", r)
    p = b "/" r
    n = split(p, s, "/"); m = 0
    for (k = 1; k <= n; k++) { if (s[k] == "..") m--; else if (s[k] != "." && s[k] != "") t[++m] = s[k] }
    q = t[1]; for (k = 2; k <= m; k++) q = q "/" t[k]
    print q, f
  }'
}

sections_of() {
  for o in "$@"; do
    objdump -h "$o" | awk -v o="$o" '$2 ~ /^\.text\./ { s = $2; sub(/^\.text\./, "", s); print o, s }'
  done
}

: > "$out/kept"
: > "$out/objects"
found=0
for app in ursatz-analyzer ursatz-composer ursatz-gui-backend; do
  lt=$(find "$dir" -path "*/CMakeFiles/$app.dir/link.txt" | head -1)
  [ -n "$lt" ] || continue
  found=$((found + 1))
  bdir=$(dirname "$(dirname "$(dirname "$lt")")")
  cmd="" objs=""
  for w in $(cat "$lt"); do
    case "$w" in
      *third_party*) cmd="$cmd $w" ;;
      *.a)
        a=$(cd "$bdir" && realpath "$w")
        for o in $(objects_of "$a"); do objs="$objs $o"; done ;;
      *.c.o) objs="$objs $(cd "$bdir" && realpath "$w")" ;;
      *) cmd="$cmd $w" ;;
    esac
  done
  # archive objects go first, so the archives after them resolve against them as before
  cmd=$(echo "$cmd" | sed -E "s# -o [^ ]+# -o $out/$app $objs#")
  (cd "$bdir" && eval "$cmd -Wl,--gc-sections -Wl,--print-gc-sections") 2> "$out/$app.gc" || {
    cat "$out/$app.gc" >&2
    echo "check-reach: relinking $app failed" >&2
    exit 1
  }
  for o in $objs; do echo "$o"; done | sort -u > "$out/$app.objects"
  cat "$out/$app.objects" >> "$out/objects"
  sed -nE "s#.*removing unused section '\.text\.([^']+)' in file '([^']+)'.*#\2 \1#p" "$out/$app.gc" \
    | while read -r o f; do echo "$(cd "$bdir" && realpath "$o") $f"; done | sort -u > "$out/$app.removed"
  # shellcheck disable=SC2046
  sections_of $(cat "$out/$app.objects") | sort -u > "$out/$app.linked"
  comm -23 "$out/$app.linked" "$out/$app.removed" >> "$out/kept"
done
[ "$found" -gt 0 ] || { echo "check-reach: no application was built in $dir" >&2; exit 1; }

# Every project object in the scan build: archive targets (linked or not) and the application mains.
find "$dir" -name '*.a' -not -path '*third_party*' | while read -r a; do objects_of "$a"; done > "$out/all.objects"
sort -u "$out/objects" "$out/all.objects" -o "$out/all.objects"
# shellcheck disable=SC2046
sections_of $(cat "$out/all.objects") | sort -u > "$out/all"
sort -u "$out/kept" -o "$out/kept"
# The VALGRIND_* inline functions of valgrind.h exist only where valgrind is installed (the CI runner has none), and are not
# Ursatz code: they are never listed, so the result does not depend on the machine.
comm -23 "$out/all" "$out/kept" | source_of | grep -v ' VALGRIND_' | sort -u > "$out/unreached.txt"

total=$(wc -l < "$out/unreached.txt")
echo "check-reach: $total functions no application reaches"
for part in library analyzer composer gui; do
  echo "  $part: $(grep -c "^$part/" "$out/unreached.txt" || true)"
done

[ -f "$allow" ] || exit 0
sed -E 's/#.*//; s/[[:space:]]+$//' "$allow" | awk 'NF == 2' | sort -u > "$out/allowed"
comm -23 "$out/unreached.txt" "$out/allowed" > "$out/new"
comm -13 "$out/unreached.txt" "$out/allowed" > "$out/stale"
status=0
if [ -s "$out/new" ]; then
  echo "check-reach: not reached by any application (delete it, or list it in tools/ci/reach-allow.txt with a reason):"
  sed 's/^/  /' "$out/new"
  status=1
fi
if [ -s "$out/stale" ]; then
  echo "check-reach: listed in tools/ci/reach-allow.txt but reached or gone (remove the line):"
  sed 's/^/  /' "$out/stale"
  status=1
fi
exit $status
