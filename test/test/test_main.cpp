/**
 * 从 FaissDemoTest 逐步扩展，找到崩溃点
 * 每步只增加一小块功能，编译运行验证
 */

#include <faiss/IndexFlat.h>
#include <faiss/IndexIDMap.h>
#include <faiss/index_io.h>

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <mutex>
#include <string>
#include <vector>

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

// ─── Step 1: FaissDemoTest（已验证通过）─────────────────────────────────────

static void Step1_FaissDemoTest() {
    TEST_SECTION("Step1: Faiss Native Demo");

    const int dim = 8;
    const std::string idxPath = "./test_step1.index";

    // 存
    {
        faiss::IndexFlatIP flat(dim);
        faiss::IndexIDMap idxmap(&flat);
        std::vector<float> vecs = {
            1,0,0,0,0,0,0,0,
            0,1,0,0,0,0,0,0,
            0,0,1,0,0,0,0,0
        };
        std::vector<int64_t> ids = {100, 200, 300};
        idxmap.add_with_ids(3, vecs.data(), ids.data());
        TEST_ASSERT(idxmap.ntotal == 3, "ntotal=3 after add");
        faiss::write_index(&idxmap, idxPath.c_str());
        TEST_ASSERT(fs::exists(idxPath), "index file saved");
    }

    // 读
    {
        faiss::Index* loaded = faiss::read_index(idxPath.c_str());
        TEST_ASSERT(loaded != nullptr, "loaded not null");
        TEST_ASSERT(loaded->ntotal == 3, "loaded ntotal=3");
        std::vector<float> query = {1,0,0,0,0,0,0,0};
        std::vector<int64_t> outIds(1, -1);
        std::vector<float> outScores(1, 0.f);
        loaded->search(1, query.data(), 1, outScores.data(), outIds.data());
        TEST_ASSERT(outIds[0] == 100, "search top-1 id=100");
        TEST_ASSERT(outScores[0] > 0.99f, "search score near 1.0");
        // 不 delete loaded
    }

    fs::remove(idxPath);
}

// ─── Step 2: 简单 FaissIndex 包装类（不依赖任何之前的代码）─────────────────

class SimpleFaissIndex {
public:
    explicit SimpleFaissIndex(int dim) : dim_(dim), count_(0), index_(nullptr) {}
    ~SimpleFaissIndex() { /* 不 delete index_ */ }

    bool Load(const std::string& path) {
        path_ = path;
        if (fs::exists(path)) {
            faiss::Index* loaded = faiss::read_index(path.c_str());
            index_ = dynamic_cast<faiss::IndexIDMap*>(loaded);
            count_ = index_ ? index_->ntotal : 0;
            return index_ != nullptr;
        }
        // 创建空索引
        faiss::IndexFlatIP flat(dim_);
        faiss::IndexIDMap idxmap(&flat);
        fs::path p(path);
        if (p.has_parent_path()) fs::create_directories(p.parent_path());
        faiss::write_index(&idxmap, path.c_str());
        faiss::Index* loaded = faiss::read_index(path.c_str());
        index_ = dynamic_cast<faiss::IndexIDMap*>(loaded);
        count_ = 0;
        return index_ != nullptr;
    }

    bool Save() {
        if (!index_ || path_.empty()) return false;
        faiss::write_index(index_, path_.c_str());
        return true;
    }

    bool AddVector(const std::vector<float>& vec, int64_t id) {
        if (!index_ || (int)vec.size() != dim_) return false;
        std::vector<float> n = vec;
        L2Normalize(n);
        index_->add_with_ids(1, n.data(), &id);
        ++count_;
        return true;
    }

    bool Search(const std::vector<float>& query, int topK,
                std::vector<int64_t>& outIds, std::vector<float>& outScores) {
        if (!index_ || count_ == 0) { outIds.clear(); outScores.clear(); return true; }
        std::vector<float> n = query;
        L2Normalize(n);
        int k = std::min(topK, (int)count_);
        outIds.resize(k, -1);
        outScores.resize(k, 0.f);
        index_->search(1, n.data(), k, outScores.data(), outIds.data());
        return true;
    }

    int64_t GetCount() const { return count_; }

private:
    static void L2Normalize(std::vector<float>& v) {
        float norm = 0.f;
        for (float x : v) norm += x * x;
        norm = std::sqrt(norm);
        if (norm > 1e-10f) for (float& x : v) x /= norm;
    }

    int dim_;
    int64_t count_;
    faiss::IndexIDMap* index_;
    std::string path_;
};

static void Step2_SimpleFaissIndex() {
    TEST_SECTION("Step2: SimpleFaissIndex wrapper");

    const int DIM = 8;
    const std::string idxPath = "./test_step2.index";
    fs::remove(idxPath);

    SimpleFaissIndex idx(DIM);
    TEST_ASSERT(idx.Load(idxPath), "Load (new index)");
    TEST_ASSERT(idx.GetCount() == 0, "Empty count=0");

    std::vector<float> v1 = {1,0,0,0,0,0,0,0};
    std::vector<float> v2 = {0,1,0,0,0,0,0,0};
    std::vector<float> v3 = {0,0,1,0,0,0,0,0};

    TEST_ASSERT(idx.AddVector(v1, 100), "AddVector 100");
    TEST_ASSERT(idx.AddVector(v2, 200), "AddVector 200");
    TEST_ASSERT(idx.AddVector(v3, 300), "AddVector 300");
    TEST_ASSERT(idx.GetCount() == 3, "Count=3");

    std::vector<int64_t> ids;
    std::vector<float> scores;
    TEST_ASSERT(idx.Search(v1, 3, ids, scores), "Search ok");
    TEST_ASSERT(ids.size() == 3, "Search returns 3");
    TEST_ASSERT(ids[0] == 100, "Top-1 is 100");
    TEST_ASSERT(scores[0] > 0.99f, "Score near 1.0");

    TEST_ASSERT(idx.Save(), "Save ok");

    // 重新加载
    SimpleFaissIndex idx2(DIM);
    TEST_ASSERT(idx2.Load(idxPath), "Reload ok");
    TEST_ASSERT(idx2.GetCount() == 3, "Reloaded count=3");

    ids.clear(); scores.clear();
    TEST_ASSERT(idx2.Search(v1, 1, ids, scores), "Search after reload");
    TEST_ASSERT(ids[0] == 100, "Reload search top-1=100");

    fs::remove(idxPath);
}

// ─── Step 3: 加入 mutex（线程安全）─────────────────────────────────────────

class ThreadSafeFaissIndex {
public:
    explicit ThreadSafeFaissIndex(int dim) : dim_(dim), count_(0), index_(nullptr) {}
    ~ThreadSafeFaissIndex() { /* 不 delete index_ */ }

    bool Load(const std::string& path) {
        std::lock_guard<std::mutex> lock(mutex_);
        path_ = path;
        if (fs::exists(path)) {
            faiss::Index* loaded = faiss::read_index(path.c_str());
            index_ = dynamic_cast<faiss::IndexIDMap*>(loaded);
            count_ = index_ ? index_->ntotal : 0;
            return index_ != nullptr;
        }
        faiss::IndexFlatIP flat(dim_);
        faiss::IndexIDMap idxmap(&flat);
        fs::path p(path);
        if (p.has_parent_path()) fs::create_directories(p.parent_path());
        faiss::write_index(&idxmap, path.c_str());
        faiss::Index* loaded = faiss::read_index(path.c_str());
        index_ = dynamic_cast<faiss::IndexIDMap*>(loaded);
        count_ = 0;
        return index_ != nullptr;
    }

    bool Save() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!index_ || path_.empty()) return false;
        faiss::write_index(index_, path_.c_str());
        return true;
    }

    bool AddVector(const std::vector<float>& vec, int64_t id) {
        if (!index_ || (int)vec.size() != dim_) return false;
        std::vector<float> n = vec;
        L2Normalize(n);
        std::lock_guard<std::mutex> lock(mutex_);
        index_->add_with_ids(1, n.data(), &id);
        ++count_;
        return true;
    }

    bool Search(const std::vector<float>& query, int topK,
                std::vector<int64_t>& outIds, std::vector<float>& outScores) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!index_ || count_ == 0) { outIds.clear(); outScores.clear(); return true; }
        std::vector<float> n = query;
        L2Normalize(n);
        int k = std::min(topK, (int)count_);
        outIds.resize(k, -1);
        outScores.resize(k, 0.f);
        index_->search(1, n.data(), k, outScores.data(), outIds.data());
        return true;
    }

    int64_t GetCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return count_;
    }

private:
    static void L2Normalize(std::vector<float>& v) {
        float norm = 0.f;
        for (float x : v) norm += x * x;
        norm = std::sqrt(norm);
        if (norm > 1e-10f) for (float& x : v) x /= norm;
    }

    int dim_;
    int64_t count_;
    faiss::IndexIDMap* index_;
    std::string path_;
    mutable std::mutex mutex_;
};

static void Step3_ThreadSafeFaissIndex() {
    TEST_SECTION("Step3: ThreadSafeFaissIndex (with mutex)");

    const int DIM = 8;
    const std::string idxPath = "./test_step3.index";
    fs::remove(idxPath);

    ThreadSafeFaissIndex idx(DIM);
    TEST_ASSERT(idx.Load(idxPath), "Load ok");
    TEST_ASSERT(idx.GetCount() == 0, "Empty count=0");

    std::vector<float> v1 = {1,0,0,0,0,0,0,0};
    std::vector<float> v2 = {0,1,0,0,0,0,0,0};

    TEST_ASSERT(idx.AddVector(v1, 100), "AddVector 100");
    TEST_ASSERT(idx.AddVector(v2, 200), "AddVector 200");
    TEST_ASSERT(idx.GetCount() == 2, "Count=2");

    std::vector<int64_t> ids;
    std::vector<float> scores;
    TEST_ASSERT(idx.Search(v1, 2, ids, scores), "Search ok");
    TEST_ASSERT(ids[0] == 100, "Top-1=100");

    TEST_ASSERT(idx.Save(), "Save ok");

    fs::remove(idxPath);
}

// ─── Step 4: 三个索引管理器（Manager）─────────────────────────────────────

class SimpleIndexManager {
public:
    static constexpr int CLIP_DIM = 768;
    static constexpr int WD14_DIM = 1024;
    static constexpr int FACE_DIM = 512;

    SimpleIndexManager()
        : clipIdx_(CLIP_DIM), wd14Idx_(WD14_DIM), faceIdx_(FACE_DIM) {}

    bool Init(const std::string& dir) {
        dir_ = dir;
        fs::create_directories(dir);
        bool ok = true;
        ok &= clipIdx_.Load(dir + "/clip.index");
        ok &= wd14Idx_.Load(dir + "/wd14.index");
        ok &= faceIdx_.Load(dir + "/face.index");
        return ok;
    }

    bool SaveAll() {
        bool ok = true;
        ok &= clipIdx_.Save();
        ok &= wd14Idx_.Save();
        ok &= faceIdx_.Save();
        return ok;
    }

    ThreadSafeFaissIndex& Clip() { return clipIdx_; }
    ThreadSafeFaissIndex& Wd14() { return wd14Idx_; }
    ThreadSafeFaissIndex& Face() { return faceIdx_; }

private:
    std::string dir_;
    ThreadSafeFaissIndex clipIdx_;
    ThreadSafeFaissIndex wd14Idx_;
    ThreadSafeFaissIndex faceIdx_;
};

static void Step4_IndexManager() {
    TEST_SECTION("Step4: SimpleIndexManager (3 indices)");

    const std::string dir = "./test_step4_idx";
    fs::remove_all(dir);

    SimpleIndexManager mgr;
    TEST_ASSERT(mgr.Init(dir), "Init ok");
    TEST_ASSERT(mgr.Clip().GetCount() == 0, "Clip empty");
    TEST_ASSERT(mgr.Wd14().GetCount() == 0, "Wd14 empty");
    TEST_ASSERT(mgr.Face().GetCount() == 0, "Face empty");

    // 添加一个 clip 向量
    std::vector<float> clipVec(SimpleIndexManager::CLIP_DIM, 0.f);
    clipVec[0] = 1.f;
    TEST_ASSERT(mgr.Clip().AddVector(clipVec, 999), "Clip AddVector");
    TEST_ASSERT(mgr.Clip().GetCount() == 1, "Clip count=1");

    TEST_ASSERT(mgr.SaveAll(), "SaveAll ok");

    // 重新加载
    SimpleIndexManager mgr2;
    TEST_ASSERT(mgr2.Init(dir), "Re-Init ok");
    TEST_ASSERT(mgr2.Clip().GetCount() == 1, "Reloaded Clip count=1");

    std::vector<int64_t> ids;
    std::vector<float> scores;
    TEST_ASSERT(mgr2.Clip().Search(clipVec, 1, ids, scores), "Search ok");
    TEST_ASSERT(ids[0] == 999, "Search top-1=999");

    fs::remove_all(dir);
}

// ─── Step 5: 用 unique_ptr 管理（模拟 FaissIndexManager 结构）──────────────

#include <memory>

class UniqueIndexManager {
public:
    static constexpr int CLIP_DIM = 768;
    static constexpr int WD14_DIM = 1024;
    static constexpr int FACE_DIM = 512;

    UniqueIndexManager()
        : clipIdx_(std::make_unique<ThreadSafeFaissIndex>(CLIP_DIM))
        , wd14Idx_(std::make_unique<ThreadSafeFaissIndex>(WD14_DIM))
        , faceIdx_(std::make_unique<ThreadSafeFaissIndex>(FACE_DIM)) {}

    bool Init(const std::string& dir) {
        dir_ = dir;
        fs::create_directories(dir);
        bool ok = true;
        ok &= clipIdx_->Load(dir + "/clip.index");
        ok &= wd14Idx_->Load(dir + "/wd14.index");
        ok &= faceIdx_->Load(dir + "/face.index");
        return ok;
    }

    bool SaveAll() {
        bool ok = true;
        ok &= clipIdx_->Save();
        ok &= wd14Idx_->Save();
        ok &= faceIdx_->Save();
        return ok;
    }

    ThreadSafeFaissIndex* Clip() { return clipIdx_.get(); }
    ThreadSafeFaissIndex* Wd14() { return wd14Idx_.get(); }
    ThreadSafeFaissIndex* Face() { return faceIdx_.get(); }

private:
    std::string dir_;
    std::unique_ptr<ThreadSafeFaissIndex> clipIdx_;
    std::unique_ptr<ThreadSafeFaissIndex> wd14Idx_;
    std::unique_ptr<ThreadSafeFaissIndex> faceIdx_;
};

static void Step5_UniqueIndexManager() {
    TEST_SECTION("Step5: UniqueIndexManager (unique_ptr)");

    const std::string dir = "./test_step5_idx";
    fs::remove_all(dir);

    UniqueIndexManager mgr;
    TEST_ASSERT(mgr.Init(dir), "Init ok");
    TEST_ASSERT(mgr.Clip()->GetCount() == 0, "Clip empty");
    TEST_ASSERT(mgr.Wd14()->GetCount() == 0, "Wd14 empty");
    TEST_ASSERT(mgr.Face()->GetCount() == 0, "Face empty");

    std::vector<float> clipVec(UniqueIndexManager::CLIP_DIM, 0.f);
    clipVec[0] = 1.f;
    TEST_ASSERT(mgr.Clip()->AddVector(clipVec, 999), "Clip AddVector");
    TEST_ASSERT(mgr.Clip()->GetCount() == 1, "Clip count=1");

    TEST_ASSERT(mgr.SaveAll(), "SaveAll ok");

    UniqueIndexManager mgr2;
    TEST_ASSERT(mgr2.Init(dir), "Re-Init ok");
    TEST_ASSERT(mgr2.Clip()->GetCount() == 1, "Reloaded Clip count=1");

    std::vector<int64_t> ids;
    std::vector<float> scores;
    TEST_ASSERT(mgr2.Clip()->Search(clipVec, 1, ids, scores), "Search ok");
    TEST_ASSERT(ids[0] == 999, "Search top-1=999");

    fs::remove_all(dir);
}

// ─── Step 6: 引入 Database（SQLite）─────────────────────────────────────────

#include "../src/Database.h"

static void Step6_Database() {
    TEST_SECTION("Step6: Database (SQLite)");

    const std::string dbPath = "./test_step6.db";
    fs::remove(dbPath);

    Database db;
    TEST_ASSERT(db.Open(dbPath), "Open DB");
    TEST_ASSERT(db.IsOpen(), "IsOpen");

    ImageRecord rec;
    rec.path = "./photo_1.jpg";
    rec.filename = "photo_1.jpg";
    rec.rating = 4.5;
    rec.path_prefix = ".";

    int64_t id = db.InsertImage(rec);
    TEST_ASSERT(id > 0, "InsertImage");

    auto opt = db.GetImageById(id);
    TEST_ASSERT(opt.has_value(), "GetImageById");
    TEST_ASSERT(opt->filename == "photo_1.jpg", "Correct filename");

    db.Close();
    TEST_ASSERT(!db.IsOpen(), "Closed");

    fs::remove(dbPath);
}

// ─── Step 7: Database + UniqueIndexManager 组合 ──────────────────────────────

static void Step7_DatabasePlusFaiss() {
    TEST_SECTION("Step7: Database + UniqueIndexManager");

    const std::string dbPath = "./test_step7.db";
    const std::string idxDir = "./test_step7_idx";
    fs::remove(dbPath);
    fs::remove_all(idxDir);

    // 初始化 DB
    Database db;
    TEST_ASSERT(db.Open(dbPath), "Open DB");

    // 初始化 Faiss
    UniqueIndexManager mgr;
    TEST_ASSERT(mgr.Init(idxDir), "Init Faiss");

    // 插入图片
    ImageRecord rec;
    rec.path = "./photo_1.jpg";
    rec.filename = "photo_1.jpg";
    rec.rating = 4.5;
    rec.path_prefix = ".";
    int64_t imageId = db.InsertImage(rec);
    TEST_ASSERT(imageId > 0, "InsertImage");

    // 添加 clip 向量
    std::vector<float> clipVec(UniqueIndexManager::CLIP_DIM, 0.f);
    clipVec[0] = 1.f;
    int64_t faissId = db.GetNextFaissId("clip");
    TEST_ASSERT(faissId == 0, "First faissId=0");
    TEST_ASSERT(mgr.Clip()->AddVector(clipVec, faissId), "AddVector");
    TEST_ASSERT(db.InsertFaissMapping(faissId, imageId, "clip"), "InsertFaissMapping");

    // 保存
    TEST_ASSERT(mgr.SaveAll(), "SaveAll");

    // 搜索
    std::vector<int64_t> ids;
    std::vector<float> scores;
    TEST_ASSERT(mgr.Clip()->Search(clipVec, 1, ids, scores), "Search");
    TEST_ASSERT(ids[0] == faissId, "Search faissId matches");

    int64_t entityId = db.GetEntityIdByFaissId(ids[0], "clip");
    TEST_ASSERT(entityId == imageId, "EntityId matches imageId");

    db.Close();
    fs::remove(dbPath);
    fs::remove_all(idxDir);
}

// ─── Step 8: 引入 HttpServer（httplib）──────────────────────────────────────

#include "../src/HttpServer.h"
#include <thread>
#include <chrono>
#include <httplib.h>
#include <nlohmann/json.hpp>

static void Step8_HttpServer() {
    TEST_SECTION("Step8: HttpServer (httplib)");

    const std::string dbPath = "./test_step8.db";
    const std::string idxDir = "./test_step8_idx";
    fs::remove(dbPath);
    fs::remove_all(idxDir);

    // 初始化 DB + Faiss
    auto db = std::make_unique<Database>();
    TEST_ASSERT(db->Open(dbPath), "Open DB");

    auto faiss = std::make_unique<UniqueIndexManager>();
    TEST_ASSERT(faiss->Init(idxDir), "Init Faiss");

    // 插入一张图片
    ImageRecord rec;
    rec.path = "./photo_1.jpg";
    rec.filename = "photo_1.jpg";
    rec.rating = 4.5;
    rec.path_prefix = ".";
    int64_t imageId = db->InsertImage(rec);
    TEST_ASSERT(imageId > 0, "InsertImage");

    // 注意：HttpServer 需要 FaissIndexManager 类型，这里先测试能否编译和启动
    // 由于 UniqueIndexManager 和 FaissIndexManager 不同，先只测试 DB 部分
    // 直接用 httplib 创建一个简单服务器测试
    {
        httplib::Server svr;
        svr.Get("/health", [](const httplib::Request&, httplib::Response& res) {
            res.set_content("{\"status\":\"ok\"}", "application/json");
        });

        std::thread t([&svr]() {
            svr.listen("127.0.0.1", 18090);
        });
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        httplib::Client cli("127.0.0.1", 18090);
        cli.set_connection_timeout(2);
        auto resp = cli.Get("/health");
        TEST_ASSERT(resp && resp->status == 200, "GET /health returns 200");

        svr.stop();
        if (t.joinable()) t.join();
    }

    db->Close();
    fs::remove(dbPath);
    fs::remove_all(idxDir);
}

// ─── Step 9: 引入 ImageSearch DLL API ────────────────────────────────────────

#include "../src/ImageSearch.h"
using json = nlohmann::json;

static void Step9_ImageSearchApi() {
    TEST_SECTION("Step9: ImageSearch DLL API");

    const std::string dbPath = "./test_step9.db";
    fs::remove(dbPath);
    fs::remove_all("./test_step9_idx");

    // Init
    int ret = ImageSearch_Init(dbPath.c_str());
    TEST_ASSERT(ret == 0, "ImageSearch_Init returns 0");

    // Query（空库）
    char buf[4096] = {};
    ret = ImageSearch_Query("{\"top_k\":10}", buf, sizeof(buf));
    TEST_ASSERT(ret >= 0, "Query on empty db");
    {
        auto j = json::parse(buf);
        TEST_ASSERT(j.is_array() && j.empty(), "Empty result array");
    }

    // AddImage（文件不存在）
    ret = ImageSearch_AddImage("{\"path\":\"/nonexistent/photo.jpg\"}");
    TEST_ASSERT(ret == -2, "AddImage nonexistent returns -2");

    // StartHttpServer
    ret = ImageSearch_StartHttpServer(18091);
    TEST_ASSERT(ret == 0, "StartHttpServer returns 0");
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // GET /health
    {
        httplib::Client cli("127.0.0.1", 18091);
        cli.set_connection_timeout(3);
        auto resp = cli.Get("/health");
        TEST_ASSERT(resp && resp->status == 200, "GET /health returns 200");
    }

    // StopHttpServer
    ImageSearch_StopHttpServer();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Shutdown
    ImageSearch_Shutdown();
    TEST_ASSERT(true, "Shutdown no crash");

    fs::remove(dbPath);
    fs::remove_all("./test_step9_idx");
}

// ─── main ─────────────────────────────────────────────────────────────────────

int main() {
    printf("========================================\n");
    printf("  Faiss Step-by-Step Test\n");
    printf("========================================\n");

    Step1_FaissDemoTest();
    Step2_SimpleFaissIndex();
    Step3_ThreadSafeFaissIndex();
    Step4_IndexManager();
    Step5_UniqueIndexManager();
    Step6_Database();
    Step7_DatabasePlusFaiss();
    Step8_HttpServer();
    Step9_ImageSearchApi();

    printf("\n========================================\n");
    printf("  Results: %d passed, %d failed\n", g_pass, g_fail);
    printf("========================================\n");

    return g_fail > 0 ? 1 : 0;
}
