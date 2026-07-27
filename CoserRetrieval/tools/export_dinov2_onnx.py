"""
一次性离线脚本：导出 facebook/dinov2-small (ViT-S/14) 视觉特征为 ONNX，
用于 Route B 的服装/角色特征提取。

选择 ViT-S/14（384维）而非 HLD 原定的 ViT-B/14（768维），原因：
本机磁盘仅剩 ~19G 可用，ViT-B/14 导出的 onnx 约 340MB，ViT-S/14 约 85-90MB。
HLD §9.2 本身已把"换 ViT-S/14"列为 CPU 推理延迟超预算时的第一优化选项。

用法：
    pip install transformers torch
    python export_dinov2_onnx.py

产物 dinov2_vits14.onnx 生成在当前目录，需手动移动到
CoserRetrieval/models/clothing/dinov2_vits14.onnx。

facebook/dinov2-small 通过 hf-mirror.com 镜像下载
（直连 huggingface.co 在当前网络环境下 DNS 失败，和里程碑2下载 CLIP 模型时一致）。
"""
import os
os.environ.setdefault("HF_ENDPOINT", "https://hf-mirror.com")

import torch
from transformers import Dinov2Model

MODEL_NAME = "facebook/dinov2-small"
OUTPUT_PATH = "dinov2_vits14.onnx"


def main():
    model = Dinov2Model.from_pretrained(MODEL_NAME)
    model.eval()

    dummy_input = torch.randn(1, 3, 224, 224)

    torch.onnx.export(
        model,
        dummy_input,
        OUTPUT_PATH,
        input_names=["pixel_values"],
        output_names=["last_hidden_state", "pooler_output"],
        dynamic_axes={
            "pixel_values": {0: "batch"},
            "last_hidden_state": {0: "batch"},
            "pooler_output": {0: "batch"},
        },
        opset_version=14,
    )
    print(f"exported to {OUTPUT_PATH}")


if __name__ == "__main__":
    main()
