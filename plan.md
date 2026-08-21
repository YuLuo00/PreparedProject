# 图库项目实施计划

本文按当前仓库的实际实现状态整理。勾选项表示已有对应代码或构建产物；未勾选项表示尚未实现、尚未接入，或尚未完成验证。
功能、CLI 参数、数据表或检索规则变更时，必须同步更新本计划、`CoserRetrieval/README.md` 和对应测试说明。

## 1. 原始图库系统（ImageSearch DLL）

- [x] 建立 CMake + MSVC 的 `ImageSearch.dll` 工程。
- [x] 实现 SQLite 图片元数据、标签、特征与人脸注册表。
- [x] 实现图片目录递归扫描与 JPG 入库。
- [x] 实现 Chinese-CLIP 768 维图片/文本特征提取。
- [x] 实现 CLIP 向量的 Faiss `IndexFlatIP` 检索与 SQLite ID 映射。
- [x] 实现结构化检索：标签、日期、路径前缀、评分和 top-k。
- [x] 实现文本语义检索：文本 CLIP 向量 + Faiss 检索 + 结构化过滤。
- [x] 实现 WD14 自动打标、标签持久化与 1024 维特征索引。
- [x] 实现 InsightFace 人脸注册、512 维特征索引和身份识别。
- [x] 实现 Python 特征服务：Chinese-CLIP（18081）、WD14（18082）、InsightFace（18083）。
- [x] 实现 C 导出接口：初始化、查询、文本查询、单张/批量添加、HTTP 服务启停。
- [x] 实现嵌入式 HTTP 服务及查询、添加、扫描、人脸注册/识别、索引重建路由。
- [x] 实现索引从 SQLite 特征数据重建。

## 2. CoserRetrieval 检索扩展

- [x] 实现纯 C++ 人脸检索路径：SCRFD 检测、人脸对齐、ArcFace 特征、Faiss 检索和元数据存储。
- [x] 接入 AdaFace IR-18 作为可选备用人脸模型并完成同集 A/B；当前准确率低于 ArcFace，保持 ArcFace 默认。
- [x] 实现 CLIP 真人照片/插画候选标记工具与批量扫描命令。
- [x] 实现服装/角色识别路径：YOLOv8 Pose 人体检测、DINOv2 特征和 Faiss 检索。
- [x] 实现原图匹配路径：pHash 粗筛和 ORB 精确确认。
- [x] 为入库文件记录 MD5，并在模型推理前跳过字节内容完全重复的文件。
- [x] 实现 L5 `RetrievalOrchestrator` 与 face/clothing/exact 结果融合。
- [x] 合并 CLI 为 `coser_cli ingest|query|scan`。
- [x] 支持目录批量查询与 top-1 CSV 报告，批量任务复用一次模型加载。
- [x] 接入 vcpkg oneTBB，并将批量 `ingest` 改为 7 个在途 token 的 TBB pipeline：预处理/模型解析并行，SQLite 建档和结果汇总串行。
- [x] 提供进程内任务进度回调中心：`ingest`/`query` 发送开始、运行中、完成或失败事件，供后续 UI/服务按任务 ID 刷新。
- [x] 支持 `--role` 角色标签与 `query --mode role`；角色模式使用 LIP 像素级人体解析，仅匹配头发与可穿戴服饰，排除脸、皮肤、肢体和背景，不调用人脸或原图路线。
- [x] 提供 `visualize` 诊断命令，渲染 LIP 实际判定的头发/服装区域和脸部区域。
- [x] 下载、校验并接入 LIP ResNet-101 ONNX 人体语义分割权重。
- [x] 对 clothing/role 路线增加“无衣服”判定：仅头发或少量误分割像素不会生成服装向量，防止黑色掩码造成假高相似度。
- [x] clothing/role 向量改为按 LIP mask 覆盖的 DINO patch token 加权池化；mask 外 patch 不参与最终 embedding。
- [x] 准备 face、CLIP、YOLO Pose、DINOv2 本地 ONNX 模型。
- [x] 编写固定样本回归测试脚本和 7 张测试图片。
- [ ] 将 CoserRetrieval 的三路检索与融合接入 `ImageSearch.dll`。
- [ ] 在 ImageSearch HTTP API 中暴露三路检索、融合查询和写入能力。
- [ ] 统一旧 DLL 数据模型与 CoserRetrieval 的 SQLite/索引格式，避免两套孤立数据。

## 3. 可靠性与工程化

- [x] 为 CLI 主入口增加异常捕获，避免 ONNX/LFS 文件异常直接终止进程。
- [x] 记录 Git LFS 模型文件恢复方法。
- [x] 记录端到端评估结果与 CLIP 过滤器的已知误判风险。
- [ ] 在 Windows 下提供可直接运行的回归测试（PowerShell 或 CTest）；当前脚本依赖 Bash。
- [ ] 实际执行并固化回归测试结果；当前新增脚本与样本尚未在本环境跑通。
- [ ] 将回归测试脚本、样本和 README 更新提交到版本库。
- [ ] 为 DLL、HTTP API 和 CLI 建立统一的自动化测试入口。
- [ ] 为模型、数据库、索引文件补充版本兼容性和迁移检查。

## 4. 检索质量与性能

- [ ] 使用真实业务数据调优 face/clothing 融合权重、pHash 汉明距离和 ORB 内点阈值。
- [ ] 将 exact 路线的 ORB 原始内点数归一化，或移除无效的 `exact_score_threshold` 配置。
- [ ] 将 CLIP 真人照/插画判断调整为人工复核候选流程；不得用于自动删除或自动拒绝数据。
- [ ] 评估更高精度的视觉编码器或重新设计提示词，降低重度后期 Cosplay 照片的误判。
- [ ] 为批量 ingest 和 query 引入任务并行/线程池，并控制 ONNX、SQLite 与 Faiss 的并发访问。
- [ ] 建立可重复的精度评测集、指标基线和版本对比报告。

## 5. 产品交付

- [ ] 确定最终交付形态：DLL + HTTP 服务、常驻服务或桌面/Web 前端。
- [ ] 若继续使用 AnythingLLM，编写稳定的 API 使用说明、鉴权和部署配置。
- [ ] 提供一键启动、模型检查、数据初始化和日志收集脚本。
