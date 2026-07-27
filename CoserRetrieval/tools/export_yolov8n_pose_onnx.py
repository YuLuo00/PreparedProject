"""
一次性离线脚本：导出 Ultralytics YOLOv8n-pose 为 ONNX，用于 Route B 的人体检测+关键点。

用法：
    pip install ultralytics
    python export_yolov8n_pose_onnx.py

产物 yolov8n-pose.onnx 会生成在当前目录，需手动移动到
CoserRetrieval/models/pose/yolov8n-pose.onnx。

Ultralytics 会在首次运行时自动从其 GitHub Release 下载 yolov8n-pose.pt 权重
（~6.5MB），需要能连上 github.com/ultralytics 的 release 资产地址。
"""
from ultralytics import YOLO


def main():
    model = YOLO("yolov8n-pose.pt")
    model.export(format="onnx", imgsz=640, opset=12, simplify=True)


if __name__ == "__main__":
    main()
