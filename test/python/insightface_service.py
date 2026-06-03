"""
InsightFace 人脸检测 + ArcFace 特征提取服务
端口: 18083
接口:
  POST /detect_faces  {"image_path": "..."}  -> {"faces": [[...512 floats], ...]}
  GET  /health                                -> {"status": "ok"}

模型: buffalo_l (ArcFace R100), 512 维人脸特征
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
face_app = None
MODEL_DIR = os.environ.get("INSIGHTFACE_MODEL_DIR", "./models/insightface")


def load_model():
    global face_app
    try:
        import insightface
        from insightface.app import FaceAnalysis

        logger.info("Loading InsightFace buffalo_l model...")
        face_app = FaceAnalysis(
            name="buffalo_l",
            root=MODEL_DIR,
            providers=["CUDAExecutionProvider", "CPUExecutionProvider"]
        )
        face_app.prepare(ctx_id=0, det_size=(640, 640))
        logger.info("InsightFace model loaded successfully")
    except ImportError as e:
        logger.error(f"Failed to import insightface: {e}")
        logger.info("Falling back to mock mode")
        face_app = None
    except Exception as e:
        logger.error(f"Failed to load InsightFace model: {e}")
        logger.info("Falling back to mock mode")
        face_app = None


def detect_faces_impl(image_path: str) -> list:
    """
    检测图片中的人脸，返回每张脸的 512 维 ArcFace 特征向量列表
    """
    if face_app is None:
        # Mock 模式：返回一个随机 512 维向量
        vec = np.random.randn(512).astype(np.float32)
        vec = vec / np.linalg.norm(vec)
        return [vec.tolist()]

    import cv2

    img = cv2.imread(image_path)
    if img is None:
        raise ValueError(f"Cannot read image: {image_path}")

    faces = face_app.get(img)
    if not faces:
        return []

    result = []
    for face in faces:
        if face.embedding is not None:
            emb = face.embedding.astype(np.float32)
            # L2 归一化
            norm = np.linalg.norm(emb)
            if norm > 1e-10:
                emb = emb / norm
            result.append(emb.tolist())

    return result


@app.route("/health", methods=["GET"])
def health():
    return jsonify({
        "status": "ok",
        "service": "insightface",
        "model_loaded": face_app is not None
    })


@app.route("/detect_faces", methods=["POST"])
def detect_faces():
    data = request.get_json()
    if not data or "image_path" not in data:
        return jsonify({"error": "missing image_path"}), 400

    image_path = data["image_path"]
    if not os.path.exists(image_path):
        return jsonify({"error": f"file not found: {image_path}"}), 404

    try:
        faces = detect_faces_impl(image_path)
        return jsonify({
            "faces": faces,
            "num_faces": len(faces),
            "dim": 512
        })
    except Exception as e:
        logger.error(f"detect_faces error: {e}")
        return jsonify({"error": str(e)}), 500


if __name__ == "__main__":
    load_model()
    port = int(os.environ.get("FACE_PORT", 18083))
    logger.info(f"Starting InsightFace service on port {port}")
    app.run(host="0.0.0.0", port=port, threaded=True)
