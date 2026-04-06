# include/aam - 公共接口层

## 目录说明

本目录包含 AAM Core 的所有公共头文件，定义了 L0-L4 层的接口契约。

## 目录结构

```
include/aam/
├── core/              # 核心基础设施
├── l0/                # L0 感知层接口
├── l1/                # L1 视觉层接口
├── l2/                # L2 运动层接口
├── l3/                # L3 战术层接口
└── l4/                # L4 状态层接口
```

## 接口设计原则

### 1. 纯虚接口（ISP）
```cpp
class ICaptureBackend {
public:
    virtual ~ICaptureBackend() = default;
    virtual Frame capture() = 0;
    virtual void start() = 0;
    virtual void stop() = 0;
};
```

### 2. 非虚接口模式（NVI）
```cpp
class ActionExecutor {
public:
    bool execute(const Action& action) {
        if (!validate(action)) return false;
        return doExecute(action);
    }
protected:
    virtual bool doExecute(const Action& action) = 0;
};
```

### 3. 依赖注入
```cpp
class PerceptionPipeline {
public:
    explicit PerceptionPipeline(
        std::unique_ptr<IGPUAccelerator> gpu,
        std::unique_ptr<IOCREngine> ocr
    );
};
```

## 命名规范

### 文件命名
- 头文件: `snake_case.hpp`
- 接口类: `I` 前缀，如 `ICaptureBackend`
- 实现类: 无特殊前缀，如 `ADBCapture`

### 命名空间
```cpp
namespace aam::l0 { }  // L0 层
namespace aam::l1 { }  // L1 层
namespace aam::core { }  // 核心工具
```

## 前向声明

减少编译依赖，使用前置声明:

```cpp
// 好：前向声明
namespace cv { class Mat; }
class Frame;

// 避免：不必要的包含
#include <opencv2/core.hpp>
#include "frame.hpp"
```

## 异常规范

- 接口不抛出异常: `noexcept`
- 可能失败返回 `std::expected<T, Error>` (C++23)
- 异步操作返回 `std::future<T>`

## 相关目录

- [core/src/](../src/): 接口实现
- [proto/ama/](../../proto/ama/): 协议定义
