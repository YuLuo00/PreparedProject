"""
Model download script
Usage: python download_models.py  (run from test/python/ directory)
"""

import os
import sys

# Force UTF-8 output
if hasattr(sys.stdout, 'reconfigure'):
    sys.stdout.reconfigure(encoding='utf-8', errors='replace')


def download_chinese_clip():
    """Download Chinese-CLIP ViT-L/14 model"""
    print("\n[1/3] Downloading Chinese-CLIP ViT-L/14...")
    try:
        import cn_clip.clip as clip
        from cn_clip.clip import load_from_name
        os.makedirs("./models", exist_ok=True)
        print("  Downloading (~1.7GB, please wait)...")
        model, preprocess = load_from_name("ViT-L-14", device="cpu", download_root="./models")
        print("  [OK] Chinese-CLIP ViT-L/14 downloaded")
        return True
    except Exception as e:
        print(f"  [FAIL] {e}")
        return False


def download_wd14():
    """Download WD14 ViT-L ONNX model"""
    print("\n[2/3] Downloading WD14 ONNX model...")
    model_dir = "./models/wd14"
    os.makedirs(model_dir, exist_ok=True)

    model_path = os.path.join(model_dir, "model.onnx")
    tags_path  = os.path.join(model_dir, "selected_tags.csv")

    if os.path.exists(model_path) and os.path.exists(tags_path):
        print("  [OK] WD14 model already exists, skipping")
        return True

    try:
        from huggingface_hub import hf_hub_download
        print("  Downloading model.onnx from HuggingFace (~1.3GB)...")
        hf_hub_download(
            repo_id="SmilingWolf/wd-v1-4-vit-tagger-v2",
            filename="model.onnx",
            local_dir=model_dir,
        )
        print("  Downloading selected_tags.csv...")
        hf_hub_download(
            repo_id="SmilingWolf/wd-v1-4-vit-tagger-v2",
            filename="selected_tags.csv",
            local_dir=model_dir,
        )
        print("  [OK] WD14 model downloaded")
        return True
    except ImportError:
        print("  huggingface_hub not installed, trying direct download...")
        return download_wd14_direct()
    except Exception as e:
        print(f"  [FAIL] {e}")
        return False


def download_wd14_direct():
    """Download WD14 model directly via requests"""
    import requests
    model_dir = "./models/wd14"
    os.makedirs(model_dir, exist_ok=True)

    files = {
        "model.onnx": "https://huggingface.co/SmilingWolf/wd-v1-4-vit-tagger-v2/resolve/main/model.onnx",
        "selected_tags.csv": "https://huggingface.co/SmilingWolf/wd-v1-4-vit-tagger-v2/resolve/main/selected_tags.csv",
    }

    for filename, url in files.items():
        dest = os.path.join(model_dir, filename)
        if os.path.exists(dest):
            print(f"  {filename} already exists, skipping")
            continue
        print(f"  Downloading {filename}...")
        try:
            r = requests.get(url, stream=True, timeout=60)
            r.raise_for_status()
            total = int(r.headers.get("content-length", 0))
            downloaded = 0
            with open(dest, "wb") as f:
                for chunk in r.iter_content(chunk_size=1024*1024):
                    f.write(chunk)
                    downloaded += len(chunk)
                    if total:
                        pct = downloaded * 100 // total
                        print(f"\r  {filename}: {pct}%", end="", flush=True)
            print(f"\r  {filename}: done ({downloaded//1024//1024}MB)")
        except Exception as e:
            print(f"  [FAIL] {filename}: {e}")
            return False
    return True


def download_insightface():
    """Download InsightFace buffalo_l model"""
    print("\n[3/3] Downloading InsightFace buffalo_l model...")
    model_dir = "./models/insightface"
    os.makedirs(model_dir, exist_ok=True)

    buffalo_dir = os.path.join(model_dir, "models", "buffalo_l")
    if os.path.exists(buffalo_dir) and len(os.listdir(buffalo_dir)) > 0:
        print("  [OK] InsightFace buffalo_l already exists, skipping")
        return True

    try:
        from insightface.app import FaceAnalysis
        print("  Downloading buffalo_l (~500MB)...")
        app = FaceAnalysis(
            name="buffalo_l",
            root=model_dir,
            providers=["CPUExecutionProvider"]
        )
        app.prepare(ctx_id=-1, det_size=(640, 640))
        print("  [OK] InsightFace buffalo_l downloaded")
        return True
    except Exception as e:
        print(f"  [FAIL] {e}")
        return False


if __name__ == "__main__":
    # Change to script directory so ./models paths work correctly
    script_dir = os.path.dirname(os.path.abspath(__file__))
    os.chdir(script_dir)

    print("=" * 50)
    print("  ImageSearch Model Downloader")
    print("=" * 50)
    print(f"  Model directory: {os.path.abspath('./models')}")

    results = {}
    results["Chinese-CLIP"] = download_chinese_clip()
    results["WD14"]         = download_wd14()
    results["InsightFace"]  = download_insightface()

    print("\n" + "=" * 50)
    print("  Download Summary")
    print("=" * 50)
    for name, ok in results.items():
        status = "[OK]  " if ok else "[FAIL]"
        print(f"  {status} {name}")

    all_ok = all(results.values())
    print("\n" + ("All models downloaded!" if all_ok else "Some models failed. Check network or download manually."))
    sys.exit(0 if all_ok else 1)
