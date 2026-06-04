"""
Chinese-CLIP 特征提取服务
端口: 18081
接口:
  POST /encode_image  {"image_path": "..."}  -> {"vector": [...768 floats]}
  POST /encode_text   {"text": "..."}         -> {"vector": [...768 floats]}
  GET  /health                                -> {"status": "ok"}
"""

import os
import sys
import json
import logging
import numpy as np
from flask import Flask, request, jsonify

logging.basicConfig(level=logging.INFO, format='%(asctime)s %(levelname)s %(message)s')
logger = logging.getLogger(__name__)

app = Flask(__name__)

# 全局模型
model = None
preprocess = None
tokenize = None
device = "cpu"


def load_model():
    global model, preprocess, tokenize, device
    try:
        import torch
        import cn_clip.clip as clip
        from cn_clip.clip import load_from_name

        device = "cuda" if torch.cuda.is_available() else "cpu"
        logger.info(f"Loading Chinese-CLIP ViT-L/14 on {device}...")
        model, preprocess = load_from_name("ViT-L-14", device=device, download_root="./models")
        model.eval()
        tokenize = clip.tokenize
        logger.info("Chinese-CLIP model loaded successfully")
    except ImportError as e:
        logger.error(f"Failed to import cn_clip: {e}")
        logger.info("Falling back to mock mode (returns random vectors)")
        model = None


def encode_image_impl(image_path: str) -> list:
    """将图片编码为 768 维向量"""
    if model is None:
        # Mock 模式：返回随机向量（用于测试）
        vec = np.random.randn(768).astype(np.float32)
        vec = vec / np.linalg.norm(vec)
        return vec.tolist()

    import torch
    from PIL import Image

    image = preprocess(Image.open(image_path).convert("RGB")).unsqueeze(0).to(device)
    with torch.no_grad():
        image_features = model.encode_image(image)
        image_features = image_features / image_features.norm(dim=-1, keepdim=True)
    return image_features.squeeze(0).cpu().numpy().tolist()


def encode_text_impl(text: str) -> list:
    """将文字编码为 768 维向量"""
    if model is None:
        # Mock 模式
        vec = np.random.randn(768).astype(np.float32)
        vec = vec / np.linalg.norm(vec)
        return vec.tolist()

    import torch

    text_tokens = tokenize([text]).to(device)
    with torch.no_grad():
        text_features = model.encode_text(text_tokens)
        text_features = text_features / text_features.norm(dim=-1, keepdim=True)
    return text_features.squeeze(0).cpu().numpy().tolist()


@app.route("/health", methods=["GET"])
def health():
    return jsonify({"status": "ok", "service": "chinese-clip", "model_loaded": model is not None})


@app.route("/encode_image", methods=["POST"])
def encode_image():
    data = request.get_json()
    if not data or "image_path" not in data:
        return jsonify({"error": "missing image_path"}), 400

    image_path = data["image_path"]
    if not os.path.exists(image_path):
        return jsonify({"error": f"file not found: {image_path}"}), 404

    try:
        vector = encode_image_impl(image_path)
        return jsonify({"vector": vector, "dim": len(vector)})
    except Exception as e:
        logger.error(f"encode_image error: {e}")
        return jsonify({"error": str(e)}), 500


@app.route("/encode_text", methods=["POST"])
def encode_text():
    data = request.get_json()
    if not data or "text" not in data:
        return jsonify({"error": "missing text"}), 400

    try:
        vector = encode_text_impl(data["text"])
        return jsonify({"vector": vector, "dim": len(vector)})
    except Exception as e:
        logger.error(f"encode_text error: {e}")
        return jsonify({"error": str(e)}), 500


if __name__ == "__main__":
    load_model()
    port = int(os.environ.get("CLIP_PORT", 18081))
    logger.info(f"Starting Chinese-CLIP service on port {port}")
    app.run(host="0.0.0.0", port=port, threaded=True)
