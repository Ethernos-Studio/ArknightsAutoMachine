<!-- ==========================================================================
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
  @file PROTOCOL_REVIEW_GUIDELINES.md
  @author Ethernos Studio
  @brief Protocol Buffer 变更审查规范
  =============================================================================
  版本: v0.1.0-alpha.3
  功能: 定义 .proto 文件变更的审查流程与标准
  适用范围: 所有 proto/ 目录下的 .proto 文件变更
  ============================================================================= -->

# Protocol Buffer 变更审查规范

## 概述

本文档定义了 AAM 项目中 Protocol Buffer 文件变更的审查流程与标准。
所有对 `proto/` 目录下 `.proto` 文件的修改必须经过此审查流程。

## 审查触发条件

以下变更必须触发审查流程：

- [ ] 新增、修改或删除 `.proto` 文件
- [ ] 修改消息（Message）定义
- [ ] 修改字段（Field）定义（包括字段编号、类型、名称）
- [ ] 修改枚举（Enum）定义
- [ ] 修改服务（Service）定义
- [ ] 修改包（Package）声明
- [ ] 修改选项（Option）

## 审查人员要求

### 双 Reviewer 批准制度

所有 `.proto` 文件变更必须经过 **两名** Reviewer 的批准：

1. **架构师 Reviewer**（必须）
   - 审查接口设计的合理性和一致性
   - 确保符合 L0-L5 层级通信规范
   - 验证向后兼容性

2. **安全/质量 Reviewer**（必须）
   - 审查潜在的安全风险
   - 验证 Buf Lint 规则合规性
   - 检查 Breaking Change 影响

### Reviewer 指定方式

- 架构师 Reviewer：由 `@Ethernos-Studio/architects` 团队成员担任
- 安全/质量 Reviewer：由 `@Ethernos-Studio/security` 或 `@Ethernos-Studio/qa` 团队成员担任

## 审查检查清单

### 1. 向后兼容性检查

- [ ] **字段编号变更**：禁止修改已有字段的字段编号
- [ ] **字段删除**：删除字段时必须保留字段编号（使用 `reserved`）
- [ ] **字段类型变更**：禁止修改已有字段的类型
- [ ] **字段名称变更**：允许，但需谨慎评估影响
- [ ] **消息删除**：禁止删除已发布的消息类型

### 2. Buf Lint 合规性检查

所有变更必须通过 Buf Lint：

```bash
cd proto
buf lint
```

检查项包括：
- [ ] 包名符合规范（`aam.{layer}.{module}`）
- [ ] 字段命名符合 `lower_snake_case`
- [ ] 消息命名符合 `PascalCase`
- [ ] 枚举零值使用 `_{ENUM_NAME}_NONE` 后缀
- [ ] 服务名以 `Service` 结尾

### 3. Breaking Change 检测

所有变更必须通过 Buf Breaking Change 检测：

```bash
cd proto
buf breaking --against '.git#branch=main,subdir=proto'
```

- [ ] 零 Breaking Change，或
- [ ] Breaking Change 已记录在 `proto/BREAKING_CHANGES.md` 并获架构师批准

### 4. 层级通信规范检查

- [ ] L0 层消息：仅包含原始感知数据（帧、坐标）
- [ ] L1 层消息：仅包含视觉识别结果（目标、OCR）
- [ ] L2 层消息：仅包含原子操作（点击、滑动）
- [ ] L3 层消息：仅包含战术指令（部署、技能）
- [ ] L4 层消息：仅包含状态查询/更新
- [ ] L5 层消息：仅包含策略决策

### 5. 安全审查

- [ ] 无敏感信息硬编码（密钥、密码、IP 地址）
- [ ] 字段有适当的验证注释（`validate.rules`）
- [ ] 字符串字段有长度限制
- [ ] 数值字段有范围限制

### 6. 文档完整性

- [ ] 所有消息有 `.proto` 文件级注释
- [ ] 所有字段有字段级注释
- [ ] 复杂业务逻辑有使用示例
- [ ] 变更记录在 `proto/CHANGELOG.md`

## 审查流程

### 标准流程（非 Breaking Change）

```
开发者提交 PR
    ↓
CI 自动运行 Buf Lint + Breaking Change 检测
    ↓
架构师 Reviewer 审查接口设计
    ↓ [批准]
安全/质量 Reviewer 审查合规性
    ↓ [批准]
合并到 main 分支
```

### Breaking Change 流程

```
开发者提交 PR（标记为 Breaking Change）
    ↓
CI 自动运行 Buf Lint + Breaking Change 检测（预期失败）
    ↓
架构师 Reviewer 评估 Breaking Change 必要性
    ↓ [批准并记录]
安全/质量 Reviewer 评估影响范围
    ↓ [批准]
更新版本号（Minor 或 Major）
    ↓
合并到 main 分支
```

## 禁止的变更类型

以下变更类型**绝对禁止**，无论经过何种审查：

1. **修改已发布字段的字段编号**：会导致数据解析错误
2. **删除已发布字段的字段编号而不使用 `reserved`**：会导致字段编号重用
3. **修改已发布字段的类型**：会导致二进制不兼容
4. **在已发布的枚举中插入新值（非末尾）**：会导致数值语义改变
5. **修改已发布消息的包名**：会导致代码生成路径改变

## 允许的变更类型

以下变更类型**允许**，但需遵循审查流程：

1. **添加新字段**：使用新的字段编号
2. **添加新消息**：不影响已有代码
3. **添加新服务/方法**：不影响已有代码
4. **修改字段注释**：不影响二进制格式
5. **添加 `reserved` 声明**：保护已删除字段的编号
6. **修改选项**：如 `deprecated` 标记

## 审查工具

### 自动化检查

CI 自动执行以下检查：

- `buf lint`：代码风格检查
- `buf breaking`：兼容性检查
- `protoc` 代码生成：验证语法正确性

### 手动审查

Reviewer 使用以下工具辅助审查：

- GitHub PR 界面：行级评论
- Buf Studio：可视化消息结构
- IDE Protobuf 插件：语法高亮和验证

## 例外情况

在以下情况下，可以简化审查流程：

1. **仅修改注释**：无需双 Reviewer，一名 Reviewer 即可
2. **添加 `deprecated` 选项**：无需双 Reviewer，一名 Reviewer 即可
3. **紧急安全修复**：可先合并后补审查，但必须在 24 小时内完成

## 相关文档

- [AAM 架构规范](ARCHITECTURE.md)
- [Buf 配置](proto/buf.yaml)
- [Protobuf 风格指南](https://developers.google.com/protocol-buffers/docs/style)

## 修订历史

| 版本 | 日期 | 变更说明 |
|------|------|----------|
| v0.1.0-alpha.3 | 2026-04-04 | 初始版本，定义双 Reviewer 审查制度 |
