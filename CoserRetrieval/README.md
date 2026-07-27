# CoserRetrieval

纯 C++ 图像检索系统（替代 `test/` 下已废弃的 Python 微服务 + DLL 架构）。

## 里程碑 1：人脸识别路径

SCRFD 人脸检测 → ArcFace 512维特征提取 → FAISS 向量检索 → SQLite 元数据存储。

已通过端到端验证：
- 同一张照片自匹配 top-1 score ≈ 1.0
- 同一人不同照片 top-1 命中，且分数高于跨人比对
- 无人脸图片 ingest 优雅失败（`ingest_status='failed'`），不崩溃

### 已知问题

**Release 配置的 `faiss.dll` 存在崩溃 bug（access violation, 0xC0000005）**

- 现象：`ThirdParty/install/faiss/installed/x64-windows/bin/faiss.dll`（Release 版，8.78MB）在调用
  `faiss::write_index()` 等核心操作时会在固定偏移处崩溃。
- 范围：与本次里程碑新增代码无关。用旧的 `test/` 原型代码重新以 Release 配置编译验证，
  崩溃现象完全一致（同一 faulting module `faiss.dll`，同一 fault offset
  `0x00000000001a3583`）——证明是 vcpkg 编译产物本身的问题，而非应用层代码。
- Debug 配置的 `faiss.dll`（16.6MB）工作正常，本里程碑的端到端验证即使用 Debug 配置完成。
- **结论**：Release 配置目前不可用。后续如需 Release 构建，需要单独处理
  （例如换用不同参数重新编译 faiss，或更换 faiss 来源）。

## 里程碑 2：CLIP 真人照片 vs 插画/CG 过滤（ingest 前置门禁）

评测数据集中混入了角色原画/插画，ArcFace 是在真人照片上训练的，对插画提取出的向量语义分布与真人照片不同，
会拉低相似度、污染索引质量。新增 `PhotoAuthenticityChecker`（`src/L3/PhotoAuthenticityChecker.h/.cpp`），
在 `FaceRecognitionPipeline::Ingest` 中人脸对齐之后、ArcFace 特征提取之前跑一次 CLIP zero-shot 判断，
拒绝非真人照片的图片入库（`query_cli` 查询路径不受影响，仍然接受任意输入）。

### 实现方式

只在 C++ 侧跑 CLIP **视觉编码器**（`models/clip/clip_vision_quantized.onnx`，int8 量化，~87MB，来自
`Xenova/clip-vit-base-patch32`），不移植 BPE tokenizer / 文本编码器。两句提示语
（"a real photograph of a person" / "an illustration, drawing, or CG artwork"）的文本 embedding
是用 `tools/gen_clip_text_embeddings.py`（`openai/clip-vit-base-patch32`，与 ONNX 导出版同权重）离线算好、
L2 归一化后硬编码进 `src/L3/clip_text_embeddings.inc` 的两个 `std::array<float, 512>` 常量。运行时只需
图像 embedding 分别与这两个常量做点积，取相似度更高的一侧。

`Ingest` 的返回类型从 `bool` 改为 `IngestResult{bool ok; std::string reason;}`，非真人照片会产生
`"rejected: not a real photo (clip_score=X)"` 的 `fail_reason`，与"未检测到人脸"（`"no face detected"`）区分开。

### 已知限制

- 文本侧 embedding 是硬编码的两句英文提示语算出来的常量，换提示语/换语言需要重新跑一次
  `tools/gen_clip_text_embeddings.py` 再替换 `.inc` 常量，不是运行时可配置的。
- 量化版视觉编码器（int8）精度略低于全精度版，如果后续发现误判率不可接受，可以换成
  `vision_model.onnx`（335MB）或 `vision_model_fp16.onnx`（176MB），只需替换模型文件路径，代码不用改。
- 这是二分类的启发式过滤，不是100%准确的判定，存在两类已在评测数据集中实际观察到的边界样本：
  - **漏判（插画被判定为真人）**：高度写实、厚涂风格的手绘/CG 插画，视觉上足够接近照片，容易被误判为真人照。
  - **误判（真人照被判定为插画）**：经过大幅磨皮/美颜滤镜处理、瞳孔美瞳效果强烈的真人 cosplay 摄影，
    或叠加了游戏特效/合成元素的真人摄影，画面质感被拉向插画一侧从而被误拒。
  出现上述误杀/漏杀是启发式方法的预期内局限，不是 bug，需要人工复核被拒绝的图片而不是全信过滤器结果。
- **实测边界样本占比偏高，人工复核后确认过滤器实际可用误判率极高**：新增 `apps/scan_authenticity_cli.cpp`
  （独立小工具，只跑 CLIP 判定不做完整 ingest，用于批量扫描目录）对 `testdata_eval` 全量扫描：
  `store`（83张）23张判定为插画/CG（~28%），`query`（81张）23张判定为插画/CG（~28%）。
  经人工逐张目测复核全部46张被标记的图片，结果：
  - 仅 **2张确认为真插画**：`rioko_query_038.jpg`（score=-0.027）、`rioko_query_044.jpg`（score=-0.012）——
    已从 `testdata_eval/query` 移出（备份于仓库外的 `removed_illustrations/`，未提交/未删除，供反悔找回）。
  - 其余 **44张确认为真人照片**（分类器误判），包括 `|score|>0.02` 的高置信度样本里也有 2 张属于误判
    （`rioko_query_032.jpg`、`rioko_store_022.jpg`——重度后期/高P，风格已接近游戏CG，人工判断"可以理解
    为误判但不算离谱"）；其余分数贴近0（-0.02~0，两个提示语相似度几乎打平）的约36张边界样本**全部**
    是真人照片被误判，无一例外。
  - 结论：CLIP zero-shot 二分类在真实 cosplay 摄影（尤其是重度后期）上的误判率远高于预期
    （46张里只有2张是真插画，误判率 ~96%），`score` 绝对值大小和"是否为真插画"几乎没有可靠的相关性
    （高置信度样本里同样有误判）。**该过滤器当前精度不足以支撑自动化清洗决策**，只能作为"标记可疑图片
    交给人工复核"的辅助工具，不应被用来自动拒绝/删除数据。ingest 门禁场景下这意味着实际使用中会有相当
    比例的正常真人照片被误拒，用户需要人工复核 `fail_reason` 里 `"rejected: not a real photo"` 的图片，
    不能假设被拒绝的都是插画。后续如果要降低误判率，需要考虑换更高精度的视觉编码器（非量化版）或调整
    提示语，而不是简单调阈值。

## 里程碑 3：路线B（服装/角色识别）+ 路线C（原图匹配）+ L5 编排层

按 HLD 完整三路架构补完剩余部分：路线B（YOLOv8n-pose 裁剪 + DINOv2 特征 + FAISS）、
路线C（pHash 粗筛 + ORB 精确确认）、以及统一入口 `RetrievalOrchestrator`（带结果融合 `ResultFusion`）。
`ingest_cli`/`query_cli` 默认三路联动，`query_cli` 新增 `--mode exact|face|clothing|all` 可选单路调试。

### 路线B：服装/角色识别

`src/L3/IPoseDetector.h`（新接口，不复用人脸的 `IDetector`——17点人体关键点和5点人脸关键点语义不同，
强行复用会两头打补丁）+ `YoloPoseDetector`（YOLOv8n-pose，anchor-free，Ultralytics 导出的 ONNX 已经把
box/关键点解码到像素空间，不需要像 SCRFD 那样按 stride 做网格解码）+ `DinoV2Extractor`（**ViT-S/14，384维**，
而非 HLD 原定的 ViT-B/14/768维——本机磁盘仅剩~19G，ViT-S/14 导出约88MB 对比 ViT-B/14 约340MB，
且 HLD §9.2 本身已把"换 ViT-S/14"列为 CPU 推理延迟超预算时的第一优化选项）+ `ClothingRecognitionPipeline`
（结构镜像 `FaceRecognitionPipeline`：整框裁剪，不做精细分割）。

模型来源：`tools/export_yolov8n_pose_onnx.py`（`ultralytics` 包一键导出）、
`tools/export_dinov2_onnx.py`（`transformers.Dinov2Model` + `torch.onnx.export`，`facebook/dinov2-small`
经 hf-mirror.com 镜像下载）。两个脚本均为一次性离线操作，不进 CMake 构建，产物模型文件随脚本一起提交。

**DINOv2 导出产物是两个文件**：`dinov2_vits14.onnx`（~1.5MB 图结构）+
`dinov2_vits14.onnx.data`（~88MB，PyTorch 导出时外置的权重）。ONNX Runtime 要求两者在同一目录下才能加载，
移动/提交模型时必须两个文件一起处理——`.gitattributes` 已加 `*.onnx.data` 的 LFS 规则。

### 路线C：原图匹配

`src/L3/PHasher`（DCT-based 64位感知哈希）+ `src/L3/OrbCropMatcher`（ORB+BFMatcher 内点数确认）+
`src/L2/PHashIndex`（内存暴力扫描，几万级规模 <1ms，不需要 BK-tree）+ `src/L4/ImageMatchPipeline`。

### L5 编排层

`src/L5/ResultFusion`：exact 命中且达阈值直接短路；否则按 `person_id` 聚合 face/clothing 加权分
（默认 `face_weight=0.7, clothing_weight=0.3`，`config.json` 的 `fusion` 段，**未经真实数据调优，
仅为初始猜测值**）。`src/L5/RetrievalOrchestrator`：`IngestImage` 顺序跑三路（不引入线程池——当前是
单图 CLI 工具而非常驻服务，并行价值有限，见下方已知简化）；`Query` 按 `QueryMode` 决定实际调用哪几路。

### 已知简化（相对 HLD 字面设计的取舍，非疏漏）

- **不引入线程池**：HLD 设想三路并行执行，本里程碑改为顺序执行。当前产物是单张图片的 CLI 工具，
  真正的并行价值有限；如果后续做成常驻服务，需要重新引入线程池把三路并行化。
- **不单独建 `IngestionCoordinator` 类**：直接内联进 `RetrievalOrchestrator::IngestImage`，
  因为它在 HLD 里只是"落 pending → 三路 → 落 committed/failed"的薄封装，没有独立职责。
- exact 路线的 `PipelineMatch.score` 是 ORB 原始内点数（例如自匹配 500），不是归一化到 0-1 的置信度，
  和 `fusion.exact_score_threshold`（默认0.85）字面上不是同一量级——但因为 `ImageMatchPipeline::Query`
  已经用 `orb_min_inliers` 自己预过滤了候选，实际效果上"exact 有结果就短路"仍然成立，只是这个阈值配置项
  目前形同虚设。后续如果要让该阈值真正生效，需要把 exact score 归一化。

### 已知问题：Git LFS 指针文件未 smudge 会导致 `abort()`

`ingest_cli.exe`/`query_cli.exe` 早期版本没有捕获 `Ort::Exception`，当模型文件是未拉取内容的
Git LFS 指针占位文本（而不是真实二进制）时，ONNX Runtime 的 protobuf 解析会抛未捕获异常，
触发 MSVC CRT 的 `abort()` 弹窗（"abort() has been called"）。修复了两处：
1. `main()` 现在包一层 `try/catch`，异常改为打印 `"Unhandled exception: ..."` 并 `return 2`，不再崩溃。
2. 根因是本机工作区里几个模型文件停留在 LFS 指针状态（`git lfs ls-files` 可确认对象已在本地缓存但工作树
   未 smudge），用 `git lfs checkout <path>` 手动拉取解决。如果克隆/切换分支后模型文件读不出来，
   先检查 `head -c 60 <model>.onnx` 是不是 `version https://git-lfs.github.com/spec/v1` 文本，
   是的话跑 `git lfs pull` 或 `git lfs checkout`。

### 端到端验证

用 `testdata_eval/store`（83张）全量 ingest（三路，Debug 配置）：全部 83 张无崩溃、无异常提交
（人脸检测失败的图片走 face 路线非致命失败，clothing/exact 路线仍然成功写入）。
用 `testdata_eval/query`（80张 + 1 张误放的 `.csv`）全量 query（`mode=all`），检查 `fused` 结果 top-1：
rioko 42/45 正确（3 张跨人误判，误判对象 person_id 分数明显偏低），chichi 32/34 正确
（1 张 `.csv` 非图片文件按预期读取失败计入 nomatch，不计入分母）。整体 fused top-1 准确率 ~92.5%，
不低于里程碑1单纯人脸路线的水平。

### 已知限制

- clothing 向量维度是 384（ViT-S/14）不是 HLD 原定的 768（ViT-B/14），见上方模型选型说明。
- `face_weight`/`clothing_weight`/`exact_score_threshold`/`phash_max_hamming`/`orb_min_inliers`
  均为初始猜测默认值，待真实数据调优。
- Ingest 阶段三路顺序执行，非并行，见上方"已知简化"。
