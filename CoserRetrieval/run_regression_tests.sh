#!/bin/bash
# CoserRetrieval 回归测试：改动代码后跑一次，检查三路（face/clothing/exact）+ CLIP 门禁 + 融合
# 是否还符合预期行为。不是精度评测（那个用 testdata_eval，见 README 里程碑2/3），这里只关心
# "这几个已知场景的预期结果有没有被这次改动打破"。
#
# 用法：cd CoserRetrieval && ./run_regression_tests.sh
# 需要先 build 出 bin/coser_cli.exe（相对本文件所在目录的上一级 ../bin/）。

set -u
cd "$(dirname "$0")"

CLI="../bin/coser_cli.exe"
MODELS="models"
FIXTURES="testdata/regression"
DB="/tmp_regress.db"
INDEX="/tmp_regress_face.index"
CLOTHING_INDEX="/tmp_regress_clothing.index"

# 用相对当前目录（CoserRetrieval/）的路径，避免 Windows/Git-Bash /tmp 写入异常（历史上遇到过 /tmp 下
# cv2.imwrite 静默失败的问题，这里干脆不用 /tmp，直接放在 CoserRetrieval/ 下，跑完再删）。
DB="_regress.db"
INDEX="_regress_face.index"
CLOTHING_INDEX="_regress_clothing.index"
QUERY_REPORT="_regress_query_report.csv"
ADA_INDEX="_regress_adaface.index"
MASK_PNG="_regress_mask.png"

pass=0
fail=0

cleanup() {
  rm -f "$DB" "$INDEX" "$CLOTHING_INDEX" "$QUERY_REPORT" "$ADA_INDEX" "$MASK_PNG"
}
trap cleanup EXIT

fresh_db() {
  cleanup
}

check() {
  local desc="$1"
  local condition="$2"  # "0" = pass, non-"0" = fail
  if [ "$condition" == "0" ]; then
    echo "  [PASS] $desc"
    pass=$((pass+1))
  else
    echo "  [FAIL] $desc"
    fail=$((fail+1))
  fi
}

echo "=== CoserRetrieval 回归测试 ==="
if [ ! -f "$CLI" ]; then
  echo "找不到 $CLI，先 build: cd build && cmake --build . --config Debug"
  exit 1
fi

# ── Case 1: 正常真人照 ingest 应该三路全部成功（无 partial failure 提示） ──
echo "--- Case 1: 正常真人照 ingest（rioko_ref.jpg）应三路全部成功 ---"
fresh_db
out=$($CLI ingest --db "$DB" --index "$INDEX" --clothing-index "$CLOTHING_INDEX" \
      --models-dir "$MODELS" --person "rioko" --role "role_rioko" --image "$FIXTURES/rioko_ref.jpg" 2>&1)
echo "$out" | grep -q "^Ingested"; check "ingest 成功写入" $?
echo "$out" | grep -q "partial route failures"
if [ $? -eq 0 ]; then check "三路无 partial failure（意外出现 partial failure，见上方输出）" 1; else check "三路无 partial failure" 0; fi

# ── Case 2: 无人脸图片 ingest 应该 face+clothing 走非致命失败，但整体仍 committed ──
echo "--- Case 2: 无人脸图片（blank_noface.jpg）应 face/clothing 非致命失败但整体成功 ---"
out=$($CLI ingest --db "$DB" --index "$INDEX" --clothing-index "$CLOTHING_INDEX" \
      --models-dir "$MODELS" --person "nobody" --image "$FIXTURES/blank_noface.jpg" 2>&1)
echo "$out" | grep -q "^Ingested"; check "无人脸图片仍成功 commit（exact 路线兜底）" $?
echo "$out" | grep -q "no face detected"; check "face 路线报告 no face detected" $?

# ── Case 3: 插画应被 CLIP 门禁拒绝（face 路线），但 clothing/exact 路线不受影响 ──
echo "--- Case 3: 插画（illustration_reject.jpg）face 路线应被 CLIP 门禁拒绝 ---"
out=$($CLI ingest --db "$DB" --index "$INDEX" --clothing-index "$CLOTHING_INDEX" \
      --models-dir "$MODELS" --person "rioko" --image "$FIXTURES/illustration_reject.jpg" 2>&1)
echo "$out" | grep -q "rejected: not a real photo"; check "face 路线因 CLIP 判定非真人照而拒绝" $?
echo "$out" | grep -q "^Ingested"; check "clothing/exact 路线不受影响，整体仍 commit" $?

# ── Case 4: 建库后同人不同照片查询，face 路线应正确命中 ──
echo "--- Case 4: rioko_query.jpg 查询应命中 rioko（face 路线） ---"
out=$($CLI query --db "$DB" --index "$INDEX" --clothing-index "$CLOTHING_INDEX" \
      --models-dir "$MODELS" --image "$FIXTURES/rioko_query.jpg" --topk 1 --mode face 2>&1)
echo "$out" | grep -q "display_name=rioko"; check "face 路线 top-1 命中 rioko" $?

# ── Case 4b: 角色查询不调用 face/exact，仅以服装/发型外观命中角色标签 ──
echo "--- Case 4b: rioko_query.jpg 角色查询应只命中 role_rioko（服装/发型） ---"
out=$($CLI query --db "$DB" --index "$INDEX" --clothing-index "$CLOTHING_INDEX" \
      --models-dir "$MODELS" --image "$FIXTURES/rioko_query.jpg" --topk 1 --mode role 2>&1)
echo "$out" | grep -q "role_name=role_rioko"; check "role 路线 top-1 命中 role_rioko" $?
echo "$out" | grep -q -- "-- face\|-- exact"
if [ $? -eq 0 ]; then check "role 查询不输出 face/exact 结果" 1; else check "role 查询不输出 face/exact 结果" 0; fi

# ── Case 5: chichi ingest 后查询应命中 chichi，且不会和 rioko 混淆 ──
echo "--- Case 5: chichi_ref.jpg ingest + chichi_query.jpg 查询应命中 chichi（不与 rioko 混淆） ---"
$CLI ingest --db "$DB" --index "$INDEX" --clothing-index "$CLOTHING_INDEX" \
    --models-dir "$MODELS" --person "chichi" --image "$FIXTURES/chichi_ref.jpg" > /dev/null 2>&1
out=$($CLI query --db "$DB" --index "$INDEX" --clothing-index "$CLOTHING_INDEX" \
      --models-dir "$MODELS" --image "$FIXTURES/chichi_query.jpg" --topk 1 --mode face 2>&1)
echo "$out" | grep -q "display_name=chichi"; check "face 路线 top-1 命中 chichi，未与 rioko 混淆" $?

# ── Case 6: 原图自匹配（exact 路线），score 应该在高位（同一文件） ──
echo "--- Case 6: rioko_ref.jpg 自匹配（exact 路线）score 应处于高位 ---"
out=$($CLI query --db "$DB" --index "$INDEX" --clothing-index "$CLOTHING_INDEX" \
      --models-dir "$MODELS" --image "$FIXTURES/rioko_ref.jpg" --topk 1 --mode exact 2>&1)
echo "$out" | grep -q "display_name=rioko"; check "exact 路线自匹配命中 rioko" $?

# ── Case 7: 轻裁剪转发图仍应被 exact 路线识别为同一原图（水印/裁剪容忍度） ──
echo "--- Case 7: 轻裁剪转发图（rioko_ref_repost.jpg，~5%裁剪+重新编码）exact 路线应仍能识别为同一原图 ---"
out=$($CLI query --db "$DB" --index "$INDEX" --clothing-index "$CLOTHING_INDEX" \
      --models-dir "$MODELS" --image "$FIXTURES/rioko_ref_repost.jpg" --topk 1 --mode exact 2>&1)
echo "$out" | grep -q "display_name=rioko"; check "exact 路线对轻裁剪/重编码转发图仍能识别（pHash+ORB 容忍度）" $?

# ── Case 8: 不存在的人/图片应该查不到任何结果，不应崩溃 ──
echo "--- Case 8: 全新未入库的人物图片查询应无匹配，且不崩溃 ---"
out=$($CLI query --db "$DB" --index "$INDEX" --clothing-index "$CLOTHING_INDEX" \
      --models-dir "$MODELS" --image "$FIXTURES/blank_noface.jpg" --topk 1 --mode all 2>&1)
rc=$?
check "无人脸图片查询不崩溃（exit code=$rc）" $([ $rc -eq 0 ] && echo 0 || echo 1)
echo "$out" | grep -q "No matches"; check "无人脸图片查询报告 No matches" $?

# ── Case 9: 角色目录批量入库应复用模型/索引，并能用角色模式查询 ──
echo "--- Case 9: regression 目录角色批量入库 + role 查询 ---"
fresh_db
out=$($CLI ingest --db "$DB" --clothing-index "$CLOTHING_INDEX" \
      --models-dir "$MODELS" --role "role_batch" --dir "$FIXTURES" 2>&1)
echo "$out" | grep -q "Batch complete: total=7 succeeded=7 skipped=0 failed=0"; check "角色目录批量入库全部成功" $?
out=$($CLI query --db "$DB" --clothing-index "$CLOTHING_INDEX" \
      --models-dir "$MODELS" --image "$FIXTURES/rioko_ref.jpg" --topk 1 --mode role 2>&1)
echo "$out" | grep -q "role_name=role_batch"; check "角色批量库可通过 role 模式查询" $?

# ── Case 10: 完全相同的文件按 MD5 跳过，不重复写入索引 ──
echo "--- Case 10: 重复文件应按 MD5 跳过 ---"
out=$($CLI ingest --db "$DB" --clothing-index "$CLOTHING_INDEX" \
      --models-dir "$MODELS" --role "role_batch" --image "$FIXTURES/rioko_ref.jpg" 2>&1)
echo "$out" | grep -q "Duplicate skipped: image_id="; check "同内容文件按 MD5 跳过" $?

# ── Case 11: 目录批量查询应复用模型，并写出每张图片的 top-1 报告 ──
echo "--- Case 11: regression 目录批量 role 查询 + CSV 报告 ---"
out=$($CLI query --db "$DB" --clothing-index "$CLOTHING_INDEX" \
      --models-dir "$MODELS" --dir "$FIXTURES" --topk 1 --mode role --report "$QUERY_REPORT" 2>&1)
echo "$out" | grep -q "Batch complete: total=7 processed=7"; check "目录批量查询完成" $?
test -s "$QUERY_REPORT"; check "批量查询写出 CSV 报告" $?

# ── Case 12: AdaFace 备用模型至少应完成最小入库/查询闭环 ──
echo "--- Case 12: AdaFace 备用模型 smoke test ---"
fresh_db
rm -f "$ADA_INDEX"
$CLI ingest --db "$DB" --index "$ADA_INDEX" --clothing-index "$CLOTHING_INDEX" \
    --models-dir "$MODELS" --face-model adaface --person "rioko" --image "$FIXTURES/rioko_ref.jpg" > /dev/null 2>&1
out=$($CLI query --db "$DB" --index "$ADA_INDEX" --clothing-index "$CLOTHING_INDEX" \
      --models-dir "$MODELS" --face-model adaface --image "$FIXTURES/rioko_query.jpg" --topk 1 --mode face 2>&1)
echo "$out" | grep -q "display_name=rioko"; check "AdaFace 备用模型可完成 face 查询" $?
rm -f "$ADA_INDEX"

# ── Case 13: 服装/发型遮罩可视化应生成 PNG ──
echo "--- Case 13: clothing/face mask visualization ---"
$CLI visualize --models-dir "$MODELS" --image "$FIXTURES/rioko_ref.jpg" --output "$MASK_PNG" > /dev/null 2>&1
test -s "$MASK_PNG"; check "visualize 生成服装/脸部遮罩 PNG" $?

echo ""
echo "=== 结果：$pass passed, $fail failed ==="
cleanup
[ $fail -eq 0 ]
