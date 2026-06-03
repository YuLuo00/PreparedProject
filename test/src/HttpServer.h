#pragma once
#include <string>
#include <memory>
#include <thread>
#include <atomic>
#include <functional>

// 前向声明，避免在头文件中包含 httplib
namespace httplib { class Server; }

class Database;
class FaissIndexManager;

/// 内嵌 HTTP 服务，监听 AnythingLLM Custom Agent 的请求
class ImageSearchHttpServer {
public:
    ImageSearchHttpServer(Database* db, FaissIndexManager* faiss);
    ~ImageSearchHttpServer();

    bool Start(int port = 18080);
    void Stop();
    bool IsRunning() const { return running_.load(); }

private:
    void SetupRoutes();

    // 路由处理函数
    // POST /query           — 结构化检索
    // POST /query_by_text   — 向量语义检索
    // POST /add_image       — 添加单张图片
    // POST /add_batch       — 批量添加
    // POST /scan_directory  — 扫描目录
    // POST /register_coser  — 注册 Coser
    // POST /identify_coser  — 识别 Coser
    // POST /rebuild_index   — 重建索引
    // GET  /health          — 健康检查
    // GET  /stats           — 统计信息

    Database* db_;
    FaissIndexManager* faiss_;
    std::unique_ptr<httplib::Server> server_;
    std::thread serverThread_;
    std::atomic<bool> running_{false};
    int port_ = 18080;
};
