# 图库AI检索系统 · 开发需求说明

在 `test/` 文件夹下搭建 C++ 工程，用 MSVC 编译，输出为 `ImageSearch.dll`。

## 核心需求

### 整体架构
- DLL 内嵌 cpp-httplib HTTP 服务（端口 18080），供 AnythingLLM Custom Agent 通过 `fetch` 调用
- 数据库：SQLite（`image_library.db`），存图片元数据、标签、模特信息
- 向量检索：Faiss 索引，SQLite 存 Faiss id → jpg 路径映射

### 图片整体抽象库（通用内容检索）
1. 检索指定目录下全部 jpg 图片，入库到 SQLite（路径、文件名、拍摄日期、评分等）
2. 搭建 pipeline：jpg → Chinese-CLIP ViT-L/14（768维特征提取）→ Faiss IndexFlatIP
3. SQLite `image_features` 表备份 clip_vector（float32[768]），支持重建 Faiss 索引
4. 支持结构化检索（氛围/场景/风格标签、日期范围、路径前缀、评分）+ 向量语义检索（文字描述→CLIP→Faiss ANN）

### 图片人像库（Coser 身份识别）
1. InsightFace buffalo_l（ArcFace R100）| 512维人脸特征，独立 Faiss 索引
2. SQLite `coser_faces` 表：每个 Coser 注册多张参考人脸（不同角色/妆容），存 float32[512]
3. 识别时对所有注册人脸取最高余弦相似度，阈值 0.4 以上视为同一人

### 自动打标（WD14-tagger，二次元/Cosplay 专用）
1. WD14 ViT-L ONNX 模型，输出 Danbooru 标签（6000+）→ 写入 SQLite `image_tags` 表
2. WD14 ViT 特征层向量（1024维）→ 独立 Faiss 索引，用于内容相似度检索

### Python 微服务（特征提取，DLL 通过 HTTP 调用）
| 端口  | 服务              | 说明                        |
|-------|-------------------|-----------------------------|
| 18081 | Chinese-CLIP      | 图片/文字 → 768维向量        |
| 18082 | WD14-tagger       | 打标 + 1024维特征向量        |
| 18083 | InsightFace       | 人脸检测 + ArcFace 512维向量 |

### DLL 导出接口（C 导出，UTF-8）
- `ImageSearch_Init(dbPath)` / `ImageSearch_Shutdown()`
- `ImageSearch_Query(queryJson, outBuf, bufSize)` — 结构化检索
- `ImageSearch_QueryByText(textQuery, filterJson, topK, outBuf, bufSize)` — 向量语义检索
- `ImageSearch_AddImage(imageJson)` / `ImageSearch_AddImageBatch(jsonLines)`
- `ImageSearch_StartHttpServer(port)` / `ImageSearch_StopHttpServer()`

### 技术栈
- 数据库：SQLite3 | JSON：nlohmann/json | HTTP：cpp-httplib | 向量：Faiss | 构建：CMake + MSVC
