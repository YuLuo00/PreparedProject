# CoserRetrieval

纯 C++ 图像检索系统（替代 `test/` 下已废弃的 Python 微服务 + DLL 架构）。

## 里程碑 1：人脸识别路径

SCRFD 人脸检测 → ArcFace 512维特征提取 → FAISS 向量检索 → SQLite 元数据存储。

已通过端到端验证：
- 同一张照片自匹配 top-1 score ≈ 1.0
- 同一人不同照片 top-1 命中，且分数高于跨人比对
- 无人脸图片 ingest 优雅失败（`ingest_status='failed'`），不崩溃

### 已知问题

**Release 配置的 `faiss.dll` 存在崩溃 bug（access violation, 0xC0000005）**

- 现象：`ThirdParty/install/faiss/installed/x64-windows/bin/faiss.dll`（Release 版，8.78MB）在调用
  `faiss::write_index()` 等核心操作时会在固定偏移处崩溃。
- 范围：与本次里程碑新增代码无关。用旧的 `test/` 原型代码重新以 Release 配置编译验证，
  崩溃现象完全一致（同一 faulting module `faiss.dll`，同一 fault offset
  `0x00000000001a3583`）——证明是 vcpkg 编译产物本身的问题，而非应用层代码。
- Debug 配置的 `faiss.dll`（16.6MB）工作正常，本里程碑的端到端验证即使用 Debug 配置完成。
- **结论**：Release 配置目前不可用。后续如需 Release 构建，需要单独处理
  （例如换用不同参数重新编译 faiss，或更换 faiss 来源）。
