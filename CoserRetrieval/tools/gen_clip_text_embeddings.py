"""
一次性离线脚本：用 openai/clip-vit-base-patch32（与 Xenova ONNX 导出版本同权重）
计算两句提示语的文本 embedding，L2 归一化后输出为 C++ 数组字面量。

用法：
    pip install transformers torch
    python gen_clip_text_embeddings.py > clip_text_embeddings.inc

输出内容需要手动粘贴进 src/L3/PhotoAuthenticityChecker.cpp 的常量定义里。
换提示语或换语言需要重新跑这个脚本，不是运行时可配置的。
"""
import torch
from transformers import CLIPModel, CLIPTokenizer

MODEL_NAME = "openai/clip-vit-base-patch32"

PROMPTS = {
    "kRealPhotoEmbedding": "a real photograph of a person",
    "kIllustrationEmbedding": "an illustration, drawing, or CG artwork",
}


def main():
    tokenizer = CLIPTokenizer.from_pretrained(MODEL_NAME)
    model = CLIPModel.from_pretrained(MODEL_NAME)
    model.eval()

    for var_name, prompt in PROMPTS.items():
        inputs = tokenizer([prompt], padding=True, return_tensors="pt")
        with torch.no_grad():
            outputs = model.get_text_features(**inputs)
        text_embeds = outputs.pooler_output
        text_embeds = text_embeds / text_embeds.norm(p=2, dim=-1, keepdim=True)
        vec = text_embeds[0].tolist()
        assert len(vec) == 512, f"expected 512-d, got {len(vec)}"

        print(f"// prompt: \"{prompt}\"")
        print(f"const std::array<float, 512> {var_name} = {{")
        for i in range(0, 512, 8):
            chunk = vec[i:i + 8]
            print("    " + ", ".join(f"{v:.8f}f" for v in chunk) + ",")
        print("};")
        print()


if __name__ == "__main__":
    main()
