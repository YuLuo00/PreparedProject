#!/bin/bash
DB="eval.db"
INDEX="eval_face.index"
MODELS="CoserRetrieval/models"
QUERY_DIR="CoserRetrieval/testdata_eval/query"

run_queries() {
  local pattern="$1"
  local expected="$2"
  local correct=0
  local wrong=0
  local noface=0
  for f in "$QUERY_DIR"/$pattern; do
    base=$(basename "$f")
    out=$(./bin/coser_cli.exe query --db "$DB" --index "$INDEX" --models-dir "$MODELS" --image "$f" --topk 1 2>&1)
    if echo "$out" | grep -q "No matches"; then
      noface=$((noface+1))
      echo "NOFACE: $base"
    elif echo "$out" | grep -q "display_name=$expected"; then
      correct=$((correct+1))
    else
      wrong=$((wrong+1))
      echo "WRONG: $base -> $out"
    fi
  done
  echo "$expected: correct=$correct wrong=$wrong noface=$noface"
}

run_queries "rioko_query_*" "rioko"
run_queries "chichi_query_*" "chichi"
