#!/bin/bash
DB="eval.db"
INDEX="eval_face.index"
MODELS="CoserRetrieval/models"
QUERY_DIR="CoserRetrieval/testdata_eval/query"

run_queries() {
  local pattern="$1"
  local expected="$2"
  for f in "$QUERY_DIR"/$pattern; do
    base=$(basename "$f")
    out=$(./bin/coser_cli.exe query --db "$DB" --index "$INDEX" --models-dir "$MODELS" --image "$f" --topk 1 2>&1)
    line=$(echo "$out" | grep "person_id=")
    if echo "$out" | grep -q "No matches"; then
      echo "$base NOFACE"
    elif [ -z "$line" ]; then
      echo "$base READFAIL"
    else
      name=$(echo "$line" | grep -o "display_name=[^ ]*" | cut -d= -f2)
      score=$(echo "$line" | grep -o "score=[^ ]*" | cut -d= -f2)
      if [ "$name" == "$expected" ]; then
        echo "$base CORRECT $score"
      else
        echo "$base WRONG->$name $score"
      fi
    fi
  done
}

run_queries "rioko_query_*" "rioko"
run_queries "chichi_query_*" "chichi"
