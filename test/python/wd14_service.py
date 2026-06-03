"""
WD14-tagger 特征提取服务
端口: 18082
接口:
  POST /tag     {"image_path": "..."}  -> {"tags": [...], "feature_vector": [...1024 floats]}
  GET  /health                          -> {"status": "ok"}

模型: WD14 ViT-L ONNX (SmilingWolf/wd-v1-4-vit-tagger-v2)
标签: Danbooru 标签体系 (6000+)
"""

import os
import json
import logging
import numpy as np
from flask import Flask, request, jsonify

logging.basicConfig(level=logging.INFO, format='%(asctime)s %(levelname)s %(message)s')
logger = logging.getLogger(__name__)

app = Flask(__name__)

# 全局模型
ort_session = None
tag_names = []
MODEL_DIR = os.environ.get("WD14_MODEL_DIR", "./models/wd14")
MODEL_PATH = os.path.join(MODEL_DIR, "model.onnx")
TAGS_PATH = os.path.join(MODEL_DIR, "selected_tags.csv")
THRESHOLD = 0.35  # 标签置信度阈值
IMAGE_SIZE = 448  # WD14 ViT-L 输入尺寸


def load_model():
    global ort_session, tag_names
    try:
        import onnxruntime as ort

        if not os.path.exists(MODEL_PATH):
            logger.warning(f"WD14 model not found at {MODEL_PATH}")
            logger.info("Falling back to mock mode")
            return

        logger.info(f"Loading WD14 ONNX model from {MODEL_PATH}...")
        providers = ["CUDAExecutionProvider", "CPUExecutionProvider"]
        ort_session = ort.InferenceSession(MODEL_PATH, providers=providers)

        # 加载标签
        if os.path.exists(TAGS_PATH):
            import csv
            with open(TAGS_PATH, "r", encoding="utf-8") as f:
                reader = csv.DictReader(f)
                tag_names = [row["name"] for row in reader]
            logger.info(f"Loaded {len(tag_names)} tags")
        else:
            logger.warning(f"Tags file not found: {TAGS_PATH}")

        logger.info("WD14 model loaded successfully")
    except ImportError as e:
        logger.error(f"Failed to import onnxruntime: {e}")
        logger.info("Falling back to mock mode")
        ort_session = None


def preprocess_image(image_path: str) -> np.ndarray:
    """预处理图片为 WD14 输入格式"""
    from PIL import Image

    img = Image.open(image_path).convert("RGB")
    # 保持宽高比 pad 到正方形
    w, h = img.size
    max_dim = max(w, h)
    pad_img = Image.new("RGB", (max_dim, max_dim), (255, 255, 255))
    pad_img.paste(img, ((max_dim - w) // 2, (max_dim - h) // 2))
    img = pad_img.resize((IMAGE_SIZE, IMAGE_SIZE), Image.BICUBIC)

    # 转为 numpy，BGR 格式（WD14 使用 BGR）
    img_array = np.array(img, dtype=np.float32)
    img_array = img_array[:, :, ::-1]  # RGB -> BGR
    img_array = np.expand_dims(img_array, axis=0)  # (1, H, W, C)
    return img_array


def tag_image_impl(image_path: str):
    """
    返回 (tags, feature_vector)
    tags: [(tag_name, confidence), ...]
    feature_vector: list of 1024 floats
    """
    if ort_session is None:
        # Mock 模式
        mock_tags = [
            ("1girl", 0.98), ("solo", 0.95), ("cosplay", 0.87),
            ("long_hair", 0.82), ("smile", 0.75), ("looking_at_viewer", 0.70)
        ]
        mock_vec = np.random.randn(1024).astype(np.float32)
        mock_vec = mock_vec / np.linalg.norm(mock_vec)
        return mock_tags, mock_vec.tolist()

    img_array = preprocess_image(image_path)

    # 获取所有输出（包括特征层）
    input_name = ort_session.get_inputs()[0].name
    output_names = [o.name for o in ort_session.get_outputs()]

    outputs = ort_session.run(output_names, {input_name: img_array})

    # 第一个输出是 logits/probs（标签预测）
    probs = outputs[0][0]  # shape: (num_tags,)

    # 提取标签
    tags = []
    if tag_names:
        for i, prob in enumerate(probs):
            if i < len(tag_names) and prob >= THRESHOLD:
                tags.append((tag_names[i], float(prob)))
        tags.sort(key=lambda x: x[1], reverse=True)
    else:
        # 无标签文件时，返回 top-20 索引
        top_indices = np.argsort(probs)[::-1][:20]
        tags = [(f"tag_{i}", float(probs[i])) for i in top_indices if probs[i] >= THRESHOLD]

    # 特征向量：使用倒数第二层输出（如果有），否则用 probs 截断/填充到 1024
    feature_vector = None
    if len(outputs) > 1:
        # 尝试找到 1024 维特征层
        for out in outputs[1:]:
            if out.ndim == 2 and out.shape[1] == 1024:
                feature_vector = out[0].tolist()
                break

    if feature_vector is None:
        # 回退：用 probs 填充/截断到 1024 维
        if len(probs) >= 1024:
            feature_vector = probs[:1024].tolist()
        else:
            padded = np.zeros(1024, dtype=np.float32)
            padded[:len(probs)] = probs
            feature_vector = padded.tolist()

    return tags, feature_vector


@app.route("/health", methods=["GET"])
def health():
    return jsonify({
        "status": "ok",
        "service": "wd14-tagger",
        "model_loaded": ort_session is not None,
        "num_tags": len(tag_names)
    })


@app.route("/tag", methods=["POST"])
def tag():
    data = request.get_json()
    if not data or "image_path" not in data:
        return jsonify({"error": "missing image_path"}), 400

    image_path = data["image_path"]
    if not os.path.exists(image_path):
        return jsonify({"error": f"file not found: {image_path}"}), 404

    try:
        tags, feature_vector = tag_image_impl(image_path)
        return jsonify({
            "tags": [{"tag": t, "confidence": c} for t, c in tags],
            "feature_vector": feature_vector,
            "num_tags": len(tags)
        })
    except Exception as e:
        logger.error(f"tag error: {e}")
        return jsonify({"error": str(e)}), 500


if __name__ == "__main__":
    load_model()
    port = int(os.environ.get("WD14_PORT", 18082))
    logger.info(f"Starting WD14-tagger service on port {port}")
    app.run(host="0.0.0.0", port=port, threaded=True)
