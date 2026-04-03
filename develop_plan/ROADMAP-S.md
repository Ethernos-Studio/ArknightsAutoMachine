**AAM 版本测试规范与验收标准（Testing Specification & Acceptance Criteria）**

本文档作为《AAM 版本化开发路线图》的补充，专注于**测试策略**、**验收标准**与**质量门禁**。

**图例说明：**
- ✅ 已验证通过
- 🔶 部分验证
- ❌ 未验证

---

## 测试总览

| 层级 | Python 实现 | C++ 实现 | 测试覆盖 |
|------|-------------|----------|----------|
| L0 感知层 | - | ❌ | ❌ |
| L1 视觉层 | ✅ | ❌ | 🔶 Python 已测试 |
| L2 运动层 | - | ❌ | ❌ |
| L3 战术层 | ✅ | ❌ | 🔶 Python 已测试 |
| L4 状态层 | ✅ | ❌ | 🔶 Python 已测试 |
| L5 决策层 | ❌ | - | ❌ |

---

## v0.1.0-alpha：契约冻结版

### Test Strategy（测试策略）

| 测试类型 | 方法 | 工具 |
|----------|------|------|
| 契约兼容性 | Breaking Change 检测 | Buf CLI |
| 生成代码编译 | 三平台编译测试 | MSVC/GCC/Clang |
| 静态安全 | 漏洞扫描 | CodeQL |

### Acceptance Criteria（验收标准）

- [ ] Buf Breaking Change 检测通过（零不兼容变更）
- [ ] 三平台编译成功，零警告（`-Werror`）
- [ ] CodeQL 扫描零 Critical/High 漏洞
- [ ] `.proto` 文件变更需双 Reviewer 批准

### Sign-off Checklist（签核清单）

- [ ] 架构师确认接口设计满足 L0-L5 通信需求
- [ ] 安全审核确认无硬编码密钥
- [ ] 版本 Tag `v0.1.0-alpha` 已打

---

## v0.2.0-alpha：感知硬化版

### Test Strategy

| 测试类型 | 方法 | 指标 |
|----------|------|------|
| 帧同步 | 连续捕获 1000 帧 | 时间戳单调递增，无丢帧 |
| 内存泄漏 | Valgrind Massif | definitely lost: 0 bytes |
| 延迟测量 | 高精度计时器 | P50/P99/P99.9 直方图 |
| 压力测试 | 72 小时连续运行 | 内存增长斜率 ≈ 0 |

### Acceptance Criteria

- [ ] **功能**：`AdbCaptureBackend` 稳定捕获 1920x1080@60fps
- [ ] **延迟**：P99 < 20ms（屏幕渲染到 FrameBuffer）
- [ ] **内存**：Valgrind 报告无泄漏
- [ ] **并发**：ThreadSanitizer 验证无数据竞争
- [ ] **兼容性**：Windows MediaFoundation / Linux VAAPI

### Sign-off Checklist

- [ ] 性能测试报告（延迟直方图 CSV）
- [ ] 内存分析报告（Valgrind XML）
- [ ] 跨平台构建验证

---

## v0.3.0-alpha：空间映射版

### Test Strategy

| 测试类型 | 方法 | 指标 |
|----------|------|------|
| 精度测试 | 9x5 网格全点测量 | 误差热力图 |
| 拟人性测试 | 1000 条轨迹统计 | KS 检验 p-value > 0.05 |
| 反馈闭环 | 1000 次点击确认 | 成功率 ≥ 99.8% |
| 多分辨率 | 1080p/2K/4K 分别测试 | 误差均 < 3px |

### Acceptance Criteria

- [ ] **精度**：坐标转换误差 < 2px（1080p），< 3px（2K/4K）
- [ ] **拟人性**：操作间隔符合正态分布 N(80ms, 15ms)
- [ ] **可靠性**：反馈闭环成功率 ≥ 99.8%
- [ ] **兼容性**：ADB 与 Win32 输入后端可运行时切换

### Sign-off Checklist

- [ ] 坐标精度测试报告（误差热力图）
- [ ] 行为统计测试报告（KS 检验结果）
- [ ] 至少 2 种输入设备配置验证

---

## v0.4.0-alpha：记忆架构版

### Python 模块测试（已完成）✅

| 模块 | 测试类型 | 状态 |
|------|----------|------|
| `data/models/*.py` | 单元测试 | ✅ 数据模型验证 |
| `data/providers/github_provider.py` | 集成测试 | ✅ GitHub API 连接 |
| `data/providers/prts_provider.py` | 集成测试 | ✅ MediaWiki API |
| `data/database/manager.py` | 单元测试 | ✅ CRUD 操作 |
| `data/operator_matcher.py` | 单元测试 | ✅ 模糊匹配算法 |

### C++ 模块测试（待开发）

| 测试类型 | 方法 | 指标 |
|----------|------|------|
| 状态机转换 | 状态序列模拟 | 切换延迟 < 50ms |
| 崩溃恢复 | `kill -9` 模拟 | 丢失进度 < 5 秒 |
| 存储压缩 | 1000 帧比较 | 压缩率 ≥ 90% |
| 数据一致性 | PRTS 源数据比对 | 字段完整率 100% |

### Acceptance Criteria

- [x] **Python 数据模型**：干员/关卡/物品/敌人模型完整
- [x] **Python 数据源**：GitHub + PRTS 双源可用
- [x] **Python 数据库**：SQLite 结构化存储可用
- [x] **Python 匹配器**：模糊匹配算法正确
- [ ] **C++ 状态机**：状态切换延迟 P99 < 50ms
- [ ] **C++ 崩溃恢复**：丢失进度 < 5 秒
- [ ] **C++ 压缩**：增量编码压缩率 ≥ 90%

### Sign-off Checklist

- [x] Python 模块单元测试通过
- [ ] C++ 状态机转换延迟测试报告
- [ ] 崩溃恢复测试录像
- [ ] PRTS 数据同步日志

---

## v0.5.0-alpha：视觉皮层版

### Python 模块测试（已完成）✅

| 模块 | 测试类型 | 状态 |
|------|----------|------|
| `vision/game_state_detector.py` | 功能测试 | ✅ 对局状态识别 |
| `vision/gui_matcher.py` | 功能测试 | ✅ 模板匹配 + OCR |
| `vision/enhanced_gui_matcher.py` | 功能测试 | ✅ 主界面分析 |
| `vision/squad_recognizer.py` | 功能测试 | ✅ 编队识别 |
| `vision/squad_analyzer.py` | 集成测试 | ✅ 数据库整合 |
| `vision/text_locator.py` | 功能测试 | ✅ 文字定位 |

### C++ 模块测试（待开发）

| 测试类型 | 方法 | 指标 |
|----------|------|------|
| GPU 性能 | NVIDIA Nsight | CUDA 核函数时间 |
| OCR 准确率 | 100 张截图测试 | 准确率 > 95% |
| YOLO 检测 | COCO 格式测试集 | mAP@0.5 > 0.85 |
| 显存占用 | 运行时监控 | 峰值 < 500MB |

### Acceptance Criteria

- [x] **Python 游戏状态**：OCR 识别"剩余可放置角色"
- [x] **Python GUI 匹配**：多尺度模板匹配 + NMS
- [x] **Python 编队识别**：精英化/等级/名称识别
- [x] **Python 文字定位**：精确像素位置返回
- [ ] **C++ GPU 预处理**：CUDA 预处理 < 8ms
- [ ] **C++ YOLO 推理**：单帧推理 < 20ms
- [ ] **C++ 总延迟**：L1 处理 < 30ms

### Sign-off Checklist

- [x] Python 视觉模块功能测试通过
- [ ] NVIDIA Nsight 性能分析报告
- [ ] OCR 准确率测试集与结果
- [ ] YOLO mAP 评估报告

---

## v0.6.0-alpha：战术执行版

### Python 模块测试（已完成）✅

| 模块 | 测试类型 | 状态 |
|------|----------|------|
| `map/level_analyzer.py` | 单元测试 | ✅ 地图/路径/波次解析 |
| `map/map_visualizer.py` | 功能测试 | ✅ 可视化输出 |

### C++ 模块测试（待开发）

| 测试类型 | 方法 | 指标 |
|----------|------|------|
| VM 性能 | 10000 条指令执行 | 单条 < 2μs |
| DSL 编译 | 10 个复杂脚本 | 零内存泄漏 |
| 费用预测 | 20 个随机曲线 | 误差 < 2 费 |
| 沙箱安全 | 恶意脚本测试 | 强制终止生效 |

### Acceptance Criteria

- [x] **Python 关卡分析**：地块/路径/波次数据结构完整
- [x] **Python 地图可视化**：颜色映射/路径绘制正确
- [ ] **C++ VM 性能**：1000 条指令 < 2ms
- [ ] **C++ DSL 编译**：零内存泄漏
- [ ] **C++ 费用预测**：误差 < 2 费
- [ ] **C++ 沙箱安全**：无限循环被强制终止

### Sign-off Checklist

- [x] Python 地图模块功能测试通过
- [ ] VM 性能基准测试报告
- [ ] DSL 编译测试用例集
- [ ] 沙箱安全测试报告

---

## v0.7.0-beta：桥接版

### Test Strategy

| 测试类型 | 方法 | 指标 |
|----------|------|------|
| 延迟测试 | 往返时间测量 | P99 < 150ms |
| 故障注入 | `kill -9` Python | 10 秒内降级 |
| 吞吐量 | 60fps 持续传输 | 5 分钟无丢帧 |
| 零拷贝 | SHM vs gRPC 对比 | 延迟降低 3 倍 |

### Acceptance Criteria

- [ ] **延迟**：跨语言往返 P99 < 150ms
- [ ] **容错**：Python 崩溃后 10 秒内降级
- [ ] **吞吐量**：支持 60fps 持续传输
- [ ] **零拷贝**：SHM 传输延迟 < gRPC 的 1/3

### Sign-off Checklist

- [ ] 跨语言延迟测试报告
- [ ] 故障注入测试录像
- [ ] 吞吐量测试日志

---

## v0.8.0-beta：决策版

### Test Strategy

| 测试类型 | 方法 | 指标 |
|----------|------|------|
| 决策准确率 | 50 局人工标注对比 | 准确率 > 90% |
| 延迟测试 | 首 Token 时间 | GPT-4V < 1000ms |
| 热重载 | 配置文件修改 | 5 秒内生效 |
| 视觉匹配 | YOLO 结果对比 | 准确率 > 95% |

### Acceptance Criteria

- [ ] **准确率**：1-7 关卡部署正确率 > 90%
- [ ] **延迟**：GPT-4V 首 Token < 1000ms
- [ ] **热重载**：配置修改 5 秒内生效
- [ ] **匹配**：干员识别准确率 > 95%

### Sign-off Checklist

- [ ] 决策准确率人工评估报告
- [ ] 延迟测试报告
- [ ] 热重载功能演示录像

---

## v0.9.0-beta：界面版

### Test Strategy

| 测试类型 | 方法 | 指标 |
|----------|------|------|
| 帧率测试 | FPS 计数器 | 掉帧率 < 1% |
| 交互延迟 | 高速摄像/软件计时 | < 100ms |
| 跨平台构建 | exe/AppImage | 独立运行 |
| OpenGL 兼容 | 多显卡测试 | 无黑屏/纹理错误 |

### Acceptance Criteria

- [ ] **帧率**：预览 60fps 稳定，掉帧率 < 1%
- [ ] **交互**：GUI 点击到 ADB 执行 < 100ms
- [ ] **跨平台**：Windows/Linux 均可启动
- [ ] **兼容性**：Intel/AMD/NVIDIA 显卡正常

### Sign-off Checklist

- [ ] FPS 测试日志
- [ ] 跨平台安装包
- [ ] UI 兼容性测试报告

---

## v1.0.0-rc：生产候选版

### Test Strategy

| 测试类型 | 方法 | 指标 |
|----------|------|------|
| 稳定性 | 8 小时连续运行 | 零崩溃 |
| 端到端 | 10 次 1-7 通关 | 成功率 ≥ 95% |
| 异常恢复 | 网络/游戏崩溃 | 10 秒内恢复 |
| 用户体验 | 3 名非技术用户 | 5 分钟内完成安装 |

### Acceptance Criteria（冻结线）

- [ ] **稳定性**：8 小时零崩溃，无内存泄漏
- [ ] **成功率**：1-7 自动通关 ≥ 95%
- [ ] **恢复**：网络断开 5 秒后 10 秒内恢复
- [ ] **易用性**：非技术用户 5 分钟内完成安装

### Sign-off Checklist（Release Checklist）

- [ ] 版本 Tag `v1.0.0` 已打
- [ ] Release Notes 已撰写
- [ ] 安装包 SHA256 校验和已公布
- [ ] 文档完整（用户手册/架构/API）
- [ ] 安全审计通过
- [ ] 性能基准报告（P99 < 500ms）

---

## 已完成模块测试详情

### Python L1 视觉层测试

#### `game_state_detector.py` 测试用例

```python
# 测试用例结构
test_cases = [
    {"image": "screenshot_in_battle.png", "expected": GameState.IN_BATTLE},
    {"image": "screenshot_main_menu.png", "expected": GameState.NOT_IN_BATTLE},
    {"image": "screenshot_transition.png", "expected": GameState.TRANSITIONING},
]
```

#### `squad_recognizer.py` 测试用例

```python
# 编队识别测试
test_cases = [
    {"elite_icon": "e1_icon.png", "expected": EliteLevel.E1},
    {"elite_icon": "e2_icon.png", "expected": EliteLevel.E2},
    {"level_image": "level_90.png", "expected": 90},
]
```

### Python L3 战术层测试

#### `level_analyzer.py` 测试用例

```python
# 关卡数据解析测试
test_cases = [
    {"level_id": "level_1_7", "expected_tiles": 45, "expected_routes": 2},
    {"level_id": "level_sk_5", "expected_tiles": 42, "expected_routes": 3},
]
```

### Python L4 状态层测试

#### `operator_matcher.py` 测试用例

```python
# 模糊匹配测试
test_cases = [
    {"input": "阿米娅", "expected": "Amiya", "match_type": "exact"},
    {"input": "能天使", "expected": "Exusiai", "match_type": "partial"},
    {"input": "银灰", "expected": "SilverAsh", "match_type": "fuzzy"},
]
```

---

## 测试工具清单

| 工具 | 用途 | 平台 |
|------|------|------|
| pytest | Python 单元测试 | 跨平台 |
| Valgrind | 内存泄漏检测 | Linux |
| ThreadSanitizer | 数据竞争检测 | Linux/macOS |
| NVIDIA Nsight | GPU 性能分析 | Windows/Linux |
| CodeQL | 静态安全分析 | GitHub Actions |
| Buf CLI | Proto 兼容性检测 | 跨平台 |

---

## 质量门禁

### 代码合并门禁

| 检查项 | 阈值 | 强制 |
|--------|------|------|
| 单元测试覆盖率 | ≥ 85% | ✅ |
| 核心路径覆盖率 | 100% | ✅ |
| 静态分析警告 | 0 | ✅ |
| CodeQL 高危漏洞 | 0 | ✅ |

### 版本发布门禁

| 检查项 | 阈值 | 强制 |
|--------|------|------|
| 集成测试通过 | 100% | ✅ |
| 性能基准达标 | P99 < 目标值 | ✅ |
| 文档完整性 | 100% | ✅ |
| 用户验收测试 | 3/3 通过 | ✅ |
