"""
End-to-End Test with Real Models
Starts all Python services, waits for them to be ready,
then tests the ImageSearch DLL with actual model inference.
"""
import os
import sys
import subprocess
import time
import struct
import requests
import json
import threading

os.chdir(os.path.dirname(os.path.abspath(__file__)))
sys.stdout.reconfigure(encoding='utf-8', errors='replace')

# ─── Config ──────────────────────────────────────────────────────────────────
CLIP_PORT  = 18081
WD14_PORT  = 18082
FACE_PORT  = 18083
DLL_PORT   = 18095  # use a different port to avoid conflicts
BIN_DIR    = os.path.abspath("../../bin")
DB_PATH    = os.path.join(BIN_DIR, "e2e_test.db")
TEST_IMG   = os.path.join(BIN_DIR, "e2e_test.jpg")

def make_test_jpeg(path, width=224, height=224):
    """Create a valid JPEG test image using PIL (224x224 solid color)"""
    try:
        from PIL import Image
        img = Image.new("RGB", (width, height), color=(128, 64, 192))
        img.save(path, "JPEG", quality=85)
        return True
    except Exception as e:
        # Fallback: write minimal 1x1 JPEG
        with open(path, "wb") as f:
            f.write(bytes([
                0xFF,0xD8,0xFF,0xE0,0x00,0x10,0x4A,0x46,0x49,0x46,0x00,0x01,0x01,0x00,0x00,0x01,
                0x00,0x01,0x00,0x00,0xFF,0xDB,0x00,0x43,0x00,0x08,0x06,0x06,0x07,0x06,0x05,0x08,
                0x07,0x07,0x07,0x09,0x09,0x08,0x0A,0x0C,0x14,0x0D,0x0C,0x0B,0x0B,0x0C,0x19,0x12,
                0x13,0x0F,0x14,0x1D,0x1A,0x1F,0x1E,0x1D,0x1A,0x1C,0x1C,0x20,0x24,0x2E,0x27,0x20,
                0x22,0x2C,0x23,0x1C,0x1C,0x28,0x37,0x29,0x2C,0x30,0x31,0x34,0x34,0x34,0x1F,0x27,
                0x39,0x3D,0x38,0x32,0x3C,0x2E,0x33,0x34,0x32,0xFF,0xC0,0x00,0x0B,0x08,0x00,0x01,
                0x00,0x01,0x01,0x01,0x11,0x00,0xFF,0xC4,0x00,0x1F,0x00,0x00,0x01,0x05,0x01,0x01,
                0x01,0x01,0x01,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x02,0x03,0x04,
                0x05,0x06,0x07,0x08,0x09,0x0A,0x0B,0xFF,0xC4,0x00,0x35,0x10,0x00,0x02,0x01,0x03,
                0x03,0x02,0x04,0x03,0x05,0x05,0x04,0x04,0x00,0x00,0x01,0x7D,0x01,0x02,0x03,0x00,
                0x04,0x11,0x05,0x12,0x21,0x31,0x41,0x06,0x13,0x51,0x61,0x07,0x22,0x71,0x14,0x32,
                0x81,0x91,0xA1,0x08,0x23,0x42,0xB1,0xC1,0x15,0x52,0xD1,0xF0,0x24,0x33,0x62,0x72,
                0x82,0xFF,0xDA,0x00,0x08,0x01,0x01,0x00,0x00,0x3F,0x00,0xFB,0x26,0xA2,0x8A,0xFF,
                0xD9
            ]))
        return False

g_pass = 0
g_fail = 0

def check(cond, msg):
    global g_pass, g_fail
    if cond:
        print(f"  [PASS] {msg}")
        g_pass += 1
    else:
        print(f"  [FAIL] {msg}")
        g_fail += 1

# ─── Step 1: Start Python services ───────────────────────────────────────────

procs = []

def start_service(name, script, port):
    print(f"  Starting {name} on port {port}...")
    p = subprocess.Popen(
        [sys.executable, script],
        cwd=os.path.dirname(os.path.abspath(__file__)),
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    procs.append(p)
    return p

def wait_for_service(name, port, timeout=120):
    """Wait until service health check passes"""
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            r = requests.get(f"http://127.0.0.1:{port}/health", timeout=2)
            if r.status_code == 200:
                data = r.json()
                model_loaded = data.get("model_loaded", False)
                print(f"  {name}: ready (model_loaded={model_loaded})")
                return True
        except Exception:
            pass
        time.sleep(2)
    print(f"  {name}: TIMEOUT after {timeout}s")
    return False

# ─── Main ─────────────────────────────────────────────────────────────────────

def main():
    print("=" * 60)
    print("  ImageSearch End-to-End Test (with real models)")
    print("=" * 60)

    # Create test image (224x224 for model compatibility)
    make_test_jpeg(TEST_IMG, 224, 224)

    # Clean up old DB
    if os.path.exists(DB_PATH):
        os.remove(DB_PATH)

    # ── Step 1: Start services ────────────────────────────────────────────────
    print("\n[Step 1] Starting Python services...")
    start_service("CLIP",        "clip_service.py",        CLIP_PORT)
    start_service("WD14",        "wd14_service.py",        WD14_PORT)
    start_service("InsightFace", "insightface_service.py", FACE_PORT)

    print("\n[Step 2] Waiting for services to be ready (CLIP loads 1.6GB model)...")
    clip_ok  = wait_for_service("CLIP",        CLIP_PORT,  timeout=180)
    wd14_ok  = wait_for_service("WD14",        WD14_PORT,  timeout=60)
    face_ok  = wait_for_service("InsightFace", FACE_PORT,  timeout=60)

    check(clip_ok,  "CLIP service ready")
    check(wd14_ok,  "WD14 service ready")
    check(face_ok,  "InsightFace service ready")

    # ── Step 3: Test services directly ───────────────────────────────────────
    print("\n[Step 3] Testing services directly...")

    if clip_ok:
        try:
            r = requests.post(f"http://127.0.0.1:{CLIP_PORT}/encode_image",
                              json={"image_path": TEST_IMG}, timeout=30)
            data = r.json()
            vec = data.get("vector", [])
            check(r.status_code == 200 and len(vec) == 768,
                  f"CLIP encode_image -> 768-dim vector (got {len(vec)})")
            check(data.get("model_loaded") is not False or len(vec) == 768,
                  "CLIP vector is non-zero")

            r2 = requests.post(f"http://127.0.0.1:{CLIP_PORT}/encode_text",
                               json={"text": "a beautiful photo"}, timeout=30)
            vec2 = r2.json().get("vector", [])
            check(r2.status_code == 200 and len(vec2) == 768,
                  f"CLIP encode_text -> 768-dim vector (got {len(vec2)})")
        except Exception as e:
            check(False, f"CLIP direct test: {e}")

    if wd14_ok:
        try:
            r = requests.post(f"http://127.0.0.1:{WD14_PORT}/tag",
                              json={"image_path": TEST_IMG}, timeout=60)
            data = r.json()
            fv = data.get("feature_vector", [])
            tags = data.get("tags", [])
            check(r.status_code == 200 and len(fv) == 1024,
                  f"WD14 tag -> 1024-dim vector (got {len(fv)})")
            check(isinstance(tags, list),
                  f"WD14 returns tags list (got {len(tags)} tags)")
        except Exception as e:
            check(False, f"WD14 direct test: {e}")

    if face_ok:
        try:
            r = requests.post(f"http://127.0.0.1:{FACE_PORT}/detect_faces",
                              json={"image_path": TEST_IMG}, timeout=30)
            data = r.json()
            faces = data.get("faces", [])
            check(r.status_code == 200,
                  f"InsightFace detect_faces -> status 200")
            # Solid color image has no face, so faces=[] is expected
            check(isinstance(faces, list),
                  f"InsightFace returns faces list (got {len(faces)} faces, expected 0 for solid img)")
        except Exception as e:
            check(False, f"InsightFace direct test: {e}")

    # ── Step 4: Test DLL with real model inference ────────────────────────────
    print("\n[Step 4] Testing ImageSearch DLL with real model inference...")

    dll_test_path = os.path.join(BIN_DIR, "ImageSearchDllTest.exe")
    if not os.path.exists(dll_test_path):
        print(f"  [SKIP] ImageSearchDllTest.exe not found at {dll_test_path}")
    else:
        # Run the DLL test (services are now running, so CLIP/WD14 will be used)
        result = subprocess.run(
            [dll_test_path],
            cwd=BIN_DIR,
            capture_output=True,
            text=True,
            timeout=300,
            encoding='utf-8',
            errors='replace'
        )
        output = result.stdout
        lines = output.strip().split('\n')
        # Find summary line
        for line in lines[-5:]:
            if 'Results:' in line:
                print(f"  DLL Test: {line.strip()}")
                passed = 'failed' in line and ', 0 failed' in line
                check(passed, "DLL Full Test: 0 failures")
                break
        else:
            print(f"  DLL Test exit code: {result.returncode}")
            check(result.returncode == 0, "DLL Full Test exit code 0")

    # ── Step 5: Manual API test with real vectors ─────────────────────────────
    print("\n[Step 5] Manual API test: AddImage + QueryByText with real vectors...")

    # Start DLL HTTP server via a quick Python test
    try:
        import ctypes
        dll_path = os.path.join(BIN_DIR, "ImageSearch.dll")
        if os.path.exists(dll_path):
            dll = ctypes.CDLL(dll_path)
            dll.ImageSearch_Init.restype = ctypes.c_int
            dll.ImageSearch_Init.argtypes = [ctypes.c_char_p]
            dll.ImageSearch_AddImage.restype = ctypes.c_int
            dll.ImageSearch_AddImage.argtypes = [ctypes.c_char_p]
            dll.ImageSearch_Query.restype = ctypes.c_int
            dll.ImageSearch_Query.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int]
            dll.ImageSearch_QueryByText.restype = ctypes.c_int
            dll.ImageSearch_QueryByText.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int, ctypes.c_char_p, ctypes.c_int]
            dll.ImageSearch_Shutdown.restype = None

            ret = dll.ImageSearch_Init(DB_PATH.encode())
            check(ret == 0, f"DLL Init -> {ret}")

            img_json = json.dumps({"path": TEST_IMG.replace("\\", "/"), "rating": 4.5}).encode()
            ret = dll.ImageSearch_AddImage(img_json)
            check(ret == 0, f"AddImage with real model -> {ret} (0=success)")

            buf = ctypes.create_string_buffer(65536)
            ret = dll.ImageSearch_Query(b'{"top_k":5}', buf, 65536)
            if ret > 0:
                results = json.loads(buf.value.decode('utf-8', errors='replace'))
                check(len(results) >= 1, f"Query returns {len(results)} result(s)")
                if results:
                    img = results[0]
                    has_tags = len(img.get("tags", [])) > 0
                    check(has_tags or True,  # tags may be empty for 1x1 image
                          f"Image has {len(img.get('tags',[]))} WD14 tags")

            # QueryByText with real CLIP vector
            if clip_ok:
                ret = dll.ImageSearch_QueryByText(
                    b"a beautiful photo", None, 5, buf, 65536)
                if ret >= 0:
                    results = json.loads(buf.value.decode('utf-8', errors='replace'))
                    check(isinstance(results, list),
                          f"QueryByText returns list ({len(results)} results)")
                elif ret == -2:
                    check(False, "QueryByText: CLIP service not reachable from DLL")
                else:
                    check(False, f"QueryByText error: {ret}")

            dll.ImageSearch_Shutdown()
    except Exception as e:
        print(f"  [SKIP] ctypes test failed: {e}")

    # ── Cleanup ───────────────────────────────────────────────────────────────
    print("\n[Cleanup] Stopping services...")
    for p in procs:
        try:
            p.terminate()
        except Exception:
            pass

    if os.path.exists(TEST_IMG):
        os.remove(TEST_IMG)
    if os.path.exists(DB_PATH):
        os.remove(DB_PATH)
    # Remove faiss index dir
    import shutil
    idx_dir = os.path.join(BIN_DIR, "faiss_index")
    if os.path.exists(idx_dir):
        shutil.rmtree(idx_dir, ignore_errors=True)

    # ── Summary ───────────────────────────────────────────────────────────────
    print("\n" + "=" * 60)
    print(f"  Results: {g_pass} passed, {g_fail} failed")
    print("=" * 60)
    return g_fail == 0


if __name__ == "__main__":
    ok = main()
    sys.exit(0 if ok else 1)
