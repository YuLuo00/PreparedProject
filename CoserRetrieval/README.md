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
