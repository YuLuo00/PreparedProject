# CoserRetrieval

纯 C++ 图像检索系统。

> **项目状态（2026-08）**：当前只维护和使用本目录的 `CoserRetrieval`（通过
> `bin/coser_cli.exe` 运行）。根目录 `test/`、`bin/ImageSearch.dll` 及其 Python 微服务
> 属于旧架构，已停止开发和验证；仅为保留历史代码、模型与接口参考而留在仓库中，不能作为当前
> 图库功能的交付入口。

> **维护约定**：任何功能、CLI 参数、数据表或检索规则的规格变更，必须在同一改动中同步更新本 README、
> `plan.md` 及相应回归测试说明。

## 快速使用

以下命令均在**项目根目录**执行，使用 Windows PowerShell。首次使用前确认
`CoserRetrieval/models/` 下的模型文件完整；尤其 DINOv2 必须同时存在
`dinov2_vits14.onnx` 和 `dinov2_vits14.onnx.data`。

### 1. 构建

```powershell
cmake --build .\CoserRetrieval\build --config Debug --target coser_cli
```

生成的程序为 `bin/coser_cli.exe`。当前 Release 配置存在已知的 faiss 崩溃问题，日常使用和验证请先使用 Debug。

### 人脸模型选择

默认人脸模型是 InsightFace ArcFace（`w600k_r50.onnx`）。可通过 `--face-model adaface` 试用
AdaFace IR-18 WebFace4M（`models/face/adaface_ir18_webface4m.onnx`）；该模型使用 **BGR** 输入，
代码已使用独立预处理器，不能复用 ArcFace 的 RGB 预处理。

不同人脸模型的 embedding 空间不兼容，AdaFace 必须使用独立的数据库和人脸索引，不能与 ArcFace
的 `db`/`index` 混用：

```powershell
.\bin\coser_cli.exe ingest `
  --db .\data\coser_adaface.db `
  --index .\data\coser_adaface_face.index `
  --clothing-index .\data\coser_adaface_clothing.index `
  --models-dir .\CoserRetrieval\models `
  --face-model adaface `
  --person "rioko" `
  --image .\photos\rioko_ref.jpg
```

在当前 `testdata_eval` 人物-only 评测（79 张有效 query）中：ArcFace 为 `72/79 = 91.14%`，
AdaFace IR-18 为 `67/79 = 84.81%`。因此 ArcFace 保持默认，AdaFace 仅作为备用 A/B 方案；后续
更换 AdaFace 的更大 backbone 或重新校准参考图后，应再次评测再决定是否启用。

### 2. 入库图片

每张图片至少指定人物名称或角色名。以下示例将人脸、服装/角色和原图匹配三条路线同时写入同一套 SQLite/索引文件：

```powershell
.\bin\coser_cli.exe ingest `
  --db .\data\coser.db `
  --index .\data\coser_face.index `
  --clothing-index .\data\coser_clothing.index `
  --models-dir .\CoserRetrieval\models `
  --face-model arcface `
  --person "rioko" `
  --role "角色A" `
  --image .\photos\rioko_ref.jpg
```

首次使用时请先创建保存数据库和索引的目录：`New-Item -ItemType Directory -Force .\data`。
同一人可多次执行 `ingest`，分别加入不同角色、妆容或拍摄条件的参考图。只建设角色库时，
可省略 `--person`，仅提供 `--role`。
`--role` 为可选参数；需要按 Cos 角色检索时，同一角色的每张参考图都应使用完全相同的角色名。

> 升级到角色检索版本后，请使用新数据库/索引重新执行 `ingest`。旧 clothing 索引保存的是未遮蔽
> 脸部的向量，不能与新的服装/发型特征混用。

### 3. 批量入库目录

将 `--image` 换为 `--dir` 可递归导入目录下的 `jpg`、`jpeg`、`png`、`bmp`、`webp` 图片。
模型和索引在整批任务中只加载一次，索引在全部处理完成后保存一次，适合大批量入库。
批量入库使用 oneTBB `parallel_pipeline`，最多同时保留 7 个图片任务：MD5 与解码并行，重复检查与基础
元数据建档串行，三条模型路线并行，结果汇总串行；任一任务完成后立即补入下一张。

### 进度回调

供后续 UI 或常驻服务使用，核心库提供进程内 `TaskProgressHub`。外部模块通过
`TaskProgressHub::Instance().Register(callback)` 注册，保留返回的订阅 ID，并在销毁时调用
`Unregister(subscriptionId)`。每个 `TaskProgressEvent` 包含任务 ID、操作 (`ingest`/`query`)、
`Started`/`Running`/`Completed`/`Failed` 状态、总数、完成数、成功/跳过/失败数、当前路径和消息。

`ingest` 与 `query` 通过 `--task-id <id>` 设置任务 ID，默认分别为 `ingest`、`query`；后台同时启动多个
任务时必须传入不同 ID。回调在发出事件的工作线程同步调用，UI 回调必须尽快将事件投递到自身 UI 线程，
不能在回调内阻塞或执行耗时工作。CLI 进程本身没有 UI 订阅者，但同一进程内嵌核心库的服务/桌面模块可直接订阅。

```powershell
.\bin\coser_cli.exe ingest `
  --db .\data\coser.db `
  --clothing-index .\data\coser_clothing.index `
  --models-dir .\CoserRetrieval\models `
  --role "角色A" `
  --dir .\photos\role_a
```

同一批图片使用同一个 `--person` 和 `--role` 标签。角色-only 批次可省略 `--person` 和 `--index`；
此时不会加载或写入人脸模型/索引。命令结束时会输出 `total`、`succeeded`、`failed` 汇总。

当平铺目录中包含多个模特或角色时，可用 `--file-prefix` 仅导入指定文件名前缀，再分别传入对应标签：

```powershell
.\bin\coser_cli.exe ingest --db .\data\coser.db --index .\data\face.index `
  --clothing-index .\data\coser_clothing.index --models-dir .\CoserRetrieval\models `
  --person "rioko" --file-prefix "rioko_" --dir .\photos\mixed_store
```

批量任务会继续处理单张不可读或路线部分失败的图片；只有基础原图记录无法写入时才计为失败。

### MD5 重复保护

每个待入库文件都会计算内容 MD5，并保存到 SQLite 的 `images.md5`。再次导入字节内容
完全相同的文件时，系统会在加载图片和运行模型前输出 `Duplicate skipped`，引用已有 `image_id`，
并在批量汇总中计入 `skipped`，不会重复写入向量或索引。

MD5 只用于完全相同文件的去重；经过裁剪、重编码或加水印后的文件 MD5 会变化，仍由 `query --mode exact`
中的 pHash + ORB 判断是否属于同一原图。已有的旧数据库没有 MD5 值，需重新入库后才能受到该规则保护。

### 4. 查询图片

```powershell
.\bin\coser_cli.exe query `
  --db .\data\coser.db `
  --index .\data\coser_face.index `
  --clothing-index .\data\coser_clothing.index `
  --models-dir .\CoserRetrieval\models `
  --image .\photos\unknown.jpg `
  --topk 5 `
  --mode all
```

`--mode` 可选值：

- `all`：三路查询并融合结果，日常默认使用。
- `face`：仅按人脸身份查询。
- `clothing`：仅按服装/角色外观查询。
- `exact`：仅匹配相同原图或轻微裁剪、重编码后的转发图。
- `role`：仅按已标注角色的服装与发型外观查询，不运行人脸或原图匹配。LIP 人体解析会逐像素
  保留头发、帽子、服装、手套、围巾、袜子和鞋，排除脸、皮肤、四肢与背景；未在入库时传入
  `--role` 的图片不会出现在此模式的结果中。

角色/服装路线会先检查 LIP 的**可穿戴服饰**像素，头发不计入。少于 `max(512 像素, 人体框面积的 1%)`
时，图片判定为 `no clothing detected`：仍可写入原图匹配和人脸特征，但不会写入 clothing 向量，也不会
参与 `clothing` 或 `role` 查询。这能排除脚部、皮肤局部、裸体或无服装主体图片，避免黑色掩码产生
虚假的 `1.0` 相似度。

服装/发型向量使用 DINOv2 `last_hidden_state` 的 `16x16` patch token 网格。LIP 的外观 mask 按面积
下采样到该网格，只有被头发或服饰覆盖的 patch 会进入加权平均；mask 外的 patch 不参与最终 embedding。
人脸向量则只使用检测、对齐后的脸部裁剪。注意 DINOv2 的自注意力仍会使保留 patch 感知全图上下文；若
要连上下文也完全隔离，必须替换为支持 attention mask 的 DINO ONNX 导出。

查询结果会输出候选人物、路线、分数和对应的入库参考图片路径；没有候选时输出 `No matches`。

### 5. 批量查询与 CSV 报告

将 `--image` 换为 `--dir` 可递归查询目录中的所有支持图片。模型和索引在整批中只加载一次；
`--report` 输出每张图片的 top-1 结果，字段为 `file_path`、`mode`、`top1_label`、`top1_image_id`、
`top1_reference_path`、`top1_score`、`matched`，可直接追溯最佳参考图。

```powershell
.\bin\coser_cli.exe query `
  --db .\data\coser.db `
  --index .\data\coser_face.index `
  --clothing-index .\data\coser_clothing.index `
  --models-dir .\CoserRetrieval\models `
  --dir .\photos\query_set `
  --topk 1 `
  --mode face `
  --report .\query_report.csv
```

批量完成时输出 `total`、`processed`、`matched` 和 `unreadable`。发现不可读取文件时仍会继续处理
其他图片，但命令以非零退出码结束，便于自动化任务发现输入问题。

### 6. 服装/发型区域可视化

使用 `visualize` 生成当前 clothing/role 路线的实际语义区域示意图：红色半透明为 LIP 人体解析判定的
头发及可穿戴服饰，白色半透明为模型判定的脸部。其余像素（背景、皮肤、手脚和肢体）不参与 DINOv2
特征提取。

```powershell
.\bin\coser_cli.exe visualize `
  --models-dir .\CoserRetrieval\models `
  --image .\photos\example.jpg `
  --output .\visualizations\example_mask.png
```

角色路线依赖 `models/clothing/human_parsing_lip_resnet101.onnx`。可通过 `--human-parsing-model <path>`
覆盖其位置。该模型的输入特征与旧版“人体框减脸部椭圆”不兼容；升级后必须使用新的数据库和 clothing
索引重新执行 `ingest`，不能混用旧索引。模型权重尚待补齐时，所有会加载 clothing 路线的命令会明确失败，
不会回退到含背景或人脸的旧遮罩。

### 人体解析模型下载与替换

当前代码默认兼容 Ailia 发布的 LIP ResNet-101 模型：
`https://storage.googleapis.com/ailia-models/human_part_segmentation/resnet-lip.onnx`。下载后命名为
`human_parsing_lip_resnet101.onnx` 并覆盖 `models/clothing/` 下的同名文件。该 FP32 模型约 250 MB。

推荐优先使用较小的 SCHP LIP-20 INT8 静态 ONNX（约 66 MB）：
`https://huggingface.co/pirocheto/schp-lip-20/resolve/main/onnx/schp-lip-20-int8-static.onnx?download=true`。
它与 LIP 使用相同的 20 个语义类别，但输出结构不同；在切换前需先完成对应的 ONNX 输入/输出适配，不能
仅靠改文件名替换。下载后可使用自定义路径运行：

```powershell
.\bin\coser_cli.exe ingest ... `
  --human-parsing-model .\models\clothing\schp-lip-20-int8-static.onnx
```

替换任何人体解析模型时必须满足以下步骤：

1. 确认类别中能区分 `hair`、`face` 和主要服装类别；仅有人像前景/背景的二分类模型不可替代。
2. 在 `HumanParsingSegmenter` 中适配模型的输入尺寸、BGR/RGB 顺序、归一化、输出 tensor 和类别 ID。
3. 用 `visualize` 审核红色区域是否只覆盖头发/服饰、白色是否只覆盖脸部。
4. 使用新的 SQLite 数据库和 clothing Faiss 索引对全部图片重新 `ingest`；旧索引必须废弃，不能混用。

### 7. 批量扫描可疑插画/CG

`scan` 仅标记可能不是真人照片的文件，供人工复核；**不要**据此自动删除或拒绝图片。

```powershell
.\bin\coser_cli.exe scan `
  --dir .\photos `
  --clip-model .\CoserRetrieval\models\clip\clip_vision_quantized.onnx
```

### 8. 回归测试

固定样本位于 `CoserRetrieval/testdata/regression/`。现有 `run_regression_tests.sh` 需要 Bash；在纯 Windows 环境可按上面的 ingest/query 命令执行，或后续补充 PowerShell/CTest 测试入口。

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
拒绝非真人照片的图片入库（`coser_cli query` 查询路径不受影响，仍然接受任意输入）。

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
- **实测边界样本占比偏高，人工复核后确认过滤器实际可用误判率极高**：新增 `coser_cli scan` 子命令
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
`coser_cli ingest`/`coser_cli query` 默认三路联动，`coser_cli query` 新增
`--mode exact|face|clothing|all` 可选单路调试。

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

`ingest_cli.exe`/`query_cli.exe`（现已合并为 `coser_cli.exe ingest`/`coser_cli.exe query`）
早期版本没有捕获 `Ort::Exception`，当模型文件是未拉取内容的
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

## CLI 合并：三个可执行文件 → 单一 `coser_cli`

原先 `ingest_cli.exe`/`query_cli.exe`/`scan_authenticity_cli.exe` 是三个独立可执行文件，现已合并为
单一的 `coser_cli.exe`，用子命令区分功能（类似 `git <subcommand>` 的风格）：

```
coser_cli ingest --db <path> [--index <path>] --models-dir <dir> (--image <path> | --dir <directory>) [--person "<Name>"] [--role "<Character>"] [--face-model arcface|adaface] [--task-id <id>] [--file-prefix <prefix>] [--clothing-index <path>]
coser_cli query  --db <path> [--index <path>] --models-dir <dir> (--image <path> | --dir <directory>) --topk <N> [--mode exact|face|clothing|role|all] [--face-model arcface|adaface] [--task-id <id>] [--clothing-index <path>] [--report <csv>]
coser_cli scan   --dir <directory> --clip-model <path>
coser_cli visualize --models-dir <dir> --image <path> --output <png>
coser_cli help | --help | -h        # 或不带任何参数
```

`--index` 在人物人脸路线中必填；角色-only 入库/查询可省略。不同人脸模型的 embedding 空间不兼容，
AdaFace 必须使用单独的数据库和人脸索引。批量 `ingest`/`query` 会复用一次模型加载；`query --report`
可输出 top-1 参考图路径用于离线评测和人工复核。

## 回归测试：`run_regression_tests.sh`

改动代码后想快速确认没有把已知行为跑坏，用 `CoserRetrieval/run_regression_tests.sh`（先 build 出
`bin/coser_cli.exe`，再 `cd CoserRetrieval && ./run_regression_tests.sh`）。这不是精度评测
（精度评测见上面里程碑2/3小节，用 `testdata_eval` 全量数据），只是黑盒跑一遍 `coser_cli`，
检查几个已知场景的预期行为有没有被打破：

- 正常真人照 ingest 三路全部成功、无 partial failure
- 无人脸图片 ingest 时 face/clothing 非致命失败但整体仍 commit（exact 路线兜底）
- 插画被 CLIP 门禁拒绝（face 路线），但不影响 clothing/exact 路线
- 同人不同照片查询，face 路线正确命中且不与另一人混淆
- 原图自匹配、轻裁剪转发图仍被识别为同一原图（exact 路线的 pHash+ORB 容忍度）
- 未入库图片查询不崩溃、正确报告无匹配

用到的 case file 是 `testdata/regression/` 下 7 张图（`rioko_ref.jpg`/`rioko_query.jpg`/
`chichi_ref.jpg`/`chichi_query.jpg`/`illustration_reject.jpg`/`blank_noface.jpg`/
`rioko_ref_repost.jpg`），从 `testdata_eval`、`removed_illustrations/` 里挑出来的代表性样本
（`rioko_ref_repost.jpg` 是脚本生成的 ~5% 裁剪+重新编码变体，用于验证转发图容忍度），复制进
`testdata/regression/` 独立成一份固定不变的小测试集，不依赖体量大、会持续变动的 `testdata_eval`。
脚本自身在 `CoserRetrieval/` 目录下生成/清理临时 db 和 index 文件，不产生持久产物；
失败时打印 `[FAIL]` 并以非零退出码结束。
