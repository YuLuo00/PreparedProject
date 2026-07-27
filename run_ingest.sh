#!/bin/bash
DB="eval.db"
INDEX="eval_face.index"
MODELS="CoserRetrieval/models"
STORE_DIR="CoserRetrieval/testdata_eval/store"

ok=0
fail=0
for f in "$STORE_DIR"/rioko_store_*; do
  base=$(basename "$f")
  ./bin/ingest_cli.exe --db "$DB" --index "$INDEX" --models-dir "$MODELS" --person "rioko" --image "$f" > /tmp/ingest_log.txt 2>&1
  if grep -q "Ingested" /tmp/ingest_log.txt; then
    ok=$((ok+1))
  else
    fail=$((fail+1))
    echo "FAILED: $base"
  fi
done
echo "rioko: ok=$ok fail=$fail"

ok=0
fail=0
for f in "$STORE_DIR"/chichi_store_*; do
  base=$(basename "$f")
  ./bin/ingest_cli.exe --db "$DB" --index "$INDEX" --models-dir "$MODELS" --person "chichi" --image "$f" > /tmp/ingest_log.txt 2>&1
  if grep -q "Ingested" /tmp/ingest_log.txt; then
    ok=$((ok+1))
  else
    fail=$((fail+1))
    echo "FAILED: $base"
  fi
done
echo "chichi: ok=$ok fail=$fail"
