#pragma once

#ifdef IMAGESEARCH_EXPORTS
#define IMAGESEARCH_API __declspec(dllexport)
#else
#define IMAGESEARCH_API __declspec(dllimport)
#endif

extern "C" {

/// 初始化：打开/创建数据库，加载 Faiss 索引
/// @param dbPath  SQLite 数据库文件路径（UTF-8）
/// @return 0 成功，非0 失败
IMAGESEARCH_API int ImageSearch_Init(const char* dbPath);

/// 关闭：停止 HTTP 服务，释放所有资源
IMAGESEARCH_API void ImageSearch_Shutdown();

/// 结构化检索
/// @param queryJson  JSON 查询条件（tags/date_from/date_to/path_prefix/rating_min/top_k）
/// @param outBuf     输出缓冲区（JSON 数组）
/// @param bufSize    缓冲区大小
/// @return 写入字节数，负数表示错误
IMAGESEARCH_API int ImageSearch_Query(const char* queryJson, char* outBuf, int bufSize);

/// 向量语义检索（文字描述 → CLIP → Faiss ANN）
/// @param textQuery  文字描述（UTF-8）
/// @param filterJson 附加结构化过滤条件（可为 null）
/// @param topK       返回结果数量
/// @param outBuf     输出缓冲区（JSON 数组）
/// @param bufSize    缓冲区大小
/// @return 写入字节数，负数表示错误
IMAGESEARCH_API int ImageSearch_QueryByText(const char* textQuery, const char* filterJson,
                                             int topK, char* outBuf, int bufSize);

/// 添加单张图片（入库 + 特征提取 + 索引）
/// @param imageJson  JSON 对象，包含 path（必填）、rating、tags 等
/// @return 0 成功，非0 失败
IMAGESEARCH_API int ImageSearch_AddImage(const char* imageJson);

/// 批量添加图片（每行一个 JSON 对象）
/// @param jsonLines  多行 JSON（newline-delimited JSON）
/// @return 成功入库数量，负数表示错误
IMAGESEARCH_API int ImageSearch_AddImageBatch(const char* jsonLines);

/// 启动内嵌 HTTP 服务
/// @param port  监听端口（默认 18080）
/// @return 0 成功，非0 失败
IMAGESEARCH_API int ImageSearch_StartHttpServer(int port);

/// 停止内嵌 HTTP 服务
IMAGESEARCH_API void ImageSearch_StopHttpServer();

/// 扫描目录，将所有 jpg 图片入库
/// @param dirPath  目录路径（UTF-8）
/// @return 成功入库数量，负数表示错误
IMAGESEARCH_API int ImageSearch_ScanDirectory(const char* dirPath);

/// 注册 Coser 人脸（用于人像识别）
/// @param coserJson  JSON 对象：{"name":"xxx","image_path":"xxx.jpg"}
/// @return 0 成功，非0 失败
IMAGESEARCH_API int ImageSearch_RegisterCoser(const char* coserJson);

/// 识别图片中的 Coser
/// @param imagePath  图片路径（UTF-8）
/// @param outBuf     输出缓冲区（JSON 数组，含 name/similarity）
/// @param bufSize    缓冲区大小
/// @return 写入字节数，负数表示错误
IMAGESEARCH_API int ImageSearch_IdentifyCoser(const char* imagePath, char* outBuf, int bufSize);

/// 重建 Faiss 索引（从 SQLite 中的向量数据重建）
/// @param indexType  "clip" / "wd14" / "face"（null 表示全部重建）
/// @return 0 成功，非0 失败
IMAGESEARCH_API int ImageSearch_RebuildIndex(const char* indexType);

} // extern "C"
