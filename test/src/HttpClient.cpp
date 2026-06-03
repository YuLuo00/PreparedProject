#include "HttpClient.h"
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <stdexcept>

using json = nlohmann::json;

static std::optional<std::vector<float>> PostAndParseVector(
    int port, const std::string& path, const json& body, const std::string& key)
{
    httplib::Client cli(PythonServiceClient::HOST, port);
    cli.set_connection_timeout(5);
    cli.set_read_timeout(30);

    auto res = cli.Post(path.c_str(), body.dump(), "application/json");
    if (!res || res->status != 200) return std::nullopt;

    try {
        auto j = json::parse(res->body);
        if (!j.contains(key)) return std::nullopt;
        auto& arr = j[key];
        std::vector<float> vec;
        vec.reserve(arr.size());
        for (auto& v : arr) vec.push_back(v.get<float>());
        return vec;
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<std::vector<float>> PythonServiceClient::EncodeImage(const std::string& imagePath) {
    json body = {{"image_path", imagePath}};
    return PostAndParseVector(CLIP_PORT, "/encode_image", body, "vector");
}

std::optional<std::vector<float>> PythonServiceClient::EncodeText(const std::string& text) {
    json body = {{"text", text}};
    return PostAndParseVector(CLIP_PORT, "/encode_text", body, "vector");
}

std::optional<PythonServiceClient::Wd14Result> PythonServiceClient::TagImage(const std::string& imagePath) {
    httplib::Client cli(HOST, WD14_PORT);
    cli.set_connection_timeout(5);
    cli.set_read_timeout(60);

    json body = {{"image_path", imagePath}};
    auto res = cli.Post("/tag", body.dump(), "application/json");
    if (!res || res->status != 200) return std::nullopt;

    try {
        auto j = json::parse(res->body);
        Wd14Result result;

        // 解析标签
        if (j.contains("tags")) {
            for (auto& t : j["tags"]) {
                std::string tag = t["tag"].get<std::string>();
                float conf = t["confidence"].get<float>();
                result.tags.emplace_back(tag, conf);
            }
        }

        // 解析特征向量
        if (j.contains("feature_vector")) {
            for (auto& v : j["feature_vector"]) {
                result.feature_vector.push_back(v.get<float>());
            }
        }

        return result;
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<std::vector<std::vector<float>>> PythonServiceClient::DetectFaces(const std::string& imagePath) {
    httplib::Client cli(HOST, FACE_PORT);
    cli.set_connection_timeout(5);
    cli.set_read_timeout(30);

    json body = {{"image_path", imagePath}};
    auto res = cli.Post("/detect_faces", body.dump(), "application/json");
    if (!res || res->status != 200) return std::nullopt;

    try {
        auto j = json::parse(res->body);
        if (!j.contains("faces")) return std::nullopt;

        std::vector<std::vector<float>> faces;
        for (auto& face : j["faces"]) {
            std::vector<float> vec;
            for (auto& v : face) vec.push_back(v.get<float>());
            faces.push_back(std::move(vec));
        }
        return faces;
    } catch (...) {
        return std::nullopt;
    }
}

static bool CheckHealth(int port) {
    httplib::Client cli(PythonServiceClient::HOST, port);
    cli.set_connection_timeout(2);
    auto res = cli.Get("/health");
    return res && res->status == 200;
}

bool PythonServiceClient::IsClipServiceAvailable()  { return CheckHealth(CLIP_PORT); }
bool PythonServiceClient::IsWd14ServiceAvailable()  { return CheckHealth(WD14_PORT); }
bool PythonServiceClient::IsFaceServiceAvailable()  { return CheckHealth(FACE_PORT); }

std::optional<std::vector<float>> PythonServiceClient::ParseFloatArray(
    const std::string& jsonStr, const std::string& key)
{
    try {
        auto j = json::parse(jsonStr);
        if (!j.contains(key)) return std::nullopt;
        std::vector<float> vec;
        for (auto& v : j[key]) vec.push_back(v.get<float>());
        return vec;
    } catch (...) {
        return std::nullopt;
    }
}
