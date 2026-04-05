<!--
  Copyright (C) 2026 Ethernos Studio
  This file is part of Arknights Auto Machine (AAM).

  AAM is free software: you can redistribute it and/or modify
  it under the terms of the GNU Affero General Public License as published
  by the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  AAM is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU Affero General Public License for more details.

  You should have received a copy of the GNU Affero General Public License
  along with AAM. If not, see <https://www.gnu.org/licenses/>.
  =============================================================================
  @file COLLABORATION.md
  @author dhjs0000
  @brief Arknights Auto Machine 协作规范
  =============================================================================
  版本: v0.1.0-alpha.3
  功能: 定义 AAM 项目的协作流程、代码审查标准和贡献指南
  =============================================================================
-->

# Arknights Auto Machine 协作规范

> **文档定位**: 本文档面向外部贡献者，定义了参与 AAM 项目的基本协作流程和规范。
>
> **内部团队**: 核心团队成员请同时参阅 [INTERNAL_COLLABORATION.md](./INTERNAL_COLLABORATION.md) 获取更详细的内部开发流程和工具使用指南。

## 概述

本文档定义了 Arknights Auto Machine (AAM) 项目的协作流程、代码审查标准和贡献指南。所有贡献者必须遵守这些规范以确保代码质量和项目一致性。

---

## 分支策略

### 主分支

- `main`: 稳定分支，仅接受通过审查的 PR 合并
- `develop`: 开发分支，功能完成后合并至此进行集成测试

### 功能分支命名规范

```
feature/<layer>-<description>    # 新功能 (例: feature/l1-add-ocr-module)
fix/<layer>-<issue-id>           # Bug 修复 (例: fix/l3-memory-leak-42)
hotfix/<description>             # 紧急修复
refactor/<layer>-<description>   # 重构
chore/<description>              # 维护任务
docs/<description>               # 文档更新
```

---

## 提交规范

### Commit Message 格式

```
<type>(<scope>): <subject>

<body>

<footer>
```

#### Type 类型

| 类型 | 说明 |
|------|------|
| `feat` | 新功能 |
| `fix` | Bug 修复 |
| `refactor` | 代码重构 |
| `perf` | 性能优化 |
| `test` | 测试相关 |
| `docs` | 文档更新 |
| `chore` | 构建/工具/依赖更新 |
| `style` | 代码格式调整 |

#### Scope 范围

- `core`: 核心框架
- `l0`: 数据采集层
- `l1`: 感知层
- `l2`: 行动层
- `l3`: 战术层
- `l4`: 状态层
- `l5`: 策略层
- `bridge`: 桥接层
- `proto`: 协议定义
- `ci`: 持续集成
- `build`: 构建系统

#### 示例

```
feat(l1): 添加基于 YOLOv8 的干员检测模块

- 实现干员头像检测模型
- 添加置信度阈值配置
- 集成到 L1PerceptionEngine

Closes #123
```

---

## Pull Request 规范

### PR 模板

创建 PR 时必须填写以下信息：

```markdown
## 变更摘要
<!-- 简要描述本次变更的内容 -->

## 关联 Issue
<!-- 关联的 Issue 编号，如: Fixes #123 -->

## 变更类型
- [ ] 新功能 (feat)
- [ ] Bug 修复 (fix)
- [ ] 重构 (refactor)
- [ ] 性能优化 (perf)
- [ ] 文档更新 (docs)
- [ ] 其他

## 架构合规性检查
- [ ] 变更符合分层架构设计
- [ ] 未引入跨层依赖
- [ ] 接口变更已更新协议文档

## 测试
- [ ] 单元测试通过
- [ ] 集成测试通过
- [ ] 手动测试验证

## 代码审查
- [ ] 自我审查完成
- [ ] 代码符合项目规范
- [ ] 无硬编码敏感信息
```

### PR 审查流程

1. **创建 PR**: 从功能分支向 `develop` 分支创建 PR
2. **自动化检查**: CI 必须全部通过
   - 代码格式检查 (clang-format)
   - 协议兼容性检查 (Buf)
   - 三平台构建 (Windows/Linux/macOS)
   - 安全扫描 (CodeQL)
3. **人工审查**: 至少需要 1 名审查者批准
4. **合并**: 通过审查后由维护者合并

---

## 代码审查标准

### 审查清单

#### 功能性
- [ ] 代码实现符合需求描述
- [ ] 边界条件处理正确
- [ ] 错误处理完善
- [ ] 无内存泄漏或资源泄漏

#### 架构合规性
- [ ] 符合分层架构设计原则
- [ ] 无跨层调用
- [ ] 接口定义符合协议规范
- [ ] 依赖关系合理

#### 代码质量
- [ ] 命名清晰、一致
- [ ] 函数职责单一
- [ ] 复杂度适中
- [ ] 注释清晰完整

#### 性能与安全
- [ ] 无性能瓶颈
- [ ] 线程安全（如适用）
- [ ] 无安全隐患
- [ ] 无硬编码密钥或凭证

#### 测试
- [ ] 单元测试覆盖核心逻辑
- [ ] 测试用例有效
- [ ] 边界条件有测试

### 审查注释规范

使用标准前缀标记审查意见：

```
[BLOCKER]  - 必须修复，阻止合并
[CRITICAL] - 严重问题，强烈建议修复
[WARNING]  - 警告，建议修复
[SUGGESTION] - 建议，可选修复
[NIT]      - 小问题，风格相关
[QUESTION] - 疑问，需要澄清
```

---

## 协议变更流程

### .proto 文件变更

1. **变更前评估**
   - 评估是否为破坏性变更
   - 如需破坏性变更，需架构师批准

2. **兼容性检查**
   ```bash
   cd proto
   buf breaking --against '.git#branch=main'
   ```

3. **审查要求**
   - 必须两名审查者批准
   - 至少一名熟悉业务逻辑
   - 至少一名熟悉技术实现

4. **版本管理**
   - 向后兼容变更：更新 minor 版本
   - 破坏性变更：更新 major 版本

---

## 发布流程

### 版本号规范

遵循 [Semantic Versioning 2.0.0](https://semver.org/lang/zh-CN/):

```
主版本号.次版本号.修订号-预发布标识

例: 0.1.0-alpha.3
```

### 发布步骤

1. **准备阶段**
   - 确保 `develop` 分支所有 CI 通过
   - 更新版本号
   - 更新 CHANGELOG.md

2. **创建发布分支**
   ```bash
   git checkout -b release/v0.1.0 develop
   ```

3. **版本冻结**
   - 仅接受 Bug 修复
   - 进行回归测试

4. **合并发布**
   ```bash
   git checkout main
   git merge --no-ff release/v0.1.0
   git tag -a v0.1.0 -m "Release version 0.1.0"
   ```

5. **后续操作**
   - 合并回 `develop`
   - 删除发布分支
   - 创建 GitHub Release

---

## 沟通规范

### Issue 报告

提交 Issue 时使用对应模板：

- **Bug 报告**: 描述问题、复现步骤、期望行为、环境信息
- **功能请求**: 描述需求、使用场景、预期收益
- **技术债务**: 描述问题、影响范围、建议方案

### 讨论渠道

- **技术讨论**: GitHub Discussions
- **Bug 报告**: GitHub Issues
- **实时沟通**: 项目指定的即时通讯工具
- **代码审查**: GitHub PR 评论

### 沟通准则

1. 尊重他人，保持专业
2. 技术讨论对事不对人
3. 及时响应审查意见
4. 重要决策需文档化

---

## 开发环境设置

### 必需工具

- CMake >= 3.20
- C++23 兼容编译器
- Python >= 3.10
- vcpkg
- Buf CLI (用于协议开发)

### 推荐 IDE 配置

- **VS Code**: 使用项目提供的 `.vscode` 配置
- **CLion**: 自动识别 CMake 项目
- **Visual Studio**: 使用 "Open Folder" 功能

### 预提交检查

提交前请运行：

```bash
# 代码格式化
find . -name "*.cpp" -o -name "*.hpp" -o -name "*.h" | xargs clang-format -i

# 协议检查
cd proto && buf lint

# 本地构建测试
cmake -B build -S .
cmake --build build --parallel
```

---

## 许可证与版权

### 代码头模板

所有源文件必须包含标准文件头：

```cpp
// =============================================================================
// Copyright (C) 2026 Ethernos Studio
// This file is part of Arknights Auto Machine (AAM).
//
// AAM is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published
// by the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// AAM is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with AAM. If not, see <https://www.gnu.org/licenses/>.
// =============================================================================
```

### 贡献者协议

通过提交 PR，您同意：
1. 您的贡献将使用 AGPL-3.0 许可证
2. 您拥有贡献内容的版权或有权提交
3. 您的贡献不侵犯第三方权利

---

## 附录

### 相关文档

- [架构设计](docs/ARCHITECTURE.md)
- [协议规范](proto/PROTOCOL.md)
- [API 文档](docs/API.md)
- [构建指南](docs/BUILD.md)
- [协议审查规范](.github/PROTOCOL_REVIEW_GUIDELINES.md)

### 联系方式

- 项目维护者: [Ethernos Studio](https://github.com/Ethernos-Studio)
- 安全问题报告: 请通过 GitHub Security Advisories 提交

---

*最后更新: 2026-04-05*
