**Arknights Auto Machine (AAM) 项目协作章程 (Internal Collaboration Charter)**

**版本**: v1.0  
**生效日期**: 2026-04-03  
**项目Owner**: @dhjs0000  
**核心协作者**: @error-0x12, @ywhdzrb, @OFFMN-SHARP  

---

## 1. 组织架构与角色定义

### 1.1 核心开发组 (Core Team)
| GitHub ID | 主要职责 | 技术栈 | 交付物目录 |
|-----------|----------|--------|------------|
| **@dhjs0000** | 项目Owner & C++架构师 | C++23/WPF/CMake | `core/`, `bridge/`, `gui/wpf/`, `proto/` |
| **@error-0x12** | Python推理后端负责人 | Python/OpenCV/ONNX/gRPC | `inference/`, `models/` |
| **@ywhdzrb** | Linux GUI开发 | Qt5/python | `gui/qt/`, `configs/gui/` |

### 1.2 辅助贡献者 (Contributor)
| GitHub ID | 职责范围 | 工作性质 | 交付物 |
|-----------|----------|----------|--------|
| **@OFFMN-SHARP** | 文档维护 & Windows工具链 | **可选(Optional)** | `docs/`, `scripts/setup/`, `tests/fixtures/` |

> **注**: @OFFMN-SHARP 的任务标记为 `[OPT]` (Optional)，不阻塞主线发布，可随时暂停/恢复，无强制截止日期。

---

## 2. 版本化分工路线图 (Version-Based RACI)

### v0.1.0-alpha ~ v0.6.0-alpha (架构与C++核心)
- **@dhjs0000**: **R/A** (Responsible/Accountable) - 所有C++层实现与协议定义
- **@error-0x12**: **C** (Consulted) - 提供Python侧接口需求，评审`proto/`文件
- **@ywhdzrb**: **I** (Informed) - 了解架构冻结状态，准备Qt环境
- **@OFFMN-SHARP [OPT]**: 维护`docs/setup_guide.md`（Windows环境搭建文档）

### v0.7.0-beta ~ v0.8.0-beta (跨语言桥接与L5)
- **@dhjs0000**: **R/A** - `bridge/`实现，gRPC/SHM传输硬化
- **@error-0x12**: **R/A** - `inference/services/`实现，LLM适配器
- **@ywhdzrb**: **C** - 评审GUI事件总线接口
- **@OFFMN-SHARP [OPT]**: 编写`scripts/setup/install_deps_windows.ps1`（一键安装脚本）

### v0.9.0-beta ~ v1.0.0-rc (GUI与生产化)
- **@dhjs0000**: **R/A** - WPF原型（Windows专业版）
- **@ywhdzrb**: **R/A** - Qt6主实现（跨平台主推）
- **@error-0x12**: **C** - 提供性能监控接口给GUI
- **@OFFMN-SHARP [OPT]**: 准备`tests/fixtures/`测试截图数据（可选，不阻塞）

### v1.1.0+ (专业版与生态)
- **@dhjs0000**: **R** - 架构演进
- **@ywhdzrb**: **R** - Linux生态适配
- **@error-0x12**: **R** - 模型蒸馏与优化
- **@OFFMN-SHARP [OPT]**: 若有意向，可参与`gui/wpf/`的XAML设计（完全可选）

---

## 3. 代码协作流程 (Git Workflow)

### 3.1 分支策略 (Branching Model)
```
main (保护分支，仅Owner可push)
  ↑
develop (集成分支，接受PR)
  ↑
feature/{github-id}/{description} (个人功能分支)
```

**命名规范**:
- 功能分支: `feature/error-0x12/l5-adapter`, `feature/ywhdzrb/qt-map-canvas`
- 修复分支: `fix/dhjs0000/shm-deadlock`
- **@OFFMN-SHARP 分支**: `opt/offmn-sharp/doc-update` (必须带`opt/`前缀标识可选性)

### 3.2 Pull Request 规范
**必须项**:
1. **标题格式**: `[L{层级}] 简短描述` (例: `[L5] Add Claude 3 Vision adapter`, `[OPT] Update Windows setup guide`)
2. **审查要求**:
   - C++代码: 必须 @dhjs0000 审查
   - Python代码: 必须 @error-0x12 审查 + @dhjs0000 架构审查
   - GUI代码: 必须 @ywhdzrb (Linux) 或 @dhjs0000 (Windows) 审查
   - **[OPT]代码**: 仅需 @dhjs0000 轻量审查，可单Reviewer批准
3. **CI通过**: 所有PR必须通过 `.github/workflows/ci.yml` 的三平台编译测试

**豁免条款**:
- @OFFMN-SHARP 的文档类PR (`docs/`, `*.md`) 可跳过C++编译检查，但需通过Markdownlint

### 3.3 提交信息规范 (Commit Message)
```
<type>(<scope>): <subject>

<body> (可选，详细说明)

Footer: (关联Issue，可选)
```

**Type定义**:
- `feat`: 新功能 (例: `feat(L5): implement local LLaVA adapter`)
- `fix`: 修复 (例: `fix(L0): resolve SHM race condition`)
- `docs`: 文档 (例: `docs(setup): add Windows winget instructions`) ← @OFFMN-SHARP 常用
- `opt`: 可选优化 (例: `opt(scripts): improve install script error handling`) ← @OFFMN-SHARP 专用type

---

## 4. 沟通与同步机制

### 4.1 异步沟通 (GitHub)
- **Discussions**: 用于架构决策RFC (Request for Comments)，必须保留至少3天收集意见
- **Issues**: 用于Bug跟踪和任务分配，标签系统：
  - `priority/critical`: 阻塞发布的Bug (仅Owner可标记)
  - `priority/high`: 主线功能
  - `priority/opt`: 可选任务 (@OFFMN-SHARP 的任务统一标记)
  - `component/L{0-5}`: 层级标签
  - `platform/windows`, `platform/linux`: 平台标签

### 4.2 实时沟通 (QQ群/Discord)
- **技术细节确认**: 截图、日志、临时代码片段
- **紧急协调**: 合并冲突、构建失败
- **非技术讨论**: 项目方向、Logo设计

**沟通纪律**:
- 重要技术决策必须在GitHub Discussions或PR评论中归档，QQ群仅作为通知渠道
- @OFFMN-SHARP 无强制参加技术会议义务，可通过异步方式(GitHub)接收信息

### 4.3 同步节奏
- **里程碑冻结**: 每个v0.x.0版本发布前3天，主分支进入冻结期，仅接受Bug修复
- **可选任务截止**: @OFFMN-SHARP 的任务无固定里程碑，可在任何版本合并，或推迟到下一版本

---

## 5. 代码所有权与审查边界 (Code Ownership)

### 5.1 文件级CODEOWNERS (`.github/CODEOWNERS`)
```gitignore
# 核心架构 - 必须Owner审查
/proto/           @dhjs0000
/core/            @dhjs0000
/bridge/          @dhjs0000 @error-0x12

# Python推理 - 必须error-0x12审查
/inference/       @error-0x12 @dhjs0000
/models/          @error-0x12

# GUI - 分平台审查
/gui/qt/          @ywhdzrb @dhjs0000
/gui/wpf/         @dhjs000000
/gui/abstract/    @dhjs0000 @ywhdzrb

# 可选工作 - 单Reviewer即可
/docs/            @dhjs0000 @OFFMN-SHARP
/scripts/setup/   @dhjs0000 @OFFMN-SHARP
/tests/fixtures/  @OFFMN-SHARP
```

### 5.2 豁免条款 (Exemption for OFFMN-SHARP)
- **文档修正**: 拼写错误、格式修正可直接提交PR，无需提前开Issue
- **安装脚本**: Windows PowerShell/Batch脚本可自主测试后提交，CI中的Windows runner会验证
- **测试数据**: 截图数据集(`tests/fixtures/screenshots/`)可批量上传，无需代码审查，仅需文件格式检查

---

## 6. 质量保证与测试责任

### 6.1 单元测试 (Unit Testing)
- **C++**: @dhjs0000 负责 `core/tests/`, @ywhdzrb 负责 `gui/qt/tests/`
- **Python**: @error-0x12 负责 `inference/tests/` 且覆盖率必须 > 80%
- **[OPT]**: @OFFMN-SHARP 无强制测试要求，但提交的`.ps1`脚本应在CI中通过 PSScriptAnalyzer

### 6.2 集成测试 (Integration)
- **L0→L2 Pipeline**: @dhjs0000 负责维护，每周至少运行一次完整测试
- **Bridge通信**: @dhjs0000 与 @error-0x12 共同负责，跨语言集成必须在双方环境验证

---

## 7. 特别条款：针对 @OFFMN-SHARP

### 7.1 参与模式 (Engagement Model)
**"随来随走"原则**:
- 无强制会议出席要求
- 无版本发布阻塞责任（即使任务未完成，也不影响v1.0.0发布）
- 可随时声明"暂停"(`/pause`)，项目进入低活跃状态

### 7.2 建议工作包 (Suggested Work Packages)
**当前可选任务池** (可任选，可多选，可延迟):
1. **[DOC-001]** 完善 `README.md` 的Windows安装章节（预计2小时）
2. **[SCRIPT-001]** 编写 `install_deps_windows.ps1`（预计4小时）
3. **[TEST-001]** 准备10张1-7关卡测试截图存入 `tests/fixtures/`（预计1小时）
4. **[WPF-EXP]** (远期) 探索WPF与C++/CLI桥接原型（完全可选，v1.1.0之后）

**任务领取流程**:
1. 在GitHub Issues中领取带`priority/opt`标签的任务
2. 创建分支 `opt/offmn-sharp/{task-id}`
3. 提交PR，标题前缀 `[OPT]`
4. 合并后由 @dhjs0000 在CHANGELOG的"Contributors"章节致谢

### 7.3 学习支持
- 若想从C#转向Python，@error-0x12 提供`inference/`代码导读（非强制，预约制）
- 若想了解WPF，@dhjs0000 提供 `gui/wpf/` 架构说明（v0.9.0之后）

---

## 8. 决策与争议解决

### 8.1 技术决策
- **架构变更** (如修改L3接口): 必须由 @dhjs0000 发起RFC，全体核心开发组(@error-0x12, @ywhdzrb) 同意
- **依赖变更** (如新增Python包或C++库): 需说明理由，评估跨平台影响
- **可选工作决策**: @OFFMN-SHARP 在负责范围内有完全自主权，无需批准

### 8.2 冲突解决
1. 技术分歧首先在PR评论中讨论
2. 若24小时内无法达成一致，升级到GitHub Discussions投票（核心开发组3票，@OFFMN-SHARP 可选投票）
3. 最终决策权归项目Owner (@dhjs0000)，但需记录决策理由

---

## 9. 附则

**文档修订**: 本章程由 @dhjs0000 维护，重大修订需经核心开发组全员同意。  

**联系我们**:
- 技术架构: @dhjs0000 (studio@ethernos.net)
- Python后端: @error-0x12
- Linux GUI: @ywhdzrb
- 一般咨询: GitHub Discussions (推荐) 或 QQ群 (非技术闲聊)