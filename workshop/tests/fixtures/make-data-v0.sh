#!/usr/bin/env bash
# Makes the version 0 data folder fixture (data-v0/): what the GUI's backend stored before the workshop's data folder layout
# (P63 B7, R-642). It starts the built ursatz-gui-backend on an empty folder, drives it with curl (compose with fixed seeds), stops it,
# then edits in the three old member forms the GUI migrated on read (a motif `key`, a form `mode`, a `progression` fill) and a
# meta.json without `ursatz_commit`. The ids and the timestamps differ from run to run: the fixture is the committed output, and the
# tests copy it.
#
# usage: make-data-v0.sh <ursatz-gui-backend> <repository root> <out folder>
set -euo pipefail

backend=$1
root=$2
out=$3
port=${URSATZ_FIXTURE_PORT:-18791}
base="http://127.0.0.1:$port"

rm -rf "$out"
mkdir -p "$out"
"$backend" --data "$out" --port "$port" > "${TMPDIR:-/tmp}/make-data-v0.log" 2>&1 &
server=$!
trap 'kill $server 2>/dev/null || true; wait $server 2>/dev/null || true' EXIT
for _ in $(seq 1 100); do
  curl -sf "$base/api/composer/status" > /dev/null 2>&1 && break
  sleep 0.1
done

# api METHOD PATH [BODY]: prints the answer, fails on a status of 400 or more.
api() {
  local method=$1 path=$2 body=${3-}
  if [ -n "$body" ]; then
    curl -sf -X "$method" -H 'Content-Type: application/json' --data "$body" "$base$path"
  else
    curl -sf -X "$method" "$base$path"
  fi
}

# job METHOD PATH BODY: starts a job route and waits for the job; prints the job's result.
job() {
  local accepted id state answer
  accepted=$(api "$1" "$2" "$3")
  id=$(jq -r '.job.id' <<< "$accepted")
  for _ in $(seq 1 6000); do
    answer=$(api GET "/api/jobs/$id")
    state=$(jq -r '.job.state' <<< "$answer")
    if [ "$state" != queued ] && [ "$state" != running ]; then
      [ "$state" = done ] || { echo "job $id: $answer" >&2; exit 1; }
      jq -c '.job.result' <<< "$answer"
      return
    fi
    sleep 0.1
  done
  echo "job $id did not finish" >&2
  exit 1
}

upload() { # upload FILE NAME
  curl -sf -X POST -H "X-Filename: $2" --data-binary "@$1" "$base/api/analyzer/pieces" | jq -r '.id'
}

project_a=$(api GET /api/materials/projects | jq -r '.projects[0].id')
project_b=$(api POST /api/materials/projects '{"name":"Second project","description":"The fixture second project"}' | jq -r '.project.id')

item() { # item TYPE NAME PROJECT BODY
  api POST /api/materials/items "$(jq -cn --arg t "$1" --arg n "$2" --arg p "$3" --argjson b "$4" '{type:$t,name:$n,project_id:$p,body:$b}')" | jq -r '.item.id'
}
motif_body() { # motif_body ROLE FORM NOTES
  jq -cn --arg r "$1" --arg f "$2" --arg n "$3" \
    '{uses:[],motif:{role:$r,form:$f,notes:$n,meter:"4/4",material:"omt.material.natural-minor",pickup:"0",features:[],analysis:null,derived:null}}'
}

# Motifs: A saved twice, B, C, an accompaniment, a variant of A, a flagged variant.
motif_a=$(item motif "A" "$project_a" "$(motif_body melodic full 'C4:e Eb4:e G4:e Eb4:e C4:e Eb4:e G4:e C5:e')")
api POST "/api/materials/items/$motif_a/versions" "$(jq -cn --argjson b "$(motif_body melodic full 'C4:e Eb4:e G4:e Eb4:e D4:e F4:e G4:e C5:e')" '{body:$b,note:"Changed the fifth note"}')" > /dev/null
motif_b=$(item motif "B" "$project_a" "$(motif_body melodic full 'G4:e F4:e Eb4:e D4:e F4:e Eb4:e D4:e C4:e')")
motif_c=$(item motif "C" "$project_b" "$(motif_body melodic full 'C4:q Eb4:e G4:e C5:h')")
motif_acc=$(item motif "Acc" "$project_a" "$(motif_body accompaniment full 'C3:q G3:q Eb3:q G3:q')")
variants=$(job POST "/api/materials/items/$motif_a/variants" '{"count":2,"seed":3}')
variant_a=$(api POST "/api/materials/items/$motif_a/variants/save" "$(jq -cn --argjson v "$(jq -c '.variants[0]' <<< "$variants")" '{variant:$v,name:"A variant"}')" | jq -r '.item.id')
variant_b=$(api POST "/api/materials/items/$motif_a/variants/save" "$(jq -cn --argjson v "$(jq -c '.variants[1]' <<< "$variants")" '{variant:$v,name:"A flagged variant",flag:"approved"}')" | jq -r '.item.id')
api POST "/api/materials/items/$variant_b/flag" '{"flag":"rejected"}' > /dev/null
api PUT "/api/materials/items/$motif_a/op-checks" '{"checks":{"transpose":{"verdict":"works","comment":"Sounds right","seed":3,"notes_text":"C4 Eb4 G4","date":"2026-11-01"}}}' > /dev/null

# Library: a MIDI piece and a MusicXML piece (one with composer, source and tags), a collection, a style built from it.
piece_midi=$(upload "$root/tests/fixtures/framework-v1-corpus/Bach 114.mid" "Bach 114.mid")
piece_xml=$(upload "$root/tests/fixtures/links/a.musicxml" "a.musicxml")
api PATCH "/api/analyzer/pieces/$piece_xml" '{"composer":"Test Composer","source":"Link fixtures","tags":["motive","short"]}' > /dev/null
collection=$(api POST /api/corpus '{"name":"Fixture collection","description":"Both pieces"}' | jq -r '.id')
api POST "/api/corpus/$collection/pieces" "$(jq -cn --arg a "$piece_midi" --arg b "$piece_xml" '{piece_ids:[$a,$b]}')" > /dev/null
style=$(api POST /api/styles "$(jq -cn --arg c "$collection" '{name:"Fixture style",collection_id:$c}')" | jq -r '.style.id')

# Themes: a placeholder, a composed theme with a brief, chords and an accompaniment motif.
theme_placeholder=$(item theme "Placeholder theme" "$project_a" '{"uses":[],"theme":{"form":null,"slots":[]}}')
slots=$(jq -cn --arg a "$motif_a" --arg b "$motif_b" '[{slot:"basic",item_id:$a,version:1},{slot:"cadential",item_id:$b,version:1}]')
theme=$(item theme "Sentence theme" "$project_a" "$(jq -cn --argjson s "$slots" --arg acc "$motif_acc" \
  '{uses:[],theme:{form:{into:"sentence",hybrid:null,material:"omt.material.natural-minor",meter:"4/4",bars:8,cadence:"PAC",first_cadence:null},slots:$s,chords:[{bar:0,text:"I"},{bar:6,text:"V"},{bar:7,text:"I"}],accompaniment:{pattern:"motive",item_id:$acc,version:1},brief:{scopes:[{kind:"all"}],piece_ids:[],style_ids:[]},style_weight:"1/2",seed:5}}')")
job POST "/api/materials/items/$theme/compose" '{"seed":1}' > /dev/null

# Pieces: a game loop, a rounded binary with the composed theme as a fill, a variations set.
loop=$(item piece "Game loop" "$project_a" "$(jq -cn --arg a "$motif_a" \
  '{uses:[],piece:{structure:{key:"D",material:"omt.material.natural-minor",meter:"4/4",sections:[{id:"s0",part:"body",beats:32,fill:{kind:"motif",item_id:$a,version:1}}]}}}')")
job POST "/api/materials/items/$loop/compose" '{"seed":1}' > /dev/null
binary=$(item piece "Rounded binary" "$project_b" "$(jq -cn --arg a "$motif_a" --arg t "$theme" \
  '{uses:[],piece:{structure:{form:"rounded-binary",key:"C",material:"omt.material.major",meter:"4/4"},motive:{item_id:$a,version:1}}}')")
job POST "/api/materials/items/$binary/compose" '{"seed":1}' > /dev/null
set_body=$(item piece "Theme in a ternary" "$project_a" "$(jq -cn --arg t "$theme" \
  '{uses:[],piece:{structure:{form:"ternary",key:"D",material:"omt.material.natural-minor",meter:"4/4",sections:[{name:"A",region:"home"},{name:"B",region:{relation:"omt.relation.dominant",anchor:"A"},fill:{kind:"theme",item_id:$t,version:2}}]}}}')")
job POST "/api/materials/items/$set_body/compose" '{"seed":1}' > /dev/null
job POST "/api/materials/items/$loop/versions/2/variations" '{"section_id":"s0","lane":"melody","count":2,"seed":20}' > /dev/null

# Subjects: two subjects, one linking an item, a piece and the other subject.
sub_a=$(api POST /api/subjects '{"name":"The hero","kind":"character","note":"Recurs in the second theme"}' | jq -r '.subject.id')
sub_b=$(api POST /api/subjects '{"name":"The castle","kind":"place"}' | jq -r '.subject.id')
api POST "/api/subjects/$sub_a/items/$motif_a" '{}' > /dev/null
api POST "/api/subjects/$sub_a/pieces/$piece_xml" '{}' > /dev/null
api POST "/api/subjects/$sub_a/related/$sub_b" '{}' > /dev/null
api POST "/api/subjects/$sub_b/items/$theme" '{}' > /dev/null

# A finished link run over the two pieces.
run=$(api POST /api/links/runs "$(jq -cn --arg c "$collection" '{scope:{kind:"collection",collection_id:$c}}')" | jq -r '.run.id')
for _ in $(seq 1 600); do
  [ "$(api GET "/api/links/runs/$run" | jq -r '.run.status')" = running ] || break
  sleep 0.1
done

kill "$server"
wait "$server" 2>/dev/null || true
trap - EXIT

# The old member forms: a motif version with `key` "C minor" and no `material`, a form with `mode`, a piece structure with a
# `progression` fill, a meta.json without `ursatz_commit`.
motif_old=$(ls -d "$out"/materials/items/"$motif_b" | head -1)
jq '.motif |= (del(.material) | .key = "C minor")' "$motif_old/v1.json" > "$motif_old/v1.json.new" && mv "$motif_old/v1.json.new" "$motif_old/v1.json"
theme_dir="$out/materials/items/$theme"
jq '.theme.form |= (del(.material) | .mode = "minor")' "$theme_dir/v1.json" > "$theme_dir/v1.json.new" && mv "$theme_dir/v1.json.new" "$theme_dir/v1.json"
loop_dir="$out/materials/items/$loop"
jq '.piece.structure.sections[0].fill = {kind:"progression", text:"i iv V i"}' "$loop_dir/v1.json" > "$loop_dir/v1.json.new" && mv "$loop_dir/v1.json.new" "$loop_dir/v1.json"
jq 'del(.ursatz_commit)' "$out/pieces/$piece_xml/meta.json" > "$out/pieces/$piece_xml/meta.json.new" && mv "$out/pieces/$piece_xml/meta.json.new" "$out/pieces/$piece_xml/meta.json"
rm -f "$out/.ursatz.lock"
echo "data-v0 written to $out"
