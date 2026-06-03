#pragma once
#include <string>
#include <vector>
#include <optional>

/// 轻量 HTTP 客户端，用于调用 Python 特征提取微服务
class PythonServiceClient {
public:
    /// 调用 Chinese-CLIP 服务（端口 18081）
    /// POST /encode_image  body: {"image_path": "..."}
    /// POST /encode_text   body: {"text": "..."}
    /// 返回 768 维向量
    static std::optional<std::vector<float>> EncodeImage(const std::string& imagePath);
    static std::optional<std::vector<float>> EncodeText(const std::string& text);

    /// 调用 WD14-tagger 服务（端口 18082）
    /// POST /tag  body: {"image_path": "..."}
    /// 返回标签列表（tag, confidence）和 1024 维特征向量
    struct Wd14Result {
        std::vector<std::pair<std::string, float>> tags;
        std::vector<float> feature_vector; // 1024-dim
    };
    static std::optional<Wd14Result> TagImage(const std::string& imagePath);

    /// 调用 InsightFace 服务（端口 18083）
    /// POST /detect_faces  body: {"image_path": "..."}
    /// 返回人脸特征向量列表（每张脸 512 维）
    static std::optional<std::vector<std::vector<float>>> DetectFaces(const std::string& imagePath);

    /// 检查服务是否可用
    static bool IsClipServiceAvailable();
    static bool IsWd14ServiceAvailable();
    static bool IsFaceServiceAvailable();

    // 服务端口配置
    static constexpr int CLIP_PORT  = 18081;
    static constexpr int WD14_PORT  = 18082;
    static constexpr int FACE_PORT  = 18083;
    static constexpr const char* HOST = "127.0.0.1";

private:
    static std::optional<std::vector<float>> ParseFloatArray(const std::string& jsonStr,
                                                              const std::string& key);
};
