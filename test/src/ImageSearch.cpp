#include "ImageSearch.h"
#include "Database.h"
#include "FaissIndex.h"
#include "HttpClient.h"
#include "HttpServer.h"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <sstream>
#include <cstring>

using json = nlohmann::json;
namespace fs = std::filesystem;

// ─── 全局状态 ─────────────────────────────────────────────────────────────────

static std::unique_ptr<Database>             g_db;
static std::unique_ptr<FaissIndexManager>    g_faiss;
static std::unique_ptr<ImageSearchHttpServer> g_httpServer;
static std::mutex                            g_mutex;
static std::string                           g_dbPath;
static std::string                           g_indexDir;

// ─── 辅助：写入输出缓冲区 ─────────────────────────────────────────────────────

static int WriteOutput(const std::string& str, char* outBuf, int bufSize) {
    if (!outBuf || bufSize <= 0) return -1;
    int len = (int)str.size();
    if (len >= bufSize) {
        // 截断
        memcpy(outBuf, str.c_str(), bufSize - 1);
        outBuf[bufSize - 1] = '\0';
        return bufSize - 1;
    }
    memcpy(outBuf, str.c_str(), len + 1);
    return len;
}

// ─── DLL 导出实现 ─────────────────────────────────────────────────────────────

IMAGESEARCH_API int ImageSearch_Init(const char* dbPath) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!dbPath) return -1;

    g_dbPath = dbPath;
    // 索引目录与数据库同级
    fs::path p(dbPath);
    g_indexDir = (p.parent_path() / "faiss_index").string();

    g_db = std::make_unique<Database>();
    if (!g_db->Open(g_dbPath)) return -2;

    g_faiss = std::make_unique<FaissIndexManager>();
    if (!g_faiss->Init(g_indexDir)) return -3;

    return 0;
}

IMAGESEARCH_API void ImageSearch_Shutdown() {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_httpServer) {
        g_httpServer->Stop();
        g_httpServer.reset();
    }
    if (g_faiss) {
        g_faiss->SaveAll();
        g_faiss.reset();
    }
    if (g_db) {
        g_db->Close();
        g_db.reset();
    }
}

IMAGESEARCH_API int ImageSearch_Query(const char* queryJson, char* outBuf, int bufSize) {
    if (!g_db || !queryJson) return -1;
    try {
        auto j = json::parse(queryJson);
        QueryFilter filter;
        if (j.contains("tags") && j["tags"].is_array()) {
            for (auto& t : j["tags"]) filter.tags.push_back(t.get<std::string>());
        }
        if (j.contains("date_from"))   filter.date_from   = j["date_from"].get<std::string>();
        if (j.contains("date_to"))     filter.date_to     = j["date_to"].get<std::string>();
        if (j.contains("path_prefix")) filter.path_prefix = j["path_prefix"].get<std::string>();
        if (j.contains("rating_min"))  filter.rating_min  = j["rating_min"].get<double>();
        if (j.contains("top_k"))       filter.top_k       = j["top_k"].get<int>();

        auto images = g_db->QueryImages(filter);
        json arr = json::array();
        for (auto& img : images) {
            json item;
            item["id"]         = img.id;
            item["path"]       = img.path;
            item["filename"]   = img.filename;
            item["shoot_date"] = img.shoot_date;
            item["rating"]     = img.rating;
            item["path_prefix"]= img.path_prefix;
            auto tags = g_db->GetTags(img.id);
            json tagArr = json::array();
            for (auto& [t, c] : tags) tagArr.push_back({{"tag", t}, {"confidence", c}});
            item["tags"] = tagArr;
            arr.push_back(item);
        }
        return WriteOutput(arr.dump(), outBuf, bufSize);
    } catch (...) {
        return -1;
    }
}

IMAGESEARCH_API int ImageSearch_QueryByText(const char* textQuery, const char* filterJson,
                                             int topK, char* outBuf, int bufSize) {
    if (!g_db || !g_faiss || !textQuery) return -1;
    try {
        auto vecOpt = PythonServiceClient::EncodeText(textQuery);
        if (!vecOpt) return -2; // CLIP 服务不可用

        std::vector<int64_t> ids;
        std::vector<float> scores;
        g_faiss->GetClipIndex()->Search(*vecOpt, topK, ids, scores);

        json arr = json::array();
        for (size_t i = 0; i < ids.size(); ++i) {
            if (ids[i] < 0) continue;
            int64_t imageId = g_db->GetEntityIdByFaissId(ids[i], "clip");
            if (imageId < 0) continue;
            auto imgOpt = g_db->GetImageById(imageId);
            if (!imgOpt) continue;

            json item;
            item["id"]         = imgOpt->id;
            item["path"]       = imgOpt->path;
            item["filename"]   = imgOpt->filename;
            item["shoot_date"] = imgOpt->shoot_date;
            item["rating"]     = imgOpt->rating;
            item["similarity"] = scores[i];
            auto tags = g_db->GetTags(imageId);
            json tagArr = json::array();
            for (auto& [t, c] : tags) tagArr.push_back({{"tag", t}, {"confidence", c}});
            item["tags"] = tagArr;
            arr.push_back(item);
        }

        // 附加过滤
        if (filterJson) {
            try {
                auto fj = json::parse(filterJson);
                json filtered = json::array();
                for (auto& item : arr) {
                    bool ok = true;
                    if (fj.contains("rating_min") && item["rating"].get<double>() < fj["rating_min"].get<double>()) ok = false;
                    if (fj.contains("path_prefix") && item["path"].get<std::string>().find(fj["path_prefix"].get<std::string>()) == std::string::npos) ok = false;
                    if (ok) filtered.push_back(item);
                }
                arr = filtered;
            } catch (...) {}
        }

        return WriteOutput(arr.dump(), outBuf, bufSize);
    } catch (...) {
        return -1;
    }
}

IMAGESEARCH_API int ImageSearch_AddImage(const char* imageJson) {
    if (!g_db || !g_faiss || !imageJson) return -1;
    try {
        auto j = json::parse(imageJson);
        if (!j.contains("path")) return -1;

        std::string path = fs::absolute(j["path"].get<std::string>()).string();
        if (!fs::exists(path)) return -2;

        ImageRecord rec;
        rec.path = path;
        rec.filename = fs::path(path).filename().string();
        rec.path_prefix = fs::path(path).parent_path().string();
        if (j.contains("rating"))     rec.rating     = j["rating"].get<double>();
        if (j.contains("shoot_date")) rec.shoot_date = j["shoot_date"].get<std::string>();
        if (j.contains("scene_tags")) rec.scene_tags = j["scene_tags"].dump();
        if (j.contains("style_tags")) rec.style_tags = j["style_tags"].dump();
        if (j.contains("mood_tags"))  rec.mood_tags  = j["mood_tags"].dump();

        int64_t imageId = g_db->InsertImage(rec);
        if (imageId < 0) return -3;

        std::vector<float> clipVec, wd14Vec;
        int64_t clipFaissId = -1, wd14FaissId = -1;

        auto clipOpt = PythonServiceClient::EncodeImage(path);
        if (clipOpt) {
            clipVec = *clipOpt;
            clipFaissId = g_db->GetNextFaissId("clip");
            g_faiss->GetClipIndex()->AddVector(clipVec, clipFaissId);
            g_db->InsertFaissMapping(clipFaissId, imageId, "clip");
        }

        auto wd14Opt = PythonServiceClient::TagImage(path);
        if (wd14Opt) {
            wd14Vec = wd14Opt->feature_vector;
            g_db->InsertTags(imageId, wd14Opt->tags);
            if (!wd14Vec.empty()) {
                wd14FaissId = g_db->GetNextFaissId("wd14");
                g_faiss->GetWd14Index()->AddVector(wd14Vec, wd14FaissId);
                g_db->InsertFaissMapping(wd14FaissId, imageId, "wd14");
            }
        }

        g_db->UpdateImageFeatures(imageId, clipVec, wd14Vec, clipFaissId, wd14FaissId);
        g_faiss->SaveAll();
        return 0;
    } catch (...) {
        return -1;
    }
}

IMAGESEARCH_API int ImageSearch_AddImageBatch(const char* jsonLines) {
    if (!g_db || !g_faiss || !jsonLines) return -1;
    int success = 0;
    try {
        std::istringstream ss(jsonLines);
        std::string line;
        g_db->BeginTransaction();
        while (std::getline(ss, line)) {
            if (line.empty()) continue;
            try {
                auto j = json::parse(line);
                if (!j.contains("path")) continue;
                std::string path = fs::absolute(j["path"].get<std::string>()).string();
                if (!fs::exists(path)) continue;

                ImageRecord rec;
                rec.path = path;
                rec.filename = fs::path(path).filename().string();
                rec.path_prefix = fs::path(path).parent_path().string();
                if (j.contains("rating"))     rec.rating     = j["rating"].get<double>();
                if (j.contains("shoot_date")) rec.shoot_date = j["shoot_date"].get<std::string>();

                int64_t imageId = g_db->InsertImage(rec);
                if (imageId < 0) continue;

                std::vector<float> clipVec, wd14Vec;
                int64_t clipFaissId = -1, wd14FaissId = -1;

                auto clipOpt = PythonServiceClient::EncodeImage(path);
                if (clipOpt) {
                    clipVec = *clipOpt;
                    clipFaissId = g_db->GetNextFaissId("clip");
                    g_faiss->GetClipIndex()->AddVector(clipVec, clipFaissId);
                    g_db->InsertFaissMapping(clipFaissId, imageId, "clip");
                }
                auto wd14Opt = PythonServiceClient::TagImage(path);
                if (wd14Opt) {
                    wd14Vec = wd14Opt->feature_vector;
                    g_db->InsertTags(imageId, wd14Opt->tags);
                    if (!wd14Vec.empty()) {
                        wd14FaissId = g_db->GetNextFaissId("wd14");
                        g_faiss->GetWd14Index()->AddVector(wd14Vec, wd14FaissId);
                        g_db->InsertFaissMapping(wd14FaissId, imageId, "wd14");
                    }
                }
                g_db->UpdateImageFeatures(imageId, clipVec, wd14Vec, clipFaissId, wd14FaissId);
                success++;
            } catch (...) {}
        }
        g_db->CommitTransaction();
        g_faiss->SaveAll();
    } catch (...) {
        g_db->RollbackTransaction();
        return -1;
    }
    return success;
}

IMAGESEARCH_API int ImageSearch_StartHttpServer(int port) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_db || !g_faiss) return -1;
    if (g_httpServer && g_httpServer->IsRunning()) return 0;

    g_httpServer = std::make_unique<ImageSearchHttpServer>(g_db.get(), g_faiss.get());
    return g_httpServer->Start(port) ? 0 : -1;
}

IMAGESEARCH_API void ImageSearch_StopHttpServer() {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_httpServer) {
        g_httpServer->Stop();
        g_httpServer.reset();
    }
}

IMAGESEARCH_API int ImageSearch_ScanDirectory(const char* dirPath) {
    if (!g_db || !g_faiss || !dirPath) return -1;
    int success = 0;
    try {
        if (!fs::exists(dirPath)) return -2;
        g_db->BeginTransaction();
        for (auto& entry : fs::recursive_directory_iterator(dirPath)) {
            if (!entry.is_regular_file()) continue;
            std::string ext = entry.path().extension().string();
            for (auto& c : ext) c = (char)tolower(c);
            if (ext != ".jpg" && ext != ".jpeg") continue;

            std::string path = fs::absolute(entry.path()).string();
            if (g_db->ImageExists(path)) continue;

            ImageRecord rec;
            rec.path = path;
            rec.filename = entry.path().filename().string();
            rec.path_prefix = entry.path().parent_path().string();

            int64_t imageId = g_db->InsertImage(rec);
            if (imageId < 0) continue;

            std::vector<float> clipVec, wd14Vec;
            int64_t clipFaissId = -1, wd14FaissId = -1;

            auto clipOpt = PythonServiceClient::EncodeImage(path);
            if (clipOpt) {
                clipVec = *clipOpt;
                clipFaissId = g_db->GetNextFaissId("clip");
                g_faiss->GetClipIndex()->AddVector(clipVec, clipFaissId);
                g_db->InsertFaissMapping(clipFaissId, imageId, "clip");
            }
            auto wd14Opt = PythonServiceClient::TagImage(path);
            if (wd14Opt) {
                wd14Vec = wd14Opt->feature_vector;
                g_db->InsertTags(imageId, wd14Opt->tags);
                if (!wd14Vec.empty()) {
                    wd14FaissId = g_db->GetNextFaissId("wd14");
                    g_faiss->GetWd14Index()->AddVector(wd14Vec, wd14FaissId);
                    g_db->InsertFaissMapping(wd14FaissId, imageId, "wd14");
                }
            }
            g_db->UpdateImageFeatures(imageId, clipVec, wd14Vec, clipFaissId, wd14FaissId);
            success++;
        }
        g_db->CommitTransaction();
        g_faiss->SaveAll();
    } catch (...) {
        g_db->RollbackTransaction();
        return -1;
    }
    return success;
}

IMAGESEARCH_API int ImageSearch_RegisterCoser(const char* coserJson) {
    if (!g_db || !g_faiss || !coserJson) return -1;
    try {
        auto j = json::parse(coserJson);
        if (!j.contains("name") || !j.contains("image_path")) return -1;

        std::string name = j["name"].get<std::string>();
        std::string imagePath = j["image_path"].get<std::string>();

        auto facesOpt = PythonServiceClient::DetectFaces(imagePath);
        if (!facesOpt || facesOpt->empty()) return -2;

        for (auto& faceVec : *facesOpt) {
            CoserFace face;
            face.coser_name = name;
            face.reference_image = imagePath;
            face.face_vector = faceVec;

            int64_t faceId = g_db->InsertCoserFace(face);
            if (faceId < 0) continue;

            int64_t faceFaissId = g_db->GetNextFaissId("face");
            g_faiss->GetFaceIndex()->AddVector(faceVec, faceFaissId);
            g_db->InsertFaissMapping(faceFaissId, faceId, "face");
        }
        g_faiss->SaveAll();
        return 0;
    } catch (...) {
        return -1;
    }
}

IMAGESEARCH_API int ImageSearch_IdentifyCoser(const char* imagePath, char* outBuf, int bufSize) {
    if (!g_db || !g_faiss || !imagePath) return -1;
    try {
        auto facesOpt = PythonServiceClient::DetectFaces(imagePath);
        if (!facesOpt || facesOpt->empty()) {
            return WriteOutput("[]", outBuf, bufSize);
        }

        json results = json::array();
        for (auto& faceVec : *facesOpt) {
            std::vector<int64_t> ids;
            std::vector<float> scores;
            g_faiss->GetFaceIndex()->Search(faceVec, 5, ids, scores);

            for (size_t i = 0; i < ids.size(); ++i) {
                if (ids[i] < 0 || scores[i] < 0.4f) continue;
                int64_t faceId = g_db->GetEntityIdByFaissId(ids[i], "face");
                if (faceId < 0) continue;

                auto allFaces = g_db->GetAllCoserFaces();
                for (auto& cf : allFaces) {
                    if (cf.id == faceId) {
                        results.push_back({
                            {"name", cf.coser_name},
                            {"similarity", scores[i]},
                            {"reference_image", cf.reference_image}
                        });
                        break;
                    }
                }
                break;
            }
        }
        return WriteOutput(results.dump(), outBuf, bufSize);
    } catch (...) {
        return -1;
    }
}

IMAGESEARCH_API int ImageSearch_RebuildIndex(const char* indexType) {
    if (!g_db || !g_faiss) return -1;
    try {
        auto rebuildOne = [&](const std::string& type) {
            FaissIndex* idx = nullptr;
            if (type == "clip")       idx = g_faiss->GetClipIndex();
            else if (type == "wd14")  idx = g_faiss->GetWd14Index();
            else if (type == "face")  idx = g_faiss->GetFaceIndex();
            else return;

            idx->Reset();
            std::vector<int64_t> faissIds;
            std::vector<std::vector<float>> vecs;
            g_db->LoadAllVectors(type, faissIds, vecs);
            if (!faissIds.empty()) idx->AddVectors(vecs, faissIds);
        };

        if (!indexType || std::string(indexType) == "all") {
            rebuildOne("clip");
            rebuildOne("wd14");
            rebuildOne("face");
        } else {
            rebuildOne(indexType);
        }
        g_faiss->SaveAll();
        return 0;
    } catch (...) {
        return -1;
    }
}

// ─── DLL 入口点 ───────────────────────────────────────────────────────────────
#ifdef _WIN32
#include <windows.h>
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
        break;
    case DLL_PROCESS_DETACH:
        ImageSearch_Shutdown();
        break;
    }
    return TRUE;
}
#endif
