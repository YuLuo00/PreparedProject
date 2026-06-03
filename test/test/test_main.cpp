/**
 * ImageSearch DLL 功能测试
 * 不依赖 Python 微服务，直接测试 C++ 层功能
 *
 * 测试覆盖：
 *   1. Database - 建表/插入/查询/标签/Faiss映射
 *   2. FaissIndex - 添加向量/搜索/重置/保存加载
 *   3. DLL API - Init/Query/AddImage(无特征)/ScanDir/HTTP Server
 *   4. HTTP Server - 启动/健康检查/结构化查询
 */

#include "../src/Database.h"
#include "../src/FaissIndex.h"
#include "../src/ImageSearch.h"
#include <httplib.h>
#include <nlohmann/json.hpp>

#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <random>
#include <string>
#include <vector>

using json = nlohmann::json;
namespace fs = std::filesystem;

// ─── 测试框架 ─────────────────────────────────────────────────────────────────

static int g_pass = 0, g_fail = 0;

#define TEST_ASSERT(cond, msg)                                          \
    do {                                                                \
        if (cond) {                                                     \
            printf("  [PASS] %s\n", msg);                              \
            ++g_pass;                                                   \
        } else {                                                        \
            printf("  [FAIL] %s  (line %d)\n", msg, __LINE__);        \
            ++g_fail;                                                   \
        }                                                               \
    } while (0)

#define TEST_SECTION(name) printf("\n=== %s ===\n", name)

// ─── 辅助：生成随机归一化向量 ─────────────────────────────────────────────────

static std::vector<float> RandomVec(int dim, unsigned seed = 42) {
    std::mt19937 rng(seed);
    std::normal_distribution<float> dist(0.f, 1.f);
    std::vector<float> v(dim);
    float norm = 0.f;
    for (auto& x : v) { x = dist(rng); norm += x * x; }
    norm = std::sqrt(norm);
    for (auto& x : v) x /= norm;
    return v;
}

// ─── 测试目录（临时） ─────────────────────────────────────────────────────────

static const std::string TEST_DIR  = "./test_tmp";
static const std::string DB_PATH   = TEST_DIR + "/test.db";
static const std::string IDX_DIR   = TEST_DIR + "/faiss";
static const std::string SCAN_DIR  = TEST_DIR + "/images";

static void SetupTestDir() {
    fs::create_directories(TEST_DIR);
    fs::create_directories(IDX_DIR);
    fs::create_directories(SCAN_DIR);
    // 创建几个假 jpg 文件（空文件，仅测试路径扫描）
    for (int i = 1; i <= 3; ++i) {
        std::string p = SCAN_DIR + "/photo_" + std::to_string(i) + ".jpg";
        FILE* f = fopen(p.c_str(), "wb");
        if (f) { fwrite("FAKE", 1, 4, f); fclose(f); }
    }
    // 一个非 jpg 文件（应被忽略）
    FILE* f = fopen((SCAN_DIR + "/readme.txt").c_str(), "w");
    if (f) { fputs("ignore me", f); fclose(f); }
}

static void CleanupTestDir() {
    fs::remove_all(TEST_DIR);
}

// ─── 1. Database 测试 ─────────────────────────────────────────────────────────

static void TestDatabase() {
    TEST_SECTION("Database");

    Database db;
    TEST_ASSERT(db.Open(DB_PATH), "Open database");
    TEST_ASSERT(db.IsOpen(), "IsOpen after open");

    // 插入图片
    ImageRecord rec;
    rec.path        = SCAN_DIR + "/photo_1.jpg";
    rec.filename    = "photo_1.jpg";
    rec.shoot_date  = "2024-06-01";
    rec.rating      = 4.5;
    rec.path_prefix = SCAN_DIR;
    rec.scene_tags  = "[\"outdoor\",\"nature\"]";

    int64_t id1 = db.InsertImage(rec);
    TEST_ASSERT(id1 > 0, "InsertImage returns valid id");

    // 重复插入应返回相同 id（INSERT OR IGNORE）
    int64_t id1b = db.InsertImage(rec);
    TEST_ASSERT(id1b == id1, "Duplicate insert returns same id");

    // 插入第二张
    rec.path     = SCAN_DIR + "/photo_2.jpg";
    rec.filename = "photo_2.jpg";
    rec.rating   = 3.0;
    int64_t id2 = db.InsertImage(rec);
    TEST_ASSERT(id2 > 0 && id2 != id1, "Second image gets different id");

    // ImageExists
    TEST_ASSERT(db.ImageExists(SCAN_DIR + "/photo_1.jpg"), "ImageExists true");
    TEST_ASSERT(!db.ImageExists("/nonexistent/path.jpg"), "ImageExists false");

    // GetImageById
    auto opt = db.GetImageById(id1);
    TEST_ASSERT(opt.has_value(), "GetImageById found");
    TEST_ASSERT(opt->filename == "photo_1.jpg", "GetImageById correct filename");
    TEST_ASSERT(std::abs(opt->rating - 4.5) < 0.001, "GetImageById correct rating");

    // UpdateImageFeatures
    auto clipVec = RandomVec(768, 1);
    auto wd14Vec = RandomVec(1024, 2);
    TEST_ASSERT(db.UpdateImageFeatures(id1, clipVec, wd14Vec, 0, 0),
                "UpdateImageFeatures");

    // InsertTags / GetTags
    std::vector<std::pair<std::string, float>> tags = {
        {"1girl", 0.98f}, {"cosplay", 0.87f}, {"smile", 0.75f}
    };
    TEST_ASSERT(db.InsertTags(id1, tags), "InsertTags");
    auto gotTags = db.GetTags(id1);
    TEST_ASSERT(gotTags.size() == 3, "GetTags count");
    TEST_ASSERT(gotTags[0].first == "1girl", "GetTags first tag");

    // QueryImages - 无过滤
    QueryFilter filter;
    filter.top_k = 10;
    auto results = db.QueryImages(filter);
    TEST_ASSERT(results.size() == 2, "QueryImages all");

    // QueryImages - 评分过滤
    filter.rating_min = 4.0;
    results = db.QueryImages(filter);
    TEST_ASSERT(results.size() == 1, "QueryImages rating_min filter");
    TEST_ASSERT(results[0].filename == "photo_1.jpg", "QueryImages rating result");

    // QueryImages - 标签过滤
    filter.rating_min = 0.0;
    filter.tags = {"cosplay"};
    results = db.QueryImages(filter);
    TEST_ASSERT(results.size() == 1, "QueryImages tag filter");

    // Faiss ID 映射
    int64_t fid = db.GetNextFaissId("clip");
    TEST_ASSERT(fid == 0, "GetNextFaissId first = 0");
    int64_t fid2 = db.GetNextFaissId("clip");
    TEST_ASSERT(fid2 == 1, "GetNextFaissId second = 1");

    TEST_ASSERT(db.InsertFaissMapping(0, id1, "clip"), "InsertFaissMapping");
    int64_t eid = db.GetEntityIdByFaissId(0, "clip");
    TEST_ASSERT(eid == id1, "GetEntityIdByFaissId");

    // LoadAllVectors
    std::vector<int64_t> faissIds;
    std::vector<std::vector<float>> vecs;
    TEST_ASSERT(db.LoadAllVectors("clip", faissIds, vecs), "LoadAllVectors");
    TEST_ASSERT(faissIds.size() == 1, "LoadAllVectors count");
    TEST_ASSERT(vecs[0].size() == 768, "LoadAllVectors dim");

    // Coser 人脸
    CoserFace face;
    face.coser_name      = "TestCoser";
    face.reference_image = SCAN_DIR + "/photo_1.jpg";
    face.face_vector     = RandomVec(512, 3);
    int64_t faceId = db.InsertCoserFace(face);
    TEST_ASSERT(faceId > 0, "InsertCoserFace");

    auto allFaces = db.GetAllCoserFaces();
    TEST_ASSERT(allFaces.size() == 1, "GetAllCoserFaces count");
    TEST_ASSERT(allFaces[0].coser_name == "TestCoser", "GetAllCoserFaces name");
    TEST_ASSERT(allFaces[0].face_vector.size() == 512, "GetAllCoserFaces dim");

    db.Close();
    TEST_ASSERT(!db.IsOpen(), "Close database");
}

// ─── 2. FaissIndex 测试 ───────────────────────────────────────────────────────

static void TestFaissIndex() {
    TEST_SECTION("FaissIndex");

    const int DIM = 128;
    FaissIndex idx(DIM);

    // 加载（文件不存在，应创建空索引）
    std::string idxPath = IDX_DIR + "/test.index";
    TEST_ASSERT(idx.Load(idxPath), "Load (new index)");
    TEST_ASSERT(idx.GetCount() == 0, "Empty index count = 0");

    // 添加向量
    auto v1 = RandomVec(DIM, 10);
    auto v2 = RandomVec(DIM, 20);
    auto v3 = RandomVec(DIM, 30);

    TEST_ASSERT(idx.AddVector(v1, 100), "AddVector id=100");
    TEST_ASSERT(idx.AddVector(v2, 200), "AddVector id=200");
    TEST_ASSERT(idx.AddVector(v3, 300), "AddVector id=300");
    TEST_ASSERT(idx.GetCount() == 3, "Count after 3 adds");

    // 搜索：查询 v1 应该找到自己（相似度最高）
    std::vector<int64_t> ids;
    std::vector<float> scores;
    TEST_ASSERT(idx.Search(v1, 3, ids, scores), "Search returns true");
    TEST_ASSERT(ids.size() == 3, "Search returns 3 results");
    TEST_ASSERT(ids[0] == 100, "Search top-1 is v1 itself");
    TEST_ASSERT(scores[0] > 0.99f, "Search top-1 score near 1.0");

    // 批量添加
    FaissIndex idx2(DIM);
    idx2.Load(IDX_DIR + "/test2.index");
    std::vector<std::vector<float>> vecs = {v1, v2, v3};
    std::vector<int64_t> batchIds = {1, 2, 3};
    TEST_ASSERT(idx2.AddVectors(vecs, batchIds), "AddVectors batch");
    TEST_ASSERT(idx2.GetCount() == 3, "Count after batch add");

    // 保存并重新加载
    TEST_ASSERT(idx.Save(idxPath), "Save index");
    FaissIndex idx3(DIM);
    TEST_ASSERT(idx3.Load(idxPath), "Load saved index");
    TEST_ASSERT(idx3.GetCount() == 3, "Loaded index count = 3");

    // 重新搜索
    ids.clear(); scores.clear();
    TEST_ASSERT(idx3.Search(v1, 1, ids, scores), "Search on loaded index");
    TEST_ASSERT(ids[0] == 100, "Loaded index search correct");

    // 重置
    idx.Reset();
    TEST_ASSERT(idx.GetCount() == 0, "Reset clears index");

    // FaissIndexManager
    FaissIndexManager mgr;
    TEST_ASSERT(mgr.Init(IDX_DIR), "FaissIndexManager Init");
    TEST_ASSERT(mgr.GetClipIndex() != nullptr, "GetClipIndex not null");
    TEST_ASSERT(mgr.GetWd14Index() != nullptr, "GetWd14Index not null");
    TEST_ASSERT(mgr.GetFaceIndex() != nullptr, "GetFaceIndex not null");

    // 添加到 clip 索引
    auto clipVec = RandomVec(FaissIndexManager::CLIP_DIM, 42);
    TEST_ASSERT(mgr.GetClipIndex()->AddVector(clipVec, 999), "Manager clip AddVector");
    TEST_ASSERT(mgr.GetClipIndex()->GetCount() == 1, "Manager clip count");

    TEST_ASSERT(mgr.SaveAll(), "FaissIndexManager SaveAll");
}

// ─── 3. DLL API 测试 ──────────────────────────────────────────────────────────

static void TestDllApi() {
    TEST_SECTION("DLL API");

    // 清理旧数据库
    fs::remove(DB_PATH);
    fs::remove_all(IDX_DIR);

    // Init
    int ret = ImageSearch_Init(DB_PATH.c_str());
    TEST_ASSERT(ret == 0, "ImageSearch_Init returns 0");

    // 重复 Init（应该成功，重用已有 DB）
    ret = ImageSearch_Init(DB_PATH.c_str());
    TEST_ASSERT(ret == 0, "ImageSearch_Init second call ok");

    // Query（空库）
    char buf[4096] = {};
    ret = ImageSearch_Query("{\"top_k\":10}", buf, sizeof(buf));
    TEST_ASSERT(ret >= 0, "ImageSearch_Query on empty db");
    {
        auto j = json::parse(buf);
        TEST_ASSERT(j.is_array(), "Query result is JSON array");
        TEST_ASSERT(j.empty(), "Query result empty on empty db");
    }

    // AddImage（文件不存在，应返回 -2）
    ret = ImageSearch_AddImage("{\"path\":\"/nonexistent/photo.jpg\"}");
    TEST_ASSERT(ret == -2, "AddImage nonexistent file returns -2");

    // AddImage（文件存在，Python 服务不可用，但应入库）
    std::string imgPath = SCAN_DIR + "/photo_1.jpg";
    std::string addJson = "{\"path\":\"" + imgPath + "\",\"rating\":4.5,\"shoot_date\":\"2024-06-01\"}";
    ret = ImageSearch_AddImage(addJson.c_str());
    // Python 服务不可用时，特征提取失败但图片仍应入库（返回 0）
    TEST_ASSERT(ret == 0, "AddImage with no Python service still inserts to DB");

    // Query 应该找到刚插入的图片
    memset(buf, 0, sizeof(buf));
    ret = ImageSearch_Query("{\"top_k\":10}", buf, sizeof(buf));
    TEST_ASSERT(ret > 0, "Query after AddImage returns data");
    {
        auto j = json::parse(buf);
        TEST_ASSERT(j.size() == 1, "Query finds 1 image");
        TEST_ASSERT(j[0]["filename"] == "photo_1.jpg", "Query correct filename");
        TEST_ASSERT(std::abs(j[0]["rating"].get<double>() - 4.5) < 0.001,
                    "Query correct rating");
    }

    // AddImageBatch（多行 JSON）
    std::string batchLines =
        "{\"path\":\"" + SCAN_DIR + "/photo_2.jpg\",\"rating\":3.0}\n"
        "{\"path\":\"" + SCAN_DIR + "/photo_3.jpg\",\"rating\":2.5}\n"
        "{\"path\":\"/bad/path.jpg\"}\n";  // 这行应该失败
    ret = ImageSearch_AddImageBatch(batchLines.c_str());
    TEST_ASSERT(ret == 2, "AddImageBatch returns 2 (2 success, 1 fail)");

    // Query 应该找到 3 张图片
    memset(buf, 0, sizeof(buf));
    ret = ImageSearch_Query("{\"top_k\":10}", buf, sizeof(buf));
    {
        auto j = json::parse(buf);
        TEST_ASSERT(j.size() == 3, "Query finds 3 images after batch");
    }

    // Query 带评分过滤
    memset(buf, 0, sizeof(buf));
    ret = ImageSearch_Query("{\"rating_min\":3.0,\"top_k\":10}", buf, sizeof(buf));
    {
        auto j = json::parse(buf);
        TEST_ASSERT(j.size() == 2, "Query rating_min=3.0 finds 2 images");
    }

    // ScanDirectory（3 个 jpg + 1 个 txt，photo_1 已存在应跳过）
    ret = ImageSearch_ScanDirectory(SCAN_DIR.c_str());
    TEST_ASSERT(ret == 0, "ScanDirectory skips existing, adds 0 new");

    // QueryByText（Python 服务不可用，应返回 -2）
    memset(buf, 0, sizeof(buf));
    ret = ImageSearch_QueryByText("cosplay girl", nullptr, 5, buf, sizeof(buf));
    TEST_ASSERT(ret == -2, "QueryByText without CLIP service returns -2");

    // RebuildIndex（从 SQLite 重建，无向量数据时应成功）
    ret = ImageSearch_RebuildIndex(nullptr);
    TEST_ASSERT(ret == 0, "RebuildIndex all returns 0");

    ret = ImageSearch_RebuildIndex("clip");
    TEST_ASSERT(ret == 0, "RebuildIndex clip returns 0");

    // Shutdown
    ImageSearch_Shutdown();
    TEST_ASSERT(true, "ImageSearch_Shutdown no crash");
}

// ─── 4. HTTP Server 测试 ──────────────────────────────────────────────────────

static void TestHttpServer() {
    TEST_SECTION("HTTP Server");

    // 重新初始化
    int ret = ImageSearch_Init(DB_PATH.c_str());
    TEST_ASSERT(ret == 0, "Re-Init for HTTP test");

    // 启动 HTTP 服务
    ret = ImageSearch_StartHttpServer(18080);
    TEST_ASSERT(ret == 0, "StartHttpServer returns 0");

    // 等待服务器启动
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // 健康检查
    {
        httplib::Client cli("127.0.0.1", 18080);
        cli.set_connection_timeout(3);
        auto res = cli.Get("/health");
        TEST_ASSERT(res && res->status == 200, "GET /health returns 200");
        if (res) {
            auto j = json::parse(res->body);
            TEST_ASSERT(j["status"] == "ok", "GET /health status=ok");
        }
    }

    // POST /query
    {
        httplib::Client cli("127.0.0.1", 18080);
        cli.set_connection_timeout(3);
        json body = {{"top_k", 10}};
        auto res = cli.Post("/query", body.dump(), "application/json");
        TEST_ASSERT(res && res->status == 200, "POST /query returns 200");
        if (res) {
            auto j = json::parse(res->body);
            TEST_ASSERT(j.contains("results"), "POST /query has results key");
            TEST_ASSERT(j["count"].get<int>() == 3, "POST /query count=3");
        }
    }

    // POST /query 带评分过滤
    {
        httplib::Client cli("127.0.0.1", 18080);
        cli.set_connection_timeout(3);
        json body = {{"rating_min", 4.0}, {"top_k", 10}};
        auto res = cli.Post("/query", body.dump(), "application/json");
        TEST_ASSERT(res && res->status == 200, "POST /query rating_min=4.0 returns 200");
        if (res) {
            auto j = json::parse(res->body);
            TEST_ASSERT(j["count"].get<int>() == 1, "POST /query rating_min=4.0 count=1");
        }
    }

    // POST /stats
    {
        httplib::Client cli("127.0.0.1", 18080);
        cli.set_connection_timeout(3);
        auto res = cli.Get("/stats");
        TEST_ASSERT(res && res->status == 200, "GET /stats returns 200");
        if (res) {
            auto j = json::parse(res->body);
            TEST_ASSERT(j.contains("clip_index_count"), "GET /stats has clip_index_count");
        }
    }

    // POST /add_image（文件不存在）
    {
        httplib::Client cli("127.0.0.1", 18080);
        cli.set_connection_timeout(3);
        json body = {{"path", "/nonexistent/photo.jpg"}};
        auto res = cli.Post("/add_image", body.dump(), "application/json");
        TEST_ASSERT(res && res->status == 400, "POST /add_image nonexistent returns 400");
    }

    // POST /rebuild_index
    {
        httplib::Client cli("127.0.0.1", 18080);
        cli.set_connection_timeout(3);
        json body = {{"index_type", "all"}};
        auto res = cli.Post("/rebuild_index", body.dump(), "application/json");
        TEST_ASSERT(res && res->status == 200, "POST /rebuild_index returns 200");
        if (res) {
            auto j = json::parse(res->body);
            TEST_ASSERT(j["success"] == true, "POST /rebuild_index success=true");
        }
    }

    // 停止服务器
    ImageSearch_StopHttpServer();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 停止后应无法连接
    {
        httplib::Client cli("127.0.0.1", 18080);
        cli.set_connection_timeout(1);
        auto res = cli.Get("/health");
        TEST_ASSERT(!res || res->status != 200, "Server stopped, health check fails");
    }

    ImageSearch_Shutdown();
    TEST_ASSERT(true, "Final Shutdown no crash");
}

// ─── main ─────────────────────────────────────────────────────────────────────

int main() {
    printf("========================================\n");
    printf("  ImageSearch DLL Test Suite\n");
    printf("========================================\n");

    SetupTestDir();

    TestDatabase();
    TestFaissIndex();
    TestDllApi();
    TestHttpServer();

    CleanupTestDir();

    printf("\n========================================\n");
    printf("  Results: %d passed, %d failed\n", g_pass, g_fail);
    printf("========================================\n");

    return g_fail > 0 ? 1 : 0;
}
