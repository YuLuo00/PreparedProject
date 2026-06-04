/**
 * ImageSearch DLL Full Test
 * ─────────────────────────────────────────────────────────────────────────────
 * 通过 DLL 导入库（ImageSearch.lib）测试所有导出的 C API 函数
 * 以及 HTTP 服务器的全部端点。
 *
 * 设计原则：
 *   - 不编译 DLL 源码，仅链接 ImageSearch.lib（import lib）
 *   - Python 服务（CLIP/WD14/InsightFace）不可用时优雅降级，不算失败
 *   - 每个 Section 独立，可单独分析
 *   - 运行目录：bin/（与 ImageSearch.dll 同目录）
 */

#include <ImageSearch/ImageSearch.h>
#include <httplib.h>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <thread>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using json = nlohmann::json;

// ─── 测试框架 ─────────────────────────────────────────────────────────────────

static int g_pass = 0, g_fail = 0;

#define TEST_ASSERT(cond, msg)                                              \
    do {                                                                    \
        if (cond) {                                                         \
            printf("  [PASS] %s\n", msg);                                  \
            ++g_pass;                                                       \
        } else {                                                            \
            printf("  [FAIL] %s  (line %d)\n", msg, __LINE__);            \
            ++g_fail;                                                       \
        }                                                                   \
    } while (0)

#define TEST_SECTION(name) printf("\n=== %s ===\n", name)

// ─── 辅助：路径转 JSON 安全字符串（反斜杠 → 正斜杠）────────────────────────

static std::string P(const std::string& path) {
    std::string r = path;
    for (auto& c : r) if (c == '\\') c = '/';
    return r;
}

// ─── 辅助：创建最小有效 JPEG（1x1 灰色像素）─────────────────────────────────

static const unsigned char kMinJpeg[] = {
    0xFF,0xD8,0xFF,0xE0,0x00,0x10,0x4A,0x46,0x49,0x46,0x00,0x01,0x01,0x00,0x00,0x01,
    0x00,0x01,0x00,0x00,0xFF,0xDB,0x00,0x43,0x00,0x08,0x06,0x06,0x07,0x06,0x05,0x08,
    0x07,0x07,0x07,0x09,0x09,0x08,0x0A,0x0C,0x14,0x0D,0x0C,0x0B,0x0B,0x0C,0x19,0x12,
    0x13,0x0F,0x14,0x1D,0x1A,0x1F,0x1E,0x1D,0x1A,0x1C,0x1C,0x20,0x24,0x2E,0x27,0x20,
    0x22,0x2C,0x23,0x1C,0x1C,0x28,0x37,0x29,0x2C,0x30,0x31,0x34,0x34,0x34,0x1F,0x27,
    0x39,0x3D,0x38,0x32,0x3C,0x2E,0x33,0x34,0x32,0xFF,0xC0,0x00,0x0B,0x08,0x00,0x01,
    0x00,0x01,0x01,0x01,0x11,0x00,0xFF,0xC4,0x00,0x1F,0x00,0x00,0x01,0x05,0x01,0x01,
    0x01,0x01,0x01,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x02,0x03,0x04,
    0x05,0x06,0x07,0x08,0x09,0x0A,0x0B,0xFF,0xC4,0x00,0x35,0x10,0x00,0x02,0x01,0x03,
    0x03,0x02,0x04,0x03,0x05,0x05,0x04,0x04,0x00,0x00,0x01,0x7D,0x01,0x02,0x03,0x00,
    0x04,0x11,0x05,0x12,0x21,0x31,0x41,0x06,0x13,0x51,0x61,0x07,0x22,0x71,0x14,0x32,
    0x81,0x91,0xA1,0x08,0x23,0x42,0xB1,0xC1,0x15,0x52,0xD1,0xF0,0x24,0x33,0x62,0x72,
    0x82,0xFF,0xDA,0x00,0x08,0x01,0x01,0x00,0x00,0x3F,0x00,0xFB,0x26,0xA2,0x8A,0xFF,
    0xD9
};

static bool MakeJpeg(const std::string& path) {
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f.write(reinterpret_cast<const char*>(kMinJpeg), sizeof(kMinJpeg));
    return f.good();
}

// ─── 全局测试环境 ─────────────────────────────────────────────────────────────

static std::string g_dir;
static std::string g_db;
static std::string g_img1, g_img2, g_img3;

static void SetupEnv() {
    g_dir  = "./dll_full_test_tmp";
    g_db   = g_dir + "/test.db";
    g_img1 = g_dir + "/img1.jpg";
    g_img2 = g_dir + "/img2.jpg";
    g_img3 = g_dir + "/img3.jpeg";
    fs::remove_all(g_dir);
    fs::create_directories(g_dir);
    MakeJpeg(g_img1);
    MakeJpeg(g_img2);
    MakeJpeg(g_img3);
}

static void CleanupEnv() {
    fs::remove_all(g_dir);
}

// ─── Section 1: DLL 生命周期 ──────────────────────────────────────────────────

static void Test_Lifecycle() {
    TEST_SECTION("Section 1: DLL Lifecycle (Init / Shutdown)");

    int ret = ImageSearch_Init(g_db.c_str());
    TEST_ASSERT(ret == 0, "Init returns 0");

    ret = ImageSearch_Init(g_db.c_str());
    TEST_ASSERT(ret == 0, "Re-Init returns 0");

    ret = ImageSearch_Init(nullptr);
    TEST_ASSERT(ret != 0, "Init(null) returns error");

    ret = ImageSearch_Init(g_db.c_str());
    TEST_ASSERT(ret == 0, "Init after null returns 0");

    ImageSearch_Shutdown();
    TEST_ASSERT(true, "Shutdown no crash");

    ImageSearch_Shutdown();
    TEST_ASSERT(true, "Double Shutdown no crash");

    ret = ImageSearch_Init(g_db.c_str());
    TEST_ASSERT(ret == 0, "Re-Init for subsequent tests");
}

// ─── Section 2: AddImage ─────────────────────────────────────────────────────

static void Test_AddImage() {
    TEST_SECTION("Section 2: AddImage");

    char buf[8192] = {};
    int ret;

    ret = ImageSearch_AddImage("{\"path\":\"/nonexistent/photo.jpg\"}");
    TEST_ASSERT(ret == -2, "AddImage nonexistent file -> -2");

    ret = ImageSearch_AddImage(nullptr);
    TEST_ASSERT(ret == -1, "AddImage null -> -1");

    ret = ImageSearch_AddImage("not json");
    TEST_ASSERT(ret == -1, "AddImage invalid JSON -> -1");

    ret = ImageSearch_AddImage("{\"rating\":4.5}");
    TEST_ASSERT(ret == -1, "AddImage missing path -> -1");

    std::string j1 = "{\"path\":\"" + P(g_img1) + "\",\"rating\":4.5,\"shoot_date\":\"2024-01-15\"}";
    ret = ImageSearch_AddImage(j1.c_str());
    TEST_ASSERT(ret == 0, "AddImage valid file -> 0");

    std::string j2 = "{\"path\":\"" + P(g_img2) + "\",\"rating\":3.0,\"shoot_date\":\"2024-03-20\"}";
    ret = ImageSearch_AddImage(j2.c_str());
    TEST_ASSERT(ret == 0, "AddImage second file -> 0");

    std::string j3 = "{\"path\":\"" + P(g_img3) + "\",\"rating\":5.0,\"shoot_date\":\"2024-06-01\"}";
    ret = ImageSearch_AddImage(j3.c_str());
    TEST_ASSERT(ret == 0, "AddImage .jpeg file -> 0");

    ret = ImageSearch_Query("{\"top_k\":10}", buf, sizeof(buf));
    TEST_ASSERT(ret > 0, "Query after AddImage returns data");
    {
        auto j = json::parse(buf);
        TEST_ASSERT(j.is_array() && (int)j.size() >= 3, "Query returns >= 3 results");
    }
}

// ─── Section 3: Query（结构化检索）──────────────────────────────────────────

static void Test_Query() {
    TEST_SECTION("Section 3: Query (Structural)");

    char buf[16384] = {};
    int ret;

    // 基本查询
    ret = ImageSearch_Query("{\"top_k\":10}", buf, sizeof(buf));
    TEST_ASSERT(ret > 0, "Query basic returns data");
    {
        auto j = json::parse(buf);
        TEST_ASSERT(j.is_array(), "Query result is array");
        TEST_ASSERT((int)j.size() >= 3, "Query returns >= 3 items");
    }

    // top_k 限制
    ret = ImageSearch_Query("{\"top_k\":1}", buf, sizeof(buf));
    TEST_ASSERT(ret > 0, "Query top_k=1 returns data");
    {
        auto j = json::parse(buf);
        TEST_ASSERT(j.is_array() && (int)j.size() <= 1, "Query top_k=1 returns <= 1 item");
    }

    // rating_min 过滤
    ret = ImageSearch_Query("{\"rating_min\":4.0,\"top_k\":10}", buf, sizeof(buf));
    TEST_ASSERT(ret > 0, "Query rating_min=4.0 returns data");
    {
        auto j = json::parse(buf);
        TEST_ASSERT(j.is_array() && !j.empty(), "Query rating_min=4.0 returns results");
        bool allOk = true;
        for (auto& item : j) {
            if (item["rating"].get<double>() < 4.0) { allOk = false; break; }
        }
        TEST_ASSERT(allOk, "All results have rating >= 4.0");
    }

    // rating_min 过高 -> 空结果
    ret = ImageSearch_Query("{\"rating_min\":9.9,\"top_k\":10}", buf, sizeof(buf));
    TEST_ASSERT(ret >= 0, "Query rating_min=9.9 returns ok");
    {
        auto j = json::parse(buf);
        TEST_ASSERT(j.is_array() && j.empty(), "Query rating_min=9.9 returns empty array");
    }

    // path_prefix 过滤
    // DLL 内部用 fs::absolute().string() 存储路径（Windows 下为反斜杠）
    // 用 nlohmann::json 构建 JSON，自动处理路径中的反斜杠转义
    {
        json pfqObj;
        pfqObj["path_prefix"] = fs::absolute(g_dir).string();
        pfqObj["top_k"] = 10;
        std::string pfq = pfqObj.dump();
        ret = ImageSearch_Query(pfq.c_str(), buf, sizeof(buf));
        TEST_ASSERT(ret > 0, "Query path_prefix returns data");
        {
            auto j = json::parse(buf);
            TEST_ASSERT(j.is_array() && !j.empty(), "Query path_prefix returns results");
        }
    }

    // date_from / date_to 过滤
    ret = ImageSearch_Query("{\"date_from\":\"2024-01-01\",\"date_to\":\"2024-02-01\",\"top_k\":10}", buf, sizeof(buf));
    TEST_ASSERT(ret >= 0, "Query date range returns ok");

    // null -> -1
    ret = ImageSearch_Query(nullptr, buf, sizeof(buf));
    TEST_ASSERT(ret == -1, "Query null -> -1");

    // 无效 JSON -> -1
    ret = ImageSearch_Query("not json", buf, sizeof(buf));
    TEST_ASSERT(ret == -1, "Query invalid JSON -> -1");

    // 空 JSON 对象（使用默认值）
    ret = ImageSearch_Query("{}", buf, sizeof(buf));
    TEST_ASSERT(ret >= 0, "Query empty JSON -> ok");

    // 结果字段完整性
    ret = ImageSearch_Query("{\"top_k\":1}", buf, sizeof(buf));
    {
        auto j = json::parse(buf);
        if (!j.empty()) {
            auto& item = j[0];
            TEST_ASSERT(item.contains("id"),         "Result has 'id'");
            TEST_ASSERT(item.contains("path"),       "Result has 'path'");
            TEST_ASSERT(item.contains("filename"),   "Result has 'filename'");
            TEST_ASSERT(item.contains("rating"),     "Result has 'rating'");
            TEST_ASSERT(item.contains("shoot_date"), "Result has 'shoot_date'");
            TEST_ASSERT(item.contains("tags"),       "Result has 'tags'");
        }
    }

    // 超小缓冲区（截断，不崩溃）
    char small[8] = {};
    ret = ImageSearch_Query("{\"top_k\":1}", small, sizeof(small));
    TEST_ASSERT(ret > 0, "Query small buffer -> truncated no crash");
}

// ─── Section 4: AddImageBatch ────────────────────────────────────────────────

static void Test_AddImageBatch() {
    TEST_SECTION("Section 4: AddImageBatch");

    std::string b1 = g_dir + "/batch1.jpg";
    std::string b2 = g_dir + "/batch2.jpg";
    MakeJpeg(b1);
    MakeJpeg(b2);

    // 正常批量添加
    std::string lines =
        "{\"path\":\"" + P(b1) + "\",\"rating\":3.5}\n"
        "{\"path\":\"" + P(b2) + "\",\"rating\":4.0}\n";
    int ret = ImageSearch_AddImageBatch(lines.c_str());
    TEST_ASSERT(ret == 2, "AddImageBatch 2 valid files -> 2");

    // 含无效路径
    std::string mixed =
        "{\"path\":\"/nonexistent/x.jpg\"}\n"
        "{\"path\":\"" + P(b1) + "\"}\n";
    ret = ImageSearch_AddImageBatch(mixed.c_str());
    TEST_ASSERT(ret >= 0, "AddImageBatch mixed -> >= 0");

    // 空输入
    ret = ImageSearch_AddImageBatch("");
    TEST_ASSERT(ret == 0, "AddImageBatch empty -> 0");

    // null
    ret = ImageSearch_AddImageBatch(nullptr);
    TEST_ASSERT(ret == -1, "AddImageBatch null -> -1");

    // 全部无效 JSON 行（跳过）
    ret = ImageSearch_AddImageBatch("not json\n{\"no_path\":true}\n");
    TEST_ASSERT(ret == 0, "AddImageBatch all invalid -> 0");
}

// ─── Section 5: ScanDirectory ────────────────────────────────────────────────

static void Test_ScanDirectory() {
    TEST_SECTION("Section 5: ScanDirectory");

    std::string scanDir = g_dir + "/scan_test";
    std::string subDir  = scanDir + "/sub";
    fs::create_directories(subDir);
    MakeJpeg(scanDir + "/s1.jpg");
    MakeJpeg(scanDir + "/s2.jpeg");
    MakeJpeg(subDir  + "/s3.jpg");
    // 非 jpg 文件（应被跳过）
    { std::ofstream f(scanDir + "/readme.txt"); f << "text"; }
    { std::ofstream f(scanDir + "/image.png");  f << "png";  }

    // 首次扫描 -> 3 个 jpg/jpeg
    int ret = ImageSearch_ScanDirectory(scanDir.c_str());
    TEST_ASSERT(ret == 3, "ScanDirectory finds 3 jpg/jpeg files");

    // 重复扫描 -> 0（已存在跳过）
    ret = ImageSearch_ScanDirectory(scanDir.c_str());
    TEST_ASSERT(ret == 0, "ScanDirectory re-scan returns 0 (all skipped)");

    // 不存在的目录 -> -2
    ret = ImageSearch_ScanDirectory("/nonexistent/dir");
    TEST_ASSERT(ret == -2, "ScanDirectory nonexistent -> -2");

    // null -> -1
    ret = ImageSearch_ScanDirectory(nullptr);
    TEST_ASSERT(ret == -1, "ScanDirectory null -> -1");
}

// ─── Section 6: RebuildIndex ─────────────────────────────────────────────────

static void Test_RebuildIndex() {
    TEST_SECTION("Section 6: RebuildIndex");

    int ret;

    ret = ImageSearch_RebuildIndex("clip");
    TEST_ASSERT(ret == 0, "RebuildIndex('clip') -> 0");

    ret = ImageSearch_RebuildIndex("wd14");
    TEST_ASSERT(ret == 0, "RebuildIndex('wd14') -> 0");

    ret = ImageSearch_RebuildIndex("face");
    TEST_ASSERT(ret == 0, "RebuildIndex('face') -> 0");

    ret = ImageSearch_RebuildIndex("all");
    TEST_ASSERT(ret == 0, "RebuildIndex('all') -> 0");

    ret = ImageSearch_RebuildIndex(nullptr);
    TEST_ASSERT(ret == 0, "RebuildIndex(null) -> 0 (rebuilds all)");

    // 无效类型（rebuildOne 内部忽略，不崩溃）
    ret = ImageSearch_RebuildIndex("invalid_type");
    TEST_ASSERT(ret == 0, "RebuildIndex('invalid_type') -> 0 (no crash)");
}

// ─── Section 7: QueryByText（CLIP 语义检索）─────────────────────────────────

static void Test_QueryByText() {
    TEST_SECTION("Section 7: QueryByText (CLIP semantic search)");

    char buf[4096] = {};
    int ret;

    // CLIP 服务不可用 -> -2；可用 -> >= 0
    ret = ImageSearch_QueryByText("a beautiful landscape", nullptr, 10, buf, sizeof(buf));
    if (ret == -2) {
        TEST_ASSERT(true, "QueryByText CLIP unavailable -> -2 (expected)");
    } else if (ret >= 0) {
        TEST_ASSERT(true, "QueryByText CLIP available -> returns data");
        auto j = json::parse(buf);
        TEST_ASSERT(j.is_array(), "QueryByText result is array");
    } else {
        TEST_ASSERT(false, "QueryByText unexpected error code");
    }

    // null textQuery -> -1
    ret = ImageSearch_QueryByText(nullptr, nullptr, 10, buf, sizeof(buf));
    TEST_ASSERT(ret == -1, "QueryByText null text -> -1");

    // 带 filterJson（CLIP 不可用时仍返回 -2）
    ret = ImageSearch_QueryByText("cat", "{\"rating_min\":3.0}", 5, buf, sizeof(buf));
    TEST_ASSERT(ret == -2 || ret >= 0, "QueryByText with filter -> -2 or ok");
}

// ─── Section 8: Coser 操作 ───────────────────────────────────────────────────

static void Test_CoserOperations() {
    TEST_SECTION("Section 8: Coser Operations (InsightFace service)");

    char buf[4096] = {};
    int ret;

    // RegisterCoser - null -> -1
    ret = ImageSearch_RegisterCoser(nullptr);
    TEST_ASSERT(ret == -1, "RegisterCoser null -> -1");

    // RegisterCoser - 缺少字段 -> -1
    ret = ImageSearch_RegisterCoser("{\"name\":\"TestCoser\"}");
    TEST_ASSERT(ret == -1, "RegisterCoser missing image_path -> -1");

    ret = ImageSearch_RegisterCoser("{\"image_path\":\"x.jpg\"}");
    TEST_ASSERT(ret == -1, "RegisterCoser missing name -> -1");

    // RegisterCoser - 有效图片（无人脸/服务不可用 -> -2）
    std::string cj = "{\"name\":\"TestCoser\",\"image_path\":\"" + P(g_img1) + "\"}";
    ret = ImageSearch_RegisterCoser(cj.c_str());
    TEST_ASSERT(ret == -2 || ret == 0, "RegisterCoser valid image -> -2 (no face) or 0");

    // IdentifyCoser - null -> -1
    ret = ImageSearch_IdentifyCoser(nullptr, buf, sizeof(buf));
    TEST_ASSERT(ret == -1, "IdentifyCoser null -> -1");

    // IdentifyCoser - 有效图片（无人脸 -> 返回空数组 "[]"）
    ret = ImageSearch_IdentifyCoser(g_img1.c_str(), buf, sizeof(buf));
    if (ret >= 0) {
        auto j = json::parse(buf);
        TEST_ASSERT(j.is_array(), "IdentifyCoser result is array");
    } else {
        TEST_ASSERT(ret == -1, "IdentifyCoser service unavailable -> -1");
    }
}

// ─── Section 9: HTTP Server 全端点测试 ───────────────────────────────────────

static const int kPort = 18093;

static void Test_HttpServer() {
    TEST_SECTION("Section 9: HTTP Server (all endpoints)");

    // 启动
    int ret = ImageSearch_StartHttpServer(kPort);
    TEST_ASSERT(ret == 0, "StartHttpServer returns 0");
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // 幂等启动
    ret = ImageSearch_StartHttpServer(kPort);
    TEST_ASSERT(ret == 0, "StartHttpServer again returns 0");

    httplib::Client cli("127.0.0.1", kPort);
    cli.set_connection_timeout(3);
    cli.set_read_timeout(120);  // WD14 服务读超时 60s，需留足余量

    // ── GET /health ──────────────────────────────────────────────────────────
    {
        auto r = cli.Get("/health");
        TEST_ASSERT(r && r->status == 200, "GET /health -> 200");
        if (r) {
            auto j = json::parse(r->body);
            TEST_ASSERT(j["status"] == "ok",       "GET /health status=ok");
            TEST_ASSERT(j.contains("service"),     "GET /health has 'service'");
        }
    }

    // ── GET /stats ───────────────────────────────────────────────────────────
    {
        auto r = cli.Get("/stats");
        TEST_ASSERT(r && r->status == 200, "GET /stats -> 200");
        if (r) {
            auto j = json::parse(r->body);
            TEST_ASSERT(j.contains("clip_index_count"), "GET /stats has clip_index_count");
            TEST_ASSERT(j.contains("wd14_index_count"), "GET /stats has wd14_index_count");
            TEST_ASSERT(j.contains("face_index_count"), "GET /stats has face_index_count");
            TEST_ASSERT(j.contains("clip_service"),     "GET /stats has clip_service");
            TEST_ASSERT(j.contains("wd14_service"),     "GET /stats has wd14_service");
            TEST_ASSERT(j.contains("face_service"),     "GET /stats has face_service");
            TEST_ASSERT(j["clip_index_count"].is_number(), "clip_index_count is number");
        }
    }

    // ── POST /query ──────────────────────────────────────────────────────────
    {
        auto r = cli.Post("/query", "{\"top_k\":5}", "application/json");
        TEST_ASSERT(r && r->status == 200, "POST /query -> 200");
        if (r) {
            auto j = json::parse(r->body);
            TEST_ASSERT(j.contains("results"), "POST /query has 'results'");
            TEST_ASSERT(j.contains("count"),   "POST /query has 'count'");
            TEST_ASSERT(j["results"].is_array(), "POST /query results is array");
            TEST_ASSERT(j["count"].get<int>() == (int)j["results"].size(),
                        "POST /query count matches results size");
        }
    }

    // POST /query with rating_min
    {
        auto r = cli.Post("/query", "{\"rating_min\":4.0,\"top_k\":10}", "application/json");
        TEST_ASSERT(r && r->status == 200, "POST /query rating_min -> 200");
        if (r) {
            auto j = json::parse(r->body);
            bool allOk = true;
            for (auto& item : j["results"]) {
                if (item["rating"].get<double>() < 4.0) { allOk = false; break; }
            }
            TEST_ASSERT(allOk, "POST /query rating_min filter works");
        }
    }

    // POST /query invalid JSON -> 400
    {
        auto r = cli.Post("/query", "not json", "application/json");
        TEST_ASSERT(r && r->status == 400, "POST /query invalid JSON -> 400");
    }

    // ── POST /add_image ──────────────────────────────────────────────────────
    {
        std::string newImg = g_dir + "/http_add.jpg";
        MakeJpeg(newImg);
        std::string body = "{\"path\":\"" + P(newImg) + "\",\"rating\":3.5}";
        auto r = cli.Post("/add_image", body, "application/json");
        TEST_ASSERT(r && r->status == 200, "POST /add_image valid -> 200");
        if (r) {
            auto j = json::parse(r->body);
            TEST_ASSERT(j["success"] == true,    "POST /add_image success=true");
            TEST_ASSERT(j.contains("image_id"),  "POST /add_image has image_id");
            TEST_ASSERT(j["image_id"].get<int64_t>() > 0, "POST /add_image image_id > 0");
        }
    }

    // POST /add_image file not found -> 400
    {
        auto r = cli.Post("/add_image", "{\"path\":\"/nonexistent/x.jpg\"}", "application/json");
        TEST_ASSERT(r && r->status == 400, "POST /add_image not found -> 400");
    }

    // POST /add_image missing path -> 400
    {
        auto r = cli.Post("/add_image", "{\"rating\":3.0}", "application/json");
        TEST_ASSERT(r && r->status == 400, "POST /add_image missing path -> 400");
    }

    // ── POST /add_batch ──────────────────────────────────────────────────────
    {
        std::string bh1 = g_dir + "/batch_http1.jpg";
        std::string bh2 = g_dir + "/batch_http2.jpg";
        MakeJpeg(bh1);
        MakeJpeg(bh2);
        std::string body =
            "{\"path\":\"" + P(bh1) + "\",\"rating\":3.0}\n"
            "{\"path\":\"" + P(bh2) + "\",\"rating\":4.0}\n"
            "{\"path\":\"/nonexistent/x.jpg\"}\n";
        auto r = cli.Post("/add_batch", body, "application/json");
        TEST_ASSERT(r && r->status == 200, "POST /add_batch -> 200");
        if (r) {
            auto j = json::parse(r->body);
            TEST_ASSERT(j["success"].get<int>() == 2, "POST /add_batch success=2");
            TEST_ASSERT(j["failed"].get<int>()  == 1, "POST /add_batch failed=1");
            TEST_ASSERT(j.contains("errors"),         "POST /add_batch has errors array");
        }
    }

    // ── POST /scan_directory ─────────────────────────────────────────────────
    {
        std::string sd = g_dir + "/http_scan";
        fs::create_directories(sd);
        MakeJpeg(sd + "/hs1.jpg");
        MakeJpeg(sd + "/hs2.jpg");
        std::string body = "{\"path\":\"" + P(sd) + "\"}";
        auto r = cli.Post("/scan_directory", body, "application/json");
        TEST_ASSERT(r && r->status == 200, "POST /scan_directory -> 200");
        if (r) {
            auto j = json::parse(r->body);
            TEST_ASSERT(j["scanned"].get<int>() == 2, "POST /scan_directory scanned=2");
            TEST_ASSERT(j.contains("directory"),      "POST /scan_directory has directory");
        }
    }

    // POST /scan_directory not found -> 400
    {
        auto r = cli.Post("/scan_directory", "{\"path\":\"/nonexistent/dir\"}", "application/json");
        TEST_ASSERT(r && r->status == 400, "POST /scan_directory not found -> 400");
    }

    // POST /scan_directory missing path -> 400
    {
        auto r = cli.Post("/scan_directory", "{}", "application/json");
        TEST_ASSERT(r && r->status == 400, "POST /scan_directory missing path -> 400");
    }

    // ── POST /rebuild_index ──────────────────────────────────────────────────
    {
        auto r = cli.Post("/rebuild_index", "{\"index_type\":\"clip\"}", "application/json");
        TEST_ASSERT(r && r->status == 200, "POST /rebuild_index clip -> 200");
        if (r) {
            auto j = json::parse(r->body);
            TEST_ASSERT(j["success"] == true,          "POST /rebuild_index success=true");
            TEST_ASSERT(j["rebuilt"].contains("clip"), "POST /rebuild_index has clip count");
        }
    }

    {
        auto r = cli.Post("/rebuild_index", "{\"index_type\":\"wd14\"}", "application/json");
        TEST_ASSERT(r && r->status == 200, "POST /rebuild_index wd14 -> 200");
    }

    {
        auto r = cli.Post("/rebuild_index", "{\"index_type\":\"face\"}", "application/json");
        TEST_ASSERT(r && r->status == 200, "POST /rebuild_index face -> 200");
    }

    {
        auto r = cli.Post("/rebuild_index", "{\"index_type\":\"all\"}", "application/json");
        TEST_ASSERT(r && r->status == 200, "POST /rebuild_index all -> 200");
        if (r) {
            auto j = json::parse(r->body);
            TEST_ASSERT(j["rebuilt"].contains("clip"), "POST /rebuild_index all has clip");
            TEST_ASSERT(j["rebuilt"].contains("wd14"), "POST /rebuild_index all has wd14");
            TEST_ASSERT(j["rebuilt"].contains("face"), "POST /rebuild_index all has face");
        }
    }

    // ── POST /query_by_text ──────────────────────────────────────────────────
    {
        auto r = cli.Post("/query_by_text",
                          "{\"text\":\"beautiful landscape\",\"top_k\":5}",
                          "application/json");
        TEST_ASSERT(r != nullptr, "POST /query_by_text no crash");
        if (r) {
            // CLIP 可用 -> 200；不可用 -> 503
            TEST_ASSERT(r->status == 200 || r->status == 503,
                        "POST /query_by_text -> 200 or 503");
            if (r->status == 200) {
                auto j = json::parse(r->body);
                TEST_ASSERT(j.contains("results"), "POST /query_by_text has results");
                TEST_ASSERT(j.contains("query"),   "POST /query_by_text has query");
            }
        }
    }

    // POST /query_by_text missing text -> 400
    {
        auto r = cli.Post("/query_by_text", "{\"top_k\":5}", "application/json");
        TEST_ASSERT(r && r->status == 400, "POST /query_by_text missing text -> 400");
    }

    // ── POST /register_coser ─────────────────────────────────────────────────
    {
        std::string body = "{\"name\":\"TestCoser\",\"image_path\":\"" + P(g_img1) + "\"}";
        auto r = cli.Post("/register_coser", body, "application/json");
        TEST_ASSERT(r != nullptr, "POST /register_coser no crash");
        if (r) {
            // 无人脸/服务不可用 -> 503；成功 -> 200
            TEST_ASSERT(r->status == 200 || r->status == 503,
                        "POST /register_coser -> 200 or 503");
        }
    }

    // POST /register_coser missing fields -> 400
    {
        auto r = cli.Post("/register_coser", "{\"name\":\"X\"}", "application/json");
        TEST_ASSERT(r && r->status == 400, "POST /register_coser missing image_path -> 400");
    }

    // ── POST /identify_coser ─────────────────────────────────────────────────
    {
        std::string body = "{\"image_path\":\"" + P(g_img1) + "\"}";
        auto r = cli.Post("/identify_coser", body, "application/json");
        TEST_ASSERT(r && r->status == 200, "POST /identify_coser -> 200");
        if (r) {
            auto j = json::parse(r->body);
            TEST_ASSERT(j.contains("results"),       "POST /identify_coser has results");
            TEST_ASSERT(j["results"].is_array(),     "POST /identify_coser results is array");
        }
    }

    // POST /identify_coser missing image_path -> 400
    {
        auto r = cli.Post("/identify_coser", "{}", "application/json");
        TEST_ASSERT(r && r->status == 400, "POST /identify_coser missing image_path -> 400");
    }

    // ── 停止服务器 ────────────────────────────────────────────────────────────
    ImageSearch_StopHttpServer();
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    TEST_ASSERT(true, "StopHttpServer no crash");

    // 幂等停止
    ImageSearch_StopHttpServer();
    TEST_ASSERT(true, "Double StopHttpServer no crash");

    // 停止后请求应失败
    {
        auto r = cli.Get("/health");
        TEST_ASSERT(!r || r->status != 200, "GET /health after stop -> fails");
    }
}

// ─── Section 10: 边界条件 ────────────────────────────────────────────────────

static void Test_EdgeCases() {
    TEST_SECTION("Section 10: Edge Cases");

    char buf[4096] = {};
    int ret;

    // 未初始化时调用
    ImageSearch_Shutdown();

    ret = ImageSearch_Query("{}", buf, sizeof(buf));
    TEST_ASSERT(ret == -1, "Query without Init -> -1");

    ret = ImageSearch_AddImage("{\"path\":\"x.jpg\"}");
    TEST_ASSERT(ret == -1, "AddImage without Init -> -1");

    ret = ImageSearch_AddImageBatch("{\"path\":\"x.jpg\"}");
    TEST_ASSERT(ret == -1, "AddImageBatch without Init -> -1");

    ret = ImageSearch_RebuildIndex("clip");
    TEST_ASSERT(ret == -1, "RebuildIndex without Init -> -1");

    ret = ImageSearch_StartHttpServer(18094);
    TEST_ASSERT(ret == -1, "StartHttpServer without Init -> -1");

    ret = ImageSearch_ScanDirectory(".");
    TEST_ASSERT(ret == -1, "ScanDirectory without Init -> -1");

    // 重新初始化
    ret = ImageSearch_Init(g_db.c_str());
    TEST_ASSERT(ret == 0, "Re-Init after edge cases -> 0");

    // 极大 top_k
    ret = ImageSearch_Query("{\"top_k\":99999}", buf, sizeof(buf));
    TEST_ASSERT(ret >= 0, "Query top_k=99999 -> ok");

    // 空 JSON 对象
    ret = ImageSearch_Query("{}", buf, sizeof(buf));
    TEST_ASSERT(ret >= 0, "Query empty JSON -> ok");

    // 超小缓冲区（截断，不崩溃）
    char tiny[4] = {};
    ret = ImageSearch_Query("{\"top_k\":1}", tiny, sizeof(tiny));
    TEST_ASSERT(ret > 0, "Query tiny buffer -> truncated no crash");
}

// ─── main ─────────────────────────────────────────────────────────────────────

int main() {
    // 禁用 stdout 缓冲，确保输出在挂起时也能看到
    setvbuf(stdout, nullptr, _IONBF, 0);

    printf("========================================\n");
    printf("  ImageSearch DLL Full Test\n");
    printf("========================================\n");

    SetupEnv();

    Test_Lifecycle();
    Test_AddImage();
    Test_Query();
    Test_AddImageBatch();
    Test_ScanDirectory();
    Test_RebuildIndex();
    Test_QueryByText();
    Test_CoserOperations();
    Test_HttpServer();
    Test_EdgeCases();

    ImageSearch_Shutdown();
    CleanupEnv();

    printf("\n========================================\n");
    printf("  Results: %d passed, %d failed\n", g_pass, g_fail);
    printf("========================================\n");

    return g_fail > 0 ? 1 : 0;
}
