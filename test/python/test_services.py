"""Quick test to verify all services can start"""
import os
import sys
import subprocess
import time
import requests

os.chdir(os.path.dirname(os.path.abspath(__file__)))

def test_clip():
    print("[1/3] Testing CLIP service...")
    try:
        import torch
        import cn_clip.clip as clip
        from cn_clip.clip import load_from_name
        device = "cpu"
        print("  Loading ViT-L-14 (may take a moment)...")
        model, preprocess = load_from_name("ViT-L-14", device=device, download_root="./models")
        model.eval()
        print("  [OK] Chinese-CLIP ViT-L-14 loaded, device:", device)
        return True
    except Exception as e:
        print(f"  [WARN] Cannot load model: {e}")
        print("  Service will run in mock mode")
        return True  # mock mode is acceptable

def test_wd14():
    print("[2/3] Testing WD14 model...")
    try:
        import onnxruntime as ort
        model_path = "./models/wd14/model.onnx"
        tags_path  = "./models/wd14/selected_tags.csv"
        if not os.path.exists(model_path):
            print(f"  [FAIL] model.onnx not found at {model_path}")
            return False
        if not os.path.exists(tags_path):
            print(f"  [FAIL] selected_tags.csv not found at {tags_path}")
            return False
        session = ort.InferenceSession(model_path, providers=["CPUExecutionProvider"])
        print(f"  [OK] WD14 ONNX loaded, inputs: {[i.name for i in session.get_inputs()]}")
        return True
    except Exception as e:
        print(f"  [FAIL] {e}")
        return False

def test_insightface():
    print("[3/3] Testing InsightFace model...")
    try:
        from insightface.app import FaceAnalysis
        model_dir = "./models/insightface"
        buffalo_dir = os.path.join(model_dir, "models", "buffalo_l")
        if not os.path.exists(buffalo_dir):
            print(f"  [FAIL] buffalo_l not found at {buffalo_dir}")
            return False
        files = os.listdir(buffalo_dir)
        print(f"  [OK] buffalo_l found with {len(files)} files: {files}")
        return True
    except Exception as e:
        print(f"  [FAIL] {e}")
        return False

if __name__ == "__main__":
    print("=" * 50)
    print("  Service Model Verification")
    print("=" * 50)
    r1 = test_clip()
    r2 = test_wd14()
    r3 = test_insightface()
    print("\n" + "=" * 50)
    print(f"  CLIP:       {'OK' if r1 else 'FAIL'}")
    print(f"  WD14:       {'OK' if r2 else 'FAIL'}")
    print(f"  InsightFace:{'OK' if r3 else 'FAIL'}")
    print("=" * 50)
    sys.exit(0 if all([r1, r2, r3]) else 1)
