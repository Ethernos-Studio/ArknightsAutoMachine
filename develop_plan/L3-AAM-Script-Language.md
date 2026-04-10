# AAMSL (Arknights Auto Machine Script Language) 规范 v1.0

## 1. 概述

AAMSL 是专为明日方舟自动化操作设计的领域特定脚本语言，提供声明式和命令式混合编程模型，支持状态机驱动的战斗自动化。

### 1.1 设计目标
- **简洁性**: 语法接近自然语言，降低学习成本
- **确定性**: 脚本执行结果可预测，便于调试
- **可扩展性**: 支持自定义函数和模块化
- **实时性**: 支持游戏状态实时感知和响应

### 1.2 执行模型
- **事件驱动**: 基于游戏状态变化触发操作
- **协作式多任务**: 通过 `yield` 实现帧级调度
- **状态机**: 内置状态机支持复杂战斗逻辑

---

## 2. 词法规范

### 2.1 字符集
- 源文件编码: UTF-8
- 标识符支持: ASCII字母、数字、下划线、中文字符

### 2.2 关键字
```
// 数据类型
int     float   bool    string  void    tuple

// 控制流
if      else    while   for     switch  case    default
break   continue return

// 脚本元指令
once    // 单次执行标记

// 模块系统
import   as      from

// 其他
yield   // 帧让出
```

### 2.3 运算符
```
// 算术
+  -  *  /  %  **

// 比较
==  !=  <  >  <=  >=

// 逻辑
&&  ||  !

// 位运算
&  |  ^  ~  <<  >>

// 赋值
=  +=  -=  *=  /=  %=
```

### 2.4 字面量
```
// 整数
42  -17  0xFF  0b1010

// 浮点数
3.14  -0.5  1e10

// 布尔
true  false

// 字符串
"hello"  'world'  "干员名称"

// 元组 (用于坐标等)
(3, 3, 1, 0)  // (x, y, dx, dy)
```

### 2.5 注释
```aamsl
// 单行注释

/*
 * 多行注释
 */
```

---

## 3. 语法规范

### 3.1 程序结构
```aamsl
// 元指令（可选，必须位于文件开头）
!once

// 全局变量声明（可选）
int globalCounter = 0

// 函数定义（可选）
function checkAndDeploy(string name, int cost, tuple pos) {
    if (getCosts() >= cost && isDeployable(name)) {
        deploy(name, pos)
        return true
    }
    return false
}

// 主执行块（必须）
while (getGameStatus() == "IN_BATTLE") {
    // 战斗逻辑
    yield(16)  // 让出16ms，约60FPS
}
```

### 3.2 变量声明
```aamsl
// 显式类型声明
int cost = 0
float ratio = 1.5
bool isReady = false
string opName = "望"
tuple position = (3, 3, 1, 0)

// 类型推断（通过初始化）
auto dynamic = 42  // 推断为 int
```

### 3.3 控制流

#### 3.3.1 条件语句
```aamsl
if (cost >= 12) {
    deploy("望", (2, 2, 1, 0))
} else if (cost >= 8) {
    deploy("先锋", (1, 1, 1, 0))
} else {
    yield(16)
}
```

#### 3.3.2 循环语句
```aamsl
// while 循环
while (getGameStatus() == "IN_BATTLE") {
    // 战斗逻辑
    yield(16)
}

// for 循环（数值范围）
for (int i = 0; i < 12; i++) {
    if (isDeployableByIndex(i)) {
        deployByIndex(i, getDefaultPosition(i))
    }
}
```

#### 3.3.3 状态机（switch）
```aamsl
int state = 0
while (getGameStatus() == "IN_BATTLE") {
    int cost = getCosts()
    
    switch (state) {
        case 0:
            if (cost >= 12 && isDeployable("望")) {
                deploy("望", (2, 2, 1, 0))
                state = 1
            }
            break
        case 1:
            if (cost >= 21 && isDeployable("赤刃明霄陈")) {
                deploy("赤刃明霄陈", (2, 3, 1, 0))
                state = 2
            }
            break
        case 2:
            if (cost >= 20 && isDeployable("黍")) {
                deploy("黍", (3, 3, 1, 0))
                state = 3
            }
            break
        default:
            // 所有干员部署完毕，进入技能监控
            monitorSkills()
            break
    }
    yield(16)
}
```

### 3.4 函数定义
```aamsl
// 函数声明语法
function 函数名(参数列表) -> 返回类型 {
    // 函数体
    return 返回值
}

// 示例
function deployIfReady(string name, int requiredCost, tuple position) -> bool {
    if (getCosts() >= requiredCost && isDeployable(name)) {
        deploy(name, position)
        return true
    }
    return false
}

// 使用
while (getGameStatus() == "IN_BATTLE") {
    deployIfReady("望", 12, (2, 2, 1, 0))
    yield(16)
}
```

### 3.5 元指令
```aamsl
// !once - 脚本加载时仅执行一次
!once
// 初始化代码
int initialCost = getCosts()

// 主循环
while (true) {
    // ...
}
```

### 3.6 模块导入

AAMSL 支持模块化编程，允许导入用户库和标准库。

#### 基本导入语法
```aamsl
// 导入整个模块
import "path/to/module.aamsl"

// 导入并指定别名
import "path/to/utils.aamsl" as utils

// 从模块导入特定函数
from "path/to/deploy_lib.aamsl" import deploySequence, retreatAll

// 从模块导入多个函数
from "path/to/operators.aamsl" import deployOperator, useOperatorSkill, OperatorConfig
```

#### 模块搜索路径
解释器按以下顺序搜索模块：
1. 相对路径（相对于当前脚本目录）
2. 用户库目录（`~/.aamsl/lib/` 或 `%USERPROFILE%\.aamsl\lib\`）
3. 标准库目录（`$AAMSL_HOME/lib/`）
4. 内置模块

#### 模块文件结构
```aamsl
// deploy_lib.aamsl - 模块文件示例

// 模块级变量（模块内部共享）
int defaultDeployDelay = 100

// 导出函数（可被其他模块使用）
export function deploySequence(string[] names, tuple[] positions) -> int {
    int successCount = 0
    for (int i = 0; i < names.length; i++) {
        if (deploy(names[i], positions[i])) {
            successCount++
            yield(defaultDeployDelay)
        }
    }
    return successCount
}

// 内部函数（不导出，仅在模块内可用）
function logInternal(string msg) {
    log("[DeployLib] " + msg)
}

// 导出常量
export const int MAX_RETRY = 3
export const string VERSION = "1.0.0"
```

#### 使用导入的模块
```aamsl
// main.aamsl
import "libs/deploy_lib.aamsl" as DeployLib
from "libs/operators.aamsl" import Vanguard, Guard

!once
// 使用模块中的常量
int maxRetry = DeployLib.MAX_RETRY

// 使用模块中的函数
string[] ops = ["桃金娘", "能天使", "塞雷娅"]
tuple[] pos = [(1, 1, 1, 0), (2, 2, 1, 0), (3, 3, 1, 0)]
int deployed = DeployLib.deploySequence(ops, pos)

// 使用导入的类型
Vanguard vanguard = Vanguard("桃金娘", 10)
if (vanguard.canDeploy()) {
    vanguard.deploy((1, 1, 1, 0))
}
```

#### 标准库模块
```aamsl
// 导入标准库
import "std:math" as math      // 数学函数库
import "std:array" as arr      // 数组操作库
import "std:string" as str     // 字符串处理库
import "std:time" as time      // 时间工具库

// 使用标准库
int maxCost = math.max(getCosts(), 20)
string[] names = str.split("望,陈,黍", ",")
int currentTime = time.now()
```

#### 模块命名空间隔离
每个模块拥有独立的命名空间，避免命名冲突：
```aamsl
// module_a.aamsl
export int counter = 0
export function increment() { counter++ }

// module_b.aamsl
export int counter = 100
export function increment() { counter += 10 }

// main.aamsl
import "module_a.aamsl" as A
import "module_b.aamsl" as B

!once
A.increment()  // A.counter = 1
B.increment()  // B.counter = 110
log("A.counter = " + A.counter)  // 输出 1
log("B.counter = " + B.counter)  // 输出 110
```

#### 循环导入检测
解释器自动检测并阻止循环导入：
```aamsl
// a.aamsl
import "b.aamsl"  // 错误：循环导入检测

// b.aamsl
import "a.aamsl"  // 错误：循环导入检测
```

---

## 4. 内置函数 API

### 4.1 游戏状态查询

#### getGameStatus() -> string
获取当前游戏状态。

**返回值:**
- `"UNKNOWN"` - 未知状态
- `"IN_BATTLE"` - 对局中
- `"NOT_IN_BATTLE"` - 非对局状态
- `"TRANSITIONING"` - 状态转换中
- `"ERROR"` - 错误状态

**对应实现:** [game_state_detector.py](../inference/src/vision/game_state_detector.py)
- `GameStateDetector.detect()` 方法
- `GameState` 枚举定义

**复杂度:** O(n + m + k)，n为图像像素，m为ROI像素，k为文本长度

**示例:**
```aamsl
while (getGameStatus() == "IN_BATTLE") {
    // 执行战斗逻辑
    yield(16)
}
```

---

#### getCosts() -> int
获取当前部署费用。

**返回值:** 当前可用费用值（整数）

**对应实现:** 需通过OCR识别游戏界面费用显示区域

**示例:**
```aamsl
int currentCost = getCosts()
if (currentCost >= 20) {
    deploy("重装", (3, 3, 1, 0))
}
```

---

#### getLifePoints() -> int
获取当前生命点数。

**返回值:** 剩余生命点数（整数，通常为0-3）

---

#### getEnemyCount() -> int
获取当前场上敌人数量。

**返回值:** 敌人数量（整数）

---

#### getWaveInfo() -> tuple
获取当前波次信息。

**返回值:** `(currentWave, totalWaves)` 元组

---

### 4.2 干员状态查询

#### getOperatorsStatus(string name) -> string
获取指定干员的当前状态。

**参数:**
- `name`: 干员名称（如 `"望"`、`"赤刃明霄陈"`）

**返回值:**
- `"UNKNOWN"` - 未知状态
- `"IN_SQUAD"` - 在编队内但未进入待部署区
- `"IN_AREA_TO_BE_DEPLOYED"` - 在待部署区
- `"ON_FIELD"` - 已部署在场上
- `"COOLDOWN"` - 再部署冷却中

**对应实现:** [squad_analyzer.py](../inference/src/vision/squad_analyzer.py)
- `SquadAnalyzer.analyze()` 方法
- 通过图像识别编队界面和待部署区

**复杂度:** O(n)，n为图像像素数

**示例:**
```aamsl
if (getOperatorsStatus("望") == "IN_AREA_TO_BE_DEPLOYED") {
    if (getCosts() >= 12) {
        deploy("望", (2, 2, 1, 0))
    }
}
```

---

#### getOperatorsStatus(int index) -> string
通过待部署区索引获取干员状态。

**参数:**
- `index`: 待部署区位置索引（从右到左，0-based）

**返回值:** 同上

**示例:**
```aamsl
// 检查待部署区最右侧干员
if (getOperatorsStatus(0) == "IN_AREA_TO_BE_DEPLOYED") {
    // ...
}
```

---

#### isDeployable(string name) -> bool
检查干员是否可部署（在待部署区且费用足够）。

**参数:**
- `name`: 干员名称

**返回值:** 是否可部署

**示例:**
```aamsl
if (isDeployable("望") && getCosts() >= 12) {
    deploy("望", (2, 2, 1, 0))
}
```

---

#### isDeployable(int index) -> bool
通过索引检查干员是否可部署。

**参数:**
- `index`: 待部署区索引（从右到左）

**返回值:** 是否可部署

---

#### getOperatorCost(string name) -> int
获取干员部署费用。

**参数:**
- `name`: 干员名称

**返回值:** 部署所需费用

**对应实现:** [data_manager.py](../inference/src/data/providers/data_manager.py)
- `DataManager.get_operator()` 方法
- 从数据库查询干员基础费用

---

### 4.3 编队操作

#### getSquad() -> SquadInfo
获取当前编队信息。

**返回值:** 编队信息对象

**对应实现:** [squad_recognizer.py](../inference/src/vision/squad_recognizer.py)
- `SquadRecognizer.recognize_squad()` 方法
- `SquadAnalyzer.analyze()` 方法

---

#### getAreaNumberToBeDeployed(string name) -> int
获取干员在待部署区的位置编号。

**参数:**
- `name`: 干员名称

**返回值:** 待部署区索引（从右到左，0-based），未找到返回-1

**示例:**
```aamsl
int index = getAreaNumberToBeDeployed("望")
if (index >= 0) {
    deployOperators(index, (2, 2, 1, 0))
}
```

---

### 4.4 部署操作

#### deploy(string name, tuple position) -> bool
部署指定干员到指定位置。

**参数:**
- `name`: 干员名称
- `position`: 部署坐标元组 `(x, y, dx, dy)`
  - `x, y`: 地图网格坐标
  - `dx, dy`: 方向偏移量（表示干员朝向）

**返回值:** 部署是否成功

**坐标系说明:**
- 地图坐标系原点在左上角
- x轴向右递增，y轴向下递增
- 方向向量 `(dx, dy)` 表示干员朝向：
  - `(1, 0)` - 向右
  - `(-1, 0)` - 向左
  - `(0, 1)` - 向下
  - `(0, -1)` - 向上

**示例:**
```aamsl
// 在坐标(3,3)部署干员，朝向右侧
deploy("黍", (3, 3, 1, 0))

// 在坐标(2,2)部署干员，朝向上方
deploy("望", (2, 2, 0, -1))
```

---

#### deploy(int index, tuple position) -> bool
通过待部署区索引部署干员。

**参数:**
- `index`: 待部署区索引（从右到左，0-based）
- `position`: 部署坐标元组

**返回值:** 部署是否成功

**示例:**
```aamsl
// 部署待部署区最右侧的干员到(2,2)
deploy(0, (2, 2, 1, 0))
```

---

#### deployOperators(int index, tuple position) -> bool
`deploy()` 的别名函数，用于兼容旧版语法。

---

#### retreat(string name) -> bool
撤退指定干员。

**参数:**
- `name`: 干员名称

**返回值:** 撤退是否成功

---

#### retreat(int index) -> bool
通过场上索引撤退干员。

**参数:**
- `index`: 场上干员索引

**返回值:** 撤退是否成功

---

### 4.5 技能操作

#### useSkill(string name) -> bool
使用指定干员的技能。

**参数:**
- `name`: 干员名称

**返回值:** 技能是否成功使用

**示例:**
```aamsl
if (isSkillReady("望")) {
    useSkill("望")
}
```

---

#### useSkill(int index) -> bool
通过场上索引使用技能。

**参数:**
- `index`: 场上干员索引

**返回值:** 技能是否成功使用

---

#### isSkillReady(string name) -> bool
检查干员技能是否就绪。

**参数:**
- `name`: 干员名称

**返回值:** 技能是否可释放

---

#### getSkillCooldown(string name) -> float
获取干员技能冷却时间。

**参数:**
- `name`: 干员名称

**返回值:** 剩余冷却时间（秒），0表示就绪

---

### 4.6 环境变量

#### getEnvironment(string key) -> any
获取环境变量值。

**参数:**
- `key`: 环境变量名

**返回值:** 变量值

**预定义环境变量:**
- `"SQUAD"` - 当前编队信息
- `"STAGE"` - 当前关卡信息
- `"CONFIG"` - 脚本配置

**示例:**
```aamsl
SquadInfo squad = getEnvironment("SQUAD")
int index = squad.getAreaNumberToBeDeployed("望")
```

---

#### setEnvironment(string key, any value) -> void
设置环境变量值。

**参数:**
- `key`: 环境变量名
- `value`: 变量值

---

### 4.7 时间控制

#### yield(int milliseconds) -> void
让出执行权，等待指定毫秒数。

**参数:**
- `milliseconds`: 等待时间（毫秒）

**说明:**
- 这是协作式调度的核心机制
- 建议每帧调用一次，约16ms对应60FPS
- 过长的等待可能导致错过关键时机

**示例:**
```aamsl
while (getGameStatus() == "IN_BATTLE") {
    // 执行逻辑
    checkAndDeploy()
    
    // 让出约一帧的时间
    yield(16)
}
```

---

#### sleep(int milliseconds) -> void
阻塞等待指定毫秒数（不推荐在战斗中使用）。

**参数:**
- `milliseconds`: 等待时间（毫秒）

**警告:** 使用 `sleep` 会阻塞脚本执行，可能导致错过游戏状态变化。优先使用 `yield`。

---

#### getTimestamp() -> int
获取当前时间戳（毫秒）。

**返回值:** 自脚本启动以来的毫秒数

---

### 4.8 日志与调试

#### log(string message) -> void
输出日志信息。

**参数:**
- `message`: 日志内容

**示例:**
```aamsl
log("开始部署望，当前费用: " + getCosts())
deploy("望", (2, 2, 1, 0))
log("部署完成")
```

---

#### logInfo(string message) -> void
输出信息级别日志。

---

#### logWarning(string message) -> void
输出警告级别日志。

---

#### logError(string message) -> void
输出错误级别日志。

---

#### screenshot(string filename) -> bool
保存当前屏幕截图。

**参数:**
- `filename`: 截图文件名（可选，自动生成时间戳）

**返回值:** 截图是否成功保存

---

## 5. 数据类型

### 5.1 基本类型

| 类型 | 说明 | 示例 |
|------|------|------|
| `int` | 整数 | `42`, `-17` |
| `float` | 浮点数 | `3.14`, `-0.5` |
| `bool` | 布尔值 | `true`, `false` |
| `string` | 字符串 | `"hello"`, `'world'` |
| `void` | 无返回值 | 函数返回类型 |

### 5.2 复合类型

#### tuple
固定长度、异构类型的有序集合。

```aamsl
tuple position = (3, 3, 1, 0)  // (x, y, dx, dy)
tuple waveInfo = getWaveInfo()  // (current, total)
```

#### array
同类型元素的有序集合。

```aamsl
int[] costs = [12, 21, 20, 18]
string[] operators = ["望", "陈", "黍"]
```

### 5.3 对象类型

#### SquadInfo
编队信息对象。

**属性:**
- `operators: OperatorInfo[]` - 干员列表
- `totalCount: int` - 干员总数
- `sortedOperators: OperatorInfo[]` - 排序后的干员列表

**方法:**
- `getAreaNumberToBeDeployed(string name) -> int` - 获取干员在待部署区的索引

#### OperatorInfo
干员信息对象。

**属性:**
- `name: string` - 干员名称
- `eliteLevel: int` - 精英化等级
- `level: int` - 等级
- `cost: int` - 部署费用
- `profession: string` - 职业

---

## 6. 执行语义

### 6.1 脚本生命周期

```
1. 加载脚本
   ↓
2. 解析语法（词法分析 → 语法分析）
   ↓
3. 语义检查（类型检查、函数存在性验证）
   ↓
4. 执行 !once 块（如果有）
   ↓
5. 进入主循环
   ↓
6. 逐行执行 / 遇到 yield 让出
   ↓
7. 游戏状态变化 → 恢复执行
   ↓
8. 脚本结束或手动停止
```

### 6.2 调度模型

AAMSL 采用协作式多任务调度：

1. 脚本执行直到遇到 `yield` 或阻塞操作
2. 解释器保存执行上下文（程序计数器、变量状态）
3. 游戏主循环继续运行
4. 达到 yield 时间后，恢复脚本执行

```aamsl
// 示例：协作式调度
while (true) {
    // 执行一些操作
    doSomething()
    
    // 让出控制权，16ms后恢复
    // 这期间游戏继续渲染，用户输入被处理
    yield(16)
    
    // 恢复执行
    doMoreThings()
}
```

### 6.3 错误处理

#### 运行时错误
- 函数参数类型不匹配
- 访问未定义变量
- 数组越界
- 除以零

**行为:** 记录错误日志，终止脚本执行。

#### 游戏状态错误
- 干员不在待部署区时尝试部署
- 费用不足时尝试部署
- 坐标超出地图范围

**行为:** 返回 `false`，记录警告日志，脚本继续执行。

---

## 7. 示例脚本

### 7.1 基础部署脚本
```aamsl
!once

// 简单的顺序部署
while (getGameStatus() == "IN_BATTLE") {
    int cost = getCosts()
    
    // 部署望
    if (cost >= 12 && isDeployable("望")) {
        deploy("望", (2, 2, 1, 0))
        log("部署望完成")
    }
    
    // 部署陈
    if (cost >= 21 && isDeployable("赤刃明霄陈")) {
        deploy("赤刃明霄陈", (2, 3, 1, 0))
        log("部署陈完成")
    }
    
    yield(16)
}
```

### 7.2 状态机脚本
```aamsl
!once

int state = 0
int deployCount = 0

while (getGameStatus() == "IN_BATTLE") {
    int cost = getCosts()
    
    switch (state) {
        case 0:  // 部署先锋
            if (cost >= 10 && isDeployable("桃金娘")) {
                deploy("桃金娘", (1, 1, 1, 0))
                state = 1
                deployCount++
            }
            break
            
        case 1:  // 部署狙击
            if (cost >= 14 && isDeployable("能天使")) {
                deploy("能天使", (2, 2, 1, 0))
                state = 2
                deployCount++
            }
            break
            
        case 2:  // 部署重装
            if (cost >= 18 && isDeployable("塞雷娅")) {
                deploy("塞雷娅", (3, 3, 1, 0))
                state = 3
                deployCount++
            }
            break
            
        default:
            // 监控技能
            if (isSkillReady("桃金娘")) {
                useSkill("桃金娘")
            }
            break
    }
    
    yield(16)
}

log("战斗结束，共部署 " + deployCount + " 名干员")
```

### 7.3 高级脚本（带错误处理）
```aamsl
!once

// 定义部署配置
struct DeployConfig {
    string name
    int cost
    tuple position
    int priority
}

// 初始化部署队列
DeployConfig[] deployQueue = [
    {"望", 12, (2, 2, 1, 0), 1},
    {"赤刃明霄陈", 21, (2, 3, 1, 0), 2},
    {"黍", 20, (3, 3, 1, 0), 3}
]

int currentIndex = 0
int retryCount = 0
const int MAX_RETRY = 3

function tryDeploy(DeployConfig config) -> bool {
    if (!isDeployable(config.name)) {
        logWarning(config.name + " 不在待部署区")
        return false
    }
    
    if (getCosts() < config.cost) {
        return false  // 费用不足，等待
    }
    
    bool success = deploy(config.name, config.position)
    if (success) {
        log("成功部署 " + config.name)
        retryCount = 0
    } else {
        retryCount++
        logError("部署 " + config.name + " 失败，重试次数: " + retryCount)
    }
    
    return success
}

// 主循环
while (getGameStatus() == "IN_BATTLE") {
    // 检查是否全部部署完成
    if (currentIndex >= deployQueue.length) {
        // 进入技能监控模式
        for (int i = 0; i < deployQueue.length; i++) {
            if (isSkillReady(deployQueue[i].name)) {
                useSkill(deployQueue[i].name)
            }
        }
        yield(16)
        continue
    }
    
    // 尝试部署当前干员
    DeployConfig current = deployQueue[currentIndex]
    if (tryDeploy(current)) {
        currentIndex++
    } else if (retryCount >= MAX_RETRY) {
        logError("部署 " + current.name + " 超过最大重试次数，跳过")
        currentIndex++
        retryCount = 0
    }
    
    yield(16)
}

log("脚本正常结束")
```

---

## 8. 解释器实现要求

### 8.1 架构设计（Tree-Walking Interpreter）

考虑到 AAMSL 的执行特性（IO密集型、不需要极高执行速度），采用 **Tree-Walking Interpreter（树遍历解释器）** 架构，而非VM架构。这种架构更简单、易于调试、启动更快。

```
┌─────────────────────────────────────────────────────────────┐
│                   AAMSL Interpreter                          │
├─────────────────────────────────────────────────────────────┤
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐      │
│  │   Lexer      │→ │   Parser     │→ │     AST      │      │
│  │  (词法分析)   │  │  (语法分析)   │  │  (抽象语法树) │      │
│  └──────────────┘  └──────────────┘  └──────┬───────┘      │
├─────────────────────────────────────────────┼──────────────┤
│  ┌──────────────────────────────────────────┘              │
│  │         Tree-Walking Evaluator                           │
│  │  ┌─────────────────────────────────────────────────┐    │
│  │  │  // 直接遍历AST节点执行，无需编译为字节码          │    │
│  │  │  eval(node):                                     │    │
│  │  │    if node is BinaryOp:                          │    │
│  │  │      return eval(node.left) + eval(node.right)   │    │
│  │  │    if node is FunctionCall:                      │    │
│  │  │      return call_function(node.name, args)       │    │
│  │  │    ...                                           │    │
│  │  └─────────────────────────────────────────────────┘    │
│  └─────────────────────────────────────────────────────────┘
├─────────────────────────────────────────────────────────────┤
│  ┌──────────────────────────────────────────────────────┐  │
│  │              Environment & Scope Chain                │  │
│  │  ┌──────────┐    ┌──────────┐    ┌──────────┐       │  │
│  │  │  Global  │───→│  Module  │───→│  Local   │       │  │
│  │  │  Scope   │    │  Scope   │    │  Scope   │       │  │
│  │  └──────────┘    └──────────┘    └──────────┘       │  │
│  └──────────────────────────────────────────────────────┘  │
├─────────────────────────────────────────────────────────────┤
│  ┌──────────────────────────────────────────────────────┐  │
│  │              Built-in Function Registry               │  │
│  │  • getGameStatus()  • deploy()  • yield()            │  │
│  │  • getCosts()       • retreat() • log()              │  │
│  │  • isDeployable()   • useSkill() • ...               │  │
│  └──────────────────────────────────────────────────────┘  │
├─────────────────────────────────────────────────────────────┤
│  ┌──────────────────────────────────────────────────────┐  │
│  │              Module Loader & Cache                    │  │
│  │  • 模块搜索路径解析  • 模块缓存  • 循环导入检测         │  │
│  └──────────────────────────────────────────────────────┘  │
├─────────────────────────────────────────────────────────────┤
│  ┌──────────────────────────────────────────────────────┐  │
│  │              Game Interface Adapter                   │  │
│  │         (与 inference 模块的桥接层)                    │  │
│  └──────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
```

#### 为什么选择 Tree-Walking Interpreter？

| 特性 | Tree-Walking | VM (字节码) |
|------|--------------|-------------|
| 实现复杂度 | 低 | 高 |
| 启动时间 | 极快（无需编译） | 快（需编译为字节码） |
| 执行速度 | 足够（IO密集型任务） | 快（CPU密集型任务） |
| 调试友好性 | 极佳（直接映射到源码） | 一般（需调试映射） |
| 内存占用 | 低 | 中等 |
| 适用场景 | **AAMSL（游戏自动化）** | 计算密集型脚本 |

AAMSL 的主要耗时在于游戏状态检测（图像识别OCR）和等待，而非脚本本身的计算，因此 Tree-Walking 架构完全满足需求。

### 8.2 核心组件说明

#### 8.2.1 词法分析器（Lexer）
```python
# 伪代码示例
class Lexer:
    def tokenize(self, source: str) -> List[Token]:
        # 将源代码转换为Token序列
        # 时间复杂度: O(n)，n为源代码长度
        pass
```

#### 8.2.2 语法分析器（Parser）
```python
class Parser:
    def parse(self, tokens: List[Token]) -> ASTNode:
        # 将Token序列解析为AST
        # 时间复杂度: O(n)，单次遍历
        pass
```

#### 8.2.3 树遍历求值器（Evaluator）
```python
class Evaluator:
    def __init__(self):
        self.global_env = Environment()  # 全局环境
        self.module_cache = {}           # 模块缓存
        
    def eval(self, node: ASTNode, env: Environment) -> Value:
        # 直接遍历AST节点执行
        # 无需编译为字节码，无需VM
        if isinstance(node, BinaryOp):
            left = self.eval(node.left, env)
            right = self.eval(node.right, env)
            return self.apply_op(node.op, left, right)
        elif isinstance(node, FunctionCall):
            func = self.eval(node.func, env)
            args = [self.eval(arg, env) for arg in node.args]
            return self.call_function(func, args)
        # ... 其他节点类型
```

#### 8.2.4 环境（Environment）与作用域链
```python
class Environment:
    def __init__(self, parent: Optional['Environment'] = None):
        self.variables = {}          # 当前作用域变量
        self.parent = parent         # 父作用域（形成链）
        self.exports = {}            # 导出的符号
        
    def define(self, name: str, value: Value, is_export: bool = False):
        self.variables[name] = value
        if is_export:
            self.exports[name] = value
            
    def get(self, name: str) -> Value:
        # 沿作用域链查找变量
        if name in self.variables:
            return self.variables[name]
        if self.parent:
            return self.parent.get(name)
        raise NameError(f"未定义变量: {name}")
```

### 8.3 模块加载机制

```python
class ModuleLoader:
    def __init__(self):
        self.cache = {}              # 已加载模块缓存
        self.loading = set()         # 正在加载的模块（用于循环检测）
        self.search_paths = [        # 模块搜索路径
            "./",                    # 当前目录
            "~/.aamsl/lib/",         # 用户库
            "$AAMSL_HOME/lib/",      # 标准库
        ]
    
    def load(self, module_path: str, current_dir: str) -> Module:
        # 1. 解析完整路径
        full_path = self.resolve_path(module_path, current_dir)
        
        # 2. 检查缓存
        if full_path in self.cache:
            return self.cache[full_path]
        
        # 3. 循环导入检测
        if full_path in self.loading:
            raise CircularImportError(f"循环导入检测: {module_path}")
        
        # 4. 加载并执行模块
        self.loading.add(full_path)
        source = self.read_file(full_path)
        ast = self.parse(source)
        module_env = self.execute_module(ast, full_path)
        self.loading.remove(full_path)
        
        # 5. 缓存模块
        module = Module(full_path, module_env.exports)
        self.cache[full_path] = module
        return module
```

### 8.4 性能要求

- **启动时间**: 脚本加载到开始执行 < 50ms（Tree-Walking无需编译）
- **解析速度**: 1000行代码 < 10ms
- **执行延迟**: 单条语句执行 < 0.1ms（AST遍历）
- **内存占用**: 典型脚本 < 5MB（无VM开销）
- **帧率保持**: yield(16) 模式下保持 60 FPS

### 8.5 内置函数映射

解释器需要将 AAMSL 内置函数映射到 inference 模块的实际实现：

| AAMSL 函数 | inference 实现 | 文件位置 |
|-----------|---------------|----------|
| `getGameStatus()` | `GameStateDetector.detect()` | vision/game_state_detector.py |
| `getOperatorsStatus()` | `SquadAnalyzer.analyze()` | vision/squad_analyzer.py |
| `getSquad()` | `SquadRecognizer.recognize_squad()` | vision/squad_recognizer.py |
| `getOperatorCost()` | `DataManager.get_operator()` | data/providers/data_manager.py |

---

## 9. 版本历史

| 版本 | 日期 | 变更说明 |
|------|------|----------|
| 1.0.0 | 2026-04-10 | 初始版本，定义基础语法和核心API |

---

## 10. 附录

### 10.1 保留字列表
```
bool      break     case      continue  default   else
export    false     float     for       from      function
if        import    int       once      return    string
switch    true      tuple     auto      void      while
yield
```

### 10.2 运算符优先级

| 优先级 | 运算符 | 结合性 |
|--------|--------|--------|
| 1 | `()` `[]` `.` | 左到右 |
| 2 | `++` `--` (后缀) | 左到右 |
| 3 | `++` `--` (前缀) `!` `~` `-` (一元) | 右到左 |
| 4 | `*` `/` `%` | 左到右 |
| 5 | `+` `-` | 左到右 |
| 6 | `<<` `>>` | 左到右 |
| 7 | `<` `<=` `>` `>=` | 左到右 |
| 8 | `==` `!=` | 左到右 |
| 9 | `&` | 左到右 |
| 10 | `^` | 左到右 |
| 11 | `\|` | 左到右 |
| 12 | `&&` | 左到右 |
| 13 | `\|\|` | 左到右 |
| 14 | `=` `+=` `-=` `*=` `/=` `%=` | 右到左 |

### 10.3 EBNF 语法

```ebnf
program         ::= shebang? metadata? declaration* statement*

shebang         ::= "!once"

metadata        ::= (key "=" value "\n")*

declaration     ::= variable_decl | function_decl

variable_decl   ::= type identifier ("=" expression)? ";"

function_decl   ::= "function" identifier "(" param_list? ")" ("->" type)? block

param_list      ::= param ("," param)*
param           ::= type identifier

statement       ::= expression_stmt
                  | if_stmt
                  | while_stmt
                  | for_stmt
                  | switch_stmt
                  | break_stmt
                  | continue_stmt
                  | return_stmt
                  | block

expression_stmt ::= expression ";"

if_stmt         ::= "if" "(" expression ")" statement ("else" statement)?

while_stmt      ::= "while" "(" expression ")" statement

for_stmt        ::= "for" "(" variable_decl expression ";" expression ")" statement

switch_stmt     ::= "switch" "(" expression ")" "{" case_clause* default_clause? "}"
case_clause     ::= "case" literal ":" statement*
default_clause  ::= "default" ":" statement*

break_stmt      ::= "break" ";"
continue_stmt   ::= "continue" ";"
return_stmt     ::= "return" expression? ";"

block           ::= "{" statement* "}"

expression      ::= assignment

assignment      ::= conditional (assignment_op assignment)?
assignment_op   ::= "=" | "+=" | "-=" | "*=" | "/=" | "%="

conditional     ::= or_expr

or_expr         ::= and_expr ("||" and_expr)*
and_expr        ::= equality ("&&" equality)*
equality        ::= relational (("==" | "!=") relational)*
relational      ::= bitwise (("<" | ">" | "<=" | ">=") bitwise)*
bitwise         ::= shift (("&" | "|" | "^") shift)*
shift           ::= additive (("<<" | ">>") additive)*
additive        ::= multiplicative (("+" | "-") multiplicative)*
multiplicative  ::= unary (("*" | "/" | "%") unary)*
unary           ::= ("!" | "-" | "~" | "++" | "--") unary | postfix
postfix         ::= primary ("++" | "--" | "(" arg_list? ")" | "[" expression "]" | "." identifier)*
arg_list        ::= expression ("," expression)*

primary         ::= literal | identifier | "(" expression ")" | tuple | array

tuple           ::= "(" expression ("," expression)+ ")"
array           ::= "[" (expression ("," expression)*)? "]"

literal         ::= integer | float | string | bool | "null"
integer         ::= [0-9]+
float           ::= [0-9]+ "." [0-9]*
string          ::= "\"" [^"]* "\"" | "'" [^']* "'"
bool            ::= "true" | "false"

type            ::= "int" | "float" | "bool" | "string" | "void" | "tuple" | identifier
identifier      ::= [a-zA-Z_][a-zA-Z0-9_]*

// 模块导入语法
import_stmt     ::= "import" string ("as" identifier)?
                  | "from" string "import" import_list
import_list     ::= identifier ("," identifier)* | "*"

// 导出语法
export_decl     ::= "export" (variable_decl | function_decl | const_decl)
const_decl      ::= "const" type identifier "=" literal ";"
```
