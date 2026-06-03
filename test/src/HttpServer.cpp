#include "HttpServer.h"
#include "Database.h"
#include "FaissIndex.h"
#include "HttpClient.h"
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <sstream>

using json = nlohmann::json;
namespace fs = std::filesystem;

// ─── 辅助函数 ─────────────────────────────────────────────────────────────────

static json ImageRecordToJson(const ImageRecord& rec,
                               const std::vector<std::pair<std::string,float>>& tags = {}) {
    json j;
    j["id"]          = rec.id;
    j["path"]        = rec.path;
    j["filename"]    = rec.filename;
    j["shoot_date"]  = rec.shoot_date;
    j["rating"]      = rec.rating;
    j["path_prefix"] = rec.path_prefix;
    j["scene_tags"]  = json::parse(rec.scene_tags.empty() ? "[]" : rec.scene_tags);
    j["style_tags"]  = json::parse(rec.style_tags.empty() ? "[]" : rec.style_tags);
    j["mood_tags"]   = json::parse(rec.mood_tags.empty() ? "[]" : rec.mood_tags);
    if (!tags.empty()) {
        json tagArr = json::array();
        for (auto& [t, c] : tags) {
            tagArr.push_back({{"tag", t}, {"confidence", c}});
        }
        j["wd14_tags"] = tagArr;
    }
    return j;
}

static void SendJson(httplib::Response& res, const json& j, int status = 200) {
    res.status = status;
    res.set_content(j.dump(), "application/json");
}

static void SendError(httplib::Response& res, const std::string& msg, int status = 400) {
    SendJson(res, {{"error", msg}}, status);
}

// ─── 构造/析构 ────────────────────────────────────────────────────────────────

ImageSearchHttpServer::ImageSearchHttpServer(Database* db, FaissIndexManager* faiss)
    : db_(db), faiss_(faiss), server_(std::make_unique<httplib::Server>()) {
    SetupRoutes();
}

ImageSearchHttpServer::~ImageSearchHttpServer() {
    Stop();
}

bool ImageSearchHttpServer::Start(int port) {
    port_ = port;
    if (running_.load()) return true;

    running_.store(true);
    serverThread_ = std::thread([this]() {
        server_->listen("0.0.0.0", port_);
        running_.store(false);
    });
    // 等待服务器启动
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    return true;
}

void ImageSearchHttpServer::Stop() {
    if (running_.load()) {
        server_->stop();
        if (serverThread_.joinable()) serverThread_.join();
        running_.store(false);
    }
}

// ─── 路由设置 ─────────────────────────────────────────────────────────────────

void ImageSearchHttpServer::SetupRoutes() {

    // GET /health
    server_->Get("/health", [](const httplib::Request&, httplib::Response& res) {
        SendJson(res, {{"status", "ok"}, {"service", "ImageSearch"}});
    });

    // GET /stats
    server_->Get("/stats", [this](const httplib::Request&, httplib::Response& res) {
        json stats;
        stats["clip_index_count"]  = faiss_->GetClipIndex()->GetCount();
        stats["wd14_index_count"]  = faiss_->GetWd14Index()->GetCount();
        stats["face_index_count"]  = faiss_->GetFaceIndex()->GetCount();
        stats["clip_service"]  = PythonServiceClient::IsClipServiceAvailable();
        stats["wd14_service"]  = PythonServiceClient::IsWd14ServiceAvailable();
        stats["face_service"]  = PythonServiceClient::IsFaceServiceAvailable();
        SendJson(res, stats);
    });

    // POST /query — 结构化检索
    server_->Post("/query", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            auto j = json::parse(req.body);
            QueryFilter filter;
            if (j.contains("tags") && j["tags"].is_array()) {
                for (auto& t : j["tags"]) filter.tags.push_back(t.get<std::string>());
            }
            if (j.contains("date_from")) filter.date_from = j["date_from"].get<std::string>();
            if (j.contains("date_to"))   filter.date_to   = j["date_to"].get<std::string>();
            if (j.contains("path_prefix")) filter.path_prefix = j["path_prefix"].get<std::string>();
            if (j.contains("rating_min")) filter.rating_min = j["rating_min"].get<double>();
            if (j.contains("top_k"))     filter.top_k = j["top_k"].get<int>();

            auto images = db_->QueryImages(filter);
            json arr = json::array();
            for (auto& img : images) {
                auto tags = db_->GetTags(img.id);
                arr.push_back(ImageRecordToJson(img, tags));
            }
            SendJson(res, {{"results", arr}, {"count", (int)arr.size()}});
        } catch (std::exception& e) {
            SendError(res, e.what());
        }
    });

    // POST /query_by_text — 向量语义检索
    server_->Post("/query_by_text", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            auto j = json::parse(req.body);
            if (!j.contains("text")) { SendError(res, "missing 'text'"); return; }

            std::string text = j["text"].get<std::string>();
            int topK = j.value("top_k", 20);
            std::string indexType = j.value("index_type", "clip"); // clip / wd14

            // 获取文字向量
            auto vecOpt = PythonServiceClient::EncodeText(text);
            if (!vecOpt) { SendError(res, "CLIP service unavailable", 503); return; }

            FaissIndex* idx = (indexType == "wd14") ? faiss_->GetWd14Index() : faiss_->GetClipIndex();
            std::vector<int64_t> ids;
            std::vector<float> scores;
            idx->Search(*vecOpt, topK, ids, scores);

            // 构建结果
            json arr = json::array();
            for (size_t i = 0; i < ids.size(); ++i) {
                if (ids[i] < 0) continue;
                int64_t imageId = db_->GetEntityIdByFaissId(ids[i], indexType);
                if (imageId < 0) continue;
                auto imgOpt = db_->GetImageById(imageId);
                if (!imgOpt) continue;
                auto tags = db_->GetTags(imageId);
                auto imgJson = ImageRecordToJson(*imgOpt, tags);
                imgJson["similarity"] = scores[i];
                arr.push_back(imgJson);
            }

            // 附加结构化过滤（后过滤）
            if (j.contains("filter") && j["filter"].is_object()) {
                auto& fj = j["filter"];
                json filtered = json::array();
                for (auto& item : arr) {
                    bool ok = true;
                    if (fj.contains("rating_min") && item["rating"].get<double>() < fj["rating_min"].get<double>()) ok = false;
                    if (fj.contains("path_prefix") && item["path"].get<std::string>().find(fj["path_prefix"].get<std::string>()) == std::string::npos) ok = false;
                    if (ok) filtered.push_back(item);
                }
                arr = filtered;
            }

            SendJson(res, {{"results", arr}, {"count", (int)arr.size()}, {"query", text}});
        } catch (std::exception& e) {
            SendError(res, e.what());
        }
    });

    // POST /add_image — 添加单张图片
    server_->Post("/add_image", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            auto j = json::parse(req.body);
            if (!j.contains("path")) { SendError(res, "missing 'path'"); return; }

            std::string path = j["path"].get<std::string>();
            if (!fs::exists(path)) { SendError(res, "file not found: " + path); return; }

            ImageRecord rec;
            rec.path = path;
            rec.filename = fs::path(path).filename().string();
            rec.path_prefix = fs::path(path).parent_path().string();
            if (j.contains("rating"))     rec.rating     = j["rating"].get<double>();
            if (j.contains("shoot_date")) rec.shoot_date = j["shoot_date"].get<std::string>();
            if (j.contains("scene_tags")) rec.scene_tags = j["scene_tags"].dump();
            if (j.contains("style_tags")) rec.style_tags = j["style_tags"].dump();
            if (j.contains("mood_tags"))  rec.mood_tags  = j["mood_tags"].dump();

            int64_t imageId = db_->InsertImage(rec);
            if (imageId < 0) { SendError(res, "DB insert failed: " + db_->GetLastError()); return; }

            // 异步特征提取（同步执行，可改为线程池）
            std::vector<float> clipVec, wd14Vec;
            int64_t clipFaissId = -1, wd14FaissId = -1;

            auto clipOpt = PythonServiceClient::EncodeImage(path);
            if (clipOpt) {
                clipVec = *clipOpt;
                clipFaissId = db_->GetNextFaissId("clip");
                faiss_->GetClipIndex()->AddVector(clipVec, clipFaissId);
                db_->InsertFaissMapping(clipFaissId, imageId, "clip");
            }

            auto wd14Opt = PythonServiceClient::TagImage(path);
            if (wd14Opt) {
                wd14Vec = wd14Opt->feature_vector;
                db_->InsertTags(imageId, wd14Opt->tags);
                if (!wd14Vec.empty()) {
                    wd14FaissId = db_->GetNextFaissId("wd14");
                    faiss_->GetWd14Index()->AddVector(wd14Vec, wd14FaissId);
                    db_->InsertFaissMapping(wd14FaissId, imageId, "wd14");
                }
            }

            db_->UpdateImageFeatures(imageId, clipVec, wd14Vec, clipFaissId, wd14FaissId);

            SendJson(res, {{"success", true}, {"image_id", imageId}, {"path", path}});
        } catch (std::exception& e) {
            SendError(res, e.what());
        }
    });

    // POST /add_batch — 批量添加（newline-delimited JSON）
    server_->Post("/add_batch", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            std::istringstream ss(req.body);
            std::string line;
            int success = 0, failed = 0;
            json errors = json::array();

            db_->BeginTransaction();
            while (std::getline(ss, line)) {
                if (line.empty()) continue;
                try {
                    auto j = json::parse(line);
                    if (!j.contains("path")) { failed++; continue; }
                    std::string path = j["path"].get<std::string>();
                    if (!fs::exists(path)) { failed++; errors.push_back(path + ": not found"); continue; }

                    ImageRecord rec;
                    rec.path = path;
                    rec.filename = fs::path(path).filename().string();
                    rec.path_prefix = fs::path(path).parent_path().string();
                    if (j.contains("rating"))     rec.rating     = j["rating"].get<double>();
                    if (j.contains("shoot_date")) rec.shoot_date = j["shoot_date"].get<std::string>();

                    int64_t imageId = db_->InsertImage(rec);
                    if (imageId < 0) { failed++; continue; }

                    // 特征提取
                    std::vector<float> clipVec, wd14Vec;
                    int64_t clipFaissId = -1, wd14FaissId = -1;

                    auto clipOpt = PythonServiceClient::EncodeImage(path);
                    if (clipOpt) {
                        clipVec = *clipOpt;
                        clipFaissId = db_->GetNextFaissId("clip");
                        faiss_->GetClipIndex()->AddVector(clipVec, clipFaissId);
                        db_->InsertFaissMapping(clipFaissId, imageId, "clip");
                    }
                    auto wd14Opt = PythonServiceClient::TagImage(path);
                    if (wd14Opt) {
                        wd14Vec = wd14Opt->feature_vector;
                        db_->InsertTags(imageId, wd14Opt->tags);
                        if (!wd14Vec.empty()) {
                            wd14FaissId = db_->GetNextFaissId("wd14");
                            faiss_->GetWd14Index()->AddVector(wd14Vec, wd14FaissId);
                            db_->InsertFaissMapping(wd14FaissId, imageId, "wd14");
                        }
                    }
                    db_->UpdateImageFeatures(imageId, clipVec, wd14Vec, clipFaissId, wd14FaissId);
                    success++;
                } catch (...) {
                    failed++;
                }
            }
            db_->CommitTransaction();
            faiss_->SaveAll();

            SendJson(res, {{"success", success}, {"failed", failed}, {"errors", errors}});
        } catch (std::exception& e) {
            db_->RollbackTransaction();
            SendError(res, e.what());
        }
    });

    // POST /scan_directory — 扫描目录
    server_->Post("/scan_directory", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            auto j = json::parse(req.body);
            if (!j.contains("path")) { SendError(res, "missing 'path'"); return; }
            std::string dirPath = j["path"].get<std::string>();
            if (!fs::exists(dirPath)) { SendError(res, "directory not found"); return; }

            int success = 0, skipped = 0;
            db_->BeginTransaction();
            for (auto& entry : fs::recursive_directory_iterator(dirPath)) {
                if (!entry.is_regular_file()) continue;
                auto ext = entry.path().extension().string();
                // 转小写比较
                std::string extLower = ext;
                for (auto& c : extLower) c = (char)tolower(c);
                if (extLower != ".jpg" && extLower != ".jpeg") continue;

                std::string path = entry.path().string();
                if (db_->ImageExists(path)) { skipped++; continue; }

                ImageRecord rec;
                rec.path = path;
                rec.filename = entry.path().filename().string();
                rec.path_prefix = entry.path().parent_path().string();

                int64_t imageId = db_->InsertImage(rec);
                if (imageId < 0) continue;

                std::vector<float> clipVec, wd14Vec;
                int64_t clipFaissId = -1, wd14FaissId = -1;

                auto clipOpt = PythonServiceClient::EncodeImage(path);
                if (clipOpt) {
                    clipVec = *clipOpt;
                    clipFaissId = db_->GetNextFaissId("clip");
                    faiss_->GetClipIndex()->AddVector(clipVec, clipFaissId);
                    db_->InsertFaissMapping(clipFaissId, imageId, "clip");
                }
                auto wd14Opt = PythonServiceClient::TagImage(path);
                if (wd14Opt) {
                    wd14Vec = wd14Opt->feature_vector;
                    db_->InsertTags(imageId, wd14Opt->tags);
                    if (!wd14Vec.empty()) {
                        wd14FaissId = db_->GetNextFaissId("wd14");
                        faiss_->GetWd14Index()->AddVector(wd14Vec, wd14FaissId);
                        db_->InsertFaissMapping(wd14FaissId, imageId, "wd14");
                    }
                }
                db_->UpdateImageFeatures(imageId, clipVec, wd14Vec, clipFaissId, wd14FaissId);
                success++;
            }
            db_->CommitTransaction();
            faiss_->SaveAll();

            SendJson(res, {{"scanned", success}, {"skipped", skipped}, {"directory", dirPath}});
        } catch (std::exception& e) {
            db_->RollbackTransaction();
            SendError(res, e.what());
        }
    });

    // POST /register_coser — 注册 Coser 人脸
    server_->Post("/register_coser", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            auto j = json::parse(req.body);
            if (!j.contains("name") || !j.contains("image_path")) {
                SendError(res, "missing 'name' or 'image_path'"); return;
            }
            std::string name = j["name"].get<std::string>();
            std::string imagePath = j["image_path"].get<std::string>();

            auto facesOpt = PythonServiceClient::DetectFaces(imagePath);
            if (!facesOpt || facesOpt->empty()) {
                SendError(res, "no face detected or InsightFace service unavailable", 503); return;
            }

            int registered = 0;
            for (auto& faceVec : *facesOpt) {
                CoserFace face;
                face.coser_name = name;
                face.reference_image = imagePath;
                face.face_vector = faceVec;

                int64_t faceId = db_->InsertCoserFace(face);
                if (faceId < 0) continue;

                int64_t faceFaissId = db_->GetNextFaissId("face");
                faiss_->GetFaceIndex()->AddVector(faceVec, faceFaissId);
                db_->InsertFaissMapping(faceFaissId, faceId, "face");
                registered++;
            }
            faiss_->SaveAll();

            SendJson(res, {{"success", true}, {"name", name}, {"faces_registered", registered}});
        } catch (std::exception& e) {
            SendError(res, e.what());
        }
    });

    // POST /identify_coser — 识别 Coser
    server_->Post("/identify_coser", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            auto j = json::parse(req.body);
            if (!j.contains("image_path")) { SendError(res, "missing 'image_path'"); return; }
            std::string imagePath = j["image_path"].get<std::string>();
            float threshold = j.value("threshold", 0.4f);

            auto facesOpt = PythonServiceClient::DetectFaces(imagePath);
            if (!facesOpt || facesOpt->empty()) {
                SendJson(res, {{"results", json::array()}, {"message", "no face detected"}});
                return;
            }

            json results = json::array();
            for (auto& faceVec : *facesOpt) {
                std::vector<int64_t> ids;
                std::vector<float> scores;
                faiss_->GetFaceIndex()->Search(faceVec, 5, ids, scores);

                for (size_t i = 0; i < ids.size(); ++i) {
                    if (ids[i] < 0 || scores[i] < threshold) continue;
                    int64_t faceId = db_->GetEntityIdByFaissId(ids[i], "face");
                    if (faceId < 0) continue;

                    // 查询 coser 名字
                    auto allFaces = db_->GetAllCoserFaces();
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
                    break; // 每张脸只取最高分
                }
            }

            SendJson(res, {{"results", results}, {"faces_detected", (int)facesOpt->size()}});
        } catch (std::exception& e) {
            SendError(res, e.what());
        }
    });

    // POST /rebuild_index — 重建 Faiss 索引
    server_->Post("/rebuild_index", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            auto j = json::parse(req.body);
            std::string indexType = j.value("index_type", "all");

            auto rebuildOne = [&](const std::string& type) -> int {
                FaissIndex* idx = nullptr;
                if (type == "clip")  idx = faiss_->GetClipIndex();
                else if (type == "wd14") idx = faiss_->GetWd14Index();
                else if (type == "face") idx = faiss_->GetFaceIndex();
                else return -1;

                idx->Reset();
                std::vector<int64_t> faissIds;
                std::vector<std::vector<float>> vecs;
                db_->LoadAllVectors(type, faissIds, vecs);
                if (!faissIds.empty()) {
                    idx->AddVectors(vecs, faissIds);
                }
                return (int)faissIds.size();
            };

            json rebuilt;
            if (indexType == "all") {
                rebuilt["clip"]  = rebuildOne("clip");
                rebuilt["wd14"]  = rebuildOne("wd14");
                rebuilt["face"]  = rebuildOne("face");
            } else {
                rebuilt[indexType] = rebuildOne(indexType);
            }
            faiss_->SaveAll();

            SendJson(res, {{"success", true}, {"rebuilt", rebuilt}});
        } catch (std::exception& e) {
            SendError(res, e.what());
        }
    });
}
