**Arknights Auto Machine (AAM) 版本化开发路线图**

基于语义化版本（SemVer），从 `v0.1.0-alpha`（契约奠基）迭代至 `v1.0.0`（生产就绪）。

**图例说明：**
- ✅ 已完成
- 🔶 部分完成
- ❌ 未开始
- 📦 依赖上游

---

## 版本总览

| 版本 | 代号 | 目标 | 状态 |
|------|------|------|------|
| v0.1.0-alpha | 契约冻结版 | 接口契约与构建系统 | ✅ |
| v0.2.0-alpha | 感知硬化版 | L0 帧捕获 | 🔶 传输层与基础设施完成 |
| v0.3.0-alpha | 空间映射版 | L2 坐标映射 | ❌ |
| v0.4.0-alpha | 记忆架构版 | L4 状态机与存储 | 🔶 Python部分完成 |
| v0.5.0-alpha | 视觉皮层版 | L1 视觉处理 | 🔶 Python部分完成 |
| v0.6.0-alpha | 战术执行版 | L3 战术DSL | 🔶 Python部分完成 |
| v0.7.0-beta | 桥接版 | C++ ↔ Python 通信 | ❌ |
| v0.8.0-beta | 决策版 | L5 LLM决策 | ❌ |
| v0.9.0-beta | 界面版 | Qt6 GUI | ❌ |
| v1.0.0-rc | 生产候选版 | 全链路集成 | ❌ |

---

## v0.1.0-alpha：契约冻结版（The Contract）

**目标**: 建立不可变的接口契约与构建系统。

### v0.1.0-alpha.1：协议定义

**协议层 (`proto/`)**
- [x] `proto/common/types.proto`：基础类型定义
- [x] `proto/ama/l0_frame.proto`：L0 帧元数据
- [x] `proto/ama/l1_perception.proto`：L1 感知结果
- [x] `proto/ama/l2_action.proto`：L2 动作指令
- [x] `proto/ama/l3_tactical.proto`：L3 战术原语
- [x] `proto/ama/l4_state.proto`：L4 状态快照
- [x] `proto/inference/l5_strategy.proto`：L5 决策接口
- [x] `proto/services/control_service.proto`：控制服务 RPC

### v0.1.0-alpha.2：构建系统

- [x] `CMakeLists.txt`（根）：C++23 标准，`aam_core` 静态库
- [x] `cmake/compiler_flags.cmake`：跨平台编译标志
- [x] `cmake/FindZeroMQ.cmake`：ZeroMQ 发现模块
- [x] `.clang-format`：代码风格配置
- [x] `.clang-tidy`：静态检查规则

### v0.1.0-alpha.3：持续集成

- [x] `.github/workflows/ci.yml`：三平台构建矩阵
  - Windows (MSVC 2022) x64 Release/Debug
  - Linux (GCC-12, Clang-16) Release/Debug
  - macOS (Apple Clang) Release/Debug
  - 代码风格检查 (clang-format, cmake-lint)
  - 协议兼容性检查 (Buf Breaking Change)
- [x] `.github/workflows/codeql.yml`：安全分析
  - C++ 代码安全扫描 (security-extended, security-and-quality)
  - Python 代码安全扫描
  - 自动检测 Critical/High 级别漏洞
- [x] `scripts/codegen/protobuf_gen.py`：代码生成脚本
  - 支持 C++ 和 Python 代码生成
  - 自动检测 protoc 和 gRPC 插件
  - 智能识别服务定义文件
  - Python 包结构自动初始化

**验收标准**
- [x] 三平台编译通过，零警告
- [x] CodeQL 扫描零 Critical/High 漏洞

---

## v0.2.0-alpha：感知硬化版（The Sensor）

**目标**: L0 层稳定帧捕获，P99 延迟 < 20ms。

### v0.2.0-alpha.1：接口定义

- [x] `include/aam/l0/capture_backend.hpp`：`ICaptureBackend` 接口
- [x] `include/aam/l0/frame_buffer.hpp`：`LockFreeFrameBuffer<T>` 模板

### v0.2.0-alpha.2：捕获后端

- [x] `src/l0_sensing/adb_capture.cpp`：ADB H264 管道捕获
- [x] `src/l0_sensing/maa_adapter.cpp`：MaaFramework 桥接
- [x] `src/l0_sensing/win32_window_capture.cpp`：Win32 后备方案

### v0.2.0-alpha.3：传输层

- [x] `src/l0_sensing/shm_transport.cpp`：共享内存传输

### v0.2.0-alpha.4：基础设施

- [x] `include/aam/core/timer.hpp`：高精度计时器（682行，纳秒级精度）
- [x] `include/aam/core/memory_pool.hpp`：定长内存池（590行，O(1)分配/释放）
- [x] `src/common/logger.cpp`：spdlog 封装（556行，支持异步/多线程）

### v0.2.0-alpha.5：测试与配置

- [x] `core/tests/test_shm_transport.cpp`：共享内存传输测试（31个测试用例，含吞吐量/延迟基准测试）
- [ ] `src/l0_sensing/tests/test_frame_sync.cpp`：帧同步测试（待实现）
- [ ] `configs/ama/capture.yaml`：L0 配置模板（待实现）

**验收标准**
- 1000 帧连续捕获，0 丢帧，P99 < 20ms
- Valgrind 报告无内存泄漏

---

## v0.3.0-alpha：空间映射版（The Mapper）

**目标**: L2 层坐标映射，支持多分辨率。

### v0.3.0-alpha.1：接口定义

- [ ] `include/aam/l2/coordinate_transform.hpp`：坐标转换器
- [ ] `include/aam/l2/human_simulator.hpp`：拟人轨迹生成
- [ ] `include/aam/l2/action_executor.hpp`：操作执行器接口

### v0.3.0-alpha.2：坐标转换

- [ ] `src/l2_motor/coordinate_transform.cpp`：4 点标定法
- [ ] `src/l2_motor/resolvers/1920x1080.cpp`：1080p 映射表
- [ ] `src/l2_motor/resolvers/2560x1440.cpp`：2K 映射表

### v0.3.0-alpha.3：轨迹生成

- [ ] `src/l2_motor/trajectory_generator.cpp`：贝塞尔曲线

### v0.3.0-alpha.4：输入适配

- [ ] `src/l2_motor/input_adapters/adb_input.cpp`：ADB 输入
- [ ] `src/l2_motor/input_adapters/win32_postmessage.cpp`：Win32 后台输入
- [ ] `src/l2_motor/feedback_loop.cpp`：操作确认闭环

### v0.3.0-alpha.5：配置

- [ ] `configs/ama/input_profiles/bluestacks.yaml`
- [ ] `configs/ama/input_profiles/rog_phone_8.yaml`

**验收标准**
- 坐标转换误差 < 2px（1080p）
- 拟人轨迹 KS 检验 p-value > 0.05

---

## v0.4.0-alpha：记忆架构版（The Memory）

**目标**: L4 层状态机与增量快照，支持崩溃恢复。

### v0.4.0-alpha.1：数据模型（Python）✅ 已完成

- [x] `inference/src/data/models/base.py`：基础数据模型
- [x] `inference/src/data/models/operator.py`：干员模型
- [x] `inference/src/data/models/stage.py`：关卡模型
- [x] `inference/src/data/models/item.py`：物品模型
- [x] `inference/src/data/models/enemy.py`：敌人模型

### v0.4.0-alpha.2：数据提供者（Python）✅ 已完成

- [x] `inference/src/data/providers/github_provider.py`：GitHub 数据同步
- [x] `inference/src/data/providers/prts_provider.py`：PRTS Wiki 爬取
- [x] `inference/src/data/providers/data_manager.py`：统一数据管理器

### v0.4.0-alpha.3：数据库存储（Python）✅ 已完成

- [x] `inference/src/data/database/schema.py`：数据库 Schema
- [x] `inference/src/data/database/manager.py`：结构化数据库管理

### v0.4.0-alpha.4：干员匹配（Python）✅ 已完成

- [x] `inference/src/data/operator_matcher.py`：干员模糊匹配

### v0.4.0-alpha.5：C++ 状态机

- [ ] `include/aam/l4/game_fsm.hpp`：分层状态机接口
- [ ] `include/aam/l4/snapshot_serializer.hpp`：快照序列化
- [ ] `src/l4_state/hfsm_impl.cpp`：HFSM 实现
- [ ] `src/l4_state/sqlite_storage.cpp`：SQLite 存储
- [ ] `src/l4_state/delta_encoder.cpp`：增量编码
- [ ] `src/l4_state/prts_data_loader.cpp`：PRTS 数据加载

### v0.4.0-alpha.6：数据脚本

- [ ] `scripts/db_migrate/fetch_prts_data.py`：PRTS 数据同步
- [ ] `tests/fixtures/level_1-7.json`：测试数据

**验收标准**
- 状态切换延迟 < 50ms
- 崩溃恢复丢失进度 < 5 秒
- 增量编码压缩率 ≥ 90%

---

## v0.5.0-alpha：视觉皮层版（The Vision）

**目标**: L1 层视觉处理，单帧 < 30ms。

### v0.5.0-alpha.1：游戏状态检测（Python）✅ 已完成

- [x] `inference/src/vision/game_state_detector.py`：对局状态检测
  - OCR 识别"剩余可放置角色"
  - 状态枚举：IN_BATTLE, NOT_IN_BATTLE, TRANSITIONING
  - 多阶段识别链：预处理 → ROI → OCR → 匹配

### v0.5.0-alpha.2：GUI 匹配（Python）✅ 已完成

- [x] `inference/src/vision/gui_matcher.py`：模板匹配 + OCR
  - 多尺度模板匹配
  - 非极大值抑制 (NMS)
- [x] `inference/src/vision/enhanced_gui_matcher.py`：增强版 GUI 分析
  - 主界面分析
  - UI 元素类型枚举

### v0.5.0-alpha.3：编队识别（Python）✅ 已完成

- [x] `inference/src/vision/squad_recognizer.py`：编队识别
  - 精英化等级识别（图标匹配）
  - 等级数字识别
  - 干员名称 OCR
- [x] `inference/src/vision/squad_analyzer.py`：编队分析
  - 整合识别与数据库查询
  - 职业/星级分布统计

### v0.5.0-alpha.4：文字定位（Python）✅ 已完成

- [x] `inference/src/vision/text_locator.py`：文字定位
  - EasyOCR 封装
  - 精确像素位置返回
  - 模糊匹配支持

### v0.5.0-alpha.5：C++ GPU 加速

- [ ] `include/aam/l1/gpu_pipeline.hpp`：GPU 流水线
- [ ] `include/aam/l1/tensor_converter.hpp`：张量转换
- [ ] `src/l1_perception/cuda_kernels/color_convert.cu`：CUDA 颜色转换
- [ ] `src/l1_perception/cuda_kernels/pyramid_down.cu`：CUDA 降采样

### v0.5.0-alpha.6：C++ 推理引擎

- [ ] `src/l1_perception/ocr_engine.cpp`：PaddleOCR 封装
- [ ] `src/l1_perception/yolo_detector.cpp`：YOLO 检测器
- [ ] `src/l1_perception/region_of_interest.cpp`：ROI 裁剪

### v0.5.0-alpha.7：模型文件

- [ ] `models/yolov8n-arknights.onnx`：量化 YOLO 模型
- [ ] `models/ppocr-v4-rec/`：OCR 模型

**验收标准**
- CUDA 预处理 < 8ms
- OCR 准确率 > 95%
- YOLO mAP > 0.85

---

## v0.6.0-alpha：战术执行版（The Tactics）

**目标**: L3 层战术 DSL 与虚拟机。

### v0.6.0-alpha.1：关卡分析（Python）✅ 已完成

- [x] `inference/src/map/level_analyzer.py`：关卡分析器
  - 地块类型枚举（TileType）
  - 敌人路径（Route, Checkpoint）
  - 波次数据（Wave, EnemySpawn）
  - 关卡选项（LevelOptions）

### v0.6.0-alpha.2：地图可视化（Python）✅ 已完成

- [x] `inference/src/map/map_visualizer.py`：地图可视化
  - 地块颜色映射
  - 路径绘制
  - 敌人位置标注

### v0.6.0-alpha.3：C++ 战术引擎

- [ ] `include/aam/l3/tactical_engine.hpp`：战术引擎接口
- [ ] `include/aam/l3/dsl_compiler.hpp`：DSL 编译器
- [ ] `src/l3_tactical/tactical_vm.cpp`：战术虚拟机
- [ ] `src/l3_tactical/bytecode/opcodes.hpp`：操作码枚举
- [ ] `src/l3_tactical/bytecode/assembler.cpp`：汇编器
- [ ] `src/l3_tactical/cost_manager.cpp`：费用管理
- [ ] `src/l3_tactical/collision_predictor.cpp`：碰撞预测

### v0.6.0-alpha.4：战术脚本

- [ ] `tactics/1-7_default.tactical`：1-7 默认战术

**验收标准**
- VM 执行 1000 条指令 < 2ms
- 费用预测误差 < 2 费

---

## v0.7.0-beta：桥接版（The Bridge）

**目标**: C++ 核心与 Python 通信，支持故障转移。

### v0.7.0-beta.1：Bridge 接口

- [ ] `bridge/include/aam_bridge/ipc_client.hpp`：IPC 客户端
- [ ] `bridge/include/aam_bridge/transport_factory.hpp`：传输工厂

### v0.7.0-beta.2：Bridge 实现

- [ ] `bridge/src/grpc_client.cpp`：gRPC 客户端
- [ ] `bridge/src/shm_segment.cpp`：共享内存传输
- [ ] `bridge/src/pybind_module.cpp`：PyBind11 模块

### v0.7.0-beta.3：Python 服务端

- [ ] `inference/services/grpc_server.py`：gRPC 服务端
- [ ] `inference/services/websocket_server.py`：WebSocket 服务端
- [ ] `inference/services/l5_controller.py`：L5 控制器

### v0.7.0-beta.4：监控与容错

- [ ] `bridge/src/pybind_adapter.hpp`：Python 健康监控
- [ ] `core/src/common/bus.cpp`：事件总线

**验收标准**
- 跨语言往返 P99 < 150ms
- Python 崩溃后 10 秒内降级

---

## v0.8.0-beta：决策版（The Brain）

**目标**: L5 层 LLM 决策。

### v0.8.0-beta.1：LLM 适配器

- [ ] `inference/services/llm_adapters/openai_adapter.py`：OpenAI GPT-4V
- [ ] `inference/services/llm_adapters/claude_adapter.py`：Claude 3
- [ ] `inference/services/llm_adapters/local_llava.py`：本地模型

### v0.8.0-beta.2：提示工程

- [ ] `configs/inference/prompt_templates/system_expert.txt`
- [ ] `configs/inference/prompt_templates/cot_format.txt`
- [ ] `configs/inference/llm_providers.yaml`

**验收标准**
- GPT-4V 首 Token < 1000ms
- 决策准确率 > 90%

---

## v0.9.0-beta：界面版（The Interface）

**目标**: Qt6 GUI。

### v0.9.0-beta.1：GUI 抽象层

- [ ] `gui/abstract/include/aam_gui/i_main_window.hpp`
- [ ] `gui/abstract/include/aam_gui/i_map_canvas.hpp`
- [ ] `gui/abstract/src/event_dispatcher.cpp`

### v0.9.0-beta.2：Qt6 实现

- [ ] `gui/qt/src/main_window.cpp`
- [ ] `gui/qt/src/map_view.cpp`
- [ ] `gui/qt/src/operator_palette.cpp`
- [ ] `gui/qt/src/qt_event_bridge.cpp`

### v0.9.0-beta.3：监控面板

- [ ] `gui/panels/monitor.py`

**验收标准**
- 预览 60fps 无掉帧
- 交互延迟 < 100ms

---

## v1.0.0-rc：生产候选版（The Release）

**目标**: 全链路集成测试。

### v1.0.0-rc.1：端到端测试

- [ ] `tests/e2e/test_1_7_clear.py`
- [ ] `tests/e2e/test_crisis_contract.py`
- [ ] `tests/e2e/test_recovery.py`

### v1.0.0-rc.2：工程化

- [ ] `scripts/setup/install_deps_windows.ps1`
- [ ] `scripts/setup/setup_vcpkg.sh`
- [ ] `.github/workflows/release.yml`

### v1.0.0-rc.3：文档

- [ ] `README.md`
- [ ] `docs/ARCHITECTURE.md`
- [ ] `docs/API.md`

**验收标准（冻结线）**
- 8 小时连续运行零崩溃
- P99 延迟 < 500ms
- 非技术用户 5 分钟内完成安装

---

## v1.1.0：专业版（The Professional）

**目标**: Windows WPF 深度集成。

- [ ] `gui/wpf/AAM.WPF/`：WPF 项目
- [ ] `gui/wpf/AAM.Native/cpp_cli_bridge.cpp`：C++/CLI 桥接
- [ ] `plugins/sdk/include/aam_plugin.hpp`：插件 SDK

---

## v1.2.0：生态版（The Ecosystem）

**目标**: 插件系统与社区市场。

- [ ] `plugins/examples/auto_credit_shop.cpp`
- [ ] `market/schemas/tactical_v1.json`
- [ ] `.github/workflows/lts.yml`

---

## 已实现功能汇总

### Python L1 感知层（已完成）

| 模块 | 文件 | 功能 |
|------|------|------|
| 游戏状态检测 | `vision/game_state_detector.py` | OCR 识别对局状态 |
| GUI 匹配 | `vision/gui_matcher.py` | 模板匹配 + OCR |
| 增强 GUI | `vision/enhanced_gui_matcher.py` | 主界面分析 |
| 编队识别 | `vision/squad_recognizer.py` | 精英化/等级/名称 |
| 编队分析 | `vision/squad_analyzer.py` | 整合数据库查询 |
| 文字定位 | `vision/text_locator.py` | 精确像素位置 |

### Python L3 战术层（已完成）

| 模块 | 文件 | 功能 |
|------|------|------|
| 关卡分析 | `map/level_analyzer.py` | 地图/路径/波次解析 |
| 地图可视化 | `map/map_visualizer.py` | 可视化输出 |

### Python L4 状态层（已完成）

| 模块 | 文件 | 功能 |
|------|------|------|
| 数据模型 | `data/models/*.py` | 干员/关卡/物品/敌人 |
| 数据提供者 | `data/providers/*.py` | GitHub/PRTS 数据源 |
| 数据库 | `data/database/*.py` | SQLite 结构化存储 |
| 干员匹配 | `data/operator_matcher.py` | 模糊匹配 |

### Python L5 决策层（未开始）

| 模块 | 文件 | 状态 |
|------|------|------|
| LLM 适配器 | `services/llm_adapters/*.py` | ❌ |
| gRPC 服务 | `services/grpc_server.py` | ❌ |
| L5 控制器 | `services/l5_controller.py` | ❌ |

---

**版本依赖关系图**:
```
v0.1.0 (契约) 
    → v0.2.0 (L0) → v0.3.0 (L2) → v0.4.0 (L4) → v0.5.0 (L1) → v0.6.0 (L3) [C++ 核心闭环]
                                                    ↓
                                              v0.7.0 (Bridge) → v0.8.0 (L5) [Python 决策闭环]
                                                    ↓
                                              v0.9.0 (GUI) → v1.0.0 (Release)
```

**并行开发路径**:
```
Python L1/L3/L4 (已完成) ──┐
                          ├──→ v0.7.0 (Bridge) 集成
C++ L0-L4 (待开发) ───────┘
```
